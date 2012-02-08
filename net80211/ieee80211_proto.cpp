/*	$OpenBSD: ieee80211_proto.c,v 1.46 2012/01/18 14:35:56 stsp Exp $	*/
/*	$NetBSD: ieee80211_proto.c,v 1.8 2004/04/30 23:58:20 dyoung Exp $	*/

/*-
 * Copyright (c) 2001 Atsushi Onoe
 * Copyright (c) 2002, 2003 Sam Leffler, Errno Consulting
 * Copyright (c) 2008, 2009 Damien Bergamini
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. The name of the author may not be used to endorse or promote products
 *    derived from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 * NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

/*
 * IEEE 802.11 protocol support.
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
#include <sys/sysctl.h>

#include <net/if.h>
#include <net/if_dl.h>
#include <net/if_media.h>
#include <net/if_arp.h>
#include <net/if_llc.h>

#if NBPFILTER > 0
#include <net/bpf.h>
#endif

#ifdef INET
#include <netinet/in.h>
#include <netinet/if_ether.h>
#endif

#include "Voodoo80211Device.h"

const char * const ieee80211_mgt_subtype_name[] = {
	"assoc_req",	"assoc_resp",	"reassoc_req",	"reassoc_resp",
	"probe_req",	"probe_resp",	"reserved#6",	"reserved#7",
	"beacon",	"atim",		"disassoc",	"auth",
	"deauth",	"action",	"action_noack",	"reserved#15"
};
const char * const ieee80211_state_name[IEEE80211_S_MAX] = {
	"INIT",		/* IEEE80211_S_INIT */
	"SCAN",		/* IEEE80211_S_SCAN */
	"AUTH",		/* IEEE80211_S_AUTH */
	"ASSOC",	/* IEEE80211_S_ASSOC */
	"RUN"		/* IEEE80211_S_RUN */
};
const char * const ieee80211_phymode_name[] = {
	"auto",		/* IEEE80211_MODE_AUTO */
	"11a",		/* IEEE80211_MODE_11A */
	"11b",		/* IEEE80211_MODE_11B */
	"11g",		/* IEEE80211_MODE_11G */
	"turbo",	/* IEEE80211_MODE_TURBO */
};

void Voodoo80211Device::
ieee80211_proto_attach(struct ieee80211com *ic)
{
	// XXX: ifp->if_hdrlen = sizeof(struct ieee80211_frame);
	// assuming that IO80211Interface automatically sets this (setMediaHeaderLength() is protected)
	
#ifdef notdef
	ic->ic_rtsthreshold = IEEE80211_RTS_DEFAULT;
#else
	ic->ic_rtsthreshold = IEEE80211_RTS_MAX;
#endif
	ic->ic_fragthreshold = 2346;		/* XXX not used yet */
	ic->ic_fixed_rate = -1;			/* no fixed rate */
	ic->ic_protmode = IEEE80211_PROT_CTSONLY;
}

void Voodoo80211Device::
ieee80211_proto_detach(struct ieee80211com *ic)
{
	//TODO ic->ic_mgtq->flush();
	//TODO ic->ic_pwrsaveq->flush();
}

void Voodoo80211Device::
ieee80211_print_essid(const u_int8_t *essid, int len)
{
	int i;
	const u_int8_t *p;
	
	if (len > IEEE80211_NWID_LEN)
		len = IEEE80211_NWID_LEN;
	/* determine printable or not */
	for (i = 0, p = essid; i < len; i++, p++) {
		if (*p < ' ' || *p > 0x7e)
			break;
	}
	if (i == len) {
		printf("\"");
		for (i = 0, p = essid; i < len; i++, p++)
			printf("%c", *p);
		printf("\"");
	} else {
		printf("0x");
		for (i = 0, p = essid; i < len; i++, p++)
			printf("%02x", *p);
	}
}

