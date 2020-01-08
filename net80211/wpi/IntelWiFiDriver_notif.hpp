//
//  IntelWiFiDriver_notif.hpp
//  net80211
//
//  Created by Administrator on 08/01/2020.
//

#ifndef IntelWiFiDriver_notif_h
#define IntelWiFiDriver_notif_h

#include <sys/queue.h>

#define MAX_NOTIF_CMDS 5

struct notificationWaitEntry {
    LIST_ENTRY(notificationWaitEntry) list;
    
    //iwlwifi specifies a pointer to function fn, but well
    //add that later on

    void*       fn_data;
    
    uint16_t    cmdIDs[MAX_NOTIF_CMDS];
    uint8_t     noCmdIDs;
    bool        triggered, aborted, fnDisabled;
    
};

#endif /* IntelWiFiDriver_notif_h */
