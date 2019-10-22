//
//  DrvStructs.h
//  net80211
//
//  Created by Administrator on 21/10/2019.
//

#ifndef DrvStructs_h
#define DrvStructs_h
#include "iwlwifi_headers/iwl-config.h"

typedef const struct iwl_cfg PCIDeviceConfig;

struct PCIDevice {
    IOPCIDevice* device;
    IOWorkLoop* workLoop;
    int capabilitiesStructOffset;
    IOMemoryMap* deviceMemoryMap;
    PCIDeviceConfig* deviceConfig;
};

#endif /* DrvStructs_h */