#ifdef IEEE80211_DEBUG
void Voodoo80211Device::
ieee80211_dump_pkt(const u_int8_t *buf, int len, int rate, int rssi)
{
	struct ieee80211_frame *wh;
	int i;
	
	wh = (struct ieee80211_frame *)buf;
	switch (wh->i_fc[1] & IEEE80211_FC1_DIR_MASK) {
		case IEEE80211_FC1_DIR_NODS:
			printf("NODS %s", ether_sprintf(wh->i_addr2));
			printf("->%s", ether_sprintf(wh->i_addr1));
			printf("(%s)", ether_sprintf(wh->i_addr3));
			break;
		case IEEE80211_FC1_DIR_TODS:
			printf("TODS %s", ether_sprintf(wh->i_addr2));
			printf("->%s", ether_sprintf(wh->i_addr3));
			printf("(%s)", ether_sprintf(wh->i_addr1));
			break;
		case IEEE80211_FC1_DIR_FROMDS:
			printf("FRDS %s", ether_sprintf(wh->i_addr3));
			printf("->%s", ether_sprintf(wh->i_addr1));
			printf("(%s)", ether_sprintf(wh->i_addr2));
			break;
		case IEEE80211_FC1_DIR_DSTODS:
			printf("DSDS %s", ether_sprintf((u_int8_t *)&wh[1]));
			printf("->%s", ether_sprintf(wh->i_addr3));
			printf("(%s", ether_sprintf(wh->i_addr2));
			printf("->%s)", ether_sprintf(wh->i_addr1));
			break;
	}
	switch (wh->i_fc[0] & IEEE80211_FC0_TYPE_MASK) {
		case IEEE80211_FC0_TYPE_DATA:
			printf(" data");
			break;
		case IEEE80211_FC0_TYPE_MGT:
			printf(" %s", ieee80211_mgt_subtype_name[
								 (wh->i_fc[0] & IEEE80211_FC0_SUBTYPE_MASK)
								 >> IEEE80211_FC0_SUBTYPE_SHIFT]);
			break;
		default:
			printf(" type#%d", wh->i_fc[0] & IEEE80211_FC0_TYPE_MASK);
			break;
	}
	if (wh->i_fc[1] & IEEE80211_FC1_WEP)
		printf(" WEP");
	if (rate >= 0)
		printf(" %d%sM", rate / 2, (rate & 1) ? ".5" : "");
	if (rssi >= 0)
		printf(" +%d", rssi);
	printf("\n");
	if (len > 0) {
		for (i = 0; i < len; i++) {
			if ((i & 1) == 0)
				printf(" ");
			printf("%02x", buf[i]);
		}
		printf("\n");
	}
}
#endif

