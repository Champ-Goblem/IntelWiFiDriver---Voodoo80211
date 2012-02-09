//
//  VoodooIntel3945.cpp
//  net80211
//
//  Copyright (c) 2012 Prashant Vaibhav. All rights reserved.
//

#include "VoodooIntel3945.h"

OSDefineMetaClassAndStructors(VoodooIntel3945, Voodoo80211Device)

struct ieee80211com* VoodooIntel3945::getIeee80211com() {
	return &fSelfData.sc_ic;
}

void VoodooIntel3945::device_netreset() {
	wpi_stop(0);
	wpi_init();
}

bool VoodooIntel3945::device_powered_on() {
	return fPoweredOn;
}

UInt32 VoodooIntel3945::outputPacket( mbuf_t m, void* param ) {
	struct wpi_softc* sc = &fSelfData;
	struct ieee80211com* ic = &sc->sc_ic;
	struct ieee80211_node *ni;
	
	DPRINTF(("TX: mbuf len=%u\n", mbuf_len(m)));
	
	if (m == 0)
		return kIOReturnOutputDropped; // ???
	
	ExtraMbufParams* p = (ExtraMbufParams*)param;
	if (p->is80211ManagementFrame) {
		ni = (struct ieee80211_node *)mbuf_pkthdr_rcvif(m);
	} else {
		if (sc->qfullmsk != 0) {
			freePacket(m);
			return kIOReturnOutputDropped;
		}
		
		if (ic->ic_state != IEEE80211_S_RUN) {
			freePacket(m);
			return kIOReturnOutputDropped;
		}
		
		/* Encapsulate and send data frames. */
		if ((m = ieee80211_encap(ic, m, &ni)) == NULL)
			return kIOReturnOutputDropped;
	}
				
	if (wpi_tx(sc, m, ni) != 0) {
		ieee80211_release_node(ic, ni);
		if (m) freePacket(m);
		return kIOReturnOutputDropped;
	} else {
		DPRINTF(("(prev tx success)\n"));
		sc->sc_tx_timer = 5;
		return kIOReturnOutputSuccess;
	}
}