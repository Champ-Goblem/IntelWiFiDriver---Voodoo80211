//
//  Voodoo80211Device.h
//  net80211
//
//  Created by Prashant Vaibhav on 20/12/11.
//  Copyright (c) 2011 Prashant Vaibhav. All rights reserved.
//

#ifndef net80211_Voodoo80211Device_h
#define net80211_Voodoo80211Device_h

#include <sys/types.h>
#include <sys/kpi_mbuf.h>
#include <kern/assert.h>

// pvaibhav: common definitions
#define MyClass         Voodoo80211Device
#define super           IOEthernetController
#define __packed        __attribute__((__packed__))
#define mtod(m, t)      (t) mbuf_data(m)
#define M_DEVBUF        2
#define malloc          _MALLOC
#define compat_free     _FREE
#define VoodooSetFunction(fptr, fn) fptr = OSMemberFunctionCast(typeof(fptr), this, &MyClass::fn)

#include <IOKit/IOService.h>
#include <IOKit/network/IOEthernetInterface.h>
#include <IOKit/IOWorkloop.h>
#include <IOKit/IOTimerEventSource.h>

#include "ieee80211.h"
#include "ieee80211_priv.h"
#include "ieee80211_var.h"

class Voodoo80211Device : public IOEthernetController
{
	OSDeclareDefaultStructors(Voodoo80211Device)
    
private:
#pragma mark Private data
    IOEthernetInterface*    fInterface;
    IOWorkLoop*             fWorkloop;
    IOTimerEventSource*     fTimer;
    
#pragma mark Compatibility functions
    int     splnet();
    void    splx(int);
    
protected:
#pragma mark ieee80211_var.h
    void	ieee80211_ifattach(struct ifnet *);
    void	ieee80211_ifdetach(struct ifnet *);
    // TODO void	ieee80211_media_init(struct ifnet *, ifm_change_cb_t, ifm_stat_cb_t);
    int     ieee80211_media_change(struct ifnet *);
    void	ieee80211_media_status(struct ifnet *, struct ifmediareq *);
    int     ieee80211_ioctl(struct ifnet *, u_long, caddr_t);
    int     ieee80211_get_rate(struct ieee80211com *);
    void	ieee80211_watchdog(struct ifnet *);
    int     ieee80211_fix_rate(struct ieee80211com *, struct ieee80211_node *, int);
    int     ieee80211_rate2media(struct ieee80211com *, int, enum ieee80211_phymode);
    int     ieee80211_media2rate(int);
    u_int8_t ieee80211_rate2plcp(u_int8_t, enum ieee80211_phymode);
    u_int8_t ieee80211_plcp2rate(u_int8_t, enum ieee80211_phymode);
    u_int	ieee80211_mhz2ieee(u_int, u_int);
    u_int	ieee80211_chan2ieee(struct ieee80211com *, const struct ieee80211_channel *);
    u_int	ieee80211_ieee2mhz(u_int, u_int);
    int     ieee80211_setmode(struct ieee80211com *, enum ieee80211_phymode);
    enum ieee80211_phymode ieee80211_next_mode(struct ifnet *);
    enum ieee80211_phymode ieee80211_chan2mode(struct ieee80211com *, const struct ieee80211_channel *);
    
#pragma mark ieee80211_node.h
    // cpp functions
    struct  ieee80211_node *ieee80211_node_alloc(struct ieee80211com *);
    void    ieee80211_node_free(struct ieee80211com *, struct ieee80211_node *);
    void    ieee80211_node_copy(struct ieee80211com *, struct ieee80211_node *, const struct ieee80211_node *);
    void    ieee80211_choose_rsnparams(struct ieee80211com *);
    u_int8_t ieee80211_node_getrssi(struct ieee80211com *, const struct ieee80211_node *);
    void    ieee80211_setup_node(struct ieee80211com *, struct ieee80211_node *, const u_int8_t *);
    void    ieee80211_free_node(struct ieee80211com *, struct ieee80211_node *);
    struct  ieee80211_node *ieee80211_alloc_node_helper(struct ieee80211com *);
    void    ieee80211_node_cleanup(struct ieee80211com *, struct ieee80211_node *);
    void    ieee80211_needs_auth(struct ieee80211com *, struct ieee80211_node *);
    // header functions
    void    ieee80211_node_attach(struct ifnet *);
    void    ieee80211_node_lateattach(struct ifnet *);
    void    ieee80211_node_detach(struct ifnet *);
    void    ieee80211_begin_scan(struct ifnet *);
    void    ieee80211_next_scan(struct ifnet *);
    void    ieee80211_end_scan(struct ifnet *);
    void    ieee80211_reset_scan(struct ifnet *);
    struct  ieee80211_node *ieee80211_alloc_node(struct ieee80211com *, const u_int8_t *);
    struct  ieee80211_node *ieee80211_dup_bss(struct ieee80211com *, const u_int8_t *);
    struct  ieee80211_node *ieee80211_find_node(struct ieee80211com *, const u_int8_t *);
    struct  ieee80211_node *ieee80211_find_rxnode(struct ieee80211com *, const struct ieee80211_frame *);
    struct  ieee80211_node *ieee80211_find_txnode(struct ieee80211com *, const u_int8_t *);
    struct  ieee80211_node *ieee80211_find_node_for_beacon(struct ieee80211com *, const u_int8_t *, const struct ieee80211_channel *, const char *, u_int8_t);
    void    ieee80211_release_node(struct ieee80211com *, struct ieee80211_node *);
    void    ieee80211_free_allnodes(struct ieee80211com *);
    typedef void ieee80211_iter_func(void *, struct ieee80211_node *);
    void    ieee80211_iterate_nodes(struct ieee80211com *ic, ieee80211_iter_func *, void *);
    void    ieee80211_clean_nodes(struct ieee80211com *);
    int     ieee80211_setup_rates(struct ieee80211com *, struct ieee80211_node *, const u_int8_t *, const u_int8_t *, int);
    int     ieee80211_iserp_sta(const struct ieee80211_node *);
    void    ieee80211_node_join(struct ieee80211com *, struct ieee80211_node *, int);
    void    ieee80211_node_leave(struct ieee80211com *, struct ieee80211_node *);
    int     ieee80211_match_bss(struct ieee80211com *, struct ieee80211_node *);
    void    ieee80211_create_ibss(struct ieee80211com*, struct ieee80211_channel *);
    void    ieee80211_notify_dtim(struct ieee80211com *);
    int     ieee80211_node_cmp(const struct ieee80211_node *, const struct ieee80211_node *);

#pragma mark ieee80211_crypto.h
    // cpp file
    void	ieee80211_prf(const u_int8_t *, size_t, const u_int8_t *, size_t, const u_int8_t *, size_t, u_int8_t *, size_t);
    void	ieee80211_kdf(const u_int8_t *, size_t, const u_int8_t *, size_t, const u_int8_t *, size_t, u_int8_t *, size_t);
    void	ieee80211_derive_pmkid(enum ieee80211_akm, const u_int8_t *, const u_int8_t *, const u_int8_t *, u_int8_t *);
    // header file
    void	ieee80211_crypto_attach(struct ifnet *);
    void	ieee80211_crypto_detach(struct ifnet *);
    struct	ieee80211_key *ieee80211_get_txkey(struct ieee80211com *, const struct ieee80211_frame *, struct ieee80211_node *);
    struct	ieee80211_key *ieee80211_get_rxkey(struct ieee80211com *, mbuf_t, struct ieee80211_node *);
    mbuf_t	ieee80211_encrypt(struct ieee80211com *, mbuf_t, struct ieee80211_key *);
    mbuf_t	ieee80211_decrypt(struct ieee80211com *, mbuf_t, struct ieee80211_node *);
    int     ieee80211_set_key(struct ieee80211com *, struct ieee80211_node *, struct ieee80211_key *);
    void	ieee80211_delete_key(struct ieee80211com *, struct ieee80211_node *, struct ieee80211_key *);
    void	ieee80211_eapol_key_mic(struct ieee80211_eapol_key *, const u_int8_t *);
    int     ieee80211_eapol_key_check_mic(struct ieee80211_eapol_key *, const u_int8_t *);
    int     ieee80211_eapol_key_decrypt(struct ieee80211_eapol_key *, const u_int8_t *);
    struct	ieee80211_pmk *ieee80211_pmksa_add(struct ieee80211com *, enum ieee80211_akm, const u_int8_t *, const u_int8_t *, u_int32_t);
    struct	ieee80211_pmk *ieee80211_pmksa_find(struct ieee80211com *, struct ieee80211_node *, const u_int8_t *);
    void	ieee80211_derive_ptk(enum ieee80211_akm, const u_int8_t *, const u_int8_t *, const u_int8_t *, const u_int8_t *, const u_int8_t *, struct ieee80211_ptk *);
    int     ieee80211_cipher_keylen(enum ieee80211_cipher);
    int     ieee80211_wep_set_key(struct ieee80211com *, struct ieee80211_key *);
    void	ieee80211_wep_delete_key(struct ieee80211com *, struct ieee80211_key *);
    mbuf_t	ieee80211_wep_encrypt(struct ieee80211com *, mbuf_t, struct ieee80211_key *);
    mbuf_t	ieee80211_wep_decrypt(struct ieee80211com *, mbuf_t, struct ieee80211_key *);
    int     ieee80211_tkip_set_key(struct ieee80211com *, struct ieee80211_key *);
    void	ieee80211_tkip_delete_key(struct ieee80211com *, struct ieee80211_key *);
    mbuf_t	ieee80211_tkip_encrypt(struct ieee80211com *, mbuf_t, struct ieee80211_key *);
    mbuf_t	ieee80211_tkip_decrypt(struct ieee80211com *, mbuf_t, struct ieee80211_key *);
    void	ieee80211_tkip_mic(mbuf_t, int, const u_int8_t *, u_int8_t[IEEE80211_TKIP_MICLEN]);
    void	ieee80211_michael_mic_failure(struct ieee80211com *, u_int64_t);
    int     ieee80211_ccmp_set_key(struct ieee80211com *, struct ieee80211_key *);
    void	ieee80211_ccmp_delete_key(struct ieee80211com *, struct ieee80211_key *);
    mbuf_t	ieee80211_ccmp_encrypt(struct ieee80211com *, mbuf_t, struct ieee80211_key *);
    mbuf_t	ieee80211_ccmp_decrypt(struct ieee80211com *, mbuf_t, struct ieee80211_key *);
    int     ieee80211_bip_set_key(struct ieee80211com *, struct ieee80211_key *);
    void	ieee80211_bip_delete_key(struct ieee80211com *, struct ieee80211_key *);
    mbuf_t	ieee80211_bip_encap(struct ieee80211com *, mbuf_t, struct ieee80211_key *);
    mbuf_t	ieee80211_bip_decap(struct ieee80211com *, mbuf_t, struct ieee80211_key *);  
    
#pragma mark ieee80211_input.cpp
    // cpp file
    mbuf_t  ieee80211_defrag(struct ieee80211com *, mbuf_t, int);
    void	ieee80211_defrag_timeout(void *);
#ifndef IEEE80211_NO_HT
    void	ieee80211_input_ba(struct ifnet *, mbuf_t,
                               struct ieee80211_node *, int, struct ieee80211_rxinfo *);
    void	ieee80211_ba_move_window(struct ieee80211com *,
                                     struct ieee80211_node *, u_int8_t, u_int16_t);
#endif
    mbuf_t  ieee80211_align_mbuf(mbuf_t);
    void	ieee80211_decap(struct ieee80211com *, mbuf_t,
                            struct ieee80211_node *, int);
#ifndef IEEE80211_NO_HT
    void	ieee80211_amsdu_decap(struct ieee80211com *, mbuf_t,
                                  struct ieee80211_node *, int);
#endif
    void	ieee80211_deliver_data(struct ieee80211com *, mbuf_t,
                                   struct ieee80211_node *);
    int     ieee80211_parse_edca_params_body(struct ieee80211com *,
                                             const u_int8_t *);
    int     ieee80211_parse_edca_params(struct ieee80211com *, const u_int8_t *);
    int     ieee80211_parse_wmm_params(struct ieee80211com *, const u_int8_t *);
    enum	ieee80211_cipher ieee80211_parse_rsn_cipher(const u_int8_t[]);
    enum	ieee80211_akm ieee80211_parse_rsn_akm(const u_int8_t[]);
    int     ieee80211_parse_rsn_body(struct ieee80211com *, const u_int8_t *,
                                     u_int, struct ieee80211_rsnparams *);
    int     ieee80211_save_ie(const u_int8_t *, u_int8_t **);
    void	ieee80211_recv_probe_resp(struct ieee80211com *, mbuf_t,
                                      struct ieee80211_node *, struct ieee80211_rxinfo *, int);
    void	ieee80211_recv_auth(struct ieee80211com *, mbuf_t,
                                struct ieee80211_node *, struct ieee80211_rxinfo *);
    void	ieee80211_recv_assoc_resp(struct ieee80211com *, mbuf_t,
                                      struct ieee80211_node *, int);
    void	ieee80211_recv_deauth(struct ieee80211com *, mbuf_t,
                                  struct ieee80211_node *);
    void	ieee80211_recv_disassoc(struct ieee80211com *, mbuf_t,
                                    struct ieee80211_node *);
#ifndef IEEE80211_NO_HT
    void	ieee80211_recv_addba_req(struct ieee80211com *, mbuf_t,
                                     struct ieee80211_node *);
    void	ieee80211_recv_addba_resp(struct ieee80211com *, mbuf_t,
                                      struct ieee80211_node *);
    void	ieee80211_recv_delba(struct ieee80211com *, mbuf_t,
                                 struct ieee80211_node *);
#endif
    void	ieee80211_recv_sa_query_req(struct ieee80211com *, mbuf_t,
                                        struct ieee80211_node *);
    void	ieee80211_recv_action(struct ieee80211com *, mbuf_t,
                                  struct ieee80211_node *);
#ifndef IEEE80211_NO_HT
    void	ieee80211_recv_bar(struct ieee80211com *, mbuf_t,
                               struct ieee80211_node *);
    void	ieee80211_bar_tid(struct ieee80211com *, struct ieee80211_node *,
                              u_int8_t, u_int16_t);
#endif
    
public:
#pragma mark I/O Kit specific
	virtual bool start(IOService* provider);
	virtual void stop(IOService* provider);
	IOReturn getHardwareAddress(IOEthernetAddress * addrP);
};

#endif
