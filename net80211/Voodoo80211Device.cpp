//
//  Voodoo80211Device.cpp
//  net80211
//
//  Created by Prashant Vaibhav on 20/12/11.
//  Copyright (c) 2011 Prashant Vaibhav. All rights reserved.
//

#include "Voodoo80211Device.h"
#include <libkern/c++/OSString.h>
#include <IOKit/IOLib.h>

OSDefineMetaClassAndStructors(Voodoo80211Device, IO80211Controller)
OSDefineMetaClassAndStructors(VoodooTimeout, OSObject)

#define IOCTL_GET_REQ		3223611849 // magic number alert!
#define IOC_STRUCT_RET(type)	type* ret = (type*) data; ret->version = APPLE80211_VERSION;
#define IOC_STRUCT_GOT(type)	type* got = (type*) data;

IO80211WorkLoop* Voodoo80211Device::getWorkLoop() {
	return fWorkloop;
}

bool Voodoo80211Device::start(IOService* provider) {
	if (!IO80211Controller::start(provider))
		return false;
	
	IOPCIDevice* dev = OSDynamicCast(IOPCIDevice, provider);
	if (dev == 0) {
		IOLog("PCI device cannot be cast\n");
		return false;
	}
	
	dev->retain();
	dev->open(this);
	
	if (dev->requestPowerDomainState(kIOPMPowerOn,
						 (IOPowerConnection *) getParentEntry(gIOPowerPlane),
						 IOPMLowestState) != IOPMNoErr)
	{
		IOLog("Power domain D0 not received!!\n");
		return false;
	}
	
	dev->configWrite8(0x41, 0); 	/* This comes from FreeBSD driver */
	dev->setBusMasterEnable(true);
	
	fWorkloop = IO80211WorkLoop::workLoop();
	if (fWorkloop == 0) {
		IOLog("No workloop!!\n");
		return false;
	}
	fAttachArgs.workloop = fWorkloop;
	fAttachArgs.pa_tag = dev;
	
	fLock = IOSimpleLockAlloc();
	fCommandGate = IOCommandGate::commandGate(this, (IOCommandGate::Action)tsleepHandler);
	if (fCommandGate == 0) {
		IOLog("No command gate!!\n");
		return false;
	}
	fWorkloop->addEventSource(fCommandGate);
	
	if (device_attach(&fAttachArgs) == false)
		return false;
	
	registerService();
	IOLog("Starting\n");
	return true;
}

void Voodoo80211Device::stop(IOService* provider) {
	IOLog("Stopping\n");
	device_detach(0);
	fWorkloop->release();
	fWorkloop = 0;
	fAttachArgs.workloop = 0;
	fAttachArgs.pa_tag->release();
	fAttachArgs.pa_tag = 0;
	IO80211Controller::stop(provider);
}

IOReturn Voodoo80211Device::tsleepHandler(OSObject* owner, void* arg0 = 0, void* arg1 = 0, void* arg2 = 0, void* arg3 = 0) {
	Voodoo80211Device* dev = OSDynamicCast(Voodoo80211Device, owner);
	if (dev == 0)
		return kIOReturnError;
	
	if (arg1 == 0) {
		// no deadline
		if (dev->fCommandGate->commandSleep(arg0, THREAD_INTERRUPTIBLE) == THREAD_AWAKENED)
			return kIOReturnSuccess;
		else
			return kIOReturnTimeout;
	} else {
		AbsoluteTime deadline;
		clock_interval_to_deadline((*(int*)arg1), kMillisecondScale, reinterpret_cast<uint64_t*> (&deadline));
		if (dev->fCommandGate->commandSleep(arg0, deadline, THREAD_INTERRUPTIBLE) == THREAD_AWAKENED)
			return kIOReturnSuccess;
		else
			return kIOReturnTimeout;
	}
}

int Voodoo80211Device::tsleep(void *ident, int priority, const char *wmesg, int timo) {
	if (fCommandGate == 0) {
		// no command gate so we just sleep
		IOSleep(timo);
		return 0;
	}
	DPRINTF(("%s\n", wmesg));
	IOReturn ret;
	if (timo == 0) {
		ret = fCommandGate->runCommand(ident);
	} else {
		ret = fCommandGate->runCommand(ident, &timo);
	}
	if (ret == kIOReturnSuccess)
		return 0;
	else
		return 1;
}