int Voodoo80211Device::
ieee80211_fix_rate(struct ieee80211com *ic, struct ieee80211_node *ni,
		   int flags)
{
#define	RV(v)	((v) & IEEE80211_RATE_VAL)
	int i, j, ignore, error;
	int okrate, badrate, fixedrate;
	const struct ieee80211_rateset *srs;
	struct ieee80211_rateset *nrs;
	u_int8_t r;
	
	/*
	 * If the fixed rate check was requested but no fixed rate has been
	 * defined then just remove the check.
	 */
	if ((flags & IEEE80211_F_DOFRATE) && ic->ic_fixed_rate == -1)
		flags &= ~IEEE80211_F_DOFRATE;
	
	error = 0;
	okrate = badrate = fixedrate = 0;
	srs = &ic->ic_sup_rates[ieee80211_chan2mode(ic, ni->ni_chan)];
	nrs = &ni->ni_rates;
	for (i = 0; i < nrs->rs_nrates; ) {
		ignore = 0;
		if (flags & IEEE80211_F_DOSORT) {
			/*
			 * Sort rates.
			 */
			for (j = i + 1; j < nrs->rs_nrates; j++) {
				if (RV(nrs->rs_rates[i]) >
				    RV(nrs->rs_rates[j])) {
					r = nrs->rs_rates[i];
					nrs->rs_rates[i] = nrs->rs_rates[j];
					nrs->rs_rates[j] = r;
				}
			}
		}
		r = nrs->rs_rates[i] & IEEE80211_RATE_VAL;
		badrate = r;
		if (flags & IEEE80211_F_DOFRATE) {
			/*
			 * Check fixed rate is included.
			 */
			if (r == RV(srs->rs_rates[ic->ic_fixed_rate]))
				fixedrate = r;
		}
		if (flags & IEEE80211_F_DONEGO) {
			/*
			 * Check against supported rates.
			 */
			for (j = 0; j < srs->rs_nrates; j++) {
				if (r == RV(srs->rs_rates[j])) {
					/*
					 * Overwrite with the supported rate
					 * value so any basic rate bit is set.
					 * This insures that response we send
					 * to stations have the necessary basic
					 * rate bit set.
					 */
					nrs->rs_rates[i] = srs->rs_rates[j];
					break;
				}
			}
			if (j == srs->rs_nrates) {
				/*
				 * A rate in the node's rate set is not
				 * supported.  If this is a basic rate and we
				 * are operating as an AP then this is an error.
				 * Otherwise we just discard/ignore the rate.
				 * Note that this is important for 11b stations
				 * when they want to associate with an 11g AP.
				 */
				ignore++;
			}
		}
		if (flags & IEEE80211_F_DODEL) {
			/*
			 * Delete unacceptable rates.
			 */
			if (ignore) {
				nrs->rs_nrates--;
				for (j = i; j < nrs->rs_nrates; j++)
					nrs->rs_rates[j] = nrs->rs_rates[j + 1];
				nrs->rs_rates[j] = 0;
				continue;
			}
		}
		if (!ignore)
			okrate = nrs->rs_rates[i];
		i++;
	}
	if (okrate == 0 || error != 0 ||
	    ((flags & IEEE80211_F_DOFRATE) && fixedrate == 0))
		return badrate | IEEE80211_RATE_BASIC;
	else
		return RV(okrate);
#undef RV
}

/*
 * Reset 11g-related state.
 */
void Voodoo80211Device::
ieee80211_reset_erp(struct ieee80211com *ic)
{
	ic->ic_flags &= ~IEEE80211_F_USEPROT;
	ic->ic_nonerpsta = 0;
	ic->ic_longslotsta = 0;
	
	/*
	 * Enable short slot time iff:
	 * - we're operating in 802.11a or
	 * - we're operating in 802.11g and we're not in IBSS mode and
	 *   the device supports short slot time
	 */
	ieee80211_set_shortslottime(ic,
				    ic->ic_curmode == IEEE80211_MODE_11A
				    );
	
	if (ic->ic_curmode == IEEE80211_MODE_11A ||
	    (ic->ic_caps & IEEE80211_C_SHPREAMBLE))
		ic->ic_flags |= IEEE80211_F_SHPREAMBLE;
	else
		ic->ic_flags &= ~IEEE80211_F_SHPREAMBLE;
}

/*
 * Set the short slot time state and notify the driver.
 */
void Voodoo80211Device::
ieee80211_set_shortslottime(struct ieee80211com *ic, int on)
{
	if (on)
		ic->ic_flags |= IEEE80211_F_SHSLOT;
	else
		ic->ic_flags &= ~IEEE80211_F_SHSLOT;
	
	/* notify the driver */
	ieee80211_updateslot(ic);
}

/*
 * This function is called by the 802.1X PACP machine (via an ioctl) when
 * the transmit key machine (4-Way Handshake for 802.11) should run.
 */
int Voodoo80211Device::
ieee80211_keyrun(struct ieee80211com *ic, u_int8_t *macaddr)
{
	/* STA must be associated or AP must be ready */
	if (ic->ic_state != IEEE80211_S_RUN ||
	    !(ic->ic_flags & IEEE80211_F_RSNON))
		return ENETDOWN;
	

		return 0;	/* supplicant only, do nothing */
}

