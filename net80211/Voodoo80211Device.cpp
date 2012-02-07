//
//  Voodoo80211Device.cpp
//  net80211
//
//  Created by Prashant Vaibhav on 20/12/11.
//  Copyright (c) 2011 Prashant Vaibhav. All rights reserved.
//

#include "Voodoo80211Device.h"
#include <IOKit/IOLib.h>

OSDefineMetaClassAndStructors(Voodoo80211Device, IO80211Controller)
OSDefineMetaClassAndStructors(VoodooTimeout, OSObject)

bool Voodoo80211Device::start(IOService* provider) {
	if (!IO80211Controller::start(provider))
		return false;
    registerService();
    IOLog("Starting\n");
    return true;
}

void Voodoo80211Device::stop(IOService* provider) {
    IOLog("Stopping\n");
    IO80211Controller::stop(provider);
}

IOReturn Voodoo80211Device::getHardwareAddress(IOEthernetAddress * addrP) {
	bcopy(fPriv->ic_myaddr, addrP, ETHER_ADDR_LEN);
	return kIOReturnSuccess;
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
	timeout_add_ms(t, sec * 1000);
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