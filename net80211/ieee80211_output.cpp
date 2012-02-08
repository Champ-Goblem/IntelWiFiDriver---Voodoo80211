/*	$OpenBSD: ieee80211_output.c,v 1.88 2010/07/17 16:30:01 damien Exp $	*/
/*	$NetBSD: ieee80211_output.c,v 1.13 2004/05/31 11:02:55 dyoung Exp $	*/

/*-
 * Copyright (c) 2001 Atsushi Onoe
 * Copyright (c) 2002, 2003 Sam Leffler, Errno Consulting
 * Copyright (c) 2007-2009 Damien Bergamini
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
#include <net/bpf.h>

#ifdef INET
#include <netinet/in.h>
#include <netinet/if_ether.h>
#include <netinet/in_systm.h>
#include <netinet/ip.h>
#ifdef INET6
#include <netinet/ip6.h>
#endif
#endif

#if NVLAN > 0
#include <net/if_types.h>
#include <net/if_vlan_var.h>
#endif

#include "Voodoo80211Device.h"
static const int MBUF_CLSIZE = 4096;

/*
 * IEEE 802.11 output routine. Normally this will directly call the
 * Ethernet output routine because 802.11 encapsulation is called
 * later by the driver. This function can be used to send raw frames
 * if the mbuf has been tagged with a 802.11 data link type.
 */
int Voodoo80211Device::
ieee80211_output(struct ieee80211com *ic, mbuf_t m, struct sockaddr *dst,
		 struct rtentry *rt)
{
	// TODO: implemenent sending raw 802.11 frames
	// struct ieee80211_frame *wh;
	// struct m_tag *mtag;
	// int s, len, error = 0;
	// u_short mflags;
	int error;
	
	/* Interface has to be up and running */
	if (fInterface->linkState() != kIO80211NetworkLinkUp) {
		error = ENETDOWN;
		goto bad;
	}
	
#if 0 // pvaibhav: we are not supporting sending raw 802.11 frames right now
	/* Try to get the DLT from a mbuf tag */
	if ((mtag = mbuf_tag_find(m, PACKET_TAG_DLT, NULL)) != NULL) {
		struct ieee80211com *ic = (void *)ifp;
		u_int dlt = *(u_int *)(mtag + 1);
		
		/* Fallback to ethernet for non-802.11 linktypes */
		if (!(dlt == DLT_IEEE802_11 || dlt == DLT_IEEE802_11_RADIO))
			goto fallback;
		
		if (m->m_pkthdr.len < sizeof(struct ieee80211_frame_min))
			return (EINVAL);
		wh = mtod(m, struct ieee80211_frame *);
		if ((wh->i_fc[0] & IEEE80211_FC0_VERSION_MASK) !=
		    IEEE80211_FC0_VERSION_0)
			return (EINVAL);
		if (!(ic->ic_caps & IEEE80211_C_RAWCTL) &&
		    (wh->i_fc[0] & IEEE80211_FC0_TYPE_MASK) ==
		    IEEE80211_FC0_TYPE_CTL)
			return (EINVAL);
		
		/*
		 * Queue message on interface without adding any
		 * further headers, and start output if interface not
		 * yet active.
		 */
		mflags = m->m_flags;
		len = m->m_pkthdr.len;
		s = splnet();
		IFQ_ENQUEUE(&ifp->if_snd, m, NULL, error);
		if (error) {
			/* mbuf is already freed */
			splx(s);
			printf("%s: failed to queue raw tx frame\n",
			       ifp->if_xname);
			return (error);
		}
		ifp->if_obytes += len;
		if (mflags & M_MCAST)
			ifp->if_omcasts++;
		if ((ifp->if_flags & IFF_OACTIVE) == 0)
			(*ifp->if_start)(ifp);
		splx(s);
		
		return (error);
	}
#endif // 0
fallback:
	getOutputQueue()->enqueue(m, 0);
	return getOutputQueue()->start();
	
bad:
	if (m)
		mbuf_freem(m);
	return (error);
}

/*
 * Send a management frame to the specified node.  The node pointer
 * must have a reference as the pointer will be passed to the driver
 * and potentially held for a long time.  If the frame is successfully
 * dispatched to the driver, then it is responsible for freeing the
 * reference (and potentially free'ing up any associated storage).
 */
int Voodoo80211Device::
ieee80211_mgmt_output(struct ieee80211com *ic, struct ieee80211_node *ni,
		      mbuf_t m, int type)
{
	struct ieee80211_frame *wh;
	
	if (ni == NULL)
		panic("null node");
	ni->ni_inact = 0;
	
	/*
	 * Yech, hack alert!  We want to pass the node down to the
	 * driver's start routine.  We could stick this in an m_tag
	 * and tack that on to the mbuf.  However that's rather
	 * expensive to do for every frame so instead we stuff it in
	 * the rcvif field since outbound frames do not (presently)
	 * use this.
	 */
	mbuf_prepend(&m, sizeof(struct ieee80211_frame), MBUF_DONTWAIT);
	if (m == NULL)
		return ENOMEM;
	mbuf_pkthdr_setrcvif(m, (ifnet_t)ni);
	
	wh = mtod(m, struct ieee80211_frame *);
	wh->i_fc[0] = IEEE80211_FC0_VERSION_0 | IEEE80211_FC0_TYPE_MGT | type;
	wh->i_fc[1] = IEEE80211_FC1_DIR_NODS;
	*(u_int16_t *)&wh->i_dur[0] = 0;
	*(u_int16_t *)&wh->i_seq[0] =
	htole16(ni->ni_txseq << IEEE80211_SEQ_SEQ_SHIFT);
	ni->ni_txseq++;
	IEEE80211_ADDR_COPY(wh->i_addr1, ni->ni_macaddr);
	IEEE80211_ADDR_COPY(wh->i_addr2, ic->ic_myaddr);
	IEEE80211_ADDR_COPY(wh->i_addr3, ni->ni_bssid);
	
	/* check if protection is required for this mgmt frame */
	if ((ic->ic_caps & IEEE80211_C_MFP) &&
	    (type == IEEE80211_FC0_SUBTYPE_DISASSOC ||
	     type == IEEE80211_FC0_SUBTYPE_DEAUTH ||
	     type == IEEE80211_FC0_SUBTYPE_ACTION)) {
		    /*
		     * Hack: we should not set the Protected bit in outgoing
		     * group management frames, however it is used as an
		     * indication to the drivers that they must encrypt the
		     * frame.  Drivers should clear this bit from group
		     * management frames (software crypto code will do it).
		     * XXX could use an mbuf flag..
		     */
		    if (IEEE80211_IS_MULTICAST(wh->i_addr1) ||
			(ni->ni_flags & IEEE80211_NODE_TXMGMTPROT))
			    wh->i_fc[1] |= IEEE80211_FC1_PROTECTED;
	    }
	
#if 0 // TODO debug
	if (fInterface->getFlags() & IFF_DEBUG) {
		/* avoid to print too many frames */
		if (
#ifdef IEEE80211_DEBUG
		    ieee80211_debug > 1 ||
#endif
		    (type & IEEE80211_FC0_SUBTYPE_MASK) !=
		    IEEE80211_FC0_SUBTYPE_PROBE_RESP)
			printf("%s: sending %s to %s on channel %u mode %s\n",
			       ifp->if_xname,
			       ieee80211_mgt_subtype_name[
							  (type & IEEE80211_FC0_SUBTYPE_MASK)
							  >> IEEE80211_FC0_SUBTYPE_SHIFT],
			       ether_sprintf(ni->ni_macaddr),
			       ieee80211_chan2ieee(ic, ni->ni_chan),
			       ieee80211_phymode_name[
						      ieee80211_chan2mode(ic, ni->ni_chan)]);
	}
#endif // debug
	// TODO: ic->ic_mgtq->enqueue(m); (enqueue on management queue?)
	getOutputQueue()->enqueue(m, (void*) &ieee80211_is_mgmt_frame);
	getOutputQueue()->start();
	return 0;
}

