/*	$OpenBSD: ieee80211_node.c,v 1.64 2012/01/18 14:35:34 stsp Exp $	*/
/*	$NetBSD: ieee80211_node.c,v 1.14 2004/05/09 09:18:47 dyoung Exp $	*/

/*-
 * Copyright (c) 2001 Atsushi Onoe
 * Copyright (c) 2002, 2003 Sam Leffler, Errno Consulting
 * Copyright (c) 2008 Damien Bergamini
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

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/mbuf.h>
#include <sys/malloc.h>
#include <sys/kernel.h>
#include <sys/socket.h>
#include <sys/sockio.h>
#include "sys/endian.h"
#include <sys/errno.h>
#include <sys/proc.h>
#include <sys/sysctl.h>
#include "sys/tree.h"

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

#if NBRIDGE > 0
#include <net/if_bridge.h>
#endif

#include "Voodoo80211Device.h"

//#include <dev/rndvar.h>

#define M_80211_NODE	M_DEVBUF

void Voodoo80211Device::
ieee80211_node_attach(struct ieee80211com *ic)
{
	RB_INIT(&ic->ic_tree);

	ic->ic_scangen = 1;
	ic->ic_max_nnodes = ieee80211_cache_size;
    
	if (ic->ic_max_aid == 0)
		ic->ic_max_aid = IEEE80211_AID_DEF;
	else if (ic->ic_max_aid > IEEE80211_AID_MAX)
		ic->ic_max_aid = IEEE80211_AID_MAX;
}

struct ieee80211_node * Voodoo80211Device::
ieee80211_alloc_node_helper(struct ieee80211com *ic)
{
	struct ieee80211_node *ni;
	if (ic->ic_nnodes >= ic->ic_max_nnodes)
		ieee80211_clean_nodes(ic);
	if (ic->ic_nnodes >= ic->ic_max_nnodes)
		return NULL;
	ni = ieee80211_node_alloc(ic);
	if (ni != NULL)
		ic->ic_nnodes++;
	return ni;
}

void Voodoo80211Device::
ieee80211_node_lateattach(struct ieee80211com *ic)
{
	struct ieee80211_node *ni;
    
	ni = ieee80211_alloc_node_helper(ic);
	if (ni == NULL)
		panic("unable to setup inital BSS node");
	ni->ni_chan = IEEE80211_CHAN_ANYC;
	ic->ic_bss = ieee80211_ref_node(ni);
	ic->ic_txpower = IEEE80211_TXPOWER_MAX;
}

void Voodoo80211Device::
ieee80211_node_detach(struct ieee80211com *ic)
{
	if (ic->ic_bss != NULL) {
		ieee80211_node_free(ic, ic->ic_bss);
		ic->ic_bss = NULL;
	}
	ieee80211_free_allnodes(ic);
	timeout_del(ic->ic_rsn_timeout);
}

/*
 * AP scanning support.
 */

/*
 * Initialize the active channel set based on the set
 * of available channels and the current PHY mode.
 */
void Voodoo80211Device::
ieee80211_reset_scan(struct ieee80211com *ic)
{
	memcpy(ic->ic_chan_scan, ic->ic_chan_active,
           sizeof(ic->ic_chan_active));
	/* NB: hack, setup so next_scan starts with the first channel */
	if (ic->ic_bss != NULL && ic->ic_bss->ni_chan == IEEE80211_CHAN_ANYC)
		ic->ic_bss->ni_chan = &ic->ic_channels[IEEE80211_CHAN_MAX];
}

/*
 * Begin an active scan.
 */
