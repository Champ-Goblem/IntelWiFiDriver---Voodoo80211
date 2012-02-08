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