/*-
 * EDCA tables are computed using the following formulas:
 *
 * 1) EDCATable (non-AP QSTA)
 *
 * AC     CWmin 	   CWmax	   AIFSN  TXOP limit(ms)
 * -------------------------------------------------------------
 * AC_BK  aCWmin	   aCWmax	   7	  0
 * AC_BE  aCWmin	   aCWmax	   3	  0
 * AC_VI  (aCWmin+1)/2-1   aCWmin	   2	  agn=3.008 b=6.016 others=0
 * AC_VO  (aCWmin+1)/4-1   (aCWmin+1)/2-1  2	  agn=1.504 b=3.264 others=0
 *
 * 2) QAPEDCATable (QAP)
 *
 * AC     CWmin 	   CWmax	   AIFSN  TXOP limit(ms)
 * -------------------------------------------------------------
 * AC_BK  aCWmin	   aCWmax	   7	  0
 * AC_BE  aCWmin	   4*(aCWmin+1)-1  3	  0
 * AC_VI  (aCWmin+1)/2-1   aCWmin	   1	  agn=3.008 b=6.016 others=0
 * AC_VO  (aCWmin+1)/4-1   (aCWmin+1)/2-1  1	  agn=1.504 b=3.264 others=0
 *
 * and the following aCWmin/aCWmax values:
 *
 * PHY		aCWmin	aCWmax
 * ---------------------------
 * 11A		15	1023
 * 11B  	31	1023
 * 11G		15*	1023	(*) aCWmin(1)
 * Turbo A/G	7	1023	(Atheros proprietary mode)
 */
static const struct ieee80211_edca_ac_params
ieee80211_edca_table[IEEE80211_MODE_MAX][EDCA_NUM_AC] = {
	/*[IEEE80211_MODE_AUTO] = */{
		/*[EDCA_AC_BE] = */{ 0,  0, 0,   0 },
		/*[EDCA_AC_BK] = */{ 0,  0, 0,   0 },
		/*[EDCA_AC_VI] = */{ 0,  0, 0,   0 },
		/*[EDCA_AC_VO] = */{ 0,  0, 0,   0 }
	},
	/*[IEEE80211_MODE_11A] = */{
		/*[EDCA_AC_BE] = */{ 4, 10, 3,   0 },
		/*[EDCA_AC_BK] = */{ 4, 10, 7,   0 },
		/*[EDCA_AC_VI] = */{ 3,  4, 2,  94 },
		/*[EDCA_AC_VO] = */{ 2,  3, 2,  47 }
	},
	/*[IEEE80211_MODE_11B] = */{
		/*[EDCA_AC_BE] = */{ 5, 10, 3,   0 },
		/*[EDCA_AC_BK] = */{ 5, 10, 7,   0 },
		/*[EDCA_AC_VI] = */{ 4,  5, 2, 188 },
		/*[EDCA_AC_VO] = */{ 3,  4, 2, 102 }
	},
	/*[IEEE80211_MODE_11G] = */{
		/*[EDCA_AC_BE] = */{ 4, 10, 3,   0 },
		/*[EDCA_AC_BK] = */{ 4, 10, 7,   0 },
		/*[EDCA_AC_VI] = */{ 3,  4, 2,  94 },
		/*[EDCA_AC_VO] = */{ 2,  3, 2,  47 }
	},
	/*[IEEE80211_MODE_TURBO] = */{
		/*[EDCA_AC_BE] = */{ 3, 10, 2,   0 },
		/*[EDCA_AC_BK] = */{ 3, 10, 7,   0 },
		/*[EDCA_AC_VI] = */{ 2,  3, 2,  94 },
		/*[EDCA_AC_VO] = */{ 2,  2, 1,  47 }
	}
};


/*
 * Return the EDCA Access Category to be used for transmitting a frame with
 * user-priority `up'.
 */
enum ieee80211_edca_ac Voodoo80211Device::
ieee80211_up_to_ac(struct ieee80211com *ic, int up)
{
	/* see Table 9-1 */
	static const enum ieee80211_edca_ac up_to_ac[] = {
		EDCA_AC_BE,	/* BE */
		EDCA_AC_BK,	/* BK */
		EDCA_AC_BK,	/* -- */
		EDCA_AC_BE,	/* EE */
		EDCA_AC_VI,	/* CL */
		EDCA_AC_VI,	/* VI */
		EDCA_AC_VO,	/* VO */
		EDCA_AC_VO	/* NC */
	};
	enum ieee80211_edca_ac ac;
	
	ac = (up <= 7) ? up_to_ac[up] : EDCA_AC_BE;
	
	/*
	 * We do not support the admission control procedure defined in
	 * IEEE Std 802.11-2007 section 9.9.3.1.2.  The spec says that
	 * non-AP QSTAs that don't support this procedure shall use EDCA
	 * parameters of a lower priority AC that does not require
	 * admission control.
	 */
	while (ac != EDCA_AC_BK && ic->ic_edca_ac[ac].ac_acm) {
		switch (ac) {
			case EDCA_AC_BK:
				/* can't get there */
				break;
			case EDCA_AC_BE:
				/* BE shouldn't require admission control */
				ac = EDCA_AC_BK;
				break;
			case EDCA_AC_VI:
				ac = EDCA_AC_BE;
				break;
			case EDCA_AC_VO:
				ac = EDCA_AC_VI;
				break;
		}
	}
	return ac;
}