#ifndef IEEE80211_NO_HT
void Voodoo80211Device::
ieee80211_tx_ba_timeout(void *arg)
{
	struct ieee80211_tx_ba *ba = (struct ieee80211_tx_ba *)arg;
	struct ieee80211_node *ni = ba->ba_ni;
	struct ieee80211com *ic = ni->ni_ic;
	u_int8_t tid;
	int s;
	
	s = splnet();
	if (ba->ba_state == IEEE80211_BA_REQUESTED) {
		/* MLME-ADDBA.confirm(TIMEOUT) */
		ba->ba_state = IEEE80211_BA_INIT;
		
	} else if (ba->ba_state == IEEE80211_BA_AGREED) {
		/* Block Ack inactivity timeout */
		tid = ((caddr_t)ba - (caddr_t)ni->ni_tx_ba) / sizeof(*ba);
		ieee80211_delba_request(ic, ni, IEEE80211_REASON_TIMEOUT,
					1, tid);
	}
	splx(s);
}

void Voodoo80211Device::
ieee80211_rx_ba_timeout(void *arg)
{
	struct ieee80211_rx_ba *ba = (struct ieee80211_rx_ba *)arg;
	struct ieee80211_node *ni = ba->ba_ni;
	struct ieee80211com *ic = ni->ni_ic;
	u_int8_t tid;
	int s;
	
	s = splnet();
	
	/* Block Ack inactivity timeout */
	tid = ((caddr_t)ba - (caddr_t)ni->ni_rx_ba) / sizeof(*ba);
	ieee80211_delba_request(ic, ni, IEEE80211_REASON_TIMEOUT, 0, tid);
	
	splx(s);
}

/*
 * Request initiation of Block Ack with the specified peer.
 */
int Voodoo80211Device::
ieee80211_addba_request(struct ieee80211com *ic, struct ieee80211_node *ni,
			u_int16_t ssn, u_int8_t tid)
{
	struct ieee80211_tx_ba *ba = &ni->ni_tx_ba[tid];
	
	/* MLME-ADDBA.request */
	
	/* setup Block Ack */
	ba->ba_state = IEEE80211_BA_REQUESTED;
	ba->ba_token = ic->ic_dialog_token++;
	ba->ba_timeout_val = IEEE80211_BA_MAX_TIMEOUT;
	timeout_set(ba->ba_to, OSMemberFunctionCast(VoodooTimeout::CallbackFunction, this, &Voodoo80211Device::ieee80211_tx_ba_timeout), ba);
	ba->ba_winsize = IEEE80211_BA_MAX_WINSZ;
	ba->ba_winstart = ssn;
	ba->ba_winend = (ba->ba_winstart + ba->ba_winsize - 1) & 0xfff;
	
	timeout_add_sec(ba->ba_to, 1);	/* dot11ADDBAResponseTimeout */
	IEEE80211_SEND_ACTION(ic, ni, IEEE80211_CATEG_BA,
			      IEEE80211_ACTION_ADDBA_REQ, tid);
	return 0;
}

/*
 * Request the deletion of Block Ack with a peer.
 */
