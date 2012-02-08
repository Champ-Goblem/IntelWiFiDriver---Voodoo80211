/*	$OpenBSD: ieee80211_input.c,v 1.119 2011/04/05 11:48:28 blambert Exp $	*/

/*-
 * Copyright (c) 2001 Atsushi Onoe
 * Copyright (c) 2002, 2003 Sam Leffler, Errno Consulting
 * Copyright (c) 2007-2009 Damien Bergamini
 * Copyright (c) 2011 Prashant Vaibhav
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

#include "Voodoo80211Device.h"
#include <libkern/OSMalloc.h>
#include <net/if_llc.h>
#include <sys/systm.h>

/*
 * Retrieve the length in bytes of an 802.11 header.
 */
u_int Voodoo80211Device::
ieee80211_get_hdrlen(const struct ieee80211_frame *wh)
{
	u_int size = sizeof(*wh);
    
	/* NB: does not work with control frames */
	assert(ieee80211_has_seq(wh));
    
	if (ieee80211_has_addr4(wh))
		size += IEEE80211_ADDR_LEN;	/* i_addr4 */
	if (ieee80211_has_qos(wh))
		size += sizeof(u_int16_t);	/* i_qos */
	if (ieee80211_has_htc(wh))
		size += sizeof(u_int32_t);	/* i_ht */
	return size;
}

/*
 * Process a received frame.  The node associated with the sender
 * should be supplied.  If nothing was found in the node table then
 * the caller is assumed to supply a reference to ic_bss instead.
 * The RSSI and a timestamp are also supplied.  The RSSI data is used
 * during AP scanning to select a AP to associate with; it can have
 * any units so long as values have consistent units and higher values
 * mean ``better signal''.  The receive timestamp is currently not used
 * by the 802.11 layer.
 */
void Voodoo80211Device::
ieee80211_input(struct ieee80211com *ic, mbuf_t m, struct ieee80211_node *ni,
                struct ieee80211_rxinfo *rxi)
{
	struct ieee80211_frame *wh;
	u_int16_t *orxseq, nrxseq, qos;
	u_int8_t dir, type, subtype, tid;
	int hdrlen, hasqos;
    
	assert(ni != NULL);
    
	/* in monitor mode, send everything directly to bpf */
	if (ic->ic_opmode == IEEE80211_M_MONITOR)
		goto out;
    
	/*
	 * Do not process frames without an Address 2 field any further.
	 * Only CTS and ACK control frames do not have this field.
	 */
	if (mbuf_len(m) < sizeof(struct ieee80211_frame_min)) {
		DPRINTF(("frame too short, len %u\n", mbuf_len(m)));
		ic->ic_stats.is_rx_tooshort++;
		goto out;
	}
    
	wh = mtod(m, struct ieee80211_frame *);
	if ((wh->i_fc[0] & IEEE80211_FC0_VERSION_MASK) !=
	    IEEE80211_FC0_VERSION_0) {
		DPRINTF(("frame with wrong version: %x\n", wh->i_fc[0]));
		ic->ic_stats.is_rx_badversion++;
		goto err;
	}
    
	dir = wh->i_fc[1] & IEEE80211_FC1_DIR_MASK;
	type = wh->i_fc[0] & IEEE80211_FC0_TYPE_MASK;
    
	if (type != IEEE80211_FC0_TYPE_CTL) {
		hdrlen = ieee80211_get_hdrlen(wh);
		if (mbuf_len(m) < hdrlen) {
			DPRINTF(("frame too short, len %u\n", mbuf_len(m)));
			ic->ic_stats.is_rx_tooshort++;
			goto err;
		}
	}
	if ((hasqos = ieee80211_has_qos(wh))) {
		qos = ieee80211_get_qos(wh);
		tid = qos & IEEE80211_QOS_TID;
	}
    
	/* duplicate detection (see 9.2.9) */
	if (ieee80211_has_seq(wh) &&
	    ic->ic_state != IEEE80211_S_SCAN) {
		nrxseq = letoh16(*(u_int16_t *)wh->i_seq) >>
        IEEE80211_SEQ_SEQ_SHIFT;
		if (hasqos)
			orxseq = &ni->ni_qos_rxseqs[tid];
		else
			orxseq = &ni->ni_rxseq;
		if ((wh->i_fc[1] & IEEE80211_FC1_RETRY) &&
		    nrxseq == *orxseq) {
			/* duplicate, silently discarded */
			ic->ic_stats.is_rx_dup++;
			goto out;
		}
		*orxseq = nrxseq;
	}
	if (ic->ic_state != IEEE80211_S_SCAN) {
		ni->ni_rssi = rxi->rxi_rssi;
		ni->ni_rstamp = rxi->rxi_tstamp;
		ni->ni_inact = 0;
	}
    
	switch (type) {
        case IEEE80211_FC0_TYPE_DATA:
            switch (ic->ic_opmode) {
                case IEEE80211_M_STA:
                    if (dir != IEEE80211_FC1_DIR_FROMDS) {
                        ic->ic_stats.is_rx_wrongdir++;
                        goto out;
                    }
                    if (ic->ic_state != IEEE80211_S_SCAN &&
                        !IEEE80211_ADDR_EQ(wh->i_addr2, ni->ni_bssid)) {
                        /* Source address is not our BSS. */
                        DPRINTF(("discard frame from SA %s\n",
                                 ether_sprintf(wh->i_addr2)));
                        ic->ic_stats.is_rx_wrongbss++;
                        goto out;
                    }
                    if ((fInterface->getFlags() & IFF_SIMPLEX) &&
                        IEEE80211_IS_MULTICAST(wh->i_addr1) &&
                        IEEE80211_ADDR_EQ(wh->i_addr3, ic->ic_myaddr)) {
                        /*
                         * In IEEE802.11 network, multicast frame
                         * sent from me is broadcasted from AP.
                         * It should be silently discarded for
                         * SIMPLEX interface.
                         */
                        ic->ic_stats.is_rx_mcastecho++;
                        goto out;
                    }
                    break;
                default:
                    /* can't get there */
                    goto out;
            }
            
#ifndef IEEE80211_NO_HT
            if (!(rxi->rxi_flags & IEEE80211_RXI_AMPDU_DONE) &&
                hasqos && (qos & IEEE80211_QOS_ACK_POLICY_MASK) ==
                IEEE80211_QOS_ACK_POLICY_BA) {
                /* check if we have a BA agreement for this RA/TID */
                if (ni->ni_rx_ba[tid].ba_state !=
                    IEEE80211_BA_AGREED) {
                    DPRINTF(("no BA agreement for %s, TID %d\n",
                             ether_sprintf(ni->ni_macaddr), tid));
                    /* send a DELBA with reason code UNKNOWN-BA */
                    IEEE80211_SEND_ACTION(ic, ni,
                                          IEEE80211_CATEG_BA, IEEE80211_ACTION_DELBA,
                                          IEEE80211_REASON_SETUP_REQUIRED << 16 |
                                          tid);
                    goto err;
                }
                /* go through A-MPDU reordering */
                ieee80211_input_ba(ic, m, ni, tid, rxi);
                return;	/* don't free m! */
            }
#endif
            if ((ic->ic_flags & IEEE80211_F_WEPON) ||
                ((ic->ic_flags & IEEE80211_F_RSNON) &&
                 (ni->ni_flags & IEEE80211_NODE_RXPROT))) {
                    /* protection is on for Rx */
                    if (!(rxi->rxi_flags & IEEE80211_RXI_HWDEC)) {
                        if (!(wh->i_fc[1] & IEEE80211_FC1_PROTECTED)) {
                            /* drop unencrypted */
                            ic->ic_stats.is_rx_unencrypted++;
                            goto err;
                        }
                        /* do software decryption */
                        m = ieee80211_decrypt(ic, m, ni);
                        if (m == NULL) {
                            ic->ic_stats.is_rx_wepfail++;
                            goto err;
                        }
                        wh = mtod(m, struct ieee80211_frame *);
                    }
                } else if ((wh->i_fc[1] & IEEE80211_FC1_PROTECTED) ||
                           (rxi->rxi_flags & IEEE80211_RXI_HWDEC)) {
                    /* frame encrypted but protection off for Rx */
                    ic->ic_stats.is_rx_nowep++;
                    goto out;
                }
            
            
#ifndef IEEE80211_NO_HT
            if ((ni->ni_flags & IEEE80211_NODE_HT) &&
                hasqos && (qos & IEEE80211_QOS_AMSDU))
                ieee80211_amsdu_decap(ic, m, ni, hdrlen);
            else
#endif
                ieee80211_decap(ic, m, ni, hdrlen);
            return;
            
        case IEEE80211_FC0_TYPE_MGT:
            if (dir != IEEE80211_FC1_DIR_NODS) {
                ic->ic_stats.is_rx_wrongdir++;
                goto err;
            }
            
            subtype = wh->i_fc[0] & IEEE80211_FC0_SUBTYPE_MASK;
            
            /* drop frames without interest */
            if (ic->ic_state == IEEE80211_S_SCAN) {
                if (subtype != IEEE80211_FC0_SUBTYPE_BEACON &&
                    subtype != IEEE80211_FC0_SUBTYPE_PROBE_RESP) {
                    ic->ic_stats.is_rx_mgtdiscard++;
                    goto out;
                }
            }
            
            if (ni->ni_flags & IEEE80211_NODE_RXMGMTPROT) {
                /* MMPDU protection is on for Rx */
                if (subtype == IEEE80211_FC0_SUBTYPE_DISASSOC ||
                    subtype == IEEE80211_FC0_SUBTYPE_DEAUTH ||
                    subtype == IEEE80211_FC0_SUBTYPE_ACTION) {
                    if (!IEEE80211_IS_MULTICAST(wh->i_addr1) &&
                        !(wh->i_fc[1] & IEEE80211_FC1_PROTECTED)) {
                        /* unicast mgmt not encrypted */
                        goto out;
                    }
                    /* do software decryption */
                    m = ieee80211_decrypt(ic, m, ni);
                    if (m == NULL) {
                        /* XXX stats */
                        goto out;
                    }
                    wh = mtod(m, struct ieee80211_frame *);
                }
            } else if ((ic->ic_flags & IEEE80211_F_RSNON) &&
                       (wh->i_fc[1] & IEEE80211_FC1_PROTECTED)) {
                /* encrypted but MMPDU Rx protection off for TA */
                goto out;
            }
            
            if (fInterface->getFlags() & IFF_DEBUG) {
                /* avoid to print too many frames */
                int doprint = 0;
                
                switch (subtype) {
                    case IEEE80211_FC0_SUBTYPE_BEACON:
                        if (ic->ic_state == IEEE80211_S_SCAN)
                            doprint = 1;
                        break;
                    default:
                        doprint = 1;
                        break;
                }
#ifdef IEEE80211_DEBUG
                doprint += ieee80211_debug;
#endif
		    /* TODO debug
                if (doprint)
                    printf("%s: received %s from %s rssi %d mode %s\n",
                           "voodoo_wifi",
                           ieee80211_mgt_subtype_name[subtype
                                                      >> IEEE80211_FC0_SUBTYPE_SHIFT],
                           ether_sprintf(wh->i_addr2), rxi->rxi_rssi,
                           ieee80211_phymode_name[ieee80211_chan2mode(ic,
                                                                      ic->ic_bss->ni_chan)]);
		     */
            }
            ieee80211_recv_mgmt(ic, m, ni, rxi, subtype);
            mbuf_freem(m);
            return;
            
        case IEEE80211_FC0_TYPE_CTL:
            ic->ic_stats.is_rx_ctl++;
            subtype = wh->i_fc[0] & IEEE80211_FC0_SUBTYPE_MASK;
            switch (subtype) {
#ifndef IEEE80211_NO_HT
                case IEEE80211_FC0_SUBTYPE_BAR:
                    ieee80211_recv_bar(ic, m, ni);
                    break;
#endif
                default:
                    break;
            }
            goto out;
            
        default:
            DPRINTF(("bad frame type %x\n", type));
            /* should not come here */
            break;
	}
err:
	// TODO ifp->if_ierrors++;
out:
	if (m != NULL) {
		mbuf_freem(m);
	}
}