/*
 * Get mbuf's user-priority: if mbuf is not VLAN tagged, select user-priority
 * based on the DSCP (Differentiated Services Codepoint) field.
 */
int Voodoo80211Device::
ieee80211_classify(struct ieee80211com *ic, mbuf_t m)
{
	return 0;
#if 0 // TODO disable this function and default to best-effort
#ifdef INET
	struct ether_header *eh;
	u_int8_t ds_field;
#endif
#if NVLAN > 0
	if (mbuf_flags(m) & M_VLANTAG)	/* use VLAN 802.1D user-priority */
		return EVL_PRIOFTAG(m->m_pkthdr.ether_vtag);
#endif
#ifdef INET
	eh = mtod(m, struct ether_header *);
	if (eh->ether_type == htons(ETHERTYPE_IP)) {
		struct ip *ip = (struct ip *)&eh[1];
		if (ip->ip_v != 4)
			return 0;
		ds_field = ip->ip_tos;
	}
#ifdef INET6
	else if (eh->ether_type == htons(ETHERTYPE_IPV6)) {
		struct ip6_hdr *ip6 = (struct ip6_hdr *)&eh[1];
		u_int32_t flowlabel;
		
		flowlabel = ntohl(ip6->ip6_flow);
		if ((flowlabel >> 28) != 6)
			return 0;
		ds_field = (flowlabel >> 20) & 0xff;
	}
#endif	/* INET6 */
	else	/* neither IPv4 nor IPv6 */
		return 0;
	
	/*
	 * Map Differentiated Services Codepoint field (see RFC2474).
	 * Preserves backward compatibility with IP Precedence field.
	 */
	switch (ds_field & 0xfc) {
		case IPTOS_PREC_PRIORITY:
			return 2;
		case IPTOS_PREC_IMMEDIATE:
			return 1;
		case IPTOS_PREC_FLASH:
			return 3;
		case IPTOS_PREC_FLASHOVERRIDE:
			return 4;
		case IPTOS_PREC_CRITIC_ECP:
			return 5;
		case IPTOS_PREC_INTERNETCONTROL:
			return 6;
		case IPTOS_PREC_NETCONTROL:
			return 7;
	}
#endif	/* INET */
	return 0;	/* default to Best-Effort */
#endif
}

/*
 * Encapsulate an outbound data frame.  The mbuf chain is updated and
 * a reference to the destination node is returned.  If an error is
 * encountered NULL is returned and the node reference will also be NULL.
 *
 * NB: The caller is responsible for free'ing a returned node reference.
 *     The convention is ic_bss is not reference counted; the caller must
 *     maintain that.
 */
mbuf_t Voodoo80211Device::
ieee80211_encap(struct ieee80211com *ic, mbuf_t m, struct ieee80211_node **pni)
{
	struct ether_header eh;
	struct ieee80211_frame *wh;
	struct ieee80211_node *ni = NULL;
	struct llc *llc;
//	struct m_tag *mtag;
//	u_int8_t *addr;
	u_int /*dlt, */hdrlen;
	int addqos, tid;
	
#if 0 // TODO: handle raw frames
	/* Handle raw frames if mbuf is tagged as 802.11 */
	if ((mtag = m_tag_find(m, PACKET_TAG_DLT, NULL)) != NULL) {
		dlt = *(u_int *)(mtag + 1);
		
		if (!(dlt == DLT_IEEE802_11 || dlt == DLT_IEEE802_11_RADIO))
			goto fallback;
		
		wh = mtod(m, struct ieee80211_frame *);
		switch (wh->i_fc[1] & IEEE80211_FC1_DIR_MASK) {
			case IEEE80211_FC1_DIR_NODS:
			case IEEE80211_FC1_DIR_FROMDS:
				addr = wh->i_addr1;
				break;
			case IEEE80211_FC1_DIR_DSTODS:
			case IEEE80211_FC1_DIR_TODS:
				addr = wh->i_addr3;
				break;
			default:
				goto bad;
		}
		
		ni = ieee80211_find_txnode(ic, addr);
		if (ni == NULL)
			ni = ieee80211_ref_node(ic->ic_bss);
		if (ni == NULL) {
			printf("%s: no node for dst %s, "
			       "discard raw tx frame\n", ifp->if_xname,
			       ether_sprintf(addr));
			ic->ic_stats.is_tx_nonode++;
			goto bad;
		}
		ni->ni_inact = 0;
		
		*pni = ni;
		return (m);
	}
#endif
	
fallback:
	if (mbuf_len(m) < sizeof(struct ether_header)) {
		mbuf_pullup(&m, sizeof(struct ether_header));
		if (m == NULL) {
			ic->ic_stats.is_tx_nombuf++;
			goto bad;
		}
	}
	memcpy(&eh, mtod(m, caddr_t), sizeof(struct ether_header));
	
	ni = ieee80211_find_txnode(ic, eh.ether_dhost);
	if (ni == NULL) {
		DPRINTF(("no node for dst %s, discard frame\n",
			 ether_sprintf(eh.ether_dhost)));
		ic->ic_stats.is_tx_nonode++;
		goto bad;
	}
	
	if ((ic->ic_flags & IEEE80211_F_RSNON) &&
	    !ni->ni_port_valid &&
	    eh.ether_type != htons(ETHERTYPE_PAE)) {
		DPRINTF(("port not valid: %s\n",
			 ether_sprintf(eh.ether_dhost)));
		ic->ic_stats.is_tx_noauth++;
		goto bad;
	}
	
	if ((ic->ic_flags & IEEE80211_F_COUNTERM) &&
	    ni->ni_rsncipher == IEEE80211_CIPHER_TKIP)
	{
	/* XXX TKIP countermeasures! */
	}
	
	ni->ni_inact = 0;
	
	if ((ic->ic_flags & IEEE80211_F_QOS) &&
	    (ni->ni_flags & IEEE80211_NODE_QOS) &&
	    /* do not QoS-encapsulate EAPOL frames */
	    eh.ether_type != htons(ETHERTYPE_PAE)) {
		tid = ieee80211_classify(ic, m);
		hdrlen = sizeof(struct ieee80211_qosframe);
		addqos = 1;
	} else {
		hdrlen = sizeof(struct ieee80211_frame);
		addqos = 0;
	}
	mbuf_adj(m, sizeof(struct ether_header) - LLC_SNAPFRAMELEN);
	llc = mtod(m, struct llc *);
	llc->llc_dsap = llc->llc_ssap = LLC_SNAP_LSAP;
	llc->llc_control = LLC_UI;
	llc->llc_snap.org_code[0] = 0;
	llc->llc_snap.org_code[1] = 0;
	llc->llc_snap.org_code[2] = 0;
	llc->llc_snap.ether_type = eh.ether_type;
	mbuf_prepend(&m, hdrlen, MBUF_DONTWAIT);
	if (m == NULL) {
		ic->ic_stats.is_tx_nombuf++;
		goto bad;
	}
	wh = mtod(m, struct ieee80211_frame *);
	wh->i_fc[0] = IEEE80211_FC0_VERSION_0 | IEEE80211_FC0_TYPE_DATA;
	*(u_int16_t *)&wh->i_dur[0] = 0;
	if (addqos) {
		struct ieee80211_qosframe *qwh =
		(struct ieee80211_qosframe *)wh;
		u_int16_t qos = tid;
		
		if (ic->ic_tid_noack & (1 << tid))
			qos |= IEEE80211_QOS_ACK_POLICY_NOACK;
#ifndef IEEE80211_NO_HT
		else if (ni->ni_tx_ba[tid].ba_state == IEEE80211_BA_AGREED)
			qos |= IEEE80211_QOS_ACK_POLICY_BA;
#endif
		qwh->i_fc[0] |= IEEE80211_FC0_SUBTYPE_QOS;
		*(u_int16_t *)qwh->i_qos = htole16(qos);
		*(u_int16_t *)qwh->i_seq =
		htole16(ni->ni_qos_txseqs[tid] << IEEE80211_SEQ_SEQ_SHIFT);
		ni->ni_qos_txseqs[tid]++;
	} else {
		*(u_int16_t *)&wh->i_seq[0] =
		htole16(ni->ni_txseq << IEEE80211_SEQ_SEQ_SHIFT);
		ni->ni_txseq++;
	}
	switch (ic->ic_opmode) {
		case IEEE80211_M_STA:
			wh->i_fc[1] = IEEE80211_FC1_DIR_TODS;
			IEEE80211_ADDR_COPY(wh->i_addr1, ni->ni_bssid);
			IEEE80211_ADDR_COPY(wh->i_addr2, eh.ether_shost);
			IEEE80211_ADDR_COPY(wh->i_addr3, eh.ether_dhost);
			break;
		default:
			/* should not get there */
			goto bad;
	}
	
	if ((ic->ic_flags & IEEE80211_F_WEPON) ||
	    ((ic->ic_flags & IEEE80211_F_RSNON) &&
	     (ni->ni_flags & IEEE80211_NODE_TXPROT)))
		wh->i_fc[1] |= IEEE80211_FC1_PROTECTED;
	
	*pni = ni;
	return m;
bad:
	if (m != NULL)
		mbuf_freem(m);
	if (ni != NULL)
		ieee80211_release_node(ic, ni);
	*pni = NULL;
	return NULL;
}

