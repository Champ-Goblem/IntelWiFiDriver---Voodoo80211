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
#define __packed	__attribute__((__packed__))
#define mtod(m, t)      (t) mbuf_data(m)
#define M_DEVBUF        2
enum { DVACT_SUSPEND, DVACT_RESUME };

#include <IOKit/IOService.h>
#include <IOKit/network/IOEthernetInterface.h>
#include <IOKit/IOWorkloop.h>
#include <IOKit/network/IOGatedOutputQueue.h>
#include <IOKit/IOTimerEventSource.h>
#include <IOKit/IOLocks.h>

#include "ieee80211.h"
#include "ieee80211_priv.h"
#include "ieee80211_var.h"
#include "ieee80211_amrr.h"
#include "VoodooTimeout.h"

#include "apple80211/Lion/IO80211Controller.h"
#include "apple80211/Lion/IO80211Interface.h"
#include "apple80211/Lion/IO80211WorkLoop.h"

#include "compat.h"

struct ExtraMbufParams {
	bool	is80211ManagementFrame;
};

const ExtraMbufParams ieee80211_is_mgmt_frame = { true };

class Voodoo80211Device : public IO80211Controller
{
	OSDeclareDefaultStructors(Voodoo80211Device)
	
public:
	static IOReturn		tsleepHandler(OSObject* owner, void* arg0, void* arg1, void* arg2, void* arg3);
#pragma mark I/O Kit specific
	bool			start(IOService* provider);
	void			stop(IOService* provider);
	virtual IO80211WorkLoop* getWorkLoop() ;
	IOReturn		registerWithPolicyMaker	( IOService* policyMaker );
	SInt32			apple80211Request	( UInt32 request_type, int request_number, IO80211Interface* interface, void* data );
	IOReturn		apple80211Request_SET	( int request_number, void* data );
	IOReturn		apple80211Request_GET	( int request_number, void* data );
	IOReturn		enable			( IONetworkInterface* aNetif );
	IOReturn		disable			( IONetworkInterface* aNetif );
	IOOutputQueue*		createOutputQueue	( );
	virtual UInt32		outputPacket		( mbuf_t m, void* param );
	IOReturn		getMaxPacketSize	( UInt32 *maxSize ) const;
	const OSString*		newVendorString		( ) const;
	const OSString*		newModelString		( ) const;
	const OSString*		newRevisionString	( ) const;
	IOReturn		getHardwareAddressForInterface( IO80211Interface* netif, IOEthernetAddress* addr );
	IOReturn		getHardwareAddress	( IOEthernetAddress* addr );
	virtual IOReturn	setPromiscuousMode	( IOEnetPromiscuousMode mode );
	virtual IOReturn	setMulticastMode	( IOEnetMulticastMode mode );
	virtual IOReturn	setMulticastList	( IOEthernetAddress* addr, UInt32 len );
	virtual SInt32		monitorModeSetEnabled	( IO80211Interface * interface, bool enabled, UInt32 dlt );
	
private:
#pragma mark Private data
	IO80211Interface*	fInterface;
	IO80211WorkLoop*	fWorkloop;
	IOCommandGate*		fCommandGate;
	IOTimerEventSource*     fTimer;
	IOGatedOutputQueue*	fOutputQueue;
	struct pci_attach_args	fAttachArgs;
	struct ieee80211_node*	fNextNodeToSend; // as scan result
	bool			fScanResultWrapping;
	IOSimpleLock*	fLock; // for enable()

protected:
#pragma mark Protected data
	IO80211Interface* getInterface();

#pragma mark Compatibility functions
	void*	malloc(vm_size_t len, int type, int how);
	void	free(void* addr);
	int	tsleep(void *ident, int priority, const char *wmesg, int timo);
	void	wakeupOn(void* ident);
	int     splnet();
	void    splx(int);
	void	voodooTimeoutOccurred(OSObject* owner, IOTimerEventSource* timer);
	IOBufferMemoryDescriptor* allocDmaMemory	( size_t size, int alignment, void** vaddr, uint32_t* paddr );
	void	timeout_set(VoodooTimeout*, void (*func)(void *), void* arg);
	void	timeout_add_sec(VoodooTimeout*, const unsigned int sec);
	void	timeout_add_msec(VoodooTimeout*, const unsigned int ms);
	void	timeout_add_usec(VoodooTimeout*, const unsigned int usec);
	void	timeout_del(VoodooTimeout* t);
	
#pragma mark Device routines to be implemented
	virtual bool	device_attach(void *) { return false; }
	virtual int	device_detach(int) { return 1; }
	virtual int	device_activate(int) { return 1; }
	virtual void	device_netreset() { return; }
	virtual bool	device_powered_on() { return false; }
	
#pragma mark ieee80211_amrr.h
	void	ieee80211_amrr_node_init(const struct ieee80211_amrr *, struct ieee80211_amrr_node *);
	void	ieee80211_amrr_choose(struct ieee80211_amrr *, struct ieee80211_node *, struct ieee80211_amrr_node *);
	
#pragma mark ieee80211_var.h
	// Overloadable functions from ieee80211com*
	virtual struct ieee80211com* getIeee80211com() { return 0; }
	virtual void	ieee80211_newassoc(struct ieee80211com *, struct ieee80211_node *, int) {}
	virtual void	ieee80211_updateslot(struct ieee80211com *) {}
	virtual void	ieee80211_updateedca(struct ieee80211com *) {}
	virtual void	ieee80211_set_tim(struct ieee80211com *, int, int) {}
	virtual int	ieee80211_ampdu_tx_start(struct ieee80211com *, struct ieee80211_node *, u_int8_t) { return 0; }
	virtual void	ieee80211_ampdu_tx_stop(struct ieee80211com *, struct ieee80211_node *, u_int8_t) {}
	virtual int	ieee80211_ampdu_rx_start(struct ieee80211com *, struct ieee80211_node *, u_int8_t) { return 0; }
	virtual void	ieee80211_ampdu_rx_stop(struct ieee80211com *, struct ieee80211_node *, u_int8_t) {}
	void	ieee80211_ifattach(struct ieee80211com *);
	void	ieee80211_ifdetach(struct ieee80211com *);
	void	ieee80211_media_init(struct ieee80211com */*, ifm_change_cb_t, ifm_stat_cb_t*/);
	int     ieee80211_media_change(struct ieee80211com *);
	void	ieee80211_media_status(struct ieee80211com *ic, IONetworkMedium* imr);
	int     ieee80211_ioctl(struct ifnet *, u_long, caddr_t);
	int     ieee80211_get_rate(struct ieee80211com *);
	void	ieee80211_watchdog(struct ieee80211com *);
	int     ieee80211_fix_rate(struct ieee80211com *, struct ieee80211_node *, int);
	int     ieee80211_rate2media(struct ieee80211com *, int, enum ieee80211_phymode);
	int     ieee80211_media2rate(int);
	u_int8_t ieee80211_rate2plcp(u_int8_t, enum ieee80211_phymode);
	u_int8_t ieee80211_plcp2rate(u_int8_t, enum ieee80211_phymode);
	u_int	ieee80211_mhz2ieee(u_int, u_int);
	u_int	ieee80211_chan2ieee(struct ieee80211com *, const struct ieee80211_channel *);
	u_int	ieee80211_ieee2mhz(u_int, u_int);
	int     ieee80211_setmode(struct ieee80211com *, enum ieee80211_phymode);
	enum ieee80211_phymode ieee80211_next_mode(struct ieee80211com *);
	enum ieee80211_phymode ieee80211_chan2mode(struct ieee80211com *, const struct ieee80211_channel *);

#pragma mark ieee80211_node.h
	// cpp functions
	virtual struct ieee80211_node *ieee80211_node_alloc(struct ieee80211com *);
	virtual void ieee80211_node_free(struct ieee80211com *, struct ieee80211_node *);
	virtual void ieee80211_node_copy(struct ieee80211com *, struct ieee80211_node *, const struct ieee80211_node *);
	void    ieee80211_choose_rsnparams(struct ieee80211com *);
	virtual u_int8_t ieee80211_node_getrssi(struct ieee80211com *, const struct ieee80211_node *);
	void    ieee80211_setup_node(struct ieee80211com *, struct ieee80211_node *, const u_int8_t *);
	void    ieee80211_free_node(struct ieee80211com *, struct ieee80211_node *);
	struct  ieee80211_node *ieee80211_alloc_node_helper(struct ieee80211com *);
	void    ieee80211_node_cleanup(struct ieee80211com *, struct ieee80211_node *);
	void    ieee80211_needs_auth(struct ieee80211com *, struct ieee80211_node *);
	// header functions
	void    ieee80211_node_attach(struct ieee80211com *);
	void    ieee80211_node_lateattach(struct ieee80211com *);
	void    ieee80211_node_detach(struct ieee80211com *);
	void    ieee80211_begin_scan(struct ieee80211com *);
	void    ieee80211_next_scan(struct ieee80211com *);
	void    ieee80211_end_scan(struct ieee80211com *);
	void    ieee80211_reset_scan(struct ieee80211com *);
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
	virtual void ieee80211_node_join(struct ieee80211com *, struct ieee80211_node *, int) {}
	virtual void ieee80211_node_leave(struct ieee80211com *, struct ieee80211_node *) {}
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
	void	ieee80211_crypto_attach(struct ieee80211com *);
	void	ieee80211_crypto_detach(struct ieee80211com *);
	struct	ieee80211_key *ieee80211_get_txkey(struct ieee80211com *, const struct ieee80211_frame *, struct ieee80211_node *);
	struct	ieee80211_key *ieee80211_get_rxkey(struct ieee80211com *, mbuf_t, struct ieee80211_node *);
	mbuf_t	ieee80211_encrypt(struct ieee80211com *, mbuf_t, struct ieee80211_key *);
	mbuf_t	ieee80211_decrypt(struct ieee80211com *, mbuf_t, struct ieee80211_node *);
	virtual int ieee80211_set_key(struct ieee80211com *, struct ieee80211_node *, struct ieee80211_key *);
	virtual void ieee80211_delete_key(struct ieee80211com *, struct ieee80211_node *, struct ieee80211_key *);
	void	ieee80211_eapol_key_mic(struct ieee80211_eapol_key *, const u_int8_t *);
	int     ieee80211_eapol_key_check_mic(struct ieee80211_eapol_key *, const u_int8_t *);
	int     ieee80211_eapol_key_decrypt(struct ieee80211_eapol_key *, const u_int8_t *);
	struct	ieee80211_pmk *ieee80211_pmksa_add(struct ieee80211com *, enum ieee80211_akm, const u_int8_t *, const u_int8_t *, u_int32_t);
	struct	ieee80211_pmk *ieee80211_pmksa_find(struct ieee80211com *, struct ieee80211_node *, const u_int8_t *);
	void	ieee80211_derive_ptk(enum ieee80211_akm, const u_int8_t *, const u_int8_t *, const u_int8_t *, const u_int8_t *, const u_int8_t *, struct ieee80211_ptk *);
	int     ieee80211_cipher_keylen(enum ieee80211_cipher);
	int     ieee80211_wep_set_key(struct ieee80211com *, struct ieee80211_key *) { return 1; }
	void	ieee80211_wep_delete_key(struct ieee80211com *, struct ieee80211_key *) { return; }
	mbuf_t	ieee80211_wep_encrypt(struct ieee80211com *, mbuf_t, struct ieee80211_key *) { return 0; }
	mbuf_t	ieee80211_wep_decrypt(struct ieee80211com *, mbuf_t, struct ieee80211_key *) { return 0; }
	int     ieee80211_tkip_set_key(struct ieee80211com *, struct ieee80211_key *) { return 1; }
	void	ieee80211_tkip_delete_key(struct ieee80211com *, struct ieee80211_key *) { return; }
	mbuf_t	ieee80211_tkip_encrypt(struct ieee80211com *, mbuf_t, struct ieee80211_key *) { return 0; }
	mbuf_t	ieee80211_tkip_decrypt(struct ieee80211com *, mbuf_t, struct ieee80211_key *) { return 0; }
	void	ieee80211_tkip_mic(mbuf_t, int, const u_int8_t *, u_int8_t[IEEE80211_TKIP_MICLEN]) { return; }
	void	ieee80211_michael_mic_failure(struct ieee80211com *, u_int64_t) { return; }
	int     ieee80211_ccmp_set_key(struct ieee80211com *, struct ieee80211_key *);
	void	ieee80211_ccmp_delete_key(struct ieee80211com *, struct ieee80211_key *);
	mbuf_t	ieee80211_ccmp_encrypt(struct ieee80211com *, mbuf_t, struct ieee80211_key *);
	mbuf_t	ieee80211_ccmp_decrypt(struct ieee80211com *, mbuf_t, struct ieee80211_key *);
	int     ieee80211_bip_set_key(struct ieee80211com *, struct ieee80211_key *) { return 1; }
	void	ieee80211_bip_delete_key(struct ieee80211com *, struct ieee80211_key *) { return; }
	mbuf_t	ieee80211_bip_encap(struct ieee80211com *, mbuf_t, struct ieee80211_key *) { return 0; }
	mbuf_t	ieee80211_bip_decap(struct ieee80211com *, mbuf_t, struct ieee80211_key *) { return 0; }

#pragma mark ieee80211.cpp
	// cpp file
	void	ieee80211_setbasicrates(struct ieee80211com *);
	int	ieee80211_findrate(struct ieee80211com *, enum ieee80211_phymode, int);
	
#pragma mark ieee80211_input.cpp
	// cpp file
	mbuf_t	ieee80211_defrag(struct ieee80211com *, mbuf_t, int);
	void	ieee80211_defrag_timeout(void *);
	#ifndef IEEE80211_NO_HT
	void	ieee80211_input_ba(struct ieee80211com *, mbuf_t, struct ieee80211_node *, int, struct ieee80211_rxinfo *);
	void	ieee80211_ba_move_window(struct ieee80211com *, struct ieee80211_node *, u_int8_t, u_int16_t);
	#endif
	mbuf_t	ieee80211_align_mbuf(mbuf_t);
	void	ieee80211_decap(struct ieee80211com *, mbuf_t, struct ieee80211_node *, int);
	#ifndef IEEE80211_NO_HT
	void	ieee80211_amsdu_decap(struct ieee80211com *, mbuf_t, struct ieee80211_node *, int);
	#endif
	void	ieee80211_deliver_data(struct ieee80211com *, mbuf_t, struct ieee80211_node *);
	int	ieee80211_parse_edca_params_body(struct ieee80211com *, const u_int8_t *);
	int	ieee80211_parse_edca_params(struct ieee80211com *, const u_int8_t *);
	int	ieee80211_parse_wmm_params(struct ieee80211com *, const u_int8_t *);
	enum	ieee80211_cipher ieee80211_parse_rsn_cipher(const u_int8_t[]);
	enum	ieee80211_akm ieee80211_parse_rsn_akm(const u_int8_t[]);
	int	ieee80211_parse_rsn_body(struct ieee80211com *, const u_int8_t *, u_int, struct ieee80211_rsnparams *);
	int	ieee80211_save_ie(const u_int8_t *, u_int8_t **);
	void	ieee80211_recv_probe_resp(struct ieee80211com *, mbuf_t, struct ieee80211_node *, struct ieee80211_rxinfo *, int);
	void	ieee80211_recv_auth(struct ieee80211com *, mbuf_t, struct ieee80211_node *, struct ieee80211_rxinfo *);
	void	ieee80211_recv_assoc_resp(struct ieee80211com *, mbuf_t, struct ieee80211_node *, int);
	void	ieee80211_recv_deauth(struct ieee80211com *, mbuf_t, struct ieee80211_node *);
	void	ieee80211_recv_disassoc(struct ieee80211com *, mbuf_t, struct ieee80211_node *);
	#ifndef IEEE80211_NO_HT
	void	ieee80211_recv_addba_req(struct ieee80211com *, mbuf_t, struct ieee80211_node *);
	void	ieee80211_recv_addba_resp(struct ieee80211com *, mbuf_t, struct ieee80211_node *);
	void	ieee80211_recv_delba(struct ieee80211com *, mbuf_t,  struct ieee80211_node *);
	#endif
	void	ieee80211_recv_sa_query_req(struct ieee80211com *, mbuf_t, struct ieee80211_node *);
	void	ieee80211_recv_action(struct ieee80211com *, mbuf_t, struct ieee80211_node *);
	#ifndef IEEE80211_NO_HT
	void	ieee80211_recv_bar(struct ieee80211com *, mbuf_t, struct ieee80211_node *);
	void	ieee80211_bar_tid(struct ieee80211com *, struct ieee80211_node *, u_int8_t, u_int16_t);
	#endif
	
#pragma mark ieee80211_output.cpp
	// cpp file
	int	ieee80211_classify(struct ieee80211com *, mbuf_t);
	int	ieee80211_mgmt_output(struct ieee80211com *, struct ieee80211_node *, mbuf_t, int);
	u_int8_t *ieee80211_add_rsn_body(u_int8_t *, struct ieee80211com *, const struct ieee80211_node *, int);
	mbuf_t	ieee80211_getmgmt(int, int, u_int);
	mbuf_t	ieee80211_get_probe_req(struct ieee80211com *, struct ieee80211_node *);
	mbuf_t	ieee80211_get_auth(struct ieee80211com *, struct ieee80211_node *, u_int16_t, u_int16_t);
	mbuf_t	ieee80211_get_deauth(struct ieee80211com *, struct ieee80211_node *, u_int16_t);
	mbuf_t	ieee80211_get_assoc_req(struct ieee80211com *, struct ieee80211_node *, int);
	mbuf_t	ieee80211_get_disassoc(struct ieee80211com *, struct ieee80211_node *, u_int16_t);
#ifndef IEEE80211_NO_HT
	mbuf_t	ieee80211_get_addba_req(struct ieee80211com *, struct ieee80211_node *, u_int8_t);
	mbuf_t	ieee80211_get_addba_resp(struct ieee80211com *, struct ieee80211_node *, u_int8_t, u_int8_t, u_int16_t);
	mbuf_t	ieee80211_get_delba(struct ieee80211com *, struct ieee80211_node *, u_int8_t, u_int8_t, u_int16_t);
#endif
	mbuf_t	ieee80211_get_sa_query(struct ieee80211com *, struct ieee80211_node *, u_int8_t);
	mbuf_t	ieee80211_get_action(struct ieee80211com *, struct ieee80211_node *, u_int8_t, u_int8_t, int);
	
#pragma mark ieee80211_proto.h
	void	ieee80211_proto_attach(struct ieee80211com *);
	void	ieee80211_proto_detach(struct ieee80211com *);
	void	ieee80211_set_link_state(struct ieee80211com *, IO80211LinkState);
	u_int	ieee80211_get_hdrlen(const struct ieee80211_frame *);
	void	ieee80211_input(struct ieee80211com *, mbuf_t, struct ieee80211_node *, struct ieee80211_rxinfo *);
	int	ieee80211_output(struct ieee80211com *, mbuf_t, struct sockaddr *, struct rtentry *);
	virtual void ieee80211_recv_mgmt(struct ieee80211com *, mbuf_t, struct ieee80211_node *, struct ieee80211_rxinfo *, int);
	virtual int ieee80211_send_mgmt(struct ieee80211com *, struct ieee80211_node *, int, int, int);
	void	ieee80211_eapol_key_input(struct ieee80211com *, mbuf_t, struct ieee80211_node *);
	mbuf_t	ieee80211_encap(struct ieee80211com *, mbuf_t, struct ieee80211_node **);
	mbuf_t	ieee80211_get_rts(struct ieee80211com *, const struct ieee80211_frame *, u_int16_t);
	mbuf_t	ieee80211_get_cts_to_self(struct ieee80211com *, u_int16_t);
	mbuf_t	ieee80211_beacon_alloc(struct ieee80211com *, struct ieee80211_node *);
	// FIXME doesnt need to be here int	ieee80211_save_ie(const u_int8_t *, u_int8_t **);
	void	ieee80211_eapol_timeout(void *);
	int	ieee80211_send_4way_msg1(struct ieee80211com *, struct ieee80211_node *);
	int	ieee80211_send_4way_msg2(struct ieee80211com *, struct ieee80211_node *, const u_int8_t *, const struct ieee80211_ptk *);
	int	ieee80211_send_4way_msg3(struct ieee80211com *, struct ieee80211_node *);
	int	ieee80211_send_4way_msg4(struct ieee80211com *, struct ieee80211_node *);
	int	ieee80211_send_group_msg1(struct ieee80211com *, struct ieee80211_node *);
	int	ieee80211_send_group_msg2(struct ieee80211com *, struct ieee80211_node *, const struct ieee80211_key *);
	int	ieee80211_send_eapol_key_req(struct ieee80211com *, struct ieee80211_node *, u_int16_t, u_int64_t);
	int	ieee80211_pwrsave(struct ieee80211com *, mbuf_t, struct ieee80211_node *);
	enum ieee80211_edca_ac ieee80211_up_to_ac(struct ieee80211com *, int);
	u_int8_t *ieee80211_add_capinfo(u_int8_t *, struct ieee80211com *, const struct ieee80211_node *);
	u_int8_t *ieee80211_add_ssid(u_int8_t *, const u_int8_t *, u_int);
	u_int8_t *ieee80211_add_rates(u_int8_t *, const struct ieee80211_rateset *);
	u_int8_t *ieee80211_add_fh_params(u_int8_t *, struct ieee80211com *, const struct ieee80211_node *);
	u_int8_t *ieee80211_add_ds_params(u_int8_t *, struct ieee80211com *, const struct ieee80211_node *);
	u_int8_t *ieee80211_add_tim(u_int8_t *, struct ieee80211com *);
	u_int8_t *ieee80211_add_ibss_params(u_int8_t *, const struct ieee80211_node *);
	u_int8_t *ieee80211_add_edca_params(u_int8_t *, struct ieee80211com *);
	u_int8_t *ieee80211_add_erp(u_int8_t *, struct ieee80211com *);
	u_int8_t *ieee80211_add_qos_capability(u_int8_t *, struct ieee80211com *);
	u_int8_t *ieee80211_add_rsn(u_int8_t *, struct ieee80211com *, const struct ieee80211_node *);
	u_int8_t *ieee80211_add_wpa(u_int8_t *, struct ieee80211com *, const struct ieee80211_node *);
	u_int8_t *ieee80211_add_xrates(u_int8_t *, const struct ieee80211_rateset *);
	u_int8_t *ieee80211_add_htcaps(u_int8_t *, struct ieee80211com *);
	u_int8_t *ieee80211_add_htop(u_int8_t *, struct ieee80211com *);
	u_int8_t *ieee80211_add_tie(u_int8_t *, u_int8_t, u_int32_t);
	int	ieee80211_parse_rsn(struct ieee80211com *, const u_int8_t *, struct ieee80211_rsnparams *);
	int	ieee80211_parse_wpa(struct ieee80211com *, const u_int8_t *, struct ieee80211_rsnparams *);
	void	ieee80211_print_essid(const u_int8_t *, int);
#ifdef IEEE80211_DEBUG
	void	ieee80211_dump_pkt(const u_int8_t *, int, int, int);
#endif
	int	ieee80211_ibss_merge(struct ieee80211com *, struct ieee80211_node *, u_int64_t);
	void	ieee80211_reset_erp(struct ieee80211com *);
	void	ieee80211_set_shortslottime(struct ieee80211com *, int);
	void	ieee80211_auth_open(struct ieee80211com *, const struct ieee80211_frame *, struct ieee80211_node *, struct ieee80211_rxinfo *rs, u_int16_t, u_int16_t);
	void	ieee80211_gtk_rekey_timeout(void *);
	int	ieee80211_keyrun(struct ieee80211com *, u_int8_t *);
	void	ieee80211_setkeys(struct ieee80211com *);
	void	ieee80211_setkeysdone(struct ieee80211com *);
	void	ieee80211_sa_query_timeout(void *);
	void	ieee80211_sa_query_request(struct ieee80211com *, struct ieee80211_node *);
#ifndef IEEE80211_NO_HT
	void	ieee80211_tx_ba_timeout(void *);
	void	ieee80211_rx_ba_timeout(void *);
	int	ieee80211_addba_request(struct ieee80211com *, struct ieee80211_node *,  u_int16_t, u_int8_t);
	void	ieee80211_delba_request(struct ieee80211com *, struct ieee80211_node *, u_int16_t, u_int8_t, u_int8_t);
#endif
	// cpp file
	virtual int ieee80211_newstate(struct ieee80211com *, enum ieee80211_state, int);
	
#pragma mark ieee80211_pae_input.cpp
	void	ieee80211_recv_4way_msg1(struct ieee80211com *, struct ieee80211_eapol_key *, struct ieee80211_node *);
	void	ieee80211_recv_4way_msg3(struct ieee80211com *, struct ieee80211_eapol_key *, struct ieee80211_node *);
	void	ieee80211_recv_rsn_group_msg1(struct ieee80211com *, struct ieee80211_eapol_key *, struct ieee80211_node *);
	void	ieee80211_recv_wpa_group_msg1(struct ieee80211com *, struct ieee80211_eapol_key *, struct ieee80211_node *);
	
#pragma mark ieee80211_pae_output.cpp
	int	ieee80211_send_eapol_key(struct ieee80211com *, mbuf_t, struct ieee80211_node *, const struct ieee80211_ptk *);
	mbuf_t	ieee80211_get_eapol_key(int, int, u_int);
};

#endif