void Voodoo80211Device::wakeupOn(void* ident) {
	if (fCommandGate == 0)
		return;
	else
		fCommandGate->commandWakeup(ident);
}

#pragma mark -
#pragma mark Apple IOCTL
SInt32 Voodoo80211Device::apple80211Request( UInt32 type, int req, IO80211Interface * intf, void * data ) {
	if (type == IOCTL_GET_REQ)
		return apple80211Request_GET(req, data);
	else
		return apple80211Request_SET(req, data);
}

IOReturn
Voodoo80211Device::apple80211Request_SET
( int request_number, void* data )
{
	int i;
	struct ieee80211com* ic = getIeee80211com();
	switch (request_number)
	{
		case APPLE80211_IOC_POWER:
		{
			IOC_STRUCT_GOT(apple80211_power_data);
			switch (got->power_state[0]) {
				case APPLE80211_POWER_ON:
					DPRINTF(("Setting power on\n"));
					device_activate(DVACT_RESUME);
					if (fInterface)
						fInterface->postMessage(APPLE80211_M_POWER_CHANGED);
					if (fOutputQueue)
						fOutputQueue->setCapacity(200); // FIXME
					return kIOReturnSuccess;
					
				case APPLE80211_POWER_OFF:
					DPRINTF(("Setting power off\n"));
					device_activate(DVACT_SUSPEND);
					if (fInterface)
						fInterface->postMessage(APPLE80211_M_POWER_CHANGED);
					if (fOutputQueue) {
						fOutputQueue->stop();
						fOutputQueue->flush();
						fOutputQueue->setCapacity(0);
					}
					return kIOReturnSuccess;
					
				default:
					return kIOReturnError;
			};
		}
			
		case APPLE80211_IOC_SCAN_REQ:
		{
			if ((ic->ic_scan_lock & IEEE80211_SCAN_REQUEST) == 0) {
				if (ic->ic_scan_lock & IEEE80211_SCAN_LOCKED)
					ic->ic_scan_lock |= IEEE80211_SCAN_RESUME;
				ic->ic_scan_lock |= IEEE80211_SCAN_REQUEST;
				if (ic->ic_state != IEEE80211_S_SCAN)
					ieee80211_newstate(ic, IEEE80211_S_SCAN, -1);
			}
			/* Let the userspace process wait for completion */
			//tsleep(&ic->ic_scan_lock, PCATCH, "80211scan", 1000 * IEEE80211_SCAN_TIMEOUT);
			return kIOReturnSuccess;
		}
			
		case APPLE80211_IOC_ASSOCIATE:
		{
			IOC_STRUCT_GOT(apple80211_assoc_data);
			if (got->ad_ssid_len > IEEE80211_NWID_LEN)
				return kIOReturnInvalid;
			memset(ic->ic_des_essid, 0, IEEE80211_NWID_LEN);
			ic->ic_des_esslen = got->ad_ssid_len;
			memcpy(ic->ic_des_essid, got->ad_ssid, got->ad_ssid_len);
			DPRINTF(("Setting desired ESSID to %s\n", ic->ic_des_essid));
			device_netreset();
			return kIOReturnSuccess;
			// TODO: prefer BSSID if it's specified, and >>set crypto keys<<
		}
			
		case APPLE80211_IOC_DISASSOCIATE:
		{
			ieee80211_newstate(ic, IEEE80211_S_INIT, 0);
			return kIOReturnSuccess;
		}
			
		case APPLE80211_IOC_TXPOWER:
			return kIOReturnSuccess; // TODO !!
			
		default:
			DPRINTF(("Unhandled Airport SET request %u\n", request_number));
			return kIOReturnUnsupported;
	};
}

static int ieeeChanFlag2apple(int flags) {
	int ret = 0;
	if (flags & IEEE80211_CHAN_2GHZ)	ret |= APPLE80211_C_FLAG_2GHZ;
	if (flags & IEEE80211_CHAN_5GHZ)	ret |= APPLE80211_C_FLAG_5GHZ;
	if (!(flags & IEEE80211_CHAN_PASSIVE))	ret |= APPLE80211_C_FLAG_ACTIVE;
	if (flags & IEEE80211_CHAN_OFDM)	ret |= APPLE80211_C_FLAG_20MHZ; // XXX ??
	if (flags & IEEE80211_CHAN_CCK)		ret |= APPLE80211_C_FLAG_10MHZ; // XXX ??
}