/*
 * Add a Capability Information field to a frame (see 7.3.1.4).
 */
u_int8_t * Voodoo80211Device::
ieee80211_add_capinfo(u_int8_t *frm, struct ieee80211com *ic,
		      const struct ieee80211_node *ni)
{
	u_int16_t capinfo;
	
		capinfo = 0;
	/* NB: some 11a AP's reject the request when short preamble is set */
	if ((ic->ic_flags & IEEE80211_F_SHPREAMBLE) &&
	    IEEE80211_IS_CHAN_2GHZ(ni->ni_chan))
		capinfo |= IEEE80211_CAPINFO_SHORT_PREAMBLE;
	if (ic->ic_flags & IEEE80211_F_SHSLOT)
		capinfo |= IEEE80211_CAPINFO_SHORT_SLOTTIME;
	LE_WRITE_2(frm, capinfo);
	return frm + 2;
}

/*
 * Add an SSID element to a frame (see 7.3.2.1).
 */
u_int8_t * Voodoo80211Device::
ieee80211_add_ssid(u_int8_t *frm, const u_int8_t *ssid, u_int len)
{
	*frm++ = IEEE80211_ELEMID_SSID;
	*frm++ = len;
	memcpy(frm, ssid, len);
	return frm + len;
}

/*
 * Add a supported rates element to a frame (see 7.3.2.2).
 */
u_int8_t * Voodoo80211Device::
ieee80211_add_rates(u_int8_t *frm, const struct ieee80211_rateset *rs)
{
	int nrates;
	
	*frm++ = IEEE80211_ELEMID_RATES;
	nrates = min(rs->rs_nrates, IEEE80211_RATE_SIZE);
	*frm++ = nrates;
	memcpy(frm, rs->rs_rates, nrates);
	return frm + nrates;
}

/*
 * Add a QoS Capability element to a frame (see 7.3.2.35).
 */
u_int8_t * Voodoo80211Device::
ieee80211_add_qos_capability(u_int8_t *frm, struct ieee80211com *ic)
{
	*frm++ = IEEE80211_ELEMID_QOS_CAP;
	*frm++ = 1;
	*frm++ = 0;	/* QoS Info */
	return frm;
}

/*
 * Add an RSN element to a frame (see 7.3.2.25).
 */