/*
 * Handle defragmentation (see 9.5 and Annex C).  We support the concurrent
 * reception of fragments of three fragmented MSDUs or MMPDUs.
 */
mbuf_t Voodoo80211Device::
ieee80211_defrag(struct ieee80211com *ic, mbuf_t m, int hdrlen)
{
	const struct ieee80211_frame *owh, *wh;
	struct ieee80211_defrag *df;
	u_int16_t rxseq, seq;
	u_int8_t frag;
	int i;
    
	wh = mtod(m, struct ieee80211_frame *);
	rxseq = letoh16(*(const u_int16_t *)wh->i_seq);
	seq = rxseq >> IEEE80211_SEQ_SEQ_SHIFT;
	frag = rxseq & IEEE80211_SEQ_FRAG_MASK;
    
	if (frag == 0 && !(wh->i_fc[1] & IEEE80211_FC1_MORE_FRAG))
		return m;	/* not fragmented */
    
	if (frag == 0) {
		/* first fragment, setup entry in the fragment cache */
		if (++ic->ic_defrag_cur == IEEE80211_DEFRAG_SIZE)
			ic->ic_defrag_cur = 0;
		df = &ic->ic_defrag[ic->ic_defrag_cur];
		if (df->df_m != NULL)
			mbuf_freem(df->df_m);	/* discard old entry */
		df->df_seq = seq;
		df->df_frag = 0;
		df->df_m = m;
		/* start receive MSDU timer of aMaxReceiveLifetime */
		timeout_add_sec(df->df_to, 1);
		return NULL;	/* MSDU or MMPDU not yet complete */
	}
    
	/* find matching entry in the fragment cache */
	for (i = 0; i < IEEE80211_DEFRAG_SIZE; i++) {
		df = &ic->ic_defrag[i];
		if (df->df_m == NULL)
			continue;
		if (df->df_seq != seq || df->df_frag + 1 != frag)
			continue;
		owh = mtod(df->df_m, struct ieee80211_frame *);
		/* frame type, source and destination must match */
		if (((wh->i_fc[0] ^ owh->i_fc[0]) & IEEE80211_FC0_TYPE_MASK) ||
		    !IEEE80211_ADDR_EQ(wh->i_addr1, owh->i_addr1) ||
		    !IEEE80211_ADDR_EQ(wh->i_addr2, owh->i_addr2))
			continue;
		/* matching entry found */
		break;
	}
	if (i == IEEE80211_DEFRAG_SIZE) {
		/* no matching entry found, discard fragment */
		// TODO ic->ic_if.if_ierrors++;
		mbuf_freem(m);
		return NULL;
	}
    
	df->df_frag = frag;
	/* strip 802.11 header and concatenate fragment */
	mbuf_adj(m, hdrlen);
	mbuf_concatenate(df->df_m, m);
	mbuf_pkthdr_setlen(df->df_m, mbuf_pkthdr_len(df->df_m) + mbuf_pkthdr_len(m));
    
	if (wh->i_fc[1] & IEEE80211_FC1_MORE_FRAG)
		return NULL;	/* MSDU or MMPDU not yet complete */
    
	/* MSDU or MMPDU complete */
	timeout_del(df->df_to);
	m = df->df_m;
	df->df_m = NULL;
	return m;
}

/*
 * Receive MSDU defragmentation timer exceeds aMaxReceiveLifetime.
 */
void Voodoo80211Device::
ieee80211_defrag_timeout(void *arg)
{
	struct ieee80211_defrag *df = (struct ieee80211_defrag *)arg;
	int s = splnet();
    
	/* discard all received fragments */
	mbuf_freem(df->df_m);
	df->df_m = NULL;
    
	splx(s);
}

#ifndef IEEE80211_NO_HT
/*
 * Process a received data MPDU related to a specific HT-immediate Block Ack
 * agreement (see 9.10.7.6).
 */
void Voodoo80211Device::
ieee80211_input_ba(struct ieee80211com *ifp, mbuf_t m,
                   struct ieee80211_node *ni, int tid, struct ieee80211_rxinfo *rxi)
{
	struct ieee80211_rx_ba *ba = &ni->ni_rx_ba[tid];
	struct ieee80211_frame *wh;
	int idx, count;
	u_int16_t sn;
    
	wh = mtod(m, struct ieee80211_frame *);
	sn = letoh16(*(u_int16_t *)wh->i_seq) >> IEEE80211_SEQ_SEQ_SHIFT;
    
	/* reset Block Ack inactivity timer */
	timeout_add_usec(ba->ba_to, ba->ba_timeout_val);
    
	if (SEQ_LT(sn, ba->ba_winstart)) {	/* SN < WinStartB */
		// TODO ifp->if_ierrors++;
		mbuf_freem(m);	/* discard the MPDU */
		return;
	}
	if (SEQ_LT(ba->ba_winend, sn)) {	/* WinEndB < SN */
		count = (sn - ba->ba_winend) & 0xfff;
		if (count > ba->ba_winsize)	/* no overlap */
			count = ba->ba_winsize;
		while (count-- > 0) {
			/* gaps may exist */
			if (ba->ba_buf[ba->ba_head].m != NULL) {
				ieee80211_input(ifp, ba->ba_buf[ba->ba_head].m,
                                ni, &ba->ba_buf[ba->ba_head].rxi);
				ba->ba_buf[ba->ba_head].m = NULL;
			}
			ba->ba_head = (ba->ba_head + 1) %
            IEEE80211_BA_MAX_WINSZ;
		}
		/* move window forward */
		ba->ba_winend = sn;
		ba->ba_winstart = (sn - ba->ba_winsize + 1) & 0xfff;
	}
	/* WinStartB <= SN <= WinEndB */
    
	idx = (sn - ba->ba_winstart) & 0xfff;
	idx = (ba->ba_head + idx) % IEEE80211_BA_MAX_WINSZ;
	/* store the received MPDU in the buffer */
	if (ba->ba_buf[idx].m != NULL) {
		// TODO ifp->if_ierrors++;
		mbuf_freem(m);
		return;
	}
	ba->ba_buf[idx].m = m;
	/* store Rx meta-data too */
	rxi->rxi_flags |= IEEE80211_RXI_AMPDU_DONE;
	ba->ba_buf[idx].rxi = *rxi;
    
	/* pass reordered MPDUs up to the next MAC process */
	while (ba->ba_buf[ba->ba_head].m != NULL) {
		ieee80211_input(ifp, ba->ba_buf[ba->ba_head].m, ni,
                        &ba->ba_buf[ba->ba_head].rxi);
		ba->ba_buf[ba->ba_head].m = NULL;
        
		ba->ba_head = (ba->ba_head + 1) % IEEE80211_BA_MAX_WINSZ;
		/* move window forward */
		ba->ba_winstart = (ba->ba_winstart + 1) & 0xfff;
	}
	ba->ba_winend = (ba->ba_winstart + ba->ba_winsize - 1) & 0xfff;
}

/*
 * Change the value of WinStartB (move window forward) upon reception of a
 * BlockAckReq frame or an ADDBA Request (PBAC).
 */
void Voodoo80211Device::
ieee80211_ba_move_window(struct ieee80211com *ic, struct ieee80211_node *ni,
                         u_int8_t tid, u_int16_t ssn)
{
	struct ieee80211_rx_ba *ba = &ni->ni_rx_ba[tid];
	int count;
    
	/* assert(WinStartB <= SSN) */
    
	count = (ssn - ba->ba_winstart) & 0xfff;
	if (count > ba->ba_winsize)	/* no overlap */
		count = ba->ba_winsize;
	while (count-- > 0) {
		/* gaps may exist */
		if (ba->ba_buf[ba->ba_head].m != NULL) {
			ieee80211_input(ic, ba->ba_buf[ba->ba_head].m, ni,
                            &ba->ba_buf[ba->ba_head].rxi);
			ba->ba_buf[ba->ba_head].m = NULL;
		}
		ba->ba_head = (ba->ba_head + 1) % IEEE80211_BA_MAX_WINSZ;
	}
	/* move window forward */
	ba->ba_winstart = ssn;
    
	/* pass reordered MPDUs up to the next MAC process */
	while (ba->ba_buf[ba->ba_head].m != NULL) {
		ieee80211_input(ic, ba->ba_buf[ba->ba_head].m, ni,
                        &ba->ba_buf[ba->ba_head].rxi);
		ba->ba_buf[ba->ba_head].m = NULL;
        
		ba->ba_head = (ba->ba_head + 1) % IEEE80211_BA_MAX_WINSZ;
		/* move window forward */
		ba->ba_winstart = (ba->ba_winstart + 1) & 0xfff;
	}
	ba->ba_winend = (ba->ba_winstart + ba->ba_winsize - 1) & 0xfff;
}
#endif	/* !IEEE80211_NO_HT */