void Voodoo80211Device::
ieee80211_begin_scan(struct ieee80211com *ic)
{
	if (ic->ic_scan_lock & IEEE80211_SCAN_LOCKED)
		return;
	ic->ic_scan_lock |= IEEE80211_SCAN_LOCKED;
    
	/*
	 * In all but hostap mode scanning starts off in
	 * an active mode before switching to passive.
	 */
	{
		ic->ic_flags |= IEEE80211_F_ASCAN;
		ic->ic_stats.is_scan_active++;
	}
	if (false /* ifp->debugOn */)
		printf("%s: begin %s scan\n", "voodoo_wifi",
               (ic->ic_flags & IEEE80211_F_ASCAN) ?
               "active" : "passive");
    
	/*
	 * Flush any previously seen AP's. Note that the latter 
	 * assumes we don't act as both an AP and a station,
	 * otherwise we'll potentially flush state of stations
	 * associated with us.
	 */
	ieee80211_free_allnodes(ic);
    
	/*
	 * Reset the current mode. Setting the current mode will also
	 * reset scan state.
	 */
    if (getCurrentMedium()->getType() & IFM_AUTO)
		ic->ic_curmode = IEEE80211_MODE_AUTO;
	ieee80211_setmode(ic, (enum ieee80211_phymode) ic->ic_curmode);
    
	ic->ic_scan_count = 0;
    
	/* Scan the next channel. */
	ieee80211_next_scan(ic);
}

/*
 * Switch to the next channel marked for scanning.
 */
void Voodoo80211Device::
ieee80211_next_scan(struct ieee80211com *ic)
{
	struct ieee80211_channel *chan;
    
	chan = ic->ic_bss->ni_chan;
	for (;;) {
		if (++chan > &ic->ic_channels[IEEE80211_CHAN_MAX])
			chan = &ic->ic_channels[0];
		if (isset(ic->ic_chan_scan, ieee80211_chan2ieee(ic, chan))) {
			/*
			 * Ignore channels marked passive-only
			 * during an active scan.
			 */
			if ((ic->ic_flags & IEEE80211_F_ASCAN) == 0 ||
			    (chan->ic_flags & IEEE80211_CHAN_PASSIVE) == 0)
				break;
		}
		if (chan == ic->ic_bss->ni_chan) {
			ieee80211_end_scan(ic);
			return;
		}
	}
	clrbit(ic->ic_chan_scan, ieee80211_chan2ieee(ic, chan));
	DPRINTF(("chan %d->%d\n",
             ieee80211_chan2ieee(ic, ic->ic_bss->ni_chan),
             ieee80211_chan2ieee(ic, chan)));
	ic->ic_bss->ni_chan = chan;
	ieee80211_newstate(ic, IEEE80211_S_SCAN, -1);
}