u_int8_t * Voodoo80211Device::
ieee80211_add_rsn_body(u_int8_t *frm, struct ieee80211com *ic,
		       const struct ieee80211_node *ni, int wpa)
{
	const u_int8_t *oui = wpa ? MICROSOFT_OUI : IEEE80211_OUI;
	u_int8_t *pcount;
	u_int16_t count;
	
	/* write Version field */
	LE_WRITE_2(frm, 1); frm += 2;
	
	/* write Group Data Cipher Suite field (see Table 20da) */
	memcpy(frm, oui, 3); frm += 3;
	switch (ni->ni_rsngroupcipher) {
		case IEEE80211_CIPHER_WEP40:
			*frm++ = 1;
			break;
		case IEEE80211_CIPHER_TKIP:
			*frm++ = 2;
			break;
		case IEEE80211_CIPHER_CCMP:
			*frm++ = 4;
			break;
		case IEEE80211_CIPHER_WEP104:
			*frm++ = 5;
			break;
		default:
			/* can't get there */
			panic("invalid group data cipher!");
	}
	
	pcount = frm; frm += 2;
	count = 0;
	/* write Pairwise Cipher Suite List */
	if (ni->ni_rsnciphers & IEEE80211_CIPHER_USEGROUP) {
		memcpy(frm, oui, 3); frm += 3;
		*frm++ = 0;
		count++;
	}
	if (ni->ni_rsnciphers & IEEE80211_CIPHER_TKIP) {
		memcpy(frm, oui, 3); frm += 3;
		*frm++ = 2;
		count++;
	}
	if (ni->ni_rsnciphers & IEEE80211_CIPHER_CCMP) {
		memcpy(frm, oui, 3); frm += 3;
		*frm++ = 4;
		count++;
	}
	/* write Pairwise Cipher Suite Count field */
	LE_WRITE_2(pcount, count);
	
	pcount = frm; frm += 2;
	count = 0;
	/* write AKM Suite List (see Table 20dc) */
	if (ni->ni_rsnakms & IEEE80211_AKM_8021X) {
		memcpy(frm, oui, 3); frm += 3;
		*frm++ = 1;
		count++;
	}
	if (ni->ni_rsnakms & IEEE80211_AKM_PSK) {
		memcpy(frm, oui, 3); frm += 3;
		*frm++ = 2;
		count++;
	}
	if (!wpa && (ni->ni_rsnakms & IEEE80211_AKM_SHA256_8021X)) {
		memcpy(frm, oui, 3); frm += 3;
		*frm++ = 5;
		count++;
	}
	if (!wpa && (ni->ni_rsnakms & IEEE80211_AKM_SHA256_PSK)) {
		memcpy(frm, oui, 3); frm += 3;
		*frm++ = 6;
		count++;
	}
	/* write AKM Suite List Count field */
	LE_WRITE_2(pcount, count);
	
	if (wpa)
		return frm;
	
	/* write RSN Capabilities field */
	LE_WRITE_2(frm, ni->ni_rsncaps); frm += 2;
	
	if (ni->ni_flags & IEEE80211_NODE_PMKID) {
		/* write PMKID Count field */
		LE_WRITE_2(frm, 1); frm += 2;
		/* write PMKID List (only 1) */
		memcpy(frm, ni->ni_pmkid, IEEE80211_PMKID_LEN);
		frm += IEEE80211_PMKID_LEN;
	} else {
		/* no PMKID (PMKID Count=0) */
		LE_WRITE_2(frm, 0); frm += 2;
	}
	
	if (!(ic->ic_caps & IEEE80211_C_MFP))
		return frm;
	
	/* write Group Integrity Cipher Suite field */
	memcpy(frm, oui, 3); frm += 3;
	switch (ic->ic_rsngroupmgmtcipher) {
		case IEEE80211_CIPHER_BIP:
			*frm++ = 6;
			break;
		default:
			/* can't get there */
			panic("invalid integrity group cipher!");
	}
	return frm;
}

u_int8_t * Voodoo80211Device::
ieee80211_add_rsn(u_int8_t *frm, struct ieee80211com *ic,
		  const struct ieee80211_node *ni)
{
	u_int8_t *plen;
	
	*frm++ = IEEE80211_ELEMID_RSN;
	plen = frm++;	/* length filled in later */
	frm = ieee80211_add_rsn_body(frm, ic, ni, 0);
	
	/* write length field */
	*plen = frm - plen - 1;
	return frm;
}

/*
 * Add a vendor-specific WPA element to a frame.
 * This is required for compatibility with Wi-Fi Alliance WPA.
 */
u_int8_t * Voodoo80211Device::
ieee80211_add_wpa(u_int8_t *frm, struct ieee80211com *ic,
		  const struct ieee80211_node *ni)
{
	u_int8_t *plen;
	
	*frm++ = IEEE80211_ELEMID_VENDOR;
	plen = frm++;	/* length filled in later */
	memcpy(frm, MICROSOFT_OUI, 3); frm += 3;
	*frm++ = 1;	/* WPA */
	frm = ieee80211_add_rsn_body(frm, ic, ni, 1);
	
	/* write length field */
	*plen = frm - plen - 1;
	return frm;
}

/*
 * Add an extended supported rates element to a frame (see 7.3.2.14).
 */
u_int8_t * Voodoo80211Device::
ieee80211_add_xrates(u_int8_t *frm, const struct ieee80211_rateset *rs)
{
	int nrates;
	
	assert(rs->rs_nrates > IEEE80211_RATE_SIZE);
	
	*frm++ = IEEE80211_ELEMID_XRATES;
	nrates = rs->rs_nrates - IEEE80211_RATE_SIZE;
	*frm++ = nrates;
	memcpy(frm, rs->rs_rates + IEEE80211_RATE_SIZE, nrates);
	return frm + nrates;
}

#ifndef IEEE80211_NO_HT
/*
 * Add an HT Capabilities element to a frame (see 7.3.2.57).
 */
u_int8_t * Voodoo80211Device::
ieee80211_add_htcaps(u_int8_t *frm, struct ieee80211com *ic)
{
	*frm++ = IEEE80211_ELEMID_HTCAPS;
	*frm++ = 26;
	LE_WRITE_2(frm, ic->ic_htcaps); frm += 2;
	*frm++ = 0;
	memcpy(frm, ic->ic_sup_mcs, 16); frm += 16;
	LE_WRITE_2(frm, ic->ic_htxcaps); frm += 2;
	LE_WRITE_4(frm, ic->ic_txbfcaps); frm += 4;
	*frm++ = ic->ic_aselcaps;
	return frm;
}

#endif	/* !IEEE80211_NO_HT */

mbuf_t Voodoo80211Device::
ieee80211_getmgmt(int flags, int type, u_int pktlen)
{
	// TODO: carefully understand and fix this if needed
	mbuf_t m;
	
	/* reserve space for 802.11 header */
	pktlen += sizeof(struct ieee80211_frame);
	
	if (pktlen > MCLBYTES)
		panic("management frame too large: %u", pktlen);
	mbuf_gethdr(flags, type, &m);
	if (m == NULL)
		return NULL;
	if (pktlen > mbuf_get_mhlen()) {
		mbuf_getcluster(flags, type, MBUF_CLSIZE, &m);
		if (!(mbuf_flags(m) & MBUF_EXT))
			return mbuf_free(m);
	}
	mbuf_setdata(m, (u_int8_t*) mbuf_data(m) 
		     + sizeof(struct ieee80211_frame), mbuf_len(m));
	return m;
}

/*-
 * Probe request frame format:
 * [tlv] SSID
 * [tlv] Supported rates
 * [tlv] Extended Supported Rates (802.11g)
 * [tlv] HT Capabilities (802.11n)
 */
