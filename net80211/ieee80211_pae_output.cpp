/*	$OpenBSD: ieee80211_pae_output.c,v 1.16 2010/06/05 15:54:35 damien Exp $	*/

/*-
 * Copyright (c) 2007,2008 Damien Bergamini <damien.bergamini@free.fr>
 *
 * Permission to use, copy, modify, and distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
 * WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
 * ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
 * ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
 * OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 */

/*
 * This code implements the 4-Way Handshake and Group Key Handshake protocols
 * (both Supplicant and Authenticator Key Transmit state machines) defined in
 * IEEE Std 802.11-2007 section 8.5.
 */

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/mbuf.h>
#include <sys/kernel.h>
#include <sys/socket.h>
#include <sys/sockio.h>
#include "sys/endian.h"
#include <sys/errno.h>
#include <sys/proc.h>

#include <net/if.h>
#include <net/if_dl.h>
#include <net/if_media.h>
#include <net/if_arp.h>
#include <net/if_llc.h>
#include <net/bpf.h>

#ifdef INET
#include <netinet/in.h>
#include <netinet/if_ether.h>
#include <netinet/in_systm.h>
#include <netinet/ip.h>
#endif

#include <net80211/ieee80211_var.h>
#include <net80211/ieee80211_priv.h>

#include "Voodoo80211Device.h"

/*
 * Send an EAPOL-Key frame to node `ni'.  If MIC or encryption is required,
 * the PTK must be passed (otherwise it can be set to NULL.)
 */
int Voodoo80211Device::
ieee80211_send_eapol_key(struct ieee80211com *ic, mbuf_t m,
			 struct ieee80211_node *ni, const struct ieee80211_ptk *ptk)
{
	struct ether_header *eh;
	struct ieee80211_eapol_key *key;
	u_int16_t info;
	int s, len, error;
	
	mbuf_prepend(&m, sizeof(struct ether_header), MBUF_DONTWAIT);
	if (m == NULL)
		return ENOMEM;
	/* no need to m_pullup here (ok by construction) */
	eh = mtod(m, struct ether_header *);
	eh->ether_type = htons(ETHERTYPE_PAE);
	IEEE80211_ADDR_COPY(eh->ether_shost, ic->ic_myaddr);
	IEEE80211_ADDR_COPY(eh->ether_dhost, ni->ni_macaddr);
	
	key = (struct ieee80211_eapol_key *)&eh[1];
	key->version = EAPOL_VERSION;
	key->type = EAPOL_KEY;
	key->desc = (ni->ni_rsnprotos == IEEE80211_PROTO_RSN) ?
	EAPOL_KEY_DESC_IEEE80211 : EAPOL_KEY_DESC_WPA;
	
	info = BE_READ_2(key->info);
	/* use V3 descriptor if KDF is SHA256-based */
	if (ieee80211_is_sha256_akm((enum ieee80211_akm)ni->ni_rsnakms))
		info |= EAPOL_KEY_DESC_V3;
	/* use V2 descriptor if pairwise or group cipher is CCMP */
	else if (ni->ni_rsncipher == IEEE80211_CIPHER_CCMP ||
		 ni->ni_rsngroupcipher == IEEE80211_CIPHER_CCMP)
		info |= EAPOL_KEY_DESC_V2;
	else
		info |= EAPOL_KEY_DESC_V1;
	BE_WRITE_2(key->info, info);
	
	len = mbuf_len(m) - sizeof(struct ether_header);
	BE_WRITE_2(key->paylen, len - sizeof(*key));
	BE_WRITE_2(key->len, len - 4);
	
	if (info & EAPOL_KEY_KEYMIC)
		ieee80211_eapol_key_mic(key, ptk->kck);
	
	len = mbuf_pkthdr_len(m);
	s = splnet();
	getOutputQueue()->enqueue(m, 0);
	getOutputQueue()->start();
	/* TODO
	if (error == 0) {
		ifp->if_obytes += len;
		if ((ifp->if_flags & IFF_OACTIVE) == 0)
			(*ifp->if_start)(ifp);
	}*/
	splx(s);
	
	return error;
}