int Voodoo80211Device::
ieee80211_match_bss(struct ieee80211com *ic, struct ieee80211_node *ni)
{
	u_int8_t rate;
	int fail;
    
	fail = 0;
	if (isclr(ic->ic_chan_active, ieee80211_chan2ieee(ic, ni->ni_chan)))
		fail |= 0x01;
	if (ic->ic_des_chan != IEEE80211_CHAN_ANYC &&
	    ni->ni_chan != ic->ic_des_chan)
		fail |= 0x01;
	{
		if ((ni->ni_capinfo & IEEE80211_CAPINFO_ESS) == 0)
			fail |= 0x02;
	}
	if (ic->ic_flags & (IEEE80211_F_WEPON | IEEE80211_F_RSNON)) {
		if ((ni->ni_capinfo & IEEE80211_CAPINFO_PRIVACY) == 0)
			fail |= 0x04;
	} else {
		if (ni->ni_capinfo & IEEE80211_CAPINFO_PRIVACY)
			fail |= 0x04;
	}
    
	rate = ieee80211_fix_rate(ic, ni, IEEE80211_F_DONEGO);
	if (rate & IEEE80211_RATE_BASIC)
		fail |= 0x08;
	if (ic->ic_des_esslen != 0 &&
	    (ni->ni_esslen != ic->ic_des_esslen ||
	     memcmp(ni->ni_essid, ic->ic_des_essid, ic->ic_des_esslen) != 0))
		fail |= 0x10;
	if ((ic->ic_flags & IEEE80211_F_DESBSSID) &&
	    !IEEE80211_ADDR_EQ(ic->ic_des_bssid, ni->ni_bssid))
		fail |= 0x20;
    
	if (ic->ic_flags & IEEE80211_F_RSNON) {
		/*
		 * If at least one RSN IE field from the AP's RSN IE fails
		 * to overlap with any value the STA supports, the STA shall
		 * decline to associate with that AP.
		 */
		if ((ni->ni_rsnprotos & ic->ic_rsnprotos) == 0)
			fail |= 0x40;
		if ((ni->ni_rsnakms & ic->ic_rsnakms) == 0)
			fail |= 0x40;
		if ((ni->ni_rsnakms & ic->ic_rsnakms &
		     ~(IEEE80211_AKM_PSK | IEEE80211_AKM_SHA256_PSK)) == 0) {
			/* AP only supports PSK AKMPs */
			if (!(ic->ic_flags & IEEE80211_F_PSK))
				fail |= 0x40;
		}
		if (ni->ni_rsngroupcipher != IEEE80211_CIPHER_WEP40 &&
		    ni->ni_rsngroupcipher != IEEE80211_CIPHER_TKIP &&
		    ni->ni_rsngroupcipher != IEEE80211_CIPHER_CCMP &&
		    ni->ni_rsngroupcipher != IEEE80211_CIPHER_WEP104)
			fail |= 0x40;
		if ((ni->ni_rsnciphers & ic->ic_rsnciphers) == 0)
			fail |= 0x40;
        
		/* we only support BIP as the IGTK cipher */
		if ((ni->ni_rsncaps & IEEE80211_RSNCAP_MFPC) &&
		    ni->ni_rsngroupmgmtcipher != IEEE80211_CIPHER_BIP)
			fail |= 0x40;
        
		/* we do not support MFP but AP requires it */
		if (!(ic->ic_caps & IEEE80211_C_MFP) &&
		    (ni->ni_rsncaps & IEEE80211_RSNCAP_MFPR))
			fail |= 0x40;
        
		/* we require MFP but AP does not support it */
		if ((ic->ic_caps & IEEE80211_C_MFP) &&
		    (ic->ic_flags & IEEE80211_F_MFPR) &&
		    !(ni->ni_rsncaps & IEEE80211_RSNCAP_MFPC))
			fail |= 0x40;
	}
    
#ifdef IEEE80211_DEBUG
	/*
	if (ic->ic_if.if_flags & IFF_DEBUG) {
		printf(" %c %s", fail ? '-' : '+',
               ether_sprintf(ni->ni_macaddr));
		printf(" %s%c", ether_sprintf(ni->ni_bssid),
               fail & 0x20 ? '!' : ' ');
		printf(" %3d%c", ieee80211_chan2ieee(ic, ni->ni_chan),
               fail & 0x01 ? '!' : ' ');
		printf(" %+4d", ni->ni_rssi);
		printf(" %2dM%c", (rate & IEEE80211_RATE_VAL) / 2,
               fail & 0x08 ? '!' : ' ');
		printf(" %4s%c",
               (ni->ni_capinfo & IEEE80211_CAPINFO_ESS) ? "ess" :
               (ni->ni_capinfo & IEEE80211_CAPINFO_IBSS) ? "ibss" :
               "????",
               fail & 0x02 ? '!' : ' ');
		printf(" %7s%c ",
               (ni->ni_capinfo & IEEE80211_CAPINFO_PRIVACY) ?
               "privacy" : "no",
               fail & 0x04 ? '!' : ' ');
		printf(" %3s%c ",
               (ic->ic_flags & IEEE80211_F_RSNON) ?
               "rsn" : "no",
               fail & 0x40 ? '!' : ' ');
		ieee80211_print_essid(ni->ni_essid, ni->ni_esslen);
		printf("%s\n", fail & 0x10 ? "!" : "");
	}*/
#endif
	return fail;
}

/*
 * Complete a scan of potential channels.
 */
