//
//  DrvStructs.h
//  net80211
//
//  Created by Administrator on 21/10/2019.
//

#ifndef DrvStructs_h
#define DrvStructs_h
#include "iwlwifi_headers/iwl-config.h"
#include "iwlwifi_headers/internals.h"
#include "IntelWiFiDriver_mvm.hpp"
#include "Firmware.hpp"
//#include "iwlwifi_headers/mvm.h"

#define MAX_TX_QUEUES 512 //Maximum number of transmit queues

struct PCIDeviceStatus {
    bool interruptsEnabled;     //STATUS_INT_ENABLED
    bool connectionClosed;      //STATUS_TRANS_DEAD
    bool syncHCMDActive;        //STATUS_SYNC_HCMD_ACTIVE
    bool FWError;               //STATUS_FW_ERROR
    bool deviceEnabled;         //STATUS_DEVICE_ENABLED
    bool deviceAsleep;          //STATUS_TPOWER_PMI
    bool RFKillOpmodeEnabled;   //STATUS_RFKILL_OPMODE
    bool RFKillHardwareEnabled; //STATUS_RFKILL_HW
    bool PMI_TPower;            //STATUS_TPOWER_PMI
};

struct MVMSpecificConfig {
    struct notificationWaiters{
        IOSimpleLock* notifWaitLock;
        LIST_HEAD(, notificationWaitEntry) firstWaitEntry;
        IOLock* notifWaitQueue;
    }notifWaits;
    
    //Firmware specific data
    struct FirmwareRuntimeData fwRuntimeData;
    struct FirmwareData        fwData;
    int8_t firmwareRestart;
    uint8_t* errorRecoveryBuffer;
    
    MVMStatus status;
    
    bool harwareRegistered;
    bool RFKillSafeInitDone;
    
    IOLock* rxSyncWaitQueue;
};

//Contains all attrbutes of the device used by the driver
struct PCIDevice {
    //NIC related variables
    IOPCIDevice*                device;
    IOWorkLoop*                 workLoop;
    int                         capabilitiesStructOffset;
    IOMemoryMap*                deviceMemoryMap;
//    volatile void*              deviceMemoryMapVAddr;
    PCIDeviceConfig*            deviceConfig;
    struct MVMSpecificConfig    mvmConfig; //Will need replacing if we implement DVM cards
    bool                        debugRFKill;
    bool                        opmodeDown;
    bool                        isDown;
    
    //Interrupt related variables
    IOEventSource*              interruptController;
    uint32_t                    intaBitMask;
    
    //Firmware related varaibales
    IOLock*                     ucodeWriteWaitLock;
    bool                        ucodeWriteComplete = false;
    
    //Contains the statuses of the driver
    PCIDeviceStatus             status;
    
    //MSIX related variables
    bool                        msixEnabled;
    uint32_t                    msixFHInitMask;
    uint32_t                    msixFHMask;
    uint32_t                    msixHWInitMask;
    uint32_t                    msixHWMask;
    uint8_t                     sharedVecMask;
    uint32_t                    defIRQ; //Can we rename this to something more descriptive?
    
    //Used for grabbing NIC access
    bool                        holdNICAwake; //Status if a current command in flight is holding NIC awake
    bool                        comandInFlight;
    IOSimpleLock*               NICAccessLock; //Lock for grabbing NIC access [reg_lock]
    
    //Device communication queues
    //iwlwifi also defines txq_memory which simply points to the allocated txqs in memory
    //only a function in trans.c uses this later so we wont bother eek
    iwl_txq*                    txQueues[MAX_TX_QUEUES];
    bool                        txqAllocated;
    u_long                      txq_used[BITS_TO_LONGS(MAX_TX_QUEUES)];
    u_long                      txq_stopped[BITS_TO_LONGS(MAX_TX_QUEUES)];
    iwl_rxq*                    rxq;
    uint8_t                     commandQueue; //cmd_queue
    uint16_t                    tfdSize;
    uint8_t                     maxTBS;
    uint8_t                     rxQCount; //num_rx_queues

    IOLock*                     waitCommandQueue;
    
    IOSimpleLock*               mutex; //Can we rename this to something more descriptive?
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
