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

struct PCIDeviceStatus {
    Boolean interruptsEnabled;
};

//Contains all attrbutes of the device used by the driver
struct PCIDevice {
    //NIC related variables
    IOPCIDevice*        device;
    IOWorkLoop*         workLoop;
    int                 capabilitiesStructOffset;
    IOMemoryMap*        deviceMemoryMap;
//    IOVirtualAddress    deviceMemoryMapVAddr;
    PCIDeviceConfig*    deviceConfig;
    
    //Interrupt related variables
    IOEventSource*      interruptController;
    uint32_t            intaBitMask;
    
    //Firmware related varaibales
    IOLock*             ucodeWriteWaitLock;
    Boolean             ucodeWriteComplete = false;
    
    //Contains the statuses of the driver
    PCIDeviceStatus           status;
    
    //MSIX related variables
    Boolean             msixEnabled;
    uint32_t            msixFHInitMask;
    uint32_t            msixFHMask;
    uint32_t            msixHWInitMask;
    uint32_t            msixHWMask;
    
    //Used for grabbing NIC access
    bool                holdNicAwake;
    
};

struct hardwareDebugStatisticsCounters {
    uint32_t hardwareError;
    uint32_t softwareError;
    uint32_t schedulerFired;
    uint32_t aliveRecieved;
    uint32_t rfKillToggledOn;
    uint32_t ctKill;
    uint32_t wakeup;
    uint32_t rx;
    uint32_t tx;
};

enum hardwareDebugStatistics {
    hardwareError,
    softwareError,
    schedulerFired,
    aliveRecieved,
    rfKillToggledOn,
    ctKill,
    wakeup,
    rxRecieved,
    txRecieved
};

#endif /* DrvStructs_h */
