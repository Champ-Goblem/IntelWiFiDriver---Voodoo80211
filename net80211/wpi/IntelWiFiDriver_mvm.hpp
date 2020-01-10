//
//  IntelWiFiDriver_mvm.hpp
//  net80211
//
//  Created by Administrator on 09/01/2020.
//

#ifndef IntelWiFiDriver_mvm_h
#define IntelWiFiDriver_mvm_h

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

#endif /* IntelWiFiDriver_mvm_h */