void Voodoo80211Device::
ieee80211_deliver_data(struct ieee80211com *ic, mbuf_t m,
                       struct ieee80211_node *ni)
{
	struct ether_header *eh;
	mbuf_t m1;
    
	eh = mtod(m, struct ether_header *);
    
	if ((ic->ic_flags & IEEE80211_F_RSNON) && !ni->ni_port_valid &&
	    eh->ether_type != htons(ETHERTYPE_PAE)) {
		DPRINTF(("port not valid: %s\n", ether_sprintf(eh->ether_dhost)));
		ic->ic_stats.is_rx_unauth++;
		mbuf_freem(m);
		return;
	}
	// TODO ifp->if_ipackets++;
    
	/*
	 * Perform as a bridge within the AP.  Notice that we do not
	 * bridge EAPOL frames as suggested in C.1.1 of IEEE Std 802.1X.
	 */
	m1 = NULL;
	if ((ic->ic_flags & IEEE80211_F_RSNON) &&
	    eh->ether_type == htons(ETHERTYPE_PAE))
		ieee80211_eapol_key_input(ic, m, ni);
	else
		fInterface->inputPacket(m);
}

#ifdef __STRICT_ALIGNMENT
/*
 * Make sure protocol header (e.g. IP) is aligned on a 32-bit boundary.
 * This is achieved by copying mbufs so drivers should try to map their
 * buffers such that this copying is not necessary.  It is however not
 * always possible because 802.11 header length may vary (non-QoS+LLC
 * is 32 bytes while QoS+LLC is 34 bytes).  Some devices are smart and
 * add 2 padding bytes after the 802.11 header in the QoS case so this
 * function is there for stupid drivers/devices only.
 *
 * XXX -- this is horrible
 * FIXME -- pvaibhav: need to fix this whole function at some point (mbufs)
 */
mbuf_t Voodoo80211Device::
ieee80211_align_mbuf(mbuf_t m)
{
	mbuf_t n, *n0, **np;
	caddr_t newdata;
	int off, pktlen;
    
	n0 = NULL;
	np = &n0;
	off = 0;
	pktlen = mbuf_pkthdr_len(m);
	while (pktlen > off) {
		if (n0 == NULL) {
            mbuf_gethdr(MBUF_DONTWAIT, MBUF_TYPE_DATA, n);
			if (n == NULL) {
				mbuf_freem(m);
				return NULL;
			}
			if (m_dup_pkthdr(n, m, M_DONTWAIT)) {
				mbuf_free(n);
				mbuf_freem(m);
				return (NULL);
			}
			mbuf_len(n) = MHLEN;
		} else {
			MGET(n, M_DONTWAIT, MT_DATA);
			if (n == NULL) {
				mbuf_freem(m);
				mbuf_freem(n0);
				return NULL;
			}
			mbuf_len(n) = MLEN;
		}
		if (pktlen - off >= MINCLSIZE) {
			MCLGET(n, M_DONTWAIT);
			if (mbuf_flags(n) & M_EXT)
				mbuf_setlen(n, m_ext.ext_size);
		}
		if (n0 == NULL) {
			newdata = (caddr_t)ALIGN(n->m_data + ETHER_HDR_LEN) -
            ETHER_HDR_LEN;
			mbuf_len(n) -= newdata - n->m_data;
			n->m_data = newdata;
		}
		if (mbuf_len(n) > pktlen - off)
			mbuf_setlen(n, pktlen - off);
		mbuf_copydata(m, off, mbuf_len(n), mtod(n, caddr_t));
		off += mbuf_len(n);
		*np = n;
		np = &n->m_next;
	}
	mbuf_freem(m);
	return n0;
}
#endif	/* __STRICT_ALIGNMENT */

void Voodoo80211Device::
ieee80211_decap(struct ieee80211com *ic, mbuf_t m,
                struct ieee80211_node *ni, int hdrlen)
{
	struct ether_header eh;
	struct ieee80211_frame *wh;
	struct llc *llc;
    
	if (mbuf_len(m) < hdrlen + LLC_SNAPFRAMELEN &&
	    mbuf_pullup(&m, hdrlen + LLC_SNAPFRAMELEN) == NULL) {
		ic->ic_stats.is_rx_decap++;
		return;
	}
	wh = mtod(m, struct ieee80211_frame *);
	switch (wh->i_fc[1] & IEEE80211_FC1_DIR_MASK) {
        case IEEE80211_FC1_DIR_NODS:
            IEEE80211_ADDR_COPY(eh.ether_dhost, wh->i_addr1);
            IEEE80211_ADDR_COPY(eh.ether_shost, wh->i_addr2);
            break;
        case IEEE80211_FC1_DIR_TODS:
            IEEE80211_ADDR_COPY(eh.ether_dhost, wh->i_addr3);
            IEEE80211_ADDR_COPY(eh.ether_shost, wh->i_addr2);
            break;
        case IEEE80211_FC1_DIR_FROMDS:
            IEEE80211_ADDR_COPY(eh.ether_dhost, wh->i_addr1);
            IEEE80211_ADDR_COPY(eh.ether_shost, wh->i_addr3);
            break;
        case IEEE80211_FC1_DIR_DSTODS:
            IEEE80211_ADDR_COPY(eh.ether_dhost, wh->i_addr3);
            IEEE80211_ADDR_COPY(eh.ether_shost,
                                ((struct ieee80211_frame_addr4 *)wh)->i_addr4);
            break;
	}
	llc = (struct llc *)((caddr_t)wh + hdrlen);
	if (llc->llc_dsap == LLC_SNAP_LSAP &&
	    llc->llc_ssap == LLC_SNAP_LSAP &&
	    llc->llc_control == LLC_UI &&
	    llc->llc_snap.org_code[0] == 0 &&
	    llc->llc_snap.org_code[1] == 0 &&
	    llc->llc_snap.org_code[2] == 0) {
		eh.ether_type = llc->llc_snap.ether_type;
		mbuf_adj(m, hdrlen + LLC_SNAPFRAMELEN - ETHER_HDR_LEN);
	} else {
		eh.ether_type = htons(mbuf_pkthdr_len(m) - hdrlen);
		mbuf_adj(m, hdrlen - ETHER_HDR_LEN);
	}
	memcpy(mtod(m, caddr_t), &eh, ETHER_HDR_LEN);
#ifdef __STRICT_ALIGNMENT
	if (!ALIGNED_POINTER(mtod(m, caddr_t) + ETHER_HDR_LEN, u_int32_t)) {
		if ((m = ieee80211_align_mbuf(m)) == NULL) {
			ic->ic_stats.is_rx_decap++;
			return;
		}
	}
#endif
	ieee80211_deliver_data(ic, m, ni);
}

#ifndef IEEE80211_NO_HT
/*
 * Decapsulate an Aggregate MSDU (see 7.2.2.2).
 */
void Voodoo80211Device::
ieee80211_amsdu_decap(struct ieee80211com *ic, mbuf_t m,
                      struct ieee80211_node *ni, int hdrlen)
{
	mbuf_t n;
	struct ether_header *eh;
	struct llc *llc;
	int len, pad;
    
	/* strip 802.11 header */
	mbuf_adj(m, hdrlen);
    
	for (;;) {
		/* process an A-MSDU subframe */
		if (mbuf_len(m) < ETHER_HDR_LEN + LLC_SNAPFRAMELEN) {
			mbuf_pullup(&m, ETHER_HDR_LEN + LLC_SNAPFRAMELEN);
			if (m == NULL) {
				ic->ic_stats.is_rx_decap++;
				break;
			}
		}
		eh = mtod(m, struct ether_header *);
		/* examine 802.3 header */
		len = ntohs(eh->ether_type);
		if (len < LLC_SNAPFRAMELEN) {
			DPRINTF(("A-MSDU subframe too short (%d)\n", len));
			/* stop processing A-MSDU subframes */
			ic->ic_stats.is_rx_decap++;
			mbuf_freem(m);
			break;
		}
		llc = (struct llc *)&eh[1];
		/* examine 802.2 LLC header */
		if (llc->llc_dsap == LLC_SNAP_LSAP &&
		    llc->llc_ssap == LLC_SNAP_LSAP &&
		    llc->llc_control == LLC_UI &&
		    llc->llc_snap.org_code[0] == 0 &&
		    llc->llc_snap.org_code[1] == 0 &&
		    llc->llc_snap.org_code[2] == 0) {
			/* convert to Ethernet II header */
			eh->ether_type = llc->llc_snap.ether_type;
			/* strip LLC+SNAP headers */
			bcopy(eh, (u_int8_t *)eh + LLC_SNAPFRAMELEN, ETHER_HDR_LEN);
			mbuf_adj(m, LLC_SNAPFRAMELEN);
			len -= LLC_SNAPFRAMELEN;
		}
		len += ETHER_HDR_LEN;
        
		/* "detach" our A-MSDU subframe from the others */
		mbuf_split(m, len, MBUF_DONTWAIT, &n);
		if (n == NULL) {
			/* stop processing A-MSDU subframes */
			ic->ic_stats.is_rx_decap++;
			mbuf_freem(m);
			break;
		}
		ieee80211_deliver_data(ic, m, ni);
        
		m = n;
		/* remove padding */
		pad = ((len + 3) & ~3) - len;
		mbuf_adj(m, pad);
	}
}
#endif	/* !IEEE80211_NO_HT */

