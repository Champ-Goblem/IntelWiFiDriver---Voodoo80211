/*	$OpenBSD: ieee80211_crypto.h,v 1.22 2009/01/26 19:09:41 damien Exp $	*/

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

#ifndef _NET80211_IEEE80211_CRYPTO_H_
#define _NET80211_IEEE80211_CRYPTO_H_

/*
 * 802.11 protocol crypto-related definitions.
 */

#include <sys/queue.h>

/*
 * 802.11 ciphers.
 */
enum ieee80211_cipher {
	IEEE80211_CIPHER_NONE		= 0x00000000,
	IEEE80211_CIPHER_USEGROUP	= 0x00000001,
	IEEE80211_CIPHER_WEP40		= 0x00000002,
	IEEE80211_CIPHER_TKIP		= 0x00000004,
	IEEE80211_CIPHER_CCMP		= 0x00000008,
	IEEE80211_CIPHER_WEP104		= 0x00000010,
	IEEE80211_CIPHER_BIP		= 0x00000020	/* 11w */
};

/*
 * 802.11 Authentication and Key Management Protocols.
 */
enum ieee80211_akm {
	IEEE80211_AKM_NONE		= 0x00000000,
	IEEE80211_AKM_8021X		= 0x00000001,
	IEEE80211_AKM_PSK		= 0x00000002,
	IEEE80211_AKM_SHA256_8021X	= 0x00000004,	/* 11w */
	IEEE80211_AKM_SHA256_PSK	= 0x00000008	/* 11w */
};

static __inline int
ieee80211_is_8021x_akm(enum ieee80211_akm akm)
{
	return akm == IEEE80211_AKM_8021X ||
    akm == IEEE80211_AKM_SHA256_8021X;
}

static __inline int
ieee80211_is_sha256_akm(enum ieee80211_akm akm)
{
	return akm == IEEE80211_AKM_SHA256_8021X ||
    akm == IEEE80211_AKM_SHA256_PSK;
}

#define	IEEE80211_KEYBUF_SIZE	16

#define IEEE80211_TKIP_HDRLEN	8
#define IEEE80211_TKIP_MICLEN	8
#define IEEE80211_TKIP_ICVLEN	4
#define IEEE80211_CCMP_HDRLEN	8
#define IEEE80211_CCMP_MICLEN	8

#define IEEE80211_PMK_LEN	32

struct ieee80211_key {
	u_int8_t		k_id;		/* identifier (0-5) */
	enum ieee80211_cipher	k_cipher;
	u_int			k_flags;
#define IEEE80211_KEY_GROUP	0x00000001	/* group data key */
#define IEEE80211_KEY_TX	0x00000002	/* Tx+Rx */
#define IEEE80211_KEY_IGTK	0x00000004	/* integrity group key */
    
	u_int			k_len;
	u_int64_t		k_rsc[IEEE80211_NUM_TID];
	u_int64_t		k_mgmt_rsc;
	u_int64_t		k_tsc;
	u_int8_t		k_key[32];
	void			*k_priv;
};

/*
 * Entry in the PMKSA cache.
 */
struct ieee80211_pmk {
	enum ieee80211_akm	pmk_akm;
	u_int32_t		pmk_lifetime;
#define IEEE80211_PMK_INFINITE	0
    
	u_int8_t		pmk_pmkid[IEEE80211_PMKID_LEN];
	u_int8_t		pmk_macaddr[IEEE80211_ADDR_LEN];
	u_int8_t		pmk_key[IEEE80211_PMK_LEN];
    
	TAILQ_ENTRY(ieee80211_pmk) pmk_next;
};

/* forward references */
struct	ieee80211com;
struct	ieee80211_node;

#endif /* _NET80211_IEEE80211_CRYPTO_H_ */