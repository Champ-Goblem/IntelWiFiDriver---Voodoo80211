//
//  Voodoo80211Device.cpp
//  net80211
//
//  Created by Prashant Vaibhav on 20/12/11.
//  Copyright (c) 2011 Prashant Vaibhav. All rights reserved.
//

#include "Voodoo80211Device.h"
#include <IOKit/IOLib.h>
#define MyClass Voodoo80211Device
#define super   IOService

OSDefineMetaClassAndStructors(Voodoo80211Device, IOService)

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