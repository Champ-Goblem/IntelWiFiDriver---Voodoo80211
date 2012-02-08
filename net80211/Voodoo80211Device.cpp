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
	
	fAttachArgs.workloop = getWorkLoop();
	fWorkloop = OSDynamicCast(IO80211WorkLoop, fAttachArgs.workloop);
	fWorkloop->retain();
	fAttachArgs.pa_tag = dev;
	
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

SInt32 Voodoo80211Device::apple80211Request( UInt32 req, int type, IO80211Interface * intf, void * data ) {
	return 0;
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
	t->timer->cancelTimeout();
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
	device_activate(DVACT_RESUME);
	fInterface->postMessage(APPLE80211_M_POWER_CHANGED);
	fOutputQueue->setCapacity(200); // FIXME !!!!
	return kIOReturnSuccess;
}

IOReturn Voodoo80211Device::disable( IONetworkInterface* aNetif ) {
	fOutputQueue->setCapacity(0);
	fOutputQueue->flush();
	device_activate(DVACT_SUSPEND);
	fInterface->postMessage(APPLE80211_M_POWER_CHANGED);
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