/*
 * Parse an EDCA Parameter Set element (see 7.3.2.27).
 */
int Voodoo80211Device::
ieee80211_parse_edca_params_body(struct ieee80211com *ic, const u_int8_t *frm)
{
	u_int updtcount;
	int aci;
    
	/*
	 * Check if EDCA parameters have changed XXX if we miss more than
	 * 15 consecutive beacons, we might not detect changes to EDCA
	 * parameters due to wraparound of the 4-bit Update Count field.
	 */
	updtcount = frm[0] & 0xf;
	if (updtcount == ic->ic_edca_updtcount)
		return 0;	/* no changes to EDCA parameters, ignore */
	ic->ic_edca_updtcount = updtcount;
    
	frm += 2;	/* skip QoS Info & Reserved fields */
    
	/* parse AC Parameter Records */
	for (aci = 0; aci < EDCA_NUM_AC; aci++) {
		struct ieee80211_edca_ac_params *ac = &ic->ic_edca_ac[aci];
        
		ac->ac_acm       = (frm[0] >> 4) & 0x1;
		ac->ac_aifsn     = frm[0] & 0xf;
		ac->ac_ecwmin    = frm[1] & 0xf;
		ac->ac_ecwmax    = frm[1] >> 4;
		ac->ac_txoplimit = LE_READ_2(frm + 2);
		frm += 4;
	}
	/* give drivers a chance to update their settings */
	if ((ic->ic_flags & IEEE80211_F_QOS))
		ieee80211_updateedca(ic);
    
	return 0;
}

int Voodoo80211Device::
ieee80211_parse_edca_params(struct ieee80211com *ic, const u_int8_t *frm)
{
	if (frm[1] < 18) {
		ic->ic_stats.is_rx_elem_toosmall++;
		return IEEE80211_REASON_IE_INVALID;
	}
	return ieee80211_parse_edca_params_body(ic, frm + 2);
}

int Voodoo80211Device::
ieee80211_parse_wmm_params(struct ieee80211com *ic, const u_int8_t *frm)
{
	if (frm[1] < 24) {
		ic->ic_stats.is_rx_elem_toosmall++;
		return IEEE80211_REASON_IE_INVALID;
	}
	return ieee80211_parse_edca_params_body(ic, frm + 8);
}

enum ieee80211_cipher Voodoo80211Device::
ieee80211_parse_rsn_cipher(const u_int8_t selector[4])
{
	if (memcmp(selector, MICROSOFT_OUI, 3) == 0) {	/* WPA */
		switch (selector[3]) {
            case 0:	/* use group data cipher suite */
                return IEEE80211_CIPHER_USEGROUP;
            case 1:	/* WEP-40 */
                return IEEE80211_CIPHER_WEP40;
            case 2:	/* TKIP */
                return IEEE80211_CIPHER_TKIP;
            case 4:	/* CCMP (RSNA default) */
                return IEEE80211_CIPHER_CCMP;
            case 5:	/* WEP-104 */
                return IEEE80211_CIPHER_WEP104;
		}
	} else if (memcmp(selector, IEEE80211_OUI, 3) == 0) {	/* RSN */
		/* from IEEE Std 802.11 - Table 20da */
		switch (selector[3]) {
            case 0:	/* use group data cipher suite */
                return IEEE80211_CIPHER_USEGROUP;
            case 1:	/* WEP-40 */
                return IEEE80211_CIPHER_WEP40;
            case 2:	/* TKIP */
                return IEEE80211_CIPHER_TKIP;
            case 4:	/* CCMP (RSNA default) */
                return IEEE80211_CIPHER_CCMP;
            case 5:	/* WEP-104 */
                return IEEE80211_CIPHER_WEP104;
            case 6:	/* BIP */
                return IEEE80211_CIPHER_BIP;
		}
	}
	return IEEE80211_CIPHER_NONE;	/* ignore unknown ciphers */
}

enum ieee80211_akm Voodoo80211Device::
ieee80211_parse_rsn_akm(const u_int8_t selector[4])
{
	if (memcmp(selector, MICROSOFT_OUI, 3) == 0) {	/* WPA */
		switch (selector[3]) {
            case 1:	/* IEEE 802.1X (RSNA default) */
                return IEEE80211_AKM_8021X;
            case 2:	/* PSK */
                return IEEE80211_AKM_PSK;
		}
	} else if (memcmp(selector, IEEE80211_OUI, 3) == 0) {	/* RSN */
		/* from IEEE Std 802.11i-2004 - Table 20dc */
		switch (selector[3]) {
            case 1:	/* IEEE 802.1X (RSNA default) */
                return IEEE80211_AKM_8021X;
            case 2:	/* PSK */
                return IEEE80211_AKM_PSK;
            case 5:	/* IEEE 802.1X with SHA256 KDF */
                return IEEE80211_AKM_SHA256_8021X;
            case 6:	/* PSK with SHA256 KDF */
                return IEEE80211_AKM_SHA256_PSK;
		}
	}
	return IEEE80211_AKM_NONE;	/* ignore unknown AKMs */
}

/*
 * Parse an RSN element (see 7.3.2.25).
 */
int Voodoo80211Device::
ieee80211_parse_rsn_body(struct ieee80211com *ic, const u_int8_t *frm,
                         u_int len, struct ieee80211_rsnparams *rsn)
{
	const u_int8_t *efrm;
	u_int16_t m, n, s;
    
	efrm = frm + len;
    
	/* check Version field */
	if (LE_READ_2(frm) != 1)
		return IEEE80211_STATUS_RSN_IE_VER_UNSUP;
	frm += 2;
    
	/* all fields after the Version field are optional */
    
	/* if Cipher Suite missing, default to CCMP */
	rsn->rsn_groupcipher = IEEE80211_CIPHER_CCMP;
	rsn->rsn_nciphers = 1;
	rsn->rsn_ciphers = IEEE80211_CIPHER_CCMP;
	/* if Group Management Cipher Suite missing, defaut to BIP */
	rsn->rsn_groupmgmtcipher = IEEE80211_CIPHER_BIP;
	/* if AKM Suite missing, default to 802.1X */
	rsn->rsn_nakms = 1;
	rsn->rsn_akms = IEEE80211_AKM_8021X;
	/* if RSN capabilities missing, default to 0 */
	rsn->rsn_caps = 0;
	rsn->rsn_npmkids = 0;
    
	/* read Group Data Cipher Suite field */
	if (frm + 4 > efrm)
		return 0;
	rsn->rsn_groupcipher = ieee80211_parse_rsn_cipher(frm);
	if (rsn->rsn_groupcipher == IEEE80211_CIPHER_USEGROUP)
		return IEEE80211_STATUS_BAD_GROUP_CIPHER;
	frm += 4;
    
	/* read Pairwise Cipher Suite Count field */
	if (frm + 2 > efrm)
		return 0;
	m = rsn->rsn_nciphers = LE_READ_2(frm);
	frm += 2;
    
	/* read Pairwise Cipher Suite List */
	if (frm + m * 4 > efrm)
		return IEEE80211_STATUS_IE_INVALID;
	rsn->rsn_ciphers = IEEE80211_CIPHER_NONE;
	while (m-- > 0) {
		rsn->rsn_ciphers |= ieee80211_parse_rsn_cipher(frm);
		frm += 4;
	}
	if (rsn->rsn_ciphers & IEEE80211_CIPHER_USEGROUP) {
		if (rsn->rsn_ciphers != IEEE80211_CIPHER_USEGROUP)
			return IEEE80211_STATUS_BAD_PAIRWISE_CIPHER;
		if (rsn->rsn_groupcipher == IEEE80211_CIPHER_CCMP)
			return IEEE80211_STATUS_BAD_PAIRWISE_CIPHER;
	}
    
	/* read AKM Suite List Count field */
	if (frm + 2 > efrm)
		return 0;
	n = rsn->rsn_nakms = LE_READ_2(frm);
	frm += 2;
    
	/* read AKM Suite List */
	if (frm + n * 4 > efrm)
		return IEEE80211_STATUS_IE_INVALID;
	rsn->rsn_akms = IEEE80211_AKM_NONE;
	while (n-- > 0) {
		rsn->rsn_akms |= ieee80211_parse_rsn_akm(frm);
		frm += 4;
	}
    
	/* read RSN Capabilities field */
	if (frm + 2 > efrm)
		return 0;
	rsn->rsn_caps = LE_READ_2(frm);
	frm += 2;
    
	/* read PMKID Count field */
	if (frm + 2 > efrm)
		return 0;
	s = rsn->rsn_npmkids = LE_READ_2(frm);
	frm += 2;
    
	/* read PMKID List */
	if (frm + s * IEEE80211_PMKID_LEN > efrm)
		return IEEE80211_STATUS_IE_INVALID;
	if (s != 0) {
		rsn->rsn_pmkids = frm;
		frm += s * IEEE80211_PMKID_LEN;
	}
    
	/* read Group Management Cipher Suite field */
	if (frm + 4 > efrm)
		return 0;
	rsn->rsn_groupmgmtcipher = ieee80211_parse_rsn_cipher(frm);
    
	return IEEE80211_STATUS_SUCCESS;
}

int Voodoo80211Device::
ieee80211_parse_rsn(struct ieee80211com *ic, const u_int8_t *frm,
                    struct ieee80211_rsnparams *rsn)
{
	if (frm[1] < 2) {
		ic->ic_stats.is_rx_elem_toosmall++;
		return IEEE80211_STATUS_IE_INVALID;
	}
	return ieee80211_parse_rsn_body(ic, frm + 2, frm[1], rsn);
}

int Voodoo80211Device::
ieee80211_parse_wpa(struct ieee80211com *ic, const u_int8_t *frm,
                    struct ieee80211_rsnparams *rsn)
{
	if (frm[1] < 6) {
		ic->ic_stats.is_rx_elem_toosmall++;
		return IEEE80211_STATUS_IE_INVALID;
	}
	return ieee80211_parse_rsn_body(ic, frm + 6, frm[1] - 4, rsn);
}

/*
 * Create (or update) a copy of an information element.
 */