mbuf_t Voodoo80211Device::
ieee80211_get_probe_req(struct ieee80211com *ic, struct ieee80211_node *ni)
{
	const struct ieee80211_rateset *rs =
	&ic->ic_sup_rates[ieee80211_chan2mode(ic, ni->ni_chan)];
	mbuf_t m;
	u_int8_t *frm;
	
	m = ieee80211_getmgmt(MBUF_DONTWAIT, MT_DATA,
			      2 + ic->ic_des_esslen +
			      2 + min(rs->rs_nrates, IEEE80211_RATE_SIZE) +
			      ((rs->rs_nrates > IEEE80211_RATE_SIZE) ?
			       2 + rs->rs_nrates - IEEE80211_RATE_SIZE : 0) +
			      ((ni->ni_flags & IEEE80211_NODE_HT) ? 28 : 0));
	if (m == NULL)
		return NULL;
	
	frm = mtod(m, u_int8_t *);
	frm = ieee80211_add_ssid(frm, ic->ic_des_essid, ic->ic_des_esslen);
	frm = ieee80211_add_rates(frm, rs);
	if (rs->rs_nrates > IEEE80211_RATE_SIZE)
		frm = ieee80211_add_xrates(frm, rs);
#ifndef IEEE80211_NO_HT
	if (ni->ni_flags & IEEE80211_NODE_HT)
		frm = ieee80211_add_htcaps(frm, ic);
#endif
	
	mbuf_pkthdr_setlen(m, frm - mtod(m, u_int8_t *));
	mbuf_setlen(m, frm - mtod(m, u_int8_t *));
	
	return m;
}

/*-
 * Authentication frame format:
 * [2] Authentication algorithm number
 * [2] Authentication transaction sequence number
 * [2] Status code
 */
mbuf_t Voodoo80211Device::
ieee80211_get_auth(struct ieee80211com *ic, struct ieee80211_node *ni,
		   u_int16_t status, u_int16_t seq)
{
	mbuf_t m;
	u_int8_t *frm;
	
	mbuf_gethdr(MBUF_DONTWAIT, MT_DATA, &m);
	if (m == NULL)
		return NULL;
	mbuf_align_32(m, 2 * 3);
	mbuf_pkthdr_setlen(m, 2 * 3);
	mbuf_setlen(m, 2 * 3);
	
	frm = mtod(m, u_int8_t *);
	LE_WRITE_2(frm, IEEE80211_AUTH_ALG_OPEN); frm += 2;
	LE_WRITE_2(frm, seq); frm += 2;
	LE_WRITE_2(frm, status);
	
	return m;
}

/*-
 * Deauthentication frame format:
 * [2] Reason code
 */
mbuf_t Voodoo80211Device::
ieee80211_get_deauth(struct ieee80211com *ic, struct ieee80211_node *ni,
		     u_int16_t reason)
{
	mbuf_t m;
	
	mbuf_gethdr(MBUF_DONTWAIT, MT_DATA, &m);
	if (m == NULL)
		return NULL;
	mbuf_align_32(m, 2);
	
	mbuf_pkthdr_setlen(m, 2);
	mbuf_setlen(m, 2);
	*mtod(m, u_int16_t *) = htole16(reason);
	
	return m;
}

/*-
 * (Re)Association request frame format:
 * [2]   Capability information
 * [2]   Listen interval
 * [6*]  Current AP address (Reassociation only)
 * [tlv] SSID
 * [tlv] Supported rates
 * [tlv] Extended Supported Rates (802.11g)
 * [tlv] RSN (802.11i)
 * [tlv] QoS Capability (802.11e)
 * [tlv] HT Capabilities (802.11n)
 */
mbuf_t Voodoo80211Device::
ieee80211_get_assoc_req(struct ieee80211com *ic, struct ieee80211_node *ni,
			int type)
{
	const struct ieee80211_rateset *rs = &ni->ni_rates;
	mbuf_t m;
	u_int8_t *frm;
	u_int16_t capinfo;
	
	m = ieee80211_getmgmt(MBUF_DONTWAIT, MT_DATA,
			      2 + 2 +
			      ((type == IEEE80211_FC0_SUBTYPE_REASSOC_REQ) ?
			       IEEE80211_ADDR_LEN : 0) +
			      2 + ni->ni_esslen +
			      2 + min(rs->rs_nrates, IEEE80211_RATE_SIZE) +
			      ((rs->rs_nrates > IEEE80211_RATE_SIZE) ?
			       2 + rs->rs_nrates - IEEE80211_RATE_SIZE : 0) +
			      (((ic->ic_flags & IEEE80211_F_RSNON) &&
				(ni->ni_rsnprotos & IEEE80211_PROTO_RSN)) ?
			       2 + IEEE80211_RSNIE_MAXLEN : 0) +
			      ((ni->ni_flags & IEEE80211_NODE_QOS) ? 2 + 1 : 0) +
			      (((ic->ic_flags & IEEE80211_F_RSNON) &&
				(ni->ni_rsnprotos & IEEE80211_PROTO_WPA)) ?
			       2 + IEEE80211_WPAIE_MAXLEN : 0) +
			      ((ni->ni_flags & IEEE80211_NODE_HT) ? 28 : 0));
	if (m == NULL)
		return NULL;
	
	frm = mtod(m, u_int8_t *);
	capinfo = IEEE80211_CAPINFO_ESS;
	if (ic->ic_flags & IEEE80211_F_WEPON)
		capinfo |= IEEE80211_CAPINFO_PRIVACY;
	if ((ic->ic_flags & IEEE80211_F_SHPREAMBLE) &&
	    IEEE80211_IS_CHAN_2GHZ(ni->ni_chan))
		capinfo |= IEEE80211_CAPINFO_SHORT_PREAMBLE;
	if (ic->ic_caps & IEEE80211_C_SHSLOT)
		capinfo |= IEEE80211_CAPINFO_SHORT_SLOTTIME;
	LE_WRITE_2(frm, capinfo); frm += 2;
	LE_WRITE_2(frm, ic->ic_lintval); frm += 2;
	if (type == IEEE80211_FC0_SUBTYPE_REASSOC_REQ) {
		IEEE80211_ADDR_COPY(frm, ic->ic_bss->ni_bssid);
		frm += IEEE80211_ADDR_LEN;
	}
	frm = ieee80211_add_ssid(frm, ni->ni_essid, ni->ni_esslen);
	frm = ieee80211_add_rates(frm, rs);
	if (rs->rs_nrates > IEEE80211_RATE_SIZE)
		frm = ieee80211_add_xrates(frm, rs);
	if ((ic->ic_flags & IEEE80211_F_RSNON) &&
	    (ni->ni_rsnprotos & IEEE80211_PROTO_RSN))
		frm = ieee80211_add_rsn(frm, ic, ni);
	if (ni->ni_flags & IEEE80211_NODE_QOS)
		frm = ieee80211_add_qos_capability(frm, ic);
	if ((ic->ic_flags & IEEE80211_F_RSNON) &&
	    (ni->ni_rsnprotos & IEEE80211_PROTO_WPA))
		frm = ieee80211_add_wpa(frm, ic, ni);
#ifndef IEEE80211_NO_HT
	if (ni->ni_flags & IEEE80211_NODE_HT)
		frm = ieee80211_add_htcaps(frm, ic);
#endif
	
	mbuf_pkthdr_setlen(m, frm - mtod(m, u_int8_t *));
	mbuf_setlen(m, frm - mtod(m, u_int8_t *));
	
	return m;
}