void Voodoo80211Device::
ieee80211_end_scan(struct ieee80211com *ic)
{
	struct ieee80211_node *ni, *nextbs, *selbs;
    
    /* TODO
	if (ifp->if_flags & IFF_DEBUG)
		printf("%s: end %s scan\n", ifp->if_xname,
               (ic->ic_flags & IEEE80211_F_ASCAN) ?
               "active" : "passive");
     */
    
	if (ic->ic_scan_count)
		ic->ic_flags &= ~IEEE80211_F_ASCAN;
    
	ni = RB_MIN(ieee80211_tree, &ic->ic_tree);
    
	if (ni == NULL) {
		DPRINTF(("no scan candidate\n"));
    notfound:
        
		/*
		 * Scan the next mode if nothing has been found. This
		 * is necessary if the device supports different
		 * incompatible modes in the same channel range, like
		 * like 11b and "pure" 11G mode. This will loop
		 * forever except for user-initiated scans.
		 */
		if (ieee80211_next_mode(ic) == IEEE80211_MODE_AUTO) {
			if (ic->ic_scan_lock & IEEE80211_SCAN_REQUEST &&
			    ic->ic_scan_lock & IEEE80211_SCAN_RESUME) {
				ic->ic_scan_lock = IEEE80211_SCAN_LOCKED;
				/* Return from an user-initiated scan */
				wakeupOn(&ic->ic_scan_lock);
				// XXX: pvaibhav: do this here?
				fInterface->postMessage(APPLE80211_M_SCAN_DONE);
			} else if (ic->ic_scan_lock & IEEE80211_SCAN_REQUEST)
				goto wakeup;
			ic->ic_scan_count++;
		}
        
		/*
		 * Reset the list of channels to scan and start again.
		 */
		ieee80211_next_scan(ic);
		return;
	}
	selbs = NULL;
    
	for (; ni != NULL; ni = nextbs) {
		nextbs = RB_NEXT(ieee80211_tree, &ic->ic_tree, ni);
		if (ni->ni_fails) {
			/*
			 * The configuration of the access points may change
			 * during my scan.  So delete the entry for the AP
			 * and retry to associate if there is another beacon.
			 */
			if (ni->ni_fails++ > 2)
				ieee80211_free_node(ic, ni);
			continue;
		}
		if (ieee80211_match_bss(ic, ni) == 0) {
			if (selbs == NULL)
				selbs = ni;
			else if (ni->ni_rssi > selbs->ni_rssi)
				selbs = ni;
		}
	}
	if (selbs == NULL)
		goto notfound;
	ieee80211_node_copy(ic, ic->ic_bss, selbs);
	ni = ic->ic_bss;
    
	/*
	 * Set the erp state (mostly the slot time) to deal with
	 * the auto-select case; this should be redundant if the
	 * mode is locked.
	 */
	ic->ic_curmode = ieee80211_chan2mode(ic, ni->ni_chan);
	ieee80211_reset_erp(ic);
    
	if (ic->ic_flags & IEEE80211_F_RSNON)
		ieee80211_choose_rsnparams(ic);
	else if (ic->ic_flags & IEEE80211_F_WEPON)
		ni->ni_rsncipher = IEEE80211_CIPHER_USEGROUP;
    
	ieee80211_node_newstate(selbs, IEEE80211_STA_BSS);
		ieee80211_newstate(ic, IEEE80211_S_AUTH, -1);
    
wakeup:
	if (ic->ic_scan_lock & IEEE80211_SCAN_REQUEST) {
		/* Return from an user-initiated scan */
		wakeupOn(&ic->ic_scan_lock);
		// XXX: pvaibhav: do this here?
		fInterface->postMessage(APPLE80211_M_SCAN_DONE);
	}
    
	ic->ic_scan_lock = IEEE80211_SCAN_UNLOCKED;
}

/*
 * Autoselect the best RSN parameters (protocol, AKMP, pairwise cipher...)
 * that are supported by both peers (STA mode only).
 */
