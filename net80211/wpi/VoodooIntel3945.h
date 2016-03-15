//
//  VoodooIntel3945.h
//  net80211
//
//  Copyright (c) 2012 Prashant Vaibhav. All rights reserved.
//

#ifndef net80211_VoodooIntel3945_h
#define net80211_VoodooIntel3945_h

#include <net80211/Voodoo80211Device.h>
#include "if_wpireg.h"
#include "if_wpivar.h"

class VoodooIntel3945 : public Voodoo80211Device
{
	OSDeclareDefaultStructors(VoodooIntel3945)
	
	
protected:
	// Overridden functions
	virtual struct ieee80211_node *ieee80211_node_alloc(struct ieee80211com *);
	virtual void	ieee80211_newassoc(struct ieee80211com *, struct ieee80211_node *, int);
	virtual int	ieee80211_newstate(struct ieee80211com *, enum ieee80211_state, int);
	virtual void	ieee80211_updateedca(struct ieee80211com *);
	virtual int	ieee80211_set_key(struct ieee80211com *, struct ieee80211_node *, struct ieee80211_key *);
	virtual void	ieee80211_delete_key(struct ieee80211com *, struct ieee80211_node *, struct ieee80211_key *);
	virtual bool	device_attach(void *);
	virtual int	device_detach(int);
	virtual int	device_activate(int);
	virtual void	device_netreset();
	virtual bool	device_powered_on();
	virtual struct ieee80211com* getIeee80211com();
	virtual UInt32		outputPacket		( mbuf_t m, void* param );
	
private:
	wpi_softc	fSelfData;
	bool		fPoweredOn;
	IOInterruptEventSource* fInterrupt;
	IOMemoryMap*	fMap;
	void		wpi_resume();
	int		wpi_nic_lock(struct wpi_softc *);
	int		wpi_read_prom_data(struct wpi_softc *, uint32_t, void *, int);
	int		wpi_dma_contig_alloc(bus_dma_tag_t, struct wpi_dma_info *, void **, bus_size_t, bus_size_t);
	void		wpi_dma_contig_free(struct wpi_dma_info *);
	int		wpi_alloc_shared(struct wpi_softc *);
	void		wpi_free_shared(struct wpi_softc *);
	int		wpi_alloc_fwmem(struct wpi_softc *);
	void		wpi_free_fwmem(struct wpi_softc *);
	int		wpi_alloc_rx_ring(struct wpi_softc *, struct wpi_rx_ring *);
	void		wpi_reset_rx_ring(struct wpi_softc *, struct wpi_rx_ring *);
	void		wpi_free_rx_ring(struct wpi_softc *, struct wpi_rx_ring *);
	int		wpi_alloc_tx_ring(struct wpi_softc *, struct wpi_tx_ring *, int);
	void		wpi_reset_tx_ring(struct wpi_softc *, struct wpi_tx_ring *);
	void		wpi_free_tx_ring(struct wpi_softc *, struct wpi_tx_ring *);
	int		wpi_read_eeprom(struct wpi_softc *);
	void		wpi_read_eeprom_channels(struct wpi_softc *, int);
	void		wpi_read_eeprom_group(struct wpi_softc *, int);
	int		wpi_media_change();
	void		wpi_iter_func(void *, struct ieee80211_node *);
	void		wpi_calib_timeout(void *);
	int		wpi_ccmp_decap(struct wpi_softc *, mbuf_t, struct ieee80211_key *);
	void		wpi_rx_done(struct wpi_softc *, struct wpi_rx_desc *, struct wpi_rx_data *);
	void		wpi_tx_done(struct wpi_softc *, struct wpi_rx_desc *);
	void		wpi_cmd_done(struct wpi_softc *, struct wpi_rx_desc *);
	void		wpi_notif_intr(struct wpi_softc *);
	void		wpi_fatal_intr(struct wpi_softc *);
	int		wpi_intr(OSObject *ih, IOInterruptEventSource *, int count);
	int		wpi_tx(struct wpi_softc *, mbuf_t, struct ieee80211_node *);
	void		wpi_start();
	void		wpi_watchdog();
	int		wpi_ioctl(struct ieee80211com *, u_long, caddr_t);
	int		wpi_cmd(struct wpi_softc *, int, const void *, int, int);
	int		wpi_mrr_setup(struct wpi_softc *);
	void		wpi_set_led(struct wpi_softc *, uint8_t, uint8_t, uint8_t);
	int		wpi_set_timing(struct wpi_softc *, struct ieee80211_node *);
	void		wpi_power_calibration(struct wpi_softc *);
	int		wpi_set_txpower(struct wpi_softc *, int);
	int		wpi_get_power_index(struct wpi_softc *, struct wpi_power_group *, struct ieee80211_channel *, int);
	int		wpi_set_pslevel(struct wpi_softc *, int, int, int);
	int		wpi_config(struct wpi_softc *);
	int		wpi_scan(struct wpi_softc *, uint16_t);
	int		wpi_auth(struct wpi_softc *);
	int		wpi_run(struct wpi_softc *);
	int		wpi_post_alive(struct wpi_softc *);
	int		wpi_load_bootcode(struct wpi_softc *, const uint8_t *, int);
	int		wpi_load_firmware(struct wpi_softc *);
	int		wpi_read_firmware(struct wpi_softc *);
	int		wpi_clock_wait(struct wpi_softc *);
	int		wpi_apm_init(struct wpi_softc *);
	void		wpi_apm_stop_master(struct wpi_softc *);
	void		wpi_apm_stop(struct wpi_softc *);
	void		wpi_nic_config(struct wpi_softc *);
	int		wpi_hw_init(struct wpi_softc *);
	void		wpi_hw_stop(struct wpi_softc *);
	int		wpi_init();
	void		wpi_stop(int);
};

#endif
