//
//  IntelWiFiDriver_comms.c
//  net80211
//
//  Created by Administrator on 06/01/2020.
//

#include "IntelWiFiDriver.hpp"

bool IntelWiFiDriver::grabNICAccess(u_long flags) {
    //Check if device is currently already being held awake
    if (deviceProps.holdNicAwake) return true;
    
    busSetBit(WPI_GP_CNTRL, deviceProps.deviceConfig->csr->flag_mac_access_req);
    if (deviceProps.deviceConfig->device_family >= IWL_DEVICE_FAMILY_8000) udelay(2);
    
    return true;
}