void Voodoo80211Device::
ieee80211_delba_request(struct ieee80211com *ic, struct ieee80211_node *ni,
			u_int16_t reason, u_int8_t dir, u_int8_t tid)
{
	/* MLME-DELBA.request */
	
	/* transmit a DELBA frame */
	IEEE80211_SEND_ACTION(ic, ni, IEEE80211_CATEG_BA,
			      IEEE80211_ACTION_DELBA, reason << 16 | dir << 8 | tid);
	if (dir) {
		/* MLME-DELBA.confirm(Originator) */
		struct ieee80211_tx_ba *ba = &ni->ni_tx_ba[tid];
		
		ieee80211_ampdu_tx_stop(ic, ni, tid);
		
		ba->ba_state = IEEE80211_BA_INIT;
		/* stop Block Ack inactivity timer */
		timeout_del(ba->ba_to);
	} else {
		/* MLME-DELBA.confirm(Recipient) */
		struct ieee80211_rx_ba *ba = &ni->ni_rx_ba[tid];
		int i;
		
		ieee80211_ampdu_rx_stop(ic, ni, tid);
		
		ba->ba_state = IEEE80211_BA_INIT;
		/* stop Block Ack inactivity timer */
		timeout_del(ba->ba_to);
		
		if (ba->ba_buf != NULL) {
			/* free all MSDUs stored in reordering buffer */
			for (i = 0; i < IEEE80211_BA_MAX_WINSZ; i++)
				if (ba->ba_buf[i].m != NULL)
					mbuf_freem(ba->ba_buf[i].m);
			/* free reordering buffer */
			free(ba->ba_buf);
			ba->ba_buf = NULL;
		}
	}
}
#endif	/* !IEEE80211_NO_HT */

void Voodoo80211Device::
ieee80211_auth_open(struct ieee80211com *ic, const struct ieee80211_frame *wh,
		    struct ieee80211_node *ni, struct ieee80211_rxinfo *rxi, u_int16_t seq,
		    u_int16_t status)
{
	// TODO struct ifnet *ifp = &ic->ic_if;
	switch (ic->ic_opmode) {
		case IEEE80211_M_STA:
			if (ic->ic_state != IEEE80211_S_AUTH ||
			    seq != IEEE80211_AUTH_OPEN_RESPONSE) {
				ic->ic_stats.is_rx_bad_auth++;
				DPRINTF(("discard auth from %s; state %u, seq %u\n",
					 ether_sprintf((u_int8_t *)wh->i_addr2),
					 ic->ic_state, seq));
				return;
			}
			if (ic->ic_flags & IEEE80211_F_RSNON) {
				/* XXX not here! */
				ic->ic_bss->ni_flags &= ~IEEE80211_NODE_TXRXPROT;
				ic->ic_bss->ni_port_valid = 0;
				ic->ic_bss->ni_replaycnt_ok = 0;
				ieee80211_delete_key(ic, ic->ic_bss,
						     &ic->ic_bss->ni_pairwise_key);
			}
			if (status != 0) {
				/* TODO
				if (ifp->if_flags & IFF_DEBUG)
					printf("%s: open authentication failed "
					       "(reason %d) for %s\n", ifp->if_xname,
					       status,
					       ether_sprintf((u_int8_t *)wh->i_addr3));
				 */
				if (ni != ic->ic_bss)
					ni->ni_fails++;
				ic->ic_stats.is_rx_auth_fail++;
				return;
			}
			ieee80211_newstate(ic, IEEE80211_S_ASSOC,
					    wh->i_fc[0] & IEEE80211_FC0_SUBTYPE_MASK);
			break;
		default:
			break;
	}
}

