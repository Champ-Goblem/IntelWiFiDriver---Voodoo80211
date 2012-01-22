//
//  Voodoo80211Device.cpp
//  net80211
//
//  Created by Prashant Vaibhav on 20/12/11.
//  Copyright (c) 2011 Prashant Vaibhav. All rights reserved.
//

#include "Voodoo80211Device.h"
#include <IOKit/IOLib.h>

OSDefineMetaClassAndStructors(Voodoo80211Device, IOEthernetController)
OSDefineMetaClassAndStructors(VoodooTimeout, OSObject)

bool MyClass::start(IOService* provider) {
	if (!super::start(provider))
		return false;
    registerService();
    IOLog("Starting\n");
    return true;
}

void MyClass::stop(IOService* provider) {
    IOLog("Stopping\n");
    super::stop(provider);
}

IOReturn MyClass::getHardwareAddress(IOEthernetAddress * addrP) {
	return kIOReturnSuccess;
}

#pragma mark Compatibility functions
void MyClass::timeout_set(VoodooTimeout* t, VoodooTimeout::CallbackFunction fn, void* arg) {
	t = new VoodooTimeout();
	t->timer = IOTimerEventSource::timerEventSource(t, OSMemberFunctionCast(IOTimerEventSource::Action, t, &MyClass::voodooTimeoutOccurred));
	t->arg = arg;
}

void MyClass::voodooTimeoutOccurred(OSObject* owner, IOTimerEventSource* timer) {
	VoodooTimeout* t = (VoodooTimeout*) owner;
	t->fn(t->arg);
}

void MyClass::timeout_add_sec(VoodooTimeout* t, const unsigned int sec) {
	fWorkloop->addEventSource(t->timer);
	t->timer->setTimeoutMS(sec * 1000);
}

void MyClass::timeout_add_usec(VoodooTimeout* t, const unsigned int usec) {
	fWorkloop->addEventSource(t->timer);
	t->timer->setTimeoutUS(usec);
}

void MyClass::timeout_del(VoodooTimeout* t) {
	t->timer->cancelTimeout();
	fWorkloop->removeEventSource(t->timer);
}