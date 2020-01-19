//
//  IntelWiFiDriver_mvm.hpp
//  net80211
//
//  Created by Administrator on 09/01/2020.
//

#ifndef IntelWiFiDriver_mvm_h
#define IntelWiFiDriver_mvm_h

#include "iwlwifi_headers/iwl-config.h"

struct MVMStatus {
    bool HWRFKill;
    bool HWCTKill;
    bool ROCRunning;
    bool HWRestartRequested;
    bool HWRestart;
    bool D0I3;
    bool ROCAuxRunning;
    bool fwRunning;
    bool flushP2PNeeded;
};

//struct MVMReprobe {
//    //Also has pointer to device aka IOPCIDevice
//    //Probably no point definig a struct for this for no reason
//    IOWorkLoop* workSource;
//};

static inline bool deviceHasUnifiedUCode(PCIDeviceConfig* deviceConfig) {
    return deviceConfig->device_family >= IWL_DEVICE_FAMILY_22000;
}

static inline bool isRadioKilled(struct MVMStatus mvmStatus) {
    return (mvmStatus.HWCTKill || mvmStatus.HWRFKill);
}

#endif /* IntelWiFiDriver_mvm_h */