IOReturn
Voodoo80211Device::apple80211Request_GET
( int request_number, void* data )
{
	struct ieee80211com* ic = getIeee80211com();
	switch (request_number)
	{
		case APPLE80211_IOC_CARD_CAPABILITIES:
		{
			IOC_STRUCT_RET(apple80211_capability_data);
			
			uint32_t caps;
			
			if (ic->ic_caps & IEEE80211_C_WEP)		caps |= APPLE80211_CAP_WEP;
			if (ic->ic_caps & IEEE80211_C_RSN)		caps |=  APPLE80211_CAP_WPA2 | APPLE80211_CAP_AES_CCM; // TODO: add others
			if (ic->ic_caps & IEEE80211_C_AHDEMO)		caps |= APPLE80211_CAP_IBSS;
			if (ic->ic_caps & IEEE80211_C_MONITOR)		caps |= APPLE80211_CAP_MONITOR;
			if (ic->ic_caps & IEEE80211_C_PMGT)		caps |= APPLE80211_CAP_PMGT;
			if (ic->ic_caps & IEEE80211_C_TXPMGT)		caps |= APPLE80211_CAP_TXPMGT;
			if (ic->ic_caps & IEEE80211_C_SHSLOT)		caps |= APPLE80211_CAP_SHSLOT;
			if (ic->ic_caps & IEEE80211_C_SHPREAMBLE)	caps |= APPLE80211_CAP_SHPREAMBLE;
			if (ic->ic_caps & IEEE80211_C_QOS)		caps |= APPLE80211_CAP_WME;
			
			ret->capabilities[0] = (caps & 0xff);
			ret->capabilities[1] = (caps & 0xff00) >> 8;
			ret->capabilities[2] = (caps & 0xff0000) >> 16;
			
			return kIOReturnSuccess;
		}
			
		case APPLE80211_IOC_STATUS_DEV_NAME:
		{
			IOC_STRUCT_RET(apple80211_status_dev_data);
			strncpy((char*) ret->dev_name, "voodoowireless", sizeof("voodoowireless"));
			return kIOReturnSuccess;
		}
			
		case APPLE80211_IOC_DRIVER_VERSION:
		{
			IOC_STRUCT_RET(apple80211_version_data);
			ret->string_len = strlen("1.0d"); // FIXME
			strncpy(ret->string, "1.0d", ret->string_len);
			return kIOReturnSuccess;
		}
			
		case APPLE80211_IOC_HARDWARE_VERSION:
		{
			IOC_STRUCT_RET(apple80211_version_data);
			ret->string_len = strlen("1.0"); // FIXME
			strncpy(ret->string, "1.0", ret->string_len);
			return kIOReturnSuccess;
		}
			
		case APPLE80211_IOC_COUNTRY_CODE:
		{
			IOC_STRUCT_RET(apple80211_country_code_data);
			strncpy((char*) ret->cc, "IN ", sizeof("IN ")); // FIXME
			return kIOReturnSuccess;
		}
			
		case APPLE80211_IOC_LOCALE:
		{
			IOC_STRUCT_RET(apple80211_locale_data);
			ret->locale = APPLE80211_LOCALE_APAC; // FIXME
			return kIOReturnSuccess;
		}
			
		case APPLE80211_IOC_POWERSAVE:
		{
			IOC_STRUCT_RET(apple80211_powersave_data);
			ret->powersave_level = APPLE80211_POWERSAVE_MODE_DISABLED; // FIXME
			return kIOReturnSuccess;
		}
			
		case APPLE80211_IOC_POWER:
		{
			IOC_STRUCT_RET(apple80211_power_data);
			ret->num_radios = 1;
			ret->power_state[0] = device_powered_on() ? APPLE80211_POWER_ON : APPLE80211_POWER_OFF;
			return kIOReturnSuccess;
		}
			
		case APPLE80211_IOC_SSID:
		{
			IOC_STRUCT_RET(apple80211_ssid_data);
			ret->ssid_len = ic->ic_des_esslen;
			bcopy(ic->ic_des_essid, ret->ssid_bytes, ret->ssid_len);
			return kIOReturnSuccess;
		}
			
		case APPLE80211_IOC_BSSID:
		{
			IOC_STRUCT_RET(apple80211_bssid_data);
			if (ic->ic_bss == 0)
				return kIOReturnError;
			bcopy(ic->ic_bss->ni_bssid, &ret->bssid, APPLE80211_ADDR_LEN);
			return kIOReturnSuccess;
		}
			
		case APPLE80211_IOC_CHANNEL:
		{
			IOC_STRUCT_RET(apple80211_channel_data);
			if (ic->ic_bss == 0)
				return kIOReturnError;
			if (ic->ic_bss->ni_chan == 0)
				return kIOReturnError;
			ret->channel.channel = ieee80211_chan2ieee(ic, ic->ic_bss->ni_chan);
			ret->channel.flags = ieeeChanFlag2apple(ic->ic_bss->ni_chan->ic_flags);
			return kIOReturnSuccess;
		}
			
		case APPLE80211_IOC_SUPPORTED_CHANNELS:
		{
			IOC_STRUCT_RET(apple80211_sup_channel_data);
			ret->num_channels = 0;
			for (int i = 0; i < IEEE80211_CHAN_MAX; i++) {
				if (ic->ic_channels[i].ic_freq != 0) {
					ret->supported_channels[ret->num_channels++].channel	= ieee80211_chan2ieee(ic, &ic->ic_channels[i]);
					ret->supported_channels[ret->num_channels].flags	= ieeeChanFlag2apple(ic->ic_channels[i].ic_flags);
				}
			}
			return kIOReturnSuccess;
		}
			
		case APPLE80211_IOC_STATE:
		{
			IOC_STRUCT_RET(apple80211_state_data);
			ret->state = ic->ic_state;
			return kIOReturnSuccess;
		}
			
		case APPLE80211_IOC_PHY_MODE:
		{
			IOC_STRUCT_RET(apple80211_phymode_data);
			ret->phy_mode = APPLE80211_MODE_AUTO; // FIXME
			return kIOReturnSuccess;
		}
			
		case APPLE80211_IOC_OP_MODE:
		{
			IOC_STRUCT_RET(apple80211_opmode_data);
			ret->op_mode = APPLE80211_M_STA; // TODO monitor?
			return kIOReturnSuccess;
		}
			
		case APPLE80211_IOC_NOISE:
		{
			return kIOReturnUnsupported; // TODO
		}
			
		case APPLE80211_IOC_RSSI:
		{
			if (ic->ic_bss == 0)
				return kIOReturnError;
			IOC_STRUCT_RET(apple80211_rssi_data);
			ret->num_radios = 1;
			ret->rssi_unit	= APPLE80211_UNIT_DBM;
			ret->rssi[0]	= ret->aggregate_rssi
					= ret->rssi_ext[0]
					= ret->aggregate_rssi_ext
					= ic->ic_bss->ni_rssi;
			return kIOReturnSuccess;
		}
			
		case APPLE80211_IOC_LAST_RX_PKT_DATA:
		{
			return kIOReturnUnsupported; // TODO
		}
			
		case APPLE80211_IOC_RATE:
		{
			if (ic->ic_bss == 0)
				return kIOReturnError;
			IOC_STRUCT_RET(apple80211_rate_data);
			ret->num_radios = 1;
			ret->rate[0] = ic->ic_bss->ni_rates.rs_rates[ic->ic_bss->ni_txrate];
			return kIOReturnSuccess;
		}
			
		case APPLE80211_IOC_RATE_SET:
		{
			// TODO
			return kIOReturnUnsupported;
		}
			
		case APPLE80211_IOC_RSN_IE:
		{
			if (ic->ic_bss == 0)
				return kIOReturnError;
			IOC_STRUCT_RET(apple80211_rsn_ie_data);
			ret->len = ic->ic_bss->ni_rsnie[1]; // XXX
			bcopy(ic->ic_bss->ni_rsnie + 2, ret->ie, ret->len);
			return kIOReturnSuccess;
		}
			
		case APPLE80211_IOC_FRAG_THRESHOLD:
		{
			IOC_STRUCT_RET(apple80211_frag_threshold_data);
			ret->threshold = ic->ic_fragthreshold;
			return kIOReturnSuccess;
		}
			
		case APPLE80211_IOC_DEAUTH:
		{
			IOC_STRUCT_RET(apple80211_deauth_data);
			ret->deauth_reason = APPLE80211_REASON_UNSPECIFIED; // FIXME
			return kIOReturnSuccess;
		}
			
		case APPLE80211_IOC_AUTH_TYPE:
		{
			return kIOReturnUnsupported; // TODO
		}
			
		case APPLE80211_IOC_ASSOCIATION_STATUS:
		{
			IOC_STRUCT_RET(apple80211_assoc_status_data);
			if (ic->ic_state == IEEE80211_S_RUN) {
				ret->status = APPLE80211_STATUS_SUCCESS;
				return kIOReturnSuccess;
			} else if (ic->ic_state == IEEE80211_S_AUTH || ic->ic_state == IEEE80211_S_ASSOC) {
				ret->status = APPLE80211_STATUS_UNAVAILABLE;
				return kIOReturnBusy;
			} else {
				ret->status = APPLE80211_STATUS_UNSPECIFIED_FAILURE; // TODO: use reason code
				return kIOReturnSuccess;
			}
		}
			
		case APPLE80211_IOC_SCAN_RESULT:
		{
			struct ieee80211_node* ni = fNextNodeToSend;
			
			if (ni == 0) { // start at beginning if we're not in the middle
				if (fScanResultWrapping) {
					fScanResultWrapping = false;
					return -1; // XXX no more results
				} else {
					ni = RB_MIN(ieee80211_tree, &ic->ic_tree);
				}
			}
			
			DPRINTF(("Sending scan result for essid %s\n", ni->ni_essid));
			apple80211_scan_result** ret = (apple80211_scan_result**) data;
			apple80211_scan_result* oneResult = new apple80211_scan_result; // FIXME: huge memory leak here
			
			oneResult->asr_ssid_len = ni->ni_esslen;
			bcopy(ni->ni_essid, oneResult->asr_ssid, ni->ni_esslen);
			bcopy(ni->ni_bssid, oneResult->asr_bssid, IEEE80211_ADDR_LEN);
			oneResult->asr_rssi = ni->ni_rssi;
			oneResult->asr_age = 0;
			oneResult->asr_beacon_int = ni->ni_intval;
			oneResult->asr_channel.version = 1;
			oneResult->asr_channel.channel = ieee80211_chan2ieee(ic, ni->ni_chan);
			oneResult->asr_channel.flags = ieeeChanFlag2apple(ni->ni_chan->ic_flags);
			oneResult->asr_ie_len = ni->ni_rsnie[1];
			oneResult->asr_ie_data = &ni->ni_rsnie[2];
			oneResult->asr_nrates = ni->ni_rates.rs_nrates;
			for (int r = 0; r < oneResult->asr_nrates; r++)
				oneResult->asr_rates[r] = ni->ni_rates.rs_rates[r];
			
			*ret = oneResult;
			
			// done sending this one, now move to next for subsequent request
			ni = RB_NEXT(ieee80211_tree, &ic->ic_tree, ni);
			if (ni == 0) // if there is no next one then wrap next time
				fScanResultWrapping = true; // XXX next time signal that we got no more
			return kIOReturnSuccess;
		}
			
		case APPLE80211_IOC_MCS:
		{
			IOC_STRUCT_RET(apple80211_mcs_data);
			ret->index = APPLE80211_MCS_INDEX_AUTO;
			return kIOReturnSuccess;
		}
			
		case APPLE80211_IOC_PROTMODE:
		{
			IOC_STRUCT_RET(apple80211_protmode_data);
			ret->protmode = APPLE80211_PROTMODE_AUTO;
			ret->threshold = ic->ic_rtsthreshold; // XXX
			return kIOReturnSuccess;
		}
			
		case APPLE80211_IOC_INT_MIT:
		{
			IOC_STRUCT_RET(apple80211_intmit_data);
			ret->int_mit = APPLE80211_INT_MIT_AUTO;
			return kIOReturnSuccess;
		}
			
		default:
			DPRINTF(("Unhandled Airport GET request %u\n", request_number));
			return kIOReturnUnsupported;
	};
}