int Voodoo80211Device::
ieee80211_save_ie(const u_int8_t *frm, u_int8_t **ie)
{
	if (*ie == NULL || (*ie)[1] != frm[1]) {
		if (*ie != NULL)
			free(*ie);
		*ie = (u_int8_t*)malloc(2 + frm[1], M_DEVBUF, M_NOWAIT);
		if (*ie == NULL)
			return ENOMEM;
	}
	memcpy(*ie, frm, 2 + frm[1]);
	return 0;
}

/*-
 * Beacon/Probe response frame format:
 * [8]   Timestamp
 * [2]   Beacon interval
 * [2]   Capability
 * [tlv] Service Set Identifier (SSID)
 * [tlv] Supported rates
 * [tlv] DS Parameter Set (802.11g)
 * [tlv] ERP Information (802.11g)
 * [tlv] Extended Supported Rates (802.11g)
 * [tlv] RSN (802.11i)
 * [tlv] EDCA Parameter Set (802.11e)
 * [tlv] QoS Capability (Beacon only, 802.11e)
 * [tlv] HT Capabilities (802.11n)
 * [tlv] HT Operation (802.11n)
 */
void Voodoo80211Device::
ieee80211_recv_probe_resp(struct ieee80211com *ic, mbuf_t m,
                          struct ieee80211_node *ni, struct ieee80211_rxinfo *rxi, int isprobe)
{
	const struct ieee80211_frame *wh;
	const u_int8_t *frm, *efrm;
	const u_int8_t *tstamp, *ssid, *rates, *xrates, *edcaie, *wmmie;
	const u_int8_t *rsnie, *wpaie, *htcaps, *htop;
	u_int16_t capinfo, bintval;
	u_int8_t chan, bchan, erp;
	int is_new;
    
	/*
	 * We process beacon/probe response frames for:
	 *    o station mode: to collect state
	 *      updates such as 802.11g slot time and for passive
	 *      scanning of APs
	 *    o adhoc mode: to discover neighbors
	 *    o hostap mode: for passive scanning of neighbor APs
	 *    o when scanning
	 * In other words, in all modes other than monitor (which
	 * does not process incoming frames) and adhoc-demo (which
	 * does not use management frames at all).
	 */
#ifdef DIAGNOSTIC
	if (ic->ic_opmode != IEEE80211_M_STA &&
	    ic->ic_state != IEEE80211_S_SCAN) {
		panic("%s: impossible operating mode", __func__);
	}
#endif
	/* make sure all mandatory fixed fields are present */
	if (mbuf_len(m) < sizeof(*wh) + 12) {
		DPRINTF(("frame too short\n"));
		return;
	}
	wh = mtod(m, struct ieee80211_frame *);
	frm = (const u_int8_t *)&wh[1];
	efrm = mtod(m, u_int8_t *) + mbuf_len(m);
    
	tstamp  = frm; frm += 8;
	bintval = LE_READ_2(frm); frm += 2;
	capinfo = LE_READ_2(frm); frm += 2;
    
	ssid = rates = xrates = edcaie = wmmie = rsnie = wpaie = NULL;
	htcaps = htop = NULL;
	bchan = ieee80211_chan2ieee(ic, ic->ic_bss->ni_chan);
	chan = bchan;
	erp = 0;
	while (frm + 2 <= efrm) {
		if (frm + 2 + frm[1] > efrm) {
			ic->ic_stats.is_rx_elem_toosmall++;
			break;
		}
		switch (frm[0]) {
            case IEEE80211_ELEMID_SSID:
                ssid = frm;
                break;
            case IEEE80211_ELEMID_RATES:
                rates = frm;
                break;
            case IEEE80211_ELEMID_DSPARMS:
                if (frm[1] < 1) {
                    ic->ic_stats.is_rx_elem_toosmall++;
                    break;
                }
                chan = frm[2];
                break;
            case IEEE80211_ELEMID_XRATES:
                xrates = frm;
                break;
            case IEEE80211_ELEMID_ERP:
                if (frm[1] < 1) {
                    ic->ic_stats.is_rx_elem_toosmall++;
                    break;
                }
                erp = frm[2];
                break;
            case IEEE80211_ELEMID_RSN:
                rsnie = frm;
                break;
            case IEEE80211_ELEMID_EDCAPARMS:
                edcaie = frm;
                break;
#ifndef IEEE80211_NO_HT
            case IEEE80211_ELEMID_HTCAPS:
                htcaps = frm;
                break;
            case IEEE80211_ELEMID_HTOP:
                htop = frm;
                break;
#endif
            case IEEE80211_ELEMID_VENDOR:
                if (frm[1] < 4) {
                    ic->ic_stats.is_rx_elem_toosmall++;
                    break;
                }
                if (memcmp(frm + 2, MICROSOFT_OUI, 3) == 0) {
                    if (frm[5] == 1)
                        wpaie = frm;
                    else if (frm[1] >= 5 &&
                             frm[5] == 2 && frm[6] == 1)
                        wmmie = frm;
                }
                break;
		}
		frm += 2 + frm[1];
	}
	/* supported rates element is mandatory */
	if (rates == NULL || rates[1] > IEEE80211_RATE_MAXSIZE) {
		DPRINTF(("invalid supported rates element\n"));
		return;
	}
	/* SSID element is mandatory */
	if (ssid == NULL || ssid[1] > IEEE80211_NWID_LEN) {
		DPRINTF(("invalid SSID element\n"));
		return;
	}
    
	if (
#if IEEE80211_CHAN_MAX < 255
	    chan > IEEE80211_CHAN_MAX ||
#endif
	    isclr(ic->ic_chan_active, chan)) {
		DPRINTF(("ignore %s with invalid channel %u\n",
                 isprobe ? "probe response" : "beacon", chan));
		ic->ic_stats.is_rx_badchan++;
		return;
	}
	if ((ic->ic_state != IEEE80211_S_SCAN ||
	     !(ic->ic_caps & IEEE80211_C_SCANALL)) &&
	    chan != bchan) {
		/*
		 * Frame was received on a channel different from the
		 * one indicated in the DS params element id;
		 * silently discard it.
		 *
		 * NB: this can happen due to signal leakage.
		 */
		DPRINTF(("ignore %s on channel %u marked for channel %u\n",
                 isprobe ? "probe response" : "beacon", bchan, chan));
		ic->ic_stats.is_rx_chanmismatch++;
		return;
	}
	/*
	 * Use mac, channel and rssi so we collect only the
	 * best potential AP with the equal bssid while scanning.
	 * Collecting all potential APs may result in bloat of
	 * the node tree. This call will return NULL if the node
	 * for this APs does not exist or if the new node is the
	 * potential better one.
	 */
	if ((ni = ieee80211_find_node_for_beacon(ic, wh->i_addr2,
                                             &ic->ic_channels[chan], (const char*)ssid, rxi->rxi_rssi)) != NULL)
		return;
    
#ifdef IEEE80211_DEBUG
	if (ieee80211_debug &&
	    (ni == NULL || ic->ic_state == IEEE80211_S_SCAN)) {
		printf("%s: %s%s on chan %u (bss chan %u) ",
               __func__, (ni == NULL ? "new " : ""),
               isprobe ? "probe response" : "beacon",
               chan, bchan);
		ieee80211_print_essid(ssid + 2, ssid[1]);
		printf(" from %s\n", ether_sprintf((u_int8_t *)wh->i_addr2));
		printf("%s: caps 0x%x bintval %u erp 0x%x\n",
               __func__, capinfo, bintval, erp);
	}
#endif
    
	if ((ni = ieee80211_find_node(ic, wh->i_addr2)) == NULL) {
		ni = ieee80211_alloc_node(ic, wh->i_addr2);
		if (ni == NULL)
			return;
		is_new = 1;
	} else
		is_new = 0;
    
	/*
	 * When operating in station mode, check for state updates
	 * while we're associated. We consider only 11g stuff right
	 * now.
	 */
	if (ic->ic_opmode == IEEE80211_M_STA &&
	    ic->ic_state == IEEE80211_S_RUN &&
	    ni->ni_state == IEEE80211_STA_BSS) {
		/*
		 * Check if protection mode has changed since last beacon.
		 */
		if (ni->ni_erp != erp) {
			DPRINTF(("[%s] erp change: was 0x%x, now 0x%x\n",
                     ether_sprintf((u_int8_t *)wh->i_addr2),
                     ni->ni_erp, erp));
			if (ic->ic_curmode == IEEE80211_MODE_11G &&
			    (erp & IEEE80211_ERP_USE_PROTECTION))
				ic->ic_flags |= IEEE80211_F_USEPROT;
			else
				ic->ic_flags &= ~IEEE80211_F_USEPROT;
			ic->ic_bss->ni_erp = erp;
		}
		/*
		 * Check if AP short slot time setting has changed
		 * since last beacon and give the driver a chance to
		 * update the hardware.
		 */
		if ((ni->ni_capinfo ^ capinfo) &
		    IEEE80211_CAPINFO_SHORT_SLOTTIME) {
			ieee80211_set_shortslottime(ic,
                                        ic->ic_curmode == IEEE80211_MODE_11A ||
                                        (capinfo & IEEE80211_CAPINFO_SHORT_SLOTTIME));
		}
	}
	/*
	 * We do not try to update EDCA parameters if QoS was not negotiated
	 * with the AP at association time.
	 */
	if (ni->ni_flags & IEEE80211_NODE_QOS) {
		/* always prefer EDCA IE over Wi-Fi Alliance WMM IE */
		if (edcaie != NULL)
			ieee80211_parse_edca_params(ic, edcaie);
		else if (wmmie != NULL)
			ieee80211_parse_wmm_params(ic, wmmie);
	}
    