/*-
 * Disassociation frame format:
 * [2] Reason code
 */
mbuf_t Voodoo80211Device::
ieee80211_get_disassoc(struct ieee80211com *ic, struct ieee80211_node *ni,
		       u_int16_t reason)
{
	mbuf_t m;
	
	mbuf_gethdr(MBUF_DONTWAIT, MT_DATA, &m);
	if (m == NULL)
		return NULL;
	mbuf_align_32(m, 2);
	
	mbuf_pkthdr_setlen(m, 2);
	mbuf_setlen(m, 2);
	
	*mtod(m, u_int16_t *) = htole16(reason);
	
	return m;
}

#ifndef IEEE80211_NO_HT
/*-
 * ADDBA Request frame format:
 * [1] Category
 * [1] Action
 * [1] Dialog Token
 * [2] Block Ack Parameter Set
 * [2] Block Ack Timeout Value
 * [2] Block Ack Starting Sequence Control
 */
mbuf_t Voodoo80211Device::
ieee80211_get_addba_req(struct ieee80211com *ic, struct ieee80211_node *ni,
			u_int8_t tid)
{
	struct ieee80211_tx_ba *ba = &ni->ni_tx_ba[tid];
	mbuf_t m;
	u_int8_t *frm;
	u_int16_t params;
	
	m = ieee80211_getmgmt(MBUF_DONTWAIT, MT_DATA, 9);
	if (m == NULL)
		return m;
	
	frm = mtod(m, u_int8_t *);
	*frm++ = IEEE80211_CATEG_BA;
	*frm++ = IEEE80211_ACTION_ADDBA_REQ;
	*frm++ = ba->ba_token;
	params = ba->ba_winsize << 6 | tid << 2 | IEEE80211_BA_ACK_POLICY;
	LE_WRITE_2(frm, params); frm += 2;
	LE_WRITE_2(frm, ba->ba_timeout_val); frm += 2;
	LE_WRITE_2(frm, ba->ba_winstart); frm += 2;
	
	mbuf_pkthdr_setlen(m, frm - mtod(m, u_int8_t *));
	mbuf_setlen(m, frm - mtod(m, u_int8_t *));
	
	return m;
}

/*-
 * ADDBA Response frame format:
 * [1] Category
 * [1] Action
 * [1] Dialog Token
 * [2] Status Code
 * [2] Block Ack Parameter Set
 * [2] Block Ack Timeout Value
 */
mbuf_t Voodoo80211Device::
ieee80211_get_addba_resp(struct ieee80211com *ic, struct ieee80211_node *ni,
			 u_int8_t tid, u_int8_t token, u_int16_t status)
{
	struct ieee80211_rx_ba *ba = &ni->ni_rx_ba[tid];
	mbuf_t m;
	u_int8_t *frm;
	u_int16_t params;
	
	m = ieee80211_getmgmt(MBUF_DONTWAIT, MT_DATA, 9);
	if (m == NULL)
		return m;
	
	frm = mtod(m, u_int8_t *);
	*frm++ = IEEE80211_CATEG_BA;
	*frm++ = IEEE80211_ACTION_ADDBA_RESP;
	*frm++ = token;
	LE_WRITE_2(frm, status); frm += 2;
	params = tid << 2 | IEEE80211_BA_ACK_POLICY;
	if (status == 0)
		params |= ba->ba_winsize << 6;
	LE_WRITE_2(frm, params); frm += 2;
	if (status == 0)
		LE_WRITE_2(frm, ba->ba_timeout_val);
	else
		LE_WRITE_2(frm, 0);
	frm += 2;
	
	mbuf_pkthdr_setlen(m, frm - mtod(m, u_int8_t *));
	mbuf_setlen(m, frm - mtod(m, u_int8_t *));
	
	return m;
}

/*-
 * DELBA frame format:
 * [1] Category
 * [1] Action
 * [2] DELBA Parameter Set
 * [2] Reason Code
 */
mbuf_t Voodoo80211Device::
ieee80211_get_delba(struct ieee80211com *ic, struct ieee80211_node *ni,
		    u_int8_t tid, u_int8_t dir, u_int16_t reason)
{
	mbuf_t m;
	u_int8_t *frm;
	u_int16_t params;
	
	m = ieee80211_getmgmt(MBUF_DONTWAIT, MT_DATA, 6);
	if (m == NULL)
		return m;
	
	frm = mtod(m, u_int8_t *);
	*frm++ = IEEE80211_CATEG_BA;
	*frm++ = IEEE80211_ACTION_DELBA;
	params = tid << 12;
	if (dir)
		params |= IEEE80211_DELBA_INITIATOR;
	LE_WRITE_2(frm, params); frm += 2;
	LE_WRITE_2(frm, reason); frm += 2;
	
	mbuf_pkthdr_setlen(m, frm - mtod(m, u_int8_t *));
	mbuf_setlen(m, frm - mtod(m, u_int8_t *));
	
	return m;
}
#endif	/* !IEEE80211_NO_HT */

/*-
 * SA Query Request/Reponse frame format:
 * [1]  Category
 * [1]  Action
 * [16] Transaction Identifier
 */
mbuf_t Voodoo80211Device::
ieee80211_get_sa_query(struct ieee80211com *ic, struct ieee80211_node *ni,
		       u_int8_t action)
{
	mbuf_t m;
	u_int8_t *frm;
	
	m = ieee80211_getmgmt(MBUF_DONTWAIT, MT_DATA, 4);
	if (m == NULL)
		return NULL;
	
	frm = mtod(m, u_int8_t *);
	*frm++ = IEEE80211_CATEG_SA_QUERY;
	*frm++ = action;	/* ACTION_SA_QUERY_REQ/RESP */
	LE_WRITE_2(frm, ni->ni_sa_query_trid); frm += 2;
	
	mbuf_pkthdr_setlen(m, frm - mtod(m, u_int8_t *));
	mbuf_setlen(m, frm - mtod(m, u_int8_t *));
	
	return m;
}

