//
//  IntelWiFiDriver_notif.cpp
//  net80211
//
//  Created by Administrator on 08/01/2020.
//

#include "IntelWiFiDriver.hpp"

void IntelWiFiDriver::abortNotificationWaits() {
    IOSimpleLockLock(deviceProps.mvmConfig.notifWaits.notifWaitLock);
    //Get the first entry in the list
    notificationWaitEntry* notifWaitEntry = deviceProps.mvmConfig.notifWaits.firstWaitEntry.lh_first;
    //While there are still entries left set them as aborted
    do {
        notifWaitEntry->aborted = true;
    } while ((notifWaitEntry = LIST_NEXT(notifWaitEntry, list)));
    IOSimpleLockUnlock(deviceProps.mvmConfig.notifWaits.notifWaitLock);
    
    //From what I believe passing false should wake up all threads not just one?
    //TODO: Might need to pass an event here
    IOLockLock(deviceProps.mvmConfig.notifWaits.notifWaitQueue);
    IOLockWakeup(deviceProps.mvmConfig.notifWaits.notifWaitQueue, NULL, false);
    IOLockUnlock(deviceProps.mvmConfig.notifWaits.notifWaitQueue);
}
