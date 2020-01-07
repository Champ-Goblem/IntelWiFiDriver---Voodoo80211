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
    bool interruptsEnabled;
    bool connectionClosed;
    bool syncHCMDActive;
};

//Contains all attrbutes of the device used by the driver
struct PCIDevice {
    //NIC related variables
    IOPCIDevice*        device;
    IOWorkLoop*         workLoop;
    int                 capabilitiesStructOffset;
    IOMemoryMap*        deviceMemoryMap;
//    volatile void*      deviceMemoryMapVAddr;
    PCIDeviceConfig*    deviceConfig;
    
    //Interrupt related variables
    IOEventSource*      interruptController;
    uint32_t            intaBitMask;
    
    //Firmware related varaibales
    IOLock*             ucodeWriteWaitLock;
    bool                ucodeWriteComplete = false;
    
    //Contains the statuses of the driver
    PCIDeviceStatus           status;
    
    //MSIX related variables
    bool                msixEnabled;
    uint32_t            msixFHInitMask;
    uint32_t            msixFHMask;
    uint32_t            msixHWInitMask;
    uint32_t            msixHWMask;
    
    //Used for grabbing NIC access
    bool                holdNICAwake; //Status if a current command in flight is holding NIC awake
    IOSimpleLock*       NICAccessLock; //Lock for grabbing NIC access
    
    //Device communication queues
    
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
