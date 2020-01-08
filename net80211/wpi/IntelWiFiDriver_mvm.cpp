//
//  IntelWiFiDriver_mvm.c
//  net80211
//
//  Created by Administrator on 08/01/2020.
//

#include "IntelWiFiDriver.hpp"
//===================================
//      MVM only operations
//===================================

//The NIC has encountered an error during operation, log and restart
void IntelWiFiDriver::receivedNICError() {
    if (deviceProps.status.FWError) {
        //Already beed set before by another interrupt
        return;
    }
    
    if (!deviceProps.status.deviceNotAvailable) {
        //We should print out NIC error log so long as we still have
        //a communication channel with the NIC
        dumpNICErrorLog();
    }
    //Restart the NIC to reset it
    forceNICRestart();
}

void IntelWiFiDriver::forceNICRestart() {
    abortNotificationWaits();
    
    
}