#pragma mark Compatibility functions
void* Voodoo80211Device::malloc(vm_size_t len, int type, int how) {
	// Allocate some extra space and store the length of the allocation there
	// so that we can use this to free() later
	void* addr = IOMalloc(len + sizeof(vm_size_t));
	*((vm_size_t*) addr) = len;
	return (void*)((uint8_t*)addr + sizeof(vm_size_t));
}

void Voodoo80211Device::free(void* addr) {
	// Get address of actual allocation (we prepended a vm_size_t when malloc'ing)
	void* actual_addr = (void*)((uint8_t*)addr - sizeof(vm_size_t));
	vm_size_t len = *((vm_size_t*) actual_addr); // find the length of this malloc block
	IOFree(actual_addr, len + sizeof(vm_size_t)); // free the whole thing
}

IO80211Interface* Voodoo80211Device::getInterface() {
	return fInterface;
}

void Voodoo80211Device::timeout_set(VoodooTimeout* t, VoodooTimeout::CallbackFunction fn, void* arg) {
	t = new VoodooTimeout();
	t->fn = fn;
	t->arg = arg;
}

void Voodoo80211Device::voodooTimeoutOccurred(OSObject* owner, IOTimerEventSource* timer) {
	VoodooTimeout* t = OSDynamicCast(VoodooTimeout, owner);
	t->fn(t->arg);
}