int Voodoo80211Device::
ieee80211_newstate(struct ieee80211com *ic, enum ieee80211_state nstate,
		   int mgt)
{
	// TODO struct ifnet *ifp = &ic->ic_if;
	struct ieee80211_node *ni;
	enum ieee80211_state ostate;
	// TODO u_int rate; (needed in debug printf below)
	
	ostate = ic->ic_state;
	DPRINTF(("%s -> %s\n", ieee80211_state_name[ostate],
		 ieee80211_state_name[nstate]));
	ic->ic_state = nstate;			/* state transition */
	ni = ic->ic_bss;			/* NB: no reference held */
	if (ostate == IEEE80211_S_RUN)
		ieee80211_set_link_state(ic, kIO80211NetworkLinkUp);
	switch (nstate) {
		case IEEE80211_S_INIT:
			/*
			 * If mgt = -1, driver is already partway down, so do
			 * not send management frames.
			 */
			switch (ostate) {
				case IEEE80211_S_INIT:
					break;
				case IEEE80211_S_RUN:
					if (mgt == -1)
						goto justcleanup;
					switch (ic->ic_opmode) {
						case IEEE80211_M_STA:
							IEEE80211_SEND_MGMT(ic, ni,
									    IEEE80211_FC0_SUBTYPE_DISASSOC,
									    IEEE80211_REASON_ASSOC_LEAVE);
							break;
						default:
							break;
					}
					/* FALLTHROUGH */
				case IEEE80211_S_ASSOC:
					if (mgt == -1)
						goto justcleanup;
					switch (ic->ic_opmode) {
						case IEEE80211_M_STA:
							IEEE80211_SEND_MGMT(ic, ni,
									    IEEE80211_FC0_SUBTYPE_DEAUTH,
									    IEEE80211_REASON_AUTH_LEAVE);
							break;
						default:
							break;
					}
					/* FALLTHROUGH */
				case IEEE80211_S_AUTH:
				case IEEE80211_S_SCAN:
				justcleanup:
					ic->ic_mgt_timer = 0;
					//TODO ic->ic_mgtq->flush();
					//TODO ic->ic_pwrsaveq->flush();
					ieee80211_free_allnodes(ic);
					break;
			}
			break;
		case IEEE80211_S_SCAN:
			ic->ic_flags &= ~IEEE80211_F_SIBSS;
			/* initialize bss for probe request */
			IEEE80211_ADDR_COPY(ni->ni_macaddr, etherbroadcastaddr);
			IEEE80211_ADDR_COPY(ni->ni_bssid, etherbroadcastaddr);
			ni->ni_rates = ic->ic_sup_rates[
							ieee80211_chan2mode(ic, ni->ni_chan)];
			ni->ni_associd = 0;
			ni->ni_rstamp = 0;
			switch (ostate) {
				case IEEE80211_S_INIT:
						ieee80211_begin_scan(ic);
					break;
				case IEEE80211_S_SCAN:
					/* scan next */
					if (ic->ic_flags & IEEE80211_F_ASCAN) {
						IEEE80211_SEND_MGMT(ic, ni,
								    IEEE80211_FC0_SUBTYPE_PROBE_REQ, 0);
					}
					break;
				case IEEE80211_S_RUN:
					/* beacon miss */
					// TODO
					//if (ifp->if_flags & IFF_DEBUG) {
					//	/* XXX bssid clobbered above */
					//	printf("%s: no recent beacons from %s;"
					//	       " rescanning\n", ifp->if_xname,
					//	       ether_sprintf(ic->ic_bss->ni_bssid));
					//}
					ieee80211_free_allnodes(ic);
					/* FALLTHROUGH */
				case IEEE80211_S_AUTH:
				case IEEE80211_S_ASSOC:
					/* timeout restart scan */
					ni = ieee80211_find_node(ic, ic->ic_bss->ni_macaddr);
					if (ni != NULL)
						ni->ni_fails++;
					ieee80211_begin_scan(ic);
					break;
			}
			break;
		case IEEE80211_S_AUTH:
			switch (ostate) {
				case IEEE80211_S_INIT:
					DPRINTF(("invalid transition\n"));
					break;
				case IEEE80211_S_SCAN:
					IEEE80211_SEND_MGMT(ic, ni,
							    IEEE80211_FC0_SUBTYPE_AUTH, 1);
					break;
				case IEEE80211_S_AUTH:
				case IEEE80211_S_ASSOC:
					switch (mgt) {
						case IEEE80211_FC0_SUBTYPE_AUTH:
							/* ??? */
							IEEE80211_SEND_MGMT(ic, ni,
									    IEEE80211_FC0_SUBTYPE_AUTH, 2);
							break;
						case IEEE80211_FC0_SUBTYPE_DEAUTH:
							/* ignore and retry scan on timeout */
							break;
					}
					break;
				case IEEE80211_S_RUN:
					switch (mgt) {
						case IEEE80211_FC0_SUBTYPE_AUTH:
							IEEE80211_SEND_MGMT(ic, ni,
									    IEEE80211_FC0_SUBTYPE_AUTH, 2);
							ic->ic_state = ostate;	/* stay RUN */
							break;
						case IEEE80211_FC0_SUBTYPE_DEAUTH:
							/* try to reauth */
							IEEE80211_SEND_MGMT(ic, ni,
									    IEEE80211_FC0_SUBTYPE_AUTH, 1);
							break;
					}
					break;
			}
			break;
		case IEEE80211_S_ASSOC:
			switch (ostate) {
				case IEEE80211_S_INIT:
				case IEEE80211_S_SCAN:
				case IEEE80211_S_ASSOC:
					DPRINTF(("invalid transition\n"));
					break;
				case IEEE80211_S_AUTH:
					IEEE80211_SEND_MGMT(ic, ni,
							    IEEE80211_FC0_SUBTYPE_ASSOC_REQ, 0);
					break;
				case IEEE80211_S_RUN:
					IEEE80211_SEND_MGMT(ic, ni,
							    IEEE80211_FC0_SUBTYPE_ASSOC_REQ, 1);
					break;
			}
			break;
		case IEEE80211_S_RUN:
			switch (ostate) {
				case IEEE80211_S_INIT:
				case IEEE80211_S_AUTH:
				case IEEE80211_S_RUN:
					DPRINTF(("invalid transition\n"));
					break;
				case IEEE80211_S_SCAN:		/* adhoc/hostap mode */
				case IEEE80211_S_ASSOC:		/* infra mode */
					if (ni->ni_txrate >= ni->ni_rates.rs_nrates)
						panic("%s: bogus xmit rate %u setup",
						      __func__, ni->ni_txrate);
					/* TODO
					if (ifp->if_flags & IFF_DEBUG) {
						printf("%s: %s with %s ssid ",
						       ifp->if_xname,
						       ic->ic_opmode == IEEE80211_M_STA ?
						       "associated" : "synchronized",
						       ether_sprintf(ni->ni_bssid));
						ieee80211_print_essid(ic->ic_bss->ni_essid,
								      ni->ni_esslen);
						rate = ni->ni_rates.rs_rates[ni->ni_txrate] &
						IEEE80211_RATE_VAL;
						printf(" channel %d start %u%sMb",
						       ieee80211_chan2ieee(ic, ni->ni_chan),
						       rate / 2, (rate & 1) ? ".5" : "");
						printf(" %s preamble %s slot time%s\n",
						       (ic->ic_flags & IEEE80211_F_SHPREAMBLE) ?
						       "short" : "long",
						       (ic->ic_flags & IEEE80211_F_SHSLOT) ?
						       "short" : "long",
						       (ic->ic_flags & IEEE80211_F_USEPROT) ?
						       " protection enabled" : "");
					}
					 */
					if (!(ic->ic_flags & IEEE80211_F_RSNON)) {
						/*
						 * NB: When RSN is enabled, we defer setting
						 * the link up until the port is valid.
						 */
						fInterface->postMessage(APPLE80211_M_ASSOC_DONE);
						ieee80211_set_link_state(ic, kIO80211NetworkLinkUp);
					}
					ic->ic_mgt_timer = 0;
					fOutputQueue->start();
					break;
			}
			break;
	}
	return 0;
}

void Voodoo80211Device::
ieee80211_set_link_state(struct ieee80211com *ic, IO80211LinkState nstate)
{	
	switch (ic->ic_opmode) {
		case IEEE80211_M_MONITOR:
			nstate = kIO80211NetworkLinkDown;
			break;
		default:
			break;
	}
	fInterface->setLinkState((IO80211LinkState) nstate, 0);
}