	if (ic->ic_state == IEEE80211_S_SCAN &&
	    (ic->ic_flags & IEEE80211_F_RSNON)) {
		struct ieee80211_rsnparams rsn;
		const u_int8_t *saveie = NULL;
		/*
		 * If the AP advertises both RSN and WPA IEs (WPA1+WPA2),
		 * we only store the parameters of the highest protocol
		 * version we support.
		 */
		if (rsnie != NULL &&
		    (ic->ic_rsnprotos & IEEE80211_PROTO_RSN)) {
			if (ieee80211_parse_rsn(ic, rsnie, &rsn) == 0) {
				ni->ni_rsnprotos = IEEE80211_PROTO_RSN;
				saveie = rsnie;
			}
		} else if (wpaie != NULL &&
                   (ic->ic_rsnprotos & IEEE80211_PROTO_WPA)) {
			if (ieee80211_parse_wpa(ic, wpaie, &rsn) == 0) {
				ni->ni_rsnprotos = IEEE80211_PROTO_WPA;
				saveie = wpaie;
			}
		}
		if (saveie != NULL &&
		    ieee80211_save_ie(saveie, &ni->ni_rsnie) == 0) {
			ni->ni_rsnakms = rsn.rsn_akms;
			ni->ni_rsnciphers = rsn.rsn_ciphers;
			ni->ni_rsngroupcipher = rsn.rsn_groupcipher;
			ni->ni_rsngroupmgmtcipher = rsn.rsn_groupmgmtcipher;
			ni->ni_rsncaps = rsn.rsn_caps;
		} else
			ni->ni_rsnprotos = IEEE80211_PROTO_NONE;
	} else if (ic->ic_state == IEEE80211_S_SCAN)
		ni->ni_rsnprotos = IEEE80211_PROTO_NONE;
    
	if (ssid[1] != 0 && ni->ni_esslen == 0) {
		ni->ni_esslen = ssid[1];
		memset(ni->ni_essid, 0, sizeof(ni->ni_essid));
		/* we know that ssid[1] <= IEEE80211_NWID_LEN */
		memcpy(ni->ni_essid, &ssid[2], ssid[1]);
	}
	IEEE80211_ADDR_COPY(ni->ni_bssid, wh->i_addr3);
	ni->ni_rssi = rxi->rxi_rssi;
	ni->ni_rstamp = rxi->rxi_tstamp;
	memcpy(ni->ni_tstamp, tstamp, sizeof(ni->ni_tstamp));
	ni->ni_intval = bintval;
	ni->ni_capinfo = capinfo;
	/* XXX validate channel # */
	ni->ni_chan = &ic->ic_channels[chan];
	ni->ni_erp = erp;
	/* NB: must be after ni_chan is setup */
	ieee80211_setup_rates(ic, ni, rates, xrates, IEEE80211_F_DOSORT);
    
	/*
	 * When scanning we record results (nodes) with a zero
	 * refcnt.  Otherwise we want to hold the reference for
	 * ibss neighbors so the nodes don't get released prematurely.
	 * Anything else can be discarded (XXX and should be handled
	 * above so we don't do so much work).
	 */
	if (
	    (is_new && isprobe)) {
		/*
		 * Fake an association so the driver can setup it's
		 * private state.  The rate set has been setup above;
		 * there is no handshake as in ap/station operation.
		 */
		ieee80211_newassoc(ic, ni, 1);
	}
}

/*-
 * Authentication frame format:
 * [2] Authentication algorithm number
 * [2] Authentication transaction sequence number
 * [2] Status code
 */
void Voodoo80211Device::
ieee80211_recv_auth(struct ieee80211com *ic, mbuf_t m,
                    struct ieee80211_node *ni, struct ieee80211_rxinfo *rxi)
{
	const struct ieee80211_frame *wh;
	const u_int8_t *frm;
	u_int16_t algo, seq, status;
    
	/* make sure all mandatory fixed fields are present */
	if (mbuf_len(m) < sizeof(*wh) + 6) {
		DPRINTF(("frame too short\n"));
		return;
	}
	wh = mtod(m, struct ieee80211_frame *);
	frm = (const u_int8_t *)&wh[1];
    
	algo   = LE_READ_2(frm); frm += 2;
	seq    = LE_READ_2(frm); frm += 2;
	status = LE_READ_2(frm); frm += 2;
	DPRINTF(("auth %d seq %d from %s\n", algo, seq,
             ether_sprintf((u_int8_t *)wh->i_addr2)));
    
	/* only "open" auth mode is supported */
	if (algo != IEEE80211_AUTH_ALG_OPEN) {
		DPRINTF(("unsupported auth algorithm %d from %s\n",
                 algo, ether_sprintf((u_int8_t *)wh->i_addr2)));
		ic->ic_stats.is_rx_auth_unsupported++;
		return;
	}
	ieee80211_auth_open(ic, wh, ni, rxi, seq, status);
}

/*-
 * (Re)Association response frame format:
 * [2]   Capability information
 * [2]   Status code
 * [2]   Association ID (AID)
 * [tlv] Supported rates
 * [tlv] Extended Supported Rates (802.11g)
 * [tlv] EDCA Parameter Set (802.11e)
 * [tlv] HT Capabilities (802.11n)
 * [tlv] HT Operation (802.11n)
 */
void Voodoo80211Device::
ieee80211_recv_assoc_resp(struct ieee80211com *ic, mbuf_t m,
                          struct ieee80211_node *ni, int reassoc)
{
	const struct ieee80211_frame *wh;
	const u_int8_t *frm, *efrm;
	const u_int8_t *rates, *xrates, *edcaie, *wmmie, *htcaps, *htop;
	u_int16_t capinfo, status, associd;
	u_int8_t rate;
    
	if (ic->ic_opmode != IEEE80211_M_STA ||
	    ic->ic_state != IEEE80211_S_ASSOC) {
		ic->ic_stats.is_rx_mgtdiscard++;
		return;
	}
    
	/* make sure all mandatory fixed fields are present */
	if (mbuf_len(m) < sizeof(*wh) + 6) {
		DPRINTF(("frame too short\n"));
		return;
	}
	wh = mtod(m, struct ieee80211_frame *);
	frm = (const u_int8_t *)&wh[1];
	efrm = mtod(m, u_int8_t *) + mbuf_len(m);
    
	capinfo = LE_READ_2(frm); frm += 2;
	status =  LE_READ_2(frm); frm += 2;
	if (status != IEEE80211_STATUS_SUCCESS) {
		/* TODO debug
		if (fInterface->getFlags() & IFF_DEBUG)
			printf("%s: %sassociation failed (status %d)"
                   " for %s\n", "voodoo_wifi",
                   reassoc ?  "re" : "",
                   status, ether_sprintf((u_int8_t *)wh->i_addr3));
		 */
		if (ni != ic->ic_bss)
			ni->ni_fails++;
		ic->ic_stats.is_rx_auth_fail++;
		return;
	}
	associd = LE_READ_2(frm); frm += 2;
    
	rates = xrates = edcaie = wmmie = htcaps = htop = NULL;
	while (frm + 2 <= efrm) {
		if (frm + 2 + frm[1] > efrm) {
			ic->ic_stats.is_rx_elem_toosmall++;
			break;
		}
		switch (frm[0]) {
            case IEEE80211_ELEMID_RATES:
                rates = frm;
                break;
            case IEEE80211_ELEMID_XRATES:
                xrates = frm;
                break;
            case IEEE80211_ELEMID_EDCAPARMS:
                edcaie = frm;
                break;
#ifndef IEEE80211_NO_HT
            case IEEE80211_ELEMID_HTCAPS:
                htcaps = frm;
                break;
            case IEEE80211_ELEMID_HTOP:
                htop = frm;
                break;
#endif
            case IEEE80211_ELEMID_VENDOR:
                if (frm[1] < 4) {
                    ic->ic_stats.is_rx_elem_toosmall++;
                    break;
                }
                if (memcmp(frm + 2, MICROSOFT_OUI, 3) == 0) {
                    if (frm[1] >= 5 && frm[5] == 2 && frm[6] == 1)
                        wmmie = frm;
                }
                break;
		}
		frm += 2 + frm[1];
	}
	/* supported rates element is mandatory */
	if (rates == NULL || rates[1] > IEEE80211_RATE_MAXSIZE) {
		DPRINTF(("invalid supported rates element\n"));
		return;
	}
	rate = ieee80211_setup_rates(ic, ni, rates, xrates,
                                 IEEE80211_F_DOSORT | IEEE80211_F_DOFRATE | IEEE80211_F_DONEGO |
                                 IEEE80211_F_DODEL);
	if (rate & IEEE80211_RATE_BASIC) {
		DPRINTF(("rate mismatch for %s\n",
                 ether_sprintf((u_int8_t *)wh->i_addr2)));
		ic->ic_stats.is_rx_assoc_norate++;
		return;
	}
	ni->ni_capinfo = capinfo;
	ni->ni_associd = associd;
	if (edcaie != NULL || wmmie != NULL) {
		/* force update of EDCA parameters */
		ic->ic_edca_updtcount = -1;
        
		if ((edcaie != NULL &&
		     ieee80211_parse_edca_params(ic, edcaie) == 0) ||
		    (wmmie != NULL &&
		     ieee80211_parse_wmm_params(ic, wmmie) == 0))
			ni->ni_flags |= IEEE80211_NODE_QOS;
		else	/* for Reassociation */
			ni->ni_flags &= ~IEEE80211_NODE_QOS;
	}
	/*
	 * Configure state now that we are associated.
	 */
	if (ic->ic_curmode == IEEE80211_MODE_11A ||
	    (ni->ni_capinfo & IEEE80211_CAPINFO_SHORT_PREAMBLE))
		ic->ic_flags |= IEEE80211_F_SHPREAMBLE;
	else
		ic->ic_flags &= ~IEEE80211_F_SHPREAMBLE;
    
	ieee80211_set_shortslottime(ic,
                                ic->ic_curmode == IEEE80211_MODE_11A ||
                                (ni->ni_capinfo & IEEE80211_CAPINFO_SHORT_SLOTTIME));
	/*
	 * Honor ERP protection.
	 */
	if (ic->ic_curmode == IEEE80211_MODE_11G &&
	    (ni->ni_erp & IEEE80211_ERP_USE_PROTECTION))
		ic->ic_flags |= IEEE80211_F_USEPROT;
	else
		ic->ic_flags &= ~IEEE80211_F_USEPROT;
	/*
	 * If not an RSNA, mark the port as valid, otherwise wait for
	 * 802.1X authentication and 4-way handshake to complete..
	 */
	if (ic->ic_flags & IEEE80211_F_RSNON) {
		/* XXX ic->ic_mgt_timer = 5; */
	} else if (ic->ic_flags & IEEE80211_F_WEPON)
		ni->ni_flags |= IEEE80211_NODE_TXRXPROT;
    