void Voodoo80211Device::timeout_add_sec(VoodooTimeout* t, const unsigned int sec) {
	timeout_add_msec(t, sec * 1000);
}

void Voodoo80211Device::timeout_add_msec(VoodooTimeout* t, const unsigned int ms) {
	t->timer = IOTimerEventSource::timerEventSource(t, OSMemberFunctionCast(IOTimerEventSource::Action, t, &Voodoo80211Device::voodooTimeoutOccurred));
	if (t->timer == 0)
		return;
	fWorkloop->addEventSource(t->timer);
	t->timer->enable();
	t->timer->setTimeoutMS(ms);
}	

void Voodoo80211Device::timeout_add_usec(VoodooTimeout* t, const unsigned int usec) {
	t->timer = IOTimerEventSource::timerEventSource(t, OSMemberFunctionCast(IOTimerEventSource::Action, t, &Voodoo80211Device::voodooTimeoutOccurred));
	if (t->timer == 0)
		return;
	fWorkloop->addEventSource(t->timer);
	t->timer->enable();
	t->timer->setTimeoutUS(usec);
}

void Voodoo80211Device::timeout_del(VoodooTimeout* t) {
	if (t == 0)
		return;
	if (t->timer == 0)
		return;
	t->timer->cancelTimeout();
	if (fWorkloop)
		fWorkloop->removeEventSource(t->timer);
	t->timer->release();
	t->timer = 0;
}

