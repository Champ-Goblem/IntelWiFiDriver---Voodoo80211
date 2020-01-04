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
    IOVirtualAddress deviceMemoryMapVAddr;
    PCIDeviceConfig* deviceConfig;
    IOEventSource* interruptController;
    uint32_t intaBitMask;
};

struct hardwareDebugStatisticsCounters {
    uint32_t hardwareError;
    uint32_t softwareError;
    uint32_t schedulerFired;
    uint32_t aliveRecieved;
    uint32_t rfKillToggledOn;
    uint32_t ctKill;
    uint32_t wakeup;
};

enum hardwareDebugStatistics {
    hardwareError,
    softwareError,
    schedulerFired,
    aliveRecieved,
    rfKillToggledOn,
    ctKill,
    wakeup
};

#endif /* DrvStructs_h */