void Voodoo80211Device::
ieee80211_choose_rsnparams(struct ieee80211com *ic)
{
	struct ieee80211_node *ni = ic->ic_bss;
	struct ieee80211_pmk *pmk;
    
	/* filter out unsupported protocol versions */
	ni->ni_rsnprotos &= ic->ic_rsnprotos;
	/* prefer RSN (aka WPA2) over WPA */
	if (ni->ni_rsnprotos & IEEE80211_PROTO_RSN)
		ni->ni_rsnprotos = IEEE80211_PROTO_RSN;
	else
		ni->ni_rsnprotos = IEEE80211_PROTO_WPA;
    
	/* filter out unsupported AKMPs */
	ni->ni_rsnakms &= ic->ic_rsnakms;
	/* prefer SHA-256 based AKMPs */
	if ((ic->ic_flags & IEEE80211_F_PSK) && (ni->ni_rsnakms &
                                             (IEEE80211_AKM_PSK | IEEE80211_AKM_SHA256_PSK))) {
		/* AP supports PSK AKMP and a PSK is configured */
		if (ni->ni_rsnakms & IEEE80211_AKM_SHA256_PSK)
			ni->ni_rsnakms = IEEE80211_AKM_SHA256_PSK;
		else
			ni->ni_rsnakms = IEEE80211_AKM_PSK;
	} else {
		if (ni->ni_rsnakms & IEEE80211_AKM_SHA256_8021X)
			ni->ni_rsnakms = IEEE80211_AKM_SHA256_8021X;
		else
			ni->ni_rsnakms = IEEE80211_AKM_8021X;
		/* check if we have a cached PMK for this AP */
		if (ni->ni_rsnprotos == IEEE80211_PROTO_RSN &&
		    (pmk = ieee80211_pmksa_find(ic, ni, NULL)) != NULL) {
			memcpy(ni->ni_pmkid, pmk->pmk_pmkid,
                   IEEE80211_PMKID_LEN);
			ni->ni_flags |= IEEE80211_NODE_PMKID;
		}
	}
    
	/* filter out unsupported pairwise ciphers */
	ni->ni_rsnciphers &= ic->ic_rsnciphers;
	/* prefer CCMP over TKIP */
	if (ni->ni_rsnciphers & IEEE80211_CIPHER_CCMP)
		ni->ni_rsnciphers = IEEE80211_CIPHER_CCMP;
	else
		ni->ni_rsnciphers = IEEE80211_CIPHER_TKIP;
	ni->ni_rsncipher = (enum ieee80211_cipher) ni->ni_rsnciphers;
    
	/* use MFP if we both support it */
	if ((ic->ic_caps & IEEE80211_C_MFP) &&
	    (ni->ni_rsncaps & IEEE80211_RSNCAP_MFPC))
		ni->ni_flags |= IEEE80211_NODE_MFP;
}

int Voodoo80211Device::
ieee80211_get_rate(struct ieee80211com *ic)
{
	u_int8_t (*rates)[IEEE80211_RATE_MAXSIZE];
	int rate;
    
	rates = &ic->ic_bss->ni_rates.rs_rates;
    
	if (ic->ic_fixed_rate != -1)
		rate = (*rates)[ic->ic_fixed_rate];
	else if (ic->ic_state == IEEE80211_S_RUN)
		rate = (*rates)[ic->ic_bss->ni_txrate];
	else
		rate = 0;
    
	return rate & IEEE80211_RATE_VAL;
}

struct ieee80211_node * Voodoo80211Device::
ieee80211_node_alloc(struct ieee80211com *ic)
{
	return (struct ieee80211_node *)
        malloc(sizeof(struct ieee80211_node), M_80211_NODE, M_NOWAIT | M_ZERO);
}

void Voodoo80211Device::
ieee80211_node_cleanup(struct ieee80211com *ic, struct ieee80211_node *ni)
{
	if (ni->ni_rsnie != NULL) {
		free(ni->ni_rsnie);
		ni->ni_rsnie = NULL;
	}
}

void Voodoo80211Device::
ieee80211_node_free(struct ieee80211com *ic, struct ieee80211_node *ni)
{
	ieee80211_node_cleanup(ic, ni);
	free(ni);
}

void Voodoo80211Device::
ieee80211_node_copy(struct ieee80211com *ic,
                    struct ieee80211_node *dst, const struct ieee80211_node *src)
{
	ieee80211_node_cleanup(ic, dst);
	*dst = *src;
	dst->ni_rsnie = NULL;
	if (src->ni_rsnie != NULL)
		ieee80211_save_ie(src->ni_rsnie, &dst->ni_rsnie);
}

u_int8_t Voodoo80211Device::
ieee80211_node_getrssi(struct ieee80211com *ic,
                       const struct ieee80211_node *ni)
{
	return ni->ni_rssi;
}

void Voodoo80211Device::
ieee80211_setup_node(struct ieee80211com *ic,
                     struct ieee80211_node *ni, const u_int8_t *macaddr)
{
	int s;
    
	DPRINTF(("%s\n", ether_sprintf((u_int8_t *)macaddr)));
	IEEE80211_ADDR_COPY(ni->ni_macaddr, macaddr);
	ieee80211_node_newstate(ni, IEEE80211_STA_CACHE);
    
	ni->ni_ic = ic;	/* back-pointer */
    