#pragma mark IOKit functionality

// Power Management
IOReturn Voodoo80211Device::registerWithPolicyMaker
( IOService* policyMaker )
{
	static IOPMPowerState powerStateArray[ 2 ] = {
		{ 1,0,0,0,0,0,0,0,0,0,0,0 },
		{ 1,kIOPMDeviceUsable,kIOPMPowerOn,kIOPMPowerOn,0,0,0,0,0,0,0,0 }
	};
	return policyMaker->registerPowerDriver( this, powerStateArray, 2 );
}

IOReturn Voodoo80211Device::enable ( IONetworkInterface* aNetif )
{
	//IOSimpleLockLock(fLock);
	device_activate(DVACT_RESUME);
	if (fInterface) fInterface->postMessage(APPLE80211_M_POWER_CHANGED);
	if (fOutputQueue) fOutputQueue->setCapacity(200); // FIXME !!!!
	//IOSimpleLockUnlock(fLock);
	return kIOReturnSuccess;
}

IOReturn Voodoo80211Device::disable( IONetworkInterface* aNetif ) {
	//IOSimpleLockLock(fLock);
	if (fOutputQueue) {
		fOutputQueue->setCapacity(0);
		fOutputQueue->flush();	
	}
	device_activate(DVACT_SUSPEND);
	if (fInterface) fInterface->postMessage(APPLE80211_M_POWER_CHANGED);
	//IOSimpleLockUnlock(fLock);
	return kIOReturnSuccess;
}

IOOutputQueue* Voodoo80211Device::createOutputQueue( ) {
	if (fOutputQueue == 0) {
		fOutputQueue = IOGatedOutputQueue::withTarget(this, getWorkLoop());
	}
	return fOutputQueue;
}

