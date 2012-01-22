//
//  timeout.h
//  net80211
//
//  Copyright (c) 2012 Prashant Vaibhav. All rights reserved.
//

#ifndef net80211_VoodooTimeout_h
#define net80211_VoodooTimeout_h

#include <IOKit/IOTimerEventSource.h>
#include <libkern/c++/OSObject.h>

class VoodooTimeout : public OSObject {
	OSDeclareDefaultStructors(VoodooTimeout)
	
public:
	IOTimerEventSource* timer;
	typedef void (*CallbackFunction)(void*);
	CallbackFunction fn;
	void* arg;
};

#endif
