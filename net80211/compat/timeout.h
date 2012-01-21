//
//  timeout.h
//  net80211
//
//  Copyright (c) 2012 Prashant Vaibhav. All rights reserved.
//

#ifndef net80211_timeout_h
#define net80211_timeout_h

#include <IOKit/IOLib.h>

struct timeout {
    void (*fn)(void *);
    void* arg;
};

#endif