mbuf_t Voodoo80211Device::
ieee80211_get_eapol_key(int flags, int type, u_int pktlen)
{
	mbuf_t m;
	
	/* reserve space for 802.11 encapsulation and EAPOL-Key header */
	pktlen += sizeof(struct ieee80211_frame) + LLC_SNAPFRAMELEN +
	sizeof(struct ieee80211_eapol_key);
	
	if (pktlen > MCLBYTES)
		panic("EAPOL-Key frame too large: %u", pktlen);
	mbuf_gethdr(flags, type, &m);
	if (m == NULL)
		return NULL;
	if (pktlen > mbuf_get_mhlen()) {
		mbuf_getcluster(flags, type, MCLBYTES, &m);
		if (!(mbuf_flags(m) & MBUF_EXT))
			return mbuf_free(m);
	}
	mbuf_setdata(m, (char*)mbuf_data(m) + sizeof(struct ieee80211_frame) + LLC_SNAPFRAMELEN, mbuf_len(m) - sizeof(struct ieee80211_frame) + LLC_SNAPFRAMELEN);
	return m;
}

/*
 * Send 4-Way Handshake Message 2 to the authenticator.
 */
int Voodoo80211Device::
ieee80211_send_4way_msg2(struct ieee80211com *ic, struct ieee80211_node *ni,
			 const u_int8_t *replaycnt, const struct ieee80211_ptk *tptk)
{
	struct ieee80211_eapol_key *key;
	mbuf_t m;
	u_int16_t info;
	u_int8_t *frm;
	
	m = ieee80211_get_eapol_key(MBUF_DONTWAIT, MT_DATA,
				    (ni->ni_rsnprotos == IEEE80211_PROTO_WPA) ?
				    2 + IEEE80211_WPAIE_MAXLEN :
				    2 + IEEE80211_RSNIE_MAXLEN);
	if (m == NULL)
		return ENOMEM;
	key = mtod(m, struct ieee80211_eapol_key *);
	memset(key, 0, sizeof(*key));
	
	info = EAPOL_KEY_PAIRWISE | EAPOL_KEY_KEYMIC;
	BE_WRITE_2(key->info, info);
	
	/* copy key replay counter from Message 1/4 */
	memcpy(key->replaycnt, replaycnt, 8);
	
	/* copy the supplicant's nonce (SNonce) */
	memcpy(key->nonce, ic->ic_nonce, EAPOL_KEY_NONCE_LEN);
	
	frm = (u_int8_t *)&key[1];
	/* add the WPA/RSN IE used in the (Re)Association Request */
	if (ni->ni_rsnprotos == IEEE80211_PROTO_WPA) {
		int keylen;
		frm = ieee80211_add_wpa(frm, ic, ni);
		/* WPA sets the key length field here */
		keylen = ieee80211_cipher_keylen(ni->ni_rsncipher);
		BE_WRITE_2(key->keylen, keylen);
	} else	/* RSN */
		frm = ieee80211_add_rsn(frm, ic, ni);
	
	mbuf_pkthdr_setlen(m, frm - (u_int8_t *)key);
	mbuf_setlen(m, frm - (u_int8_t *)key);
	
	/* TODO debug
	if (ic->ic_if.if_flags & IFF_DEBUG)
		printf("%s: sending msg %d/%d of the %s handshake to %s\n",
		       ic->ic_if.if_xname, 2, 4, "4-way",
		       ether_sprintf(ni->ni_macaddr));
	*/
	return ieee80211_send_eapol_key(ic, m, ni, tptk);
}

/*
 * Send 4-Way Handshake Message 4 to the authenticator.
 */