	/*
	 * Note we don't enable the inactive timer when acting
	 * as a station.  Nodes created in this mode represent
	 * AP's identified while scanning.  If we time them out
	 * then several things happen: we can't return the data
	 * to users to show the list of AP's we encountered, and
	 * more importantly, we'll incorrectly deauthenticate
	 * ourself because the inactivity timer will kick us off.
	 */
	s = splnet();
	if (ic->ic_opmode != IEEE80211_M_STA &&
	    RB_EMPTY(&ic->ic_tree))
		ic->ic_inact_timer = IEEE80211_INACT_WAIT;
	RB_INSERT(ieee80211_tree, &ic->ic_tree, ni);
	splx(s);
}

struct ieee80211_node * Voodoo80211Device::
ieee80211_alloc_node(struct ieee80211com *ic, const u_int8_t *macaddr)
{
	struct ieee80211_node *ni = ieee80211_alloc_node_helper(ic);
	if (ni != NULL)
		ieee80211_setup_node(ic, ni, macaddr);
	else
		ic->ic_stats.is_rx_nodealloc++;
	return ni;
}

struct ieee80211_node * Voodoo80211Device::
ieee80211_dup_bss(struct ieee80211com *ic, const u_int8_t *macaddr)
{
	struct ieee80211_node *ni = ieee80211_alloc_node_helper(ic);
	if (ni != NULL) {
		ieee80211_setup_node(ic, ni, macaddr);
		/*
		 * Inherit from ic_bss.
		 */
		IEEE80211_ADDR_COPY(ni->ni_bssid, ic->ic_bss->ni_bssid);
		ni->ni_chan = ic->ic_bss->ni_chan;
	} else
		ic->ic_stats.is_rx_nodealloc++;
	return ni;
}

struct ieee80211_node * Voodoo80211Device::
ieee80211_find_node(struct ieee80211com *ic, const u_int8_t *macaddr)
{
	struct ieee80211_node *ni;
	int cmp;
    
	/* similar to RB_FIND except we compare keys, not nodes */
	ni = RB_ROOT(&ic->ic_tree);
	while (ni != NULL) {
		cmp = memcmp(macaddr, ni->ni_macaddr, IEEE80211_ADDR_LEN);
		if (cmp < 0)
			ni = RB_LEFT(ni, ni_node);
		else if (cmp > 0)
			ni = RB_RIGHT(ni, ni_node);
		else
			break;
	}
	return ni;
}

/*
 * Return a reference to the appropriate node for sending
 * a data frame.  This handles node discovery in adhoc networks.
 *
 * Drivers will call this, so increase the reference count before
 * returning the node.
 */
struct ieee80211_node * Voodoo80211Device::
ieee80211_find_txnode(struct ieee80211com *ic, const u_int8_t *macaddr)
{
	/*
	 * The destination address should be in the node table
	 * unless we are operating in station mode or this is a
	 * multicast/broadcast frame.
	 */
	/* pvaibhav: opmode is always gonna be STA
	if (ic->ic_opmode == IEEE80211_M_STA || IEEE80211_IS_MULTICAST(macaddr))
	 */
		return ieee80211_ref_node(ic->ic_bss);
}

/*
 * It is usually desirable to process a Rx packet using its sender's
 * node-record instead of the BSS record.
 *
 * - AP mode: keep a node-record for every authenticated/associated
 *   station *in the BSS*. For future use, we also track neighboring
 *   APs, since they might belong to the same ESS.  APs in the same
 *   ESS may bridge packets to each other, forming a Wireless
 *   Distribution System (WDS).
 *
 * - IBSS mode: keep a node-record for every station *in the BSS*.
 *   Also track neighboring stations by their beacons/probe responses.
 *
 * - monitor mode: keep a node-record for every sender, regardless
 *   of BSS.
 *
 * - STA mode: the only available node-record is the BSS record,
 *   ic->ic_bss.
 *
 * Of all the 802.11 Control packets, only the node-records for
 * RTS packets node-record can be looked up.
 *
 * Return non-zero if the packet's node-record is kept, zero
 * otherwise.
 */