UInt32 Voodoo80211Device::outputPacket( mbuf_t m, void* param ) {
	freePacket(m);
	return kIOReturnOutputDropped;
}



//*********************************************************************************************************************
// Following functions are dummy implementations but they must succeed for the network driver to work
#pragma mark Dummy functions
//*********************************************************************************************************************
IOReturn	Voodoo80211Device::setPromiscuousMode	( IOEnetPromiscuousMode mode )		{ return kIOReturnSuccess; }
IOReturn	Voodoo80211Device::setMulticastMode	( IOEnetMulticastMode mode )		{ return kIOReturnSuccess; }
IOReturn	Voodoo80211Device::setMulticastList	( IOEthernetAddress* addr, UInt32 len )	{ return kIOReturnSuccess; }
SInt32		Voodoo80211Device::monitorModeSetEnabled	( IO80211Interface * interface,
						 bool enabled, UInt32 dlt )		{ return kIOReturnSuccess; }
//*********************************************************************************************************************
#pragma mark Quick implementation of tiny functions
//*********************************************************************************************************************
const OSString*	Voodoo80211Device::newVendorString	( ) const	{ return OSString::withCString("Voodoo(R)"); }
const OSString*	Voodoo80211Device::newModelString		( ) const	{ return OSString::withCString("Wireless Device(TM)"); }
const OSString*	Voodoo80211Device::newRevisionString	( ) const	{ return OSString::withCString("1.0"); }

int Voodoo80211Device::splnet() {
	return 0; // FIXME use an iolock!
}
void Voodoo80211Device::splx(int) {
	return;
}

IOReturn
Voodoo80211Device::getHardwareAddress
( IOEthernetAddress* addr )
{
	bcopy(getIeee80211com()->ic_myaddr, addr->bytes, 6);
	return kIOReturnSuccess;
}

IOReturn
Voodoo80211Device::getHardwareAddressForInterface
( IO80211Interface* netif, IOEthernetAddress* addr )
{
	return getHardwareAddress(addr);
}

IOReturn
Voodoo80211Device::getMaxPacketSize
( UInt32 *maxSize ) const
{
	*maxSize = 1500; // FIXME !!!!
	return kIOReturnSuccess;
}

IOBufferMemoryDescriptor* Voodoo80211Device::allocDmaMemory
( size_t size, int alignment, void** vaddr, uint32_t* paddr )
{
	size_t		reqsize;
	uint64_t	phymask;
	int		i;
	
	DPRINTF(("Asked to allocate %u bytes with align=%u\n", size, alignment));
	
	if (alignment <= PAGE_SIZE) {
		reqsize = size;
		phymask = 0x00000000ffffffffull & (~(alignment - 1));
	} else {
		reqsize = size + alignment;
		phymask = 0x00000000fffff000ull; /* page-aligned */
	}
	
	IOBufferMemoryDescriptor* mem = 0;
	mem = IOBufferMemoryDescriptor::inTaskWithPhysicalMask(kernel_task, kIOMemoryPhysicallyContiguous | kIODirectionInOut,
							       reqsize, phymask);
	if (!mem) {
		DPRINTF(("Could not allocate DMA memory\n"));
		return 0;
	}
	mem->prepare();
	*paddr = mem->getPhysicalAddress();
	*vaddr = mem->getBytesNoCopy();
	
	DPRINTF(("Got allocated at paddr=0x%x, vaddr=0x%x\n", *paddr, *vaddr));
	
	/*
	 * Check the alignment and increment by 4096 until we get the
	 * requested alignment. Fail if can't obtain the alignment
	 * we requested.
	 */
	if ((*paddr & (alignment - 1)) != 0) {
		for (i = 0; i < alignment / 4096; i++) {
			if ((*paddr & (alignment - 1 )) == 0)
				break;
			*paddr += 4096;
			*vaddr = ((uint8_t*) *vaddr) + 4096;
		}
		if (i == alignment / 4096) {
			DPRINTF(("Memory alloc alignment requirement %d was not satisfied\n", alignment));
			mem->complete();
			mem->release();
			return 0;
		}
	}
	DPRINTF(("Re-aligned DMA memory to paddr=0x%x, vaddr=0x%x\n", *paddr, *vaddr));
	return mem;
}