	ieee80211_newstate(ic, IEEE80211_S_RUN,
                        IEEE80211_FC0_SUBTYPE_ASSOC_RESP);
}

/*-
 * Deauthentication frame format:
 * [2] Reason code
 */
void Voodoo80211Device::
ieee80211_recv_deauth(struct ieee80211com *ic, mbuf_t m,
                      struct ieee80211_node *ni)
{
	const struct ieee80211_frame *wh;
	const u_int8_t *frm;
	u_int16_t reason;
    
	/* make sure all mandatory fixed fields are present */
	if (mbuf_len(m) < sizeof(*wh) + 2) {
		DPRINTF(("frame too short\n"));
		return;
	}
	wh = mtod(m, struct ieee80211_frame *);
	frm = (const u_int8_t *)&wh[1];
    
	reason = LE_READ_2(frm);
    
	ic->ic_stats.is_rx_deauth++;
	switch (ic->ic_opmode) {
        case IEEE80211_M_STA:
            ieee80211_newstate(ic, IEEE80211_S_AUTH,
                                IEEE80211_FC0_SUBTYPE_DEAUTH);
            break;
        default:
            break;
	}
}

/*-
 * Disassociation frame format:
 * [2] Reason code
 */
void Voodoo80211Device::
ieee80211_recv_disassoc(struct ieee80211com *ic, mbuf_t m,
                        struct ieee80211_node *ni)
{
	const struct ieee80211_frame *wh;
	const u_int8_t *frm;
	u_int16_t reason;
    
	/* make sure all mandatory fixed fields are present */
	if (mbuf_len(m) < sizeof(*wh) + 2) {
		DPRINTF(("frame too short\n"));
		return;
	}
	wh = mtod(m, struct ieee80211_frame *);
	frm = (const u_int8_t *)&wh[1];
    
	reason = LE_READ_2(frm);
    
	ic->ic_stats.is_rx_disassoc++;
	switch (ic->ic_opmode) {
        case IEEE80211_M_STA:
            ieee80211_newstate(ic, IEEE80211_S_ASSOC,
                                IEEE80211_FC0_SUBTYPE_DISASSOC);
            break;
        default:
            break;
	}
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
void Voodoo80211Device::
ieee80211_recv_addba_req(struct ieee80211com *ic, mbuf_t m,
                         struct ieee80211_node *ni)
{
	const struct ieee80211_frame *wh;
	const u_int8_t *frm;
	struct ieee80211_rx_ba *ba;
	u_int16_t params, ssn, bufsz, timeout, status;
	u_int8_t token, tid;
    
	if (!(ni->ni_flags & IEEE80211_NODE_HT)) {
		DPRINTF(("received ADDBA req from non-HT STA %s\n",
                 ether_sprintf(ni->ni_macaddr)));
		return;
	}
	if (mbuf_len(m) < sizeof(*wh) + 9) {
		DPRINTF(("frame too short\n"));
		return;
	}
	/* MLME-ADDBA.indication */
	wh = mtod(m, struct ieee80211_frame *);
	frm = (const u_int8_t *)&wh[1];
    
	token = frm[2];
	params = LE_READ_2(&frm[3]);
	tid = (params >> 2) & 0xf;
	bufsz = (params >> 6) & 0x3ff;
	timeout = LE_READ_2(&frm[5]);
	ssn = LE_READ_2(&frm[7]) >> 4;
    
	ba = &ni->ni_rx_ba[tid];
	/* check if we already have a Block Ack agreement for this RA/TID */
	if (ba->ba_state == IEEE80211_BA_AGREED) {
		/* XXX should we update the timeout value? */
		/* reset Block Ack inactivity timer */
		timeout_add_usec(ba->ba_to, ba->ba_timeout_val);
        
		/* check if it's a Protected Block Ack agreement */
		if (!(ni->ni_flags & IEEE80211_NODE_MFP) ||
		    !(ni->ni_rsncaps & IEEE80211_RSNCAP_PBAC))
			return;	/* not a PBAC, ignore */
        
		/* PBAC: treat the ADDBA Request like a BlockAckReq */
		if (SEQ_LT(ba->ba_winstart, ssn))
			ieee80211_ba_move_window(ic, ni, tid, ssn);
		return;
	}
	/* if PBAC required but RA does not support it, refuse request */
	if ((ic->ic_flags & IEEE80211_F_PBAR) &&
	    (!(ni->ni_flags & IEEE80211_NODE_MFP) ||
	     !(ni->ni_rsncaps & IEEE80211_RSNCAP_PBAC))) {
            status = IEEE80211_STATUS_REFUSED;
            goto resp;
        }
	/*
	 * If the TID for which the Block Ack agreement is requested is
	 * configured with a no-ACK policy, refuse the agreement.
	 */
	if (ic->ic_tid_noack & (1 << tid)) {
		status = IEEE80211_STATUS_REFUSED;
		goto resp;
	}
	/* check that we support the requested Block Ack Policy */
	if (!(ic->ic_htcaps & IEEE80211_HTCAP_DELAYEDBA) &&
	    !(params & IEEE80211_BA_ACK_POLICY)) {
		status = IEEE80211_STATUS_INVALID_PARAM;
		goto resp;
	}
    
	/* setup Block Ack agreement */
	ba->ba_state = IEEE80211_BA_INIT;
	ba->ba_timeout_val = timeout * IEEE80211_DUR_TU;
	if (ba->ba_timeout_val < IEEE80211_BA_MIN_TIMEOUT)
		ba->ba_timeout_val = IEEE80211_BA_MIN_TIMEOUT;
	else if (ba->ba_timeout_val > IEEE80211_BA_MAX_TIMEOUT)
		ba->ba_timeout_val = IEEE80211_BA_MAX_TIMEOUT;
	timeout_set(ba->ba_to, OSMemberFunctionCast(VoodooTimeout::CallbackFunction, this, &Voodoo80211Device::ieee80211_rx_ba_timeout), ba);
	ba->ba_winsize = bufsz;
	if (ba->ba_winsize == 0 || ba->ba_winsize > IEEE80211_BA_MAX_WINSZ)
		ba->ba_winsize = IEEE80211_BA_MAX_WINSZ;
	ba->ba_winstart = ssn;
	ba->ba_winend = (ba->ba_winstart + ba->ba_winsize - 1) & 0xfff;
	/* allocate and setup our reordering buffer */
	ba->ba_buf = (typeof(ba->ba_buf))malloc(IEEE80211_BA_MAX_WINSZ * sizeof(*ba->ba_buf), // pvaibhav: what a hack cast
                        M_DEVBUF, M_NOWAIT | M_ZERO);
	if (ba->ba_buf == NULL) {
		status = IEEE80211_STATUS_REFUSED;
		goto resp;
	}
	ba->ba_head = 0;
    
	/* notify drivers of this new Block Ack agreement */
	if (ieee80211_ampdu_rx_start(ic, ni, tid) != 0) {
		/* driver failed to setup, rollback */
		free(ba->ba_buf);
		ba->ba_buf = NULL;
		status = IEEE80211_STATUS_REFUSED;
		goto resp;
	}
	ba->ba_state = IEEE80211_BA_AGREED;
	/* start Block Ack inactivity timer */
	timeout_add_usec(ba->ba_to, ba->ba_timeout_val);
	status = IEEE80211_STATUS_SUCCESS;
resp:
	/* MLME-ADDBA.response */
	IEEE80211_SEND_ACTION(ic, ni, IEEE80211_CATEG_BA,
                          IEEE80211_ACTION_ADDBA_RESP, status << 16 | token << 8 | tid);
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
void Voodoo80211Device::
ieee80211_recv_addba_resp(struct ieee80211com *ic, mbuf_t m,
                          struct ieee80211_node *ni)
{
	const struct ieee80211_frame *wh;
	const u_int8_t *frm;
	struct ieee80211_tx_ba *ba;
	u_int16_t status, params, bufsz, timeout;
	u_int8_t token, tid;
    
	if (mbuf_len(m) < sizeof(*wh) + 9) {
		DPRINTF(("frame too short\n"));
		return;
	}
	wh = mtod(m, struct ieee80211_frame *);
	frm = (const u_int8_t *)&wh[1];
    
	token = frm[2];
	status = LE_READ_2(&frm[3]);
	params = LE_READ_2(&frm[5]);
	tid = (params >> 2) & 0xf;
	bufsz = (params >> 6) & 0x3ff;
	timeout = LE_READ_2(&frm[7]);
    
	DPRINTF(("received ADDBA resp from %s, TID %d, status %d\n",
             ether_sprintf(ni->ni_macaddr), tid, status));
    
	/*
	 * Ignore if no ADDBA request has been sent for this RA/TID or
	 * if we already have a Block Ack agreement.
	 */
	ba = &ni->ni_tx_ba[tid];
	if (ba->ba_state != IEEE80211_BA_REQUESTED) {
		DPRINTF(("no matching ADDBA req found\n"));
		return;
	}
	if (token != ba->ba_token) {
		DPRINTF(("ignoring ADDBA resp from %s: token %x!=%x\n",
                 ether_sprintf(ni->ni_macaddr), token, ba->ba_token));
		return;
	}
	/* we got an ADDBA Response matching our request, stop timeout */
	timeout_del(ba->ba_to);
    
	if (status != IEEE80211_STATUS_SUCCESS) {
		/* MLME-ADDBA.confirm(Failure) */
		ba->ba_state = IEEE80211_BA_INIT;
		return;
	}
	/* MLME-ADDBA.confirm(Success) */
	ba->ba_state = IEEE80211_BA_AGREED;
    
	/* notify drivers of this new Block Ack agreement */
	ieee80211_ampdu_tx_start(ic, ni, tid);
    