static __inline int
ieee80211_needs_rxnode(struct ieee80211com *ic,
                       const struct ieee80211_frame *wh, const u_int8_t **bssid)
{
	int monitor, rc = 0;
    
	monitor = (ic->ic_opmode == IEEE80211_M_MONITOR);
    
	*bssid = NULL;
    
	switch (wh->i_fc[0] & IEEE80211_FC0_TYPE_MASK) {
        case IEEE80211_FC0_TYPE_CTL:
            if (!monitor)
                break;
            return (wh->i_fc[0] & IEEE80211_FC0_SUBTYPE_MASK) ==
		    IEEE80211_FC0_SUBTYPE_RTS;
        case IEEE80211_FC0_TYPE_MGT:
            *bssid = wh->i_addr3;
            switch (wh->i_fc[0] & IEEE80211_FC0_SUBTYPE_MASK) {
                case IEEE80211_FC0_SUBTYPE_BEACON:
                case IEEE80211_FC0_SUBTYPE_PROBE_RESP:
                    break;
                default:
                    break;
            }
            break;
        case IEEE80211_FC0_TYPE_DATA:
            switch (wh->i_fc[1] & IEEE80211_FC1_DIR_MASK) {
                case IEEE80211_FC1_DIR_NODS:
                    *bssid = wh->i_addr3;
                    break;
                case IEEE80211_FC1_DIR_TODS:
                    *bssid = wh->i_addr1;
                    break;
                case IEEE80211_FC1_DIR_FROMDS:
                case IEEE80211_FC1_DIR_DSTODS:
                    *bssid = wh->i_addr2;
                    break;
            }
            break;
	}
	return monitor || rc;
}

/* 
 * Drivers call this, so increase the reference count before returning
 * the node.
 */
struct ieee80211_node * Voodoo80211Device::
ieee80211_find_rxnode(struct ieee80211com *ic,
                      const struct ieee80211_frame *wh)
{
	static const u_int8_t zero[] = { 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 };
	struct ieee80211_node *ni;
	const u_int8_t *bssid;
	int s;
    
	if (!ieee80211_needs_rxnode(ic, wh, &bssid))
		return ieee80211_ref_node(ic->ic_bss);
    
	s = splnet();
	ni = ieee80211_find_node(ic, wh->i_addr2);
	splx(s);
    
	if (ni != NULL)
		return ieee80211_ref_node(ni);
	/* XXX see remarks in ieee80211_find_txnode */
	/* XXX no rate negotiation; just dup */
	if ((ni = ieee80211_dup_bss(ic, wh->i_addr2)) == NULL)
		return ieee80211_ref_node(ic->ic_bss);
    
	IEEE80211_ADDR_COPY(ni->ni_bssid, (bssid != NULL) ? bssid : zero);
    
	ni->ni_rates = ic->ic_bss->ni_rates;
	ieee80211_newassoc(ic, ni, 1);
    
	DPRINTF(("faked-up node %p for %s\n", ni,
             ether_sprintf((u_int8_t *)wh->i_addr2)));
    
	return ieee80211_ref_node(ni);
}

struct ieee80211_node * Voodoo80211Device::
ieee80211_find_node_for_beacon(struct ieee80211com *ic,
                               const u_int8_t *macaddr, const struct ieee80211_channel *chan,
                               const char *ssid, u_int8_t rssi)
{
	struct ieee80211_node *ni, *keep = NULL;
	int s, score = 0;
    
	if ((ni = ieee80211_find_node(ic, macaddr)) != NULL) {
		s = splnet();
        
		if (ni->ni_chan != chan && ni->ni_rssi >= rssi)
			score++;
		if (ssid[1] == 0 && ni->ni_esslen != 0)
			score++;
		if (score > 0)
			keep = ni;
        
		splx(s);
	}
    
	return (keep);
}

void Voodoo80211Device::
ieee80211_free_node(struct ieee80211com *ic, struct ieee80211_node *ni)
{
	if (ni == ic->ic_bss)
		panic("freeing bss node");
    
	DPRINTF(("%s\n", ether_sprintf(ni->ni_macaddr)));
	RB_REMOVE(ieee80211_tree, &ic->ic_tree, ni);
	ic->ic_nnodes--;
	if (RB_EMPTY(&ic->ic_tree))
		ic->ic_inact_timer = 0;
	ieee80211_node_free(ic, ni);
	/* TBD indicate to drivers that a new node can be allocated */
}

