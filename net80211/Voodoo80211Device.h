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
#define mtod(m, t) (t) mbuf_data(m)
#include <kern/assert.h>
#define KASSERT(x) assert(x)

#include <IOKit/IOService.h>

class Voodoo80211Device : public IOService
{
	OSDeclareDefaultStructors(Voodoo80211Device)
    
public:
	virtual bool start(IOService* provider);
	virtual void stop(IOService* provider);
};

#endif