int Voodoo80211Device::
ieee80211_send_4way_msg4(struct ieee80211com *ic, struct ieee80211_node *ni)
{
	struct ieee80211_eapol_key *key;
	mbuf_t m;
	u_int16_t info;
	
	m = ieee80211_get_eapol_key(MBUF_DONTWAIT, MT_DATA, 0);
	if (m == NULL)
		return ENOMEM;
	key = mtod(m, struct ieee80211_eapol_key *);
	memset(key, 0, sizeof(*key));
	
	info = EAPOL_KEY_PAIRWISE | EAPOL_KEY_KEYMIC;
	
	/* copy key replay counter from authenticator */
	BE_WRITE_8(key->replaycnt, ni->ni_replaycnt);
	
	if (ni->ni_rsnprotos == IEEE80211_PROTO_WPA) {
		int keylen;
		/* WPA sets the key length field here */
		keylen = ieee80211_cipher_keylen(ni->ni_rsncipher);
		BE_WRITE_2(key->keylen, keylen);
	} else
		info |= EAPOL_KEY_SECURE;
	
	/* write the key info field */
	BE_WRITE_2(key->info, info);
	
	/* empty key data field */
	mbuf_pkthdr_setlen(m, sizeof(*key));
	mbuf_setlen(m, sizeof(*key));
	
	/* TODO
	if (ic->ic_if.if_flags & IFF_DEBUG)
		printf("%s: sending msg %d/%d of the %s handshake to %s\n",
		       ic->ic_if.if_xname, 4, 4, "4-way",
		       ether_sprintf(ni->ni_macaddr));
	*/
	return ieee80211_send_eapol_key(ic, m, ni, &ni->ni_ptk);
}

/*
 * Send Group Key Handshake Message 2 to the authenticator.
 */
int Voodoo80211Device::
ieee80211_send_group_msg2(struct ieee80211com *ic, struct ieee80211_node *ni,
			  const struct ieee80211_key *k)
{
	struct ieee80211_eapol_key *key;
	u_int16_t info;
	mbuf_t m;
	
	m = ieee80211_get_eapol_key(MBUF_DONTWAIT, MT_DATA, 0);
	if (m == NULL)
		return ENOMEM;
	key = mtod(m, struct ieee80211_eapol_key *);
	memset(key, 0, sizeof(*key));
	
	info = EAPOL_KEY_KEYMIC | EAPOL_KEY_SECURE;
	
	/* copy key replay counter from authenticator */
	BE_WRITE_8(key->replaycnt, ni->ni_replaycnt);
	
	if (ni->ni_rsnprotos == IEEE80211_PROTO_WPA) {
		/* WPA sets the key length and key id fields here */
		BE_WRITE_2(key->keylen, k->k_len);
		info |= (k->k_id & 3) << EAPOL_KEY_WPA_KID_SHIFT;
	}
	
	/* write the key info field */
	BE_WRITE_2(key->info, info);
	
	/* empty key data field */
	mbuf_pkthdr_setlen(m, sizeof(*key));
	mbuf_setlen(m, sizeof(*key));
	
	/* TODO debug
	if (ic->ic_if.if_flags & IFF_DEBUG)
		printf("%s: sending msg %d/%d of the %s handshake to %s\n",
		       ic->ic_if.if_xname, 2, 2, "group key",
		       ether_sprintf(ni->ni_macaddr));
	*/
	
	return ieee80211_send_eapol_key(ic, m, ni, &ni->ni_ptk);
}

/*
 * EAPOL-Key Request frames are sent by the supplicant to request that the
 * authenticator initiates either a 4-Way Handshake or Group Key Handshake,
 * or to report a MIC failure in a TKIP MSDU.
 */
int Voodoo80211Device::
ieee80211_send_eapol_key_req(struct ieee80211com *ic,
			     struct ieee80211_node *ni, u_int16_t info, u_int64_t tsc)
{
	struct ieee80211_eapol_key *key;
	mbuf_t m;
	
	m = ieee80211_get_eapol_key(MBUF_DONTWAIT, MT_DATA, 0);
	if (m == NULL)
		return ENOMEM;
	key = mtod(m, struct ieee80211_eapol_key *);
	memset(key, 0, sizeof(*key));
	
	info |= EAPOL_KEY_REQUEST;
	BE_WRITE_2(key->info, info);
	
	/* in case of TKIP MIC failure, fill the RSC field */
	if (info & EAPOL_KEY_ERROR)
		LE_WRITE_6(key->rsc, tsc);
	
	/* use our separate key replay counter for key requests */
	BE_WRITE_8(key->replaycnt, ni->ni_reqreplaycnt);
	ni->ni_reqreplaycnt++;
	
	/* TODO debug
	if (ic->ic_if.if_flags & IFF_DEBUG)
		printf("%s: sending EAPOL-Key request to %s\n",
		       ic->ic_if.if_xname, ether_sprintf(ni->ni_macaddr));
	*/
	return ieee80211_send_eapol_key(ic, m, ni, &ni->ni_ptk);
}