void Voodoo80211Device::
ieee80211_release_node(struct ieee80211com *ic, struct ieee80211_node *ni)
{
	int s;
    
	DPRINTF(("%s refcnt %d\n", ether_sprintf(ni->ni_macaddr),
             ni->ni_refcnt));
	if (ieee80211_node_decref(ni) == 0 &&
	    ni->ni_state == IEEE80211_STA_COLLECT) {
		s = splnet();
		ieee80211_free_node(ic, ni);
		splx(s);
	}
}

void Voodoo80211Device::
ieee80211_free_allnodes(struct ieee80211com *ic)
{
	struct ieee80211_node *ni;
	int s;
    
	DPRINTF(("freeing all nodes\n"));
	s = splnet();
	while ((ni = RB_MIN(ieee80211_tree, &ic->ic_tree)) != NULL)
		ieee80211_free_node(ic, ni);
	splx(s);
    
	if (ic->ic_bss != NULL)
		ieee80211_node_cleanup(ic, ic->ic_bss);	/* for station mode */
}

/*
 * Timeout inactive nodes.
 */
void Voodoo80211Device::
ieee80211_clean_nodes(struct ieee80211com *ic)
{
	struct ieee80211_node *ni, *next_ni;
	u_int gen = ic->ic_scangen++;		/* NB: ok 'cuz single-threaded*/
	int s;
    
	s = splnet();
	for (ni = RB_MIN(ieee80211_tree, &ic->ic_tree);
         ni != NULL; ni = next_ni) {
		next_ni = RB_NEXT(ieee80211_tree, &ic->ic_tree, ni);
		if (ic->ic_nnodes < ic->ic_max_nnodes)
			break;
		if (ni->ni_scangen == gen)	/* previously handled */
			continue;
		ni->ni_scangen = gen;
		if (ni->ni_refcnt > 0)
			continue;
		DPRINTF(("station %s purged from LRU cache\n",
                 ether_sprintf(ni->ni_macaddr)));
		/*
		 * Send a deauthenticate frame.
		 */
			ieee80211_free_node(ic, ni);
		ic->ic_stats.is_node_timeout++;
	}
	splx(s);
}

void Voodoo80211Device::
ieee80211_iterate_nodes(struct ieee80211com *ic, ieee80211_iter_func *f,
                        void *arg)
{
	struct ieee80211_node *ni;
	int s;
    
	s = splnet();
	RB_FOREACH(ni, ieee80211_tree, &ic->ic_tree)
    (*f)(arg, ni);
	splx(s);
}

/*
 * Install received rate set information in the node's state block.
 */
int Voodoo80211Device::
ieee80211_setup_rates(struct ieee80211com *ic, struct ieee80211_node *ni,
                      const u_int8_t *rates, const u_int8_t *xrates, int flags)
{
	struct ieee80211_rateset *rs = &ni->ni_rates;
    
	memset(rs, 0, sizeof(*rs));
	rs->rs_nrates = rates[1];
	memcpy(rs->rs_rates, rates + 2, rs->rs_nrates);
	if (xrates != NULL) {
		u_int8_t nxrates;
		/*
		 * Tack on 11g extended supported rate element.
		 */
		nxrates = xrates[1];
		if (rs->rs_nrates + nxrates > IEEE80211_RATE_MAXSIZE) {
			nxrates = IEEE80211_RATE_MAXSIZE - rs->rs_nrates;
			DPRINTF(("extended rate set too large; "
                     "only using %u of %u rates\n",
                     nxrates, xrates[1]));
			ic->ic_stats.is_rx_rstoobig++;
		}
		memcpy(rs->rs_rates + rs->rs_nrates, xrates+2, nxrates);
		rs->rs_nrates += nxrates;
	}
	return ieee80211_fix_rate(ic, ni, flags);
}

/*
 * Compare nodes in the tree by lladdr
 */
int voodoo_ieee80211_node_cmp(const struct ieee80211_node *b1,
                              const struct ieee80211_node *b2);
int
voodoo_ieee80211_node_cmp(const struct ieee80211_node *b1,
                          const struct ieee80211_node *b2)
{
	return (memcmp(b1->ni_macaddr, b2->ni_macaddr, IEEE80211_ADDR_LEN));
}

/*
 * Generate red-black tree function logic
 */
RB_GENERATE(ieee80211_tree, ieee80211_node, ni_node, voodoo_ieee80211_node_cmp);