	/* start Block Ack inactivity timeout */
	if (ba->ba_timeout_val != 0)
		timeout_add_usec(ba->ba_to, ba->ba_timeout_val);
}

/*-
 * DELBA frame format:
 * [1] Category
 * [1] Action
 * [2] DELBA Parameter Set
 * [2] Reason Code
 */
void Voodoo80211Device::
ieee80211_recv_delba(struct ieee80211com *ic, mbuf_t m,
                     struct ieee80211_node *ni)
{
	const struct ieee80211_frame *wh;
	const u_int8_t *frm;
	u_int16_t params, reason;
	u_int8_t tid;
	int i;
    
	if (mbuf_len(m) < sizeof(*wh) + 6) {
		DPRINTF(("frame too short\n"));
		return;
	}
	wh = mtod(m, struct ieee80211_frame *);
	frm = (const u_int8_t *)&wh[1];
    
	params = LE_READ_2(&frm[2]);
	reason = LE_READ_2(&frm[4]);
	tid = params >> 12;
    
	DPRINTF(("received DELBA from %s, TID %d, reason %d\n",
             ether_sprintf(ni->ni_macaddr), tid, reason));
    
	if (params & IEEE80211_DELBA_INITIATOR) {
		/* MLME-DELBA.indication(Originator) */
		struct ieee80211_rx_ba *ba = &ni->ni_rx_ba[tid];
        
		if (ba->ba_state != IEEE80211_BA_AGREED) {
			DPRINTF(("no matching Block Ack agreement\n"));
			return;
		}
		/* notify drivers of the end of the Block Ack agreement */
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
	} else {
		/* MLME-DELBA.indication(Recipient) */
		struct ieee80211_tx_ba *ba = &ni->ni_tx_ba[tid];
        
		if (ba->ba_state != IEEE80211_BA_AGREED) {
			DPRINTF(("no matching Block Ack agreement\n"));
			return;
		}
		/* notify drivers of the end of the Block Ack agreement */
		ieee80211_ampdu_tx_stop(ic, ni, tid);
        
		ba->ba_state = IEEE80211_BA_INIT;
		/* stop Block Ack inactivity timer */
		timeout_del(ba->ba_to);
	}
}
#endif	/* !IEEE80211_NO_HT */

/*-
 * SA Query Request frame format:
 * [1] Category
 * [1] Action
 * [2] Transaction Identifier
 */
void Voodoo80211Device::
ieee80211_recv_sa_query_req(struct ieee80211com *ic, mbuf_t m,
                            struct ieee80211_node *ni)
{
	const struct ieee80211_frame *wh;
	const u_int8_t *frm;
    
	if (ic->ic_opmode != IEEE80211_M_STA ||
	    !(ni->ni_flags & IEEE80211_NODE_MFP)) {
		DPRINTF(("unexpected SA Query req from %s\n",
                 ether_sprintf(ni->ni_macaddr)));
		return;
	}
	if (mbuf_len(m) < sizeof(*wh) + 4) {
		DPRINTF(("frame too short\n"));
		return;
	}
	wh = mtod(m, struct ieee80211_frame *);
	frm = (const u_int8_t *)&wh[1];
    
	/* MLME-SAQuery.indication */
    
	/* save Transaction Identifier for SA Query Response */
	ni->ni_sa_query_trid = LE_READ_2(&frm[2]);
    
	/* MLME-SAQuery.response */
	IEEE80211_SEND_ACTION(ic, ni, IEEE80211_CATEG_SA_QUERY,
                          IEEE80211_ACTION_SA_QUERY_RESP, 0);
}


/*-
 * Action frame format:
 * [1] Category
 * [1] Action
 */
void Voodoo80211Device::
ieee80211_recv_action(struct ieee80211com *ic, mbuf_t m,
                      struct ieee80211_node *ni)
{
	const struct ieee80211_frame *wh;
	const u_int8_t *frm;
    
	if (mbuf_len(m) < sizeof(*wh) + 2) {
		DPRINTF(("frame too short\n"));
		return;
	}
	wh = mtod(m, struct ieee80211_frame *);
	frm = (const u_int8_t *)&wh[1];
    
	switch (frm[0]) {
#ifndef IEEE80211_NO_HT
        case IEEE80211_CATEG_BA:
            switch (frm[1]) {
                case IEEE80211_ACTION_ADDBA_REQ:
                    ieee80211_recv_addba_req(ic, m, ni);
                    break;
                case IEEE80211_ACTION_ADDBA_RESP:
                    ieee80211_recv_addba_resp(ic, m, ni);
                    break;
                case IEEE80211_ACTION_DELBA:
                    ieee80211_recv_delba(ic, m, ni);
                    break;
            }
            break;
#endif
        case IEEE80211_CATEG_SA_QUERY:
            switch (frm[1]) {
                case IEEE80211_ACTION_SA_QUERY_REQ:
                    ieee80211_recv_sa_query_req(ic, m, ni);
                    break;
            }
            break;
        default:
            DPRINTF(("action frame category %d not handled\n", frm[0]));
            break;
	}
}

void Voodoo80211Device::
ieee80211_recv_mgmt(struct ieee80211com *ic, mbuf_t m,
                    struct ieee80211_node *ni, struct ieee80211_rxinfo *rxi, int subtype)
{
	switch (subtype) {
        case IEEE80211_FC0_SUBTYPE_BEACON:
            ieee80211_recv_probe_resp(ic, m, ni, rxi, 0);
            break;
        case IEEE80211_FC0_SUBTYPE_PROBE_RESP:
            ieee80211_recv_probe_resp(ic, m, ni, rxi, 1);
            break;
        case IEEE80211_FC0_SUBTYPE_AUTH:
            ieee80211_recv_auth(ic, m, ni, rxi);
            break;
        case IEEE80211_FC0_SUBTYPE_ASSOC_RESP:
            ieee80211_recv_assoc_resp(ic, m, ni, 0);
            break;
        case IEEE80211_FC0_SUBTYPE_REASSOC_RESP:
            ieee80211_recv_assoc_resp(ic, m, ni, 1);
            break;
        case IEEE80211_FC0_SUBTYPE_DEAUTH:
            ieee80211_recv_deauth(ic, m, ni);
            break;
        case IEEE80211_FC0_SUBTYPE_DISASSOC:
            ieee80211_recv_disassoc(ic, m, ni);
            break;
        case IEEE80211_FC0_SUBTYPE_ACTION:
            ieee80211_recv_action(ic, m, ni);
            break;
        default:
            DPRINTF(("mgmt frame with subtype 0x%x not handled\n",
                     subtype));
            ic->ic_stats.is_rx_badsubtype++;
            break;
	}
}

#ifndef IEEE80211_NO_HT
/*
 * Process an incoming BlockAckReq control frame (see 7.2.1.7).
 */
void Voodoo80211Device::
ieee80211_recv_bar(struct ieee80211com *ic, mbuf_t m,
                   struct ieee80211_node *ni)
{
	const struct ieee80211_frame_min *wh;
	const u_int8_t *frm;
	u_int16_t ctl, ssn;
	u_int8_t tid, ntids;
    
	if (!(ni->ni_flags & IEEE80211_NODE_HT)) {
		DPRINTF(("received BlockAckReq from non-HT STA %s\n",
                 ether_sprintf(ni->ni_macaddr)));
		return;
	}
	if (mbuf_len(m) < sizeof(*wh) + 4) {
		DPRINTF(("frame too short\n"));
		return;
	}
	wh = mtod(m, struct ieee80211_frame_min *);
	frm = (const u_int8_t *)&wh[1];
    
	/* read BlockAckReq Control field */
	ctl = LE_READ_2(&frm[0]);
	tid = ctl >> 12;
    
	/* determine BlockAckReq frame variant */
	if (ctl & IEEE80211_BA_MULTI_TID) {
		/* Multi-TID BlockAckReq variant (PSMP only) */
		ntids = tid + 1;
        
		if (mbuf_len(m) < sizeof(*wh) + 2 + 4 * ntids) {
			DPRINTF(("MTBAR frame too short\n"));
			return;
		}
		frm += 2;	/* skip BlockAckReq Control field */
		while (ntids-- > 0) {
			/* read MTBAR Information field */
			tid = LE_READ_2(&frm[0]) >> 12;
			ssn = LE_READ_2(&frm[2]) >> 4;
			ieee80211_bar_tid(ic, ni, tid, ssn);
			frm += 4;
		}
	} else {
		/* Basic or Compressed BlockAckReq variants */
		ssn = LE_READ_2(&frm[2]) >> 4;
		ieee80211_bar_tid(ic, ni, tid, ssn);
	}
}

/*
 * Process a BlockAckReq for a specific TID (see 9.10.7.6.3).
 * This is the common back-end for all BlockAckReq frame variants.
 */
void Voodoo80211Device::
ieee80211_bar_tid(struct ieee80211com *ic, struct ieee80211_node *ni,
                  u_int8_t tid, u_int16_t ssn)
{
	struct ieee80211_rx_ba *ba = &ni->ni_rx_ba[tid];
    
	/* check if we have a Block Ack agreement for RA/TID */
	if (ba->ba_state != IEEE80211_BA_AGREED) {
		/* XXX not sure in PBAC case */
		/* send a DELBA with reason code UNKNOWN-BA */
		IEEE80211_SEND_ACTION(ic, ni, IEEE80211_CATEG_BA,
                              IEEE80211_ACTION_DELBA,
                              IEEE80211_REASON_SETUP_REQUIRED << 16 | tid);
		return;
	}
	/* check if it is a Protected Block Ack agreement */
	if ((ni->ni_flags & IEEE80211_NODE_MFP) &&
	    (ni->ni_rsncaps & IEEE80211_RSNCAP_PBAC)) {
		/* ADDBA Requests must be used in PBAC case */
		if (SEQ_LT(ssn, ba->ba_winstart) ||
		    SEQ_LT(ba->ba_winend, ssn))
			ic->ic_stats.is_pbac_errs++;
		return;	/* PBAC, do not move window */
	}
	/* reset Block Ack inactivity timer */
	timeout_add_usec(ba->ba_to, ba->ba_timeout_val);
    
	if (SEQ_LT(ba->ba_winstart, ssn))
		ieee80211_ba_move_window(ic, ni, tid, ssn);
}
#endif	/* !IEEE80211_NO_HT */