mbuf_t Voodoo80211Device::
ieee80211_get_action(struct ieee80211com *ic, struct ieee80211_node *ni,
		     u_int8_t categ, u_int8_t action, int arg)
{
	mbuf_t m = NULL;
	
	switch (categ) {
#ifndef IEEE80211_NO_HT
		case IEEE80211_CATEG_BA:
			switch (action) {
				case IEEE80211_ACTION_ADDBA_REQ:
					m = ieee80211_get_addba_req(ic, ni, arg & 0xffff);
					break;
				case IEEE80211_ACTION_ADDBA_RESP:
					m = ieee80211_get_addba_resp(ic, ni, arg & 0xff,
								     arg >> 8, arg >> 16);
					break;
				case IEEE80211_ACTION_DELBA:
					m = ieee80211_get_delba(ic, ni, arg & 0xff, arg >> 8,
								arg >> 16);
					break;
			}
			break;
#endif
		case IEEE80211_CATEG_SA_QUERY:
			switch (action) {
				case IEEE80211_ACTION_SA_QUERY_RESP:
					m = ieee80211_get_sa_query(ic, ni, action);
					break;
			}
			break;
	}
	return m;
}

/*
 * Send a management frame.  The node is for the destination (or ic_bss
 * when in station mode).  Nodes other than ic_bss have their reference
 * count bumped to reflect our use for an indeterminant time.
 */
int Voodoo80211Device::
ieee80211_send_mgmt(struct ieee80211com *ic, struct ieee80211_node *ni,
		    int type, int arg1, int arg2)
{
#define	senderr(_x, _v)	do { ic->ic_stats._v++; ret = _x; goto bad; } while (0)
	mbuf_t m;
	int ret, timer;
	
	if (ni == NULL)
		panic("null node");
	
	/*
	 * Hold a reference on the node so it doesn't go away until after
	 * the xmit is complete all the way in the driver.  On error we
	 * will remove our reference.
	 */
	ieee80211_ref_node(ni);
	timer = 0;
	switch (type) {
		case IEEE80211_FC0_SUBTYPE_PROBE_REQ:
			if ((m = ieee80211_get_probe_req(ic, ni)) == NULL)
				senderr(ENOMEM, is_tx_nombuf);
			
			timer = IEEE80211_TRANS_WAIT;
			break;
		case IEEE80211_FC0_SUBTYPE_AUTH:
			m = ieee80211_get_auth(ic, ni, arg1 >> 16, arg1 & 0xffff);
			if (m == NULL)
				senderr(ENOMEM, is_tx_nombuf);
			
			if (ic->ic_opmode == IEEE80211_M_STA)
				timer = IEEE80211_TRANS_WAIT;
			break;
			
		case IEEE80211_FC0_SUBTYPE_DEAUTH:
			if ((m = ieee80211_get_deauth(ic, ni, arg1)) == NULL)
				senderr(ENOMEM, is_tx_nombuf);
			
			/* TODO debug
			if (ifp->if_flags & IFF_DEBUG) {
				printf("%s: station %s deauthenticate (reason %d)\n",
				       ifp->if_xname, ether_sprintf(ni->ni_macaddr),
				       arg1);
			}
			 */
			break;
			
		case IEEE80211_FC0_SUBTYPE_ASSOC_REQ:
		case IEEE80211_FC0_SUBTYPE_REASSOC_REQ:
			if ((m = ieee80211_get_assoc_req(ic, ni, type)) == NULL)
				senderr(ENOMEM, is_tx_nombuf);
			
			timer = IEEE80211_TRANS_WAIT;
			break;
		case IEEE80211_FC0_SUBTYPE_DISASSOC:
			if ((m = ieee80211_get_disassoc(ic, ni, arg1)) == NULL)
				senderr(ENOMEM, is_tx_nombuf);
			
			/* TODO debug
			if (ifp->if_flags & IFF_DEBUG) {
				printf("%s: station %s disassociate (reason %d)\n",
				       ifp->if_xname, ether_sprintf(ni->ni_macaddr),
				       arg1);
			}
			 */
			break;
			
		case IEEE80211_FC0_SUBTYPE_ACTION:
			m = ieee80211_get_action(ic, ni, arg1 >> 16, arg1 & 0xffff,
						 arg2);
			if (m == NULL)
				senderr(ENOMEM, is_tx_nombuf);
			break;
			
		default:
			DPRINTF(("invalid mgmt frame type %u\n", type));
			senderr(EINVAL, is_tx_unknownmgt);
			/* NOTREACHED */
	}
	
	ret = ieee80211_mgmt_output(ic, ni, m, type);
	if (ret == 0) {
		if (timer)
			ic->ic_mgt_timer = timer;
	} else {
	bad:
		ieee80211_release_node(ic, ni);
	}
	return ret;
#undef senderr
}

/*
 * Build a RTS (Request To Send) control frame (see 7.2.1.1).
 */
mbuf_t Voodoo80211Device::
ieee80211_get_rts(struct ieee80211com *ic, const struct ieee80211_frame *wh,
		  u_int16_t dur)
{
	struct ieee80211_frame_rts *rts;
	mbuf_t m;
	
	mbuf_gethdr(MBUF_DONTWAIT, MT_DATA, &m);
	if (m == NULL)
		return NULL;
	
	mbuf_pkthdr_setlen(m, sizeof(struct ieee80211_frame_rts));
	mbuf_setlen(m, sizeof(struct ieee80211_frame_rts));
	
	rts = mtod(m, struct ieee80211_frame_rts *);
	rts->i_fc[0] = IEEE80211_FC0_VERSION_0 | IEEE80211_FC0_TYPE_CTL |
	IEEE80211_FC0_SUBTYPE_RTS;
	rts->i_fc[1] = IEEE80211_FC1_DIR_NODS;
	*(u_int16_t *)rts->i_dur = htole16(dur);
	IEEE80211_ADDR_COPY(rts->i_ra, wh->i_addr1);
	IEEE80211_ADDR_COPY(rts->i_ta, wh->i_addr2);
	
	return m;
}

/*
 * Build a CTS-to-self (Clear To Send) control frame (see 7.2.1.2).
 */
mbuf_t Voodoo80211Device::
ieee80211_get_cts_to_self(struct ieee80211com *ic, u_int16_t dur)
{
	struct ieee80211_frame_cts *cts;
	mbuf_t m;
	
	mbuf_gethdr(MBUF_DONTWAIT, MT_DATA, &m);
	if (m == NULL)
		return NULL;
	
	mbuf_pkthdr_setlen(m, sizeof(struct ieee80211_frame_cts));
	mbuf_setlen(m, sizeof(struct ieee80211_frame_cts));
	
	cts = mtod(m, struct ieee80211_frame_cts *);
	cts->i_fc[0] = IEEE80211_FC0_VERSION_0 | IEEE80211_FC0_TYPE_CTL |
	IEEE80211_FC0_SUBTYPE_CTS;
	cts->i_fc[1] = IEEE80211_FC1_DIR_NODS;
	*(u_int16_t *)cts->i_dur = htole16(dur);
	IEEE80211_ADDR_COPY(cts->i_ra, ic->ic_myaddr);
	
	return m;
}
