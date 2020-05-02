//
//  IntelWiFiDriver_mvm.hpp
//  net80211
//
//  Created by Administrator on 09/01/2020.
//

#ifndef IntelWiFiDriver_mvm_h
#define IntelWiFiDriver_mvm_h

#include "iwlwifi_headers/iwl-config.h"
#include "iwlwifi_headers/fw/api/iwl-scan.h"
#include "iwlwifi_headers/iwl-trans.h"
#include "iwlwifi_headers/mvm/iwl-sta.h"
#include "iwlwifi_headers/fw/api/iwl-mac.h"

#define IWL_MVM_SCAN_STOPPING_SHIFT    8

enum iwl_scan_status {
    IWL_MVM_SCAN_REGULAR        = BIT(0),
    IWL_MVM_SCAN_SCHED        = BIT(1),
    IWL_MVM_SCAN_NETDETECT        = BIT(2),
    
    IWL_MVM_SCAN_STOPPING_REGULAR    = BIT(8),
    IWL_MVM_SCAN_STOPPING_SCHED    = BIT(9),
    IWL_MVM_SCAN_STOPPING_NETDETECT    = BIT(10),
    
    IWL_MVM_SCAN_REGULAR_MASK    = IWL_MVM_SCAN_REGULAR |
    IWL_MVM_SCAN_STOPPING_REGULAR,
    IWL_MVM_SCAN_SCHED_MASK        = IWL_MVM_SCAN_SCHED |
    IWL_MVM_SCAN_STOPPING_SCHED,
    IWL_MVM_SCAN_NETDETECT_MASK    = IWL_MVM_SCAN_NETDETECT |
    IWL_MVM_SCAN_STOPPING_NETDETECT,
    
    IWL_MVM_SCAN_STOPPING_MASK    = 0xff << IWL_MVM_SCAN_STOPPING_SHIFT,
    IWL_MVM_SCAN_MASK        = 0xff,
};

enum iwl_mvm_scan_type {
    IWL_SCAN_TYPE_NOT_SET,
    IWL_SCAN_TYPE_UNASSOC,
    IWL_SCAN_TYPE_WILD,
    IWL_SCAN_TYPE_MILD,
    IWL_SCAN_TYPE_FRAGMENTED,
    IWL_SCAN_TYPE_FAST_BALANCE,
};

enum iwl_mvm_sched_scan_pass_all_states {
    SCHED_SCAN_PASS_ALL_DISABLED,
    SCHED_SCAN_PASS_ALL_ENABLED,
    SCHED_SCAN_PASS_ALL_FOUND,
};

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

/*
 * enum iwl_mvm_queue_status - queue status
 * @IWL_MVM_QUEUE_FREE: the queue is not allocated nor reserved
 *    Basically, this means that this queue can be used for any purpose
 * @IWL_MVM_QUEUE_RESERVED: queue is reserved but not yet in use
 *    This is the state of a queue that has been dedicated for some RATID
 *    (agg'd or not), but that hasn't yet gone through the actual enablement
 *    of iwl_mvm_enable_txq(), and therefore no traffic can go through it yet.
 *    Note that in this state there is no requirement to already know what TID
 *    should be used with this queue, it is just marked as a queue that will
 *    be used, and shouldn't be allocated to anyone else.
 * @IWL_MVM_QUEUE_READY: queue is ready to be used
 *    This is the state of a queue that has been fully configured (including
 *    SCD pointers, etc), has a specific RA/TID assigned to it, and can be
 *    used to send traffic.
 * @IWL_MVM_QUEUE_SHARED: queue is shared, or in a process of becoming shared
 *    This is a state in which a single queue serves more than one TID, all of
 *    which are not aggregated. Note that the queue is only associated to one
 *    RA.
 */
enum iwl_mvm_queue_status {
    IWL_MVM_QUEUE_FREE,
    IWL_MVM_QUEUE_RESERVED,
    IWL_MVM_QUEUE_READY,
    IWL_MVM_QUEUE_SHARED,
};

/**
 * struct iwl_mvm_tvqm_txq_info - maps TVQM hw queue to tid
 *
 * @sta_id: sta id
 * @txq_tid: txq tid
 */
struct iwl_mvm_tvqm_txq_info {
    u8 sta_id;
    u8 txq_tid;
};

struct iwl_mvm_dqa_txq_info {
    u8 ra_sta_id; /* The RA this queue is mapped to, if exists */
    bool reserved; /* Is this the TXQ reserved for a STA */
    u8 mac80211_ac; /* The mac80211 AC this queue is mapped to */
    u8 txq_tid; /* The TID "owner" of this queue*/
    u16 tid_bitmap; /* Bitmap of the TIDs mapped to this queue */
    /* Timestamp for inactivation per TID of this queue */
    unsigned long last_frame_time[IWL_MAX_TID_COUNT + 1];
    enum iwl_mvm_queue_status status;
};

struct MVMConfig {
    struct iwl_fw*          fw;
    
    //Max number of simultaneous scans the FW supports
    unsigned int            maxScans;
    
    //UMAC scan tracking
    uint32_t                scanUIDStatus[IWL_MVM_MAX_UMAC_SCANS];
    
    volatile SInt32         queueSyncCounter;
    
    //The status of the card
    MVMStatus               status;
    
    //-1 for always, 0 for never, >0 for that many times
    SInt8                   firmwareRestart;
    
    //Scan status, cmd and auxiliary station
    unsigned int scanStatus;
    
    enum iwl_mvm_sched_scan_pass_all_states schedScanPassAll;
    
    union {
        struct iwl_mvm_dqa_txq_info  queueInfo[IWL_MAX_HW_QUEUES];
        struct iwl_mvm_tvqm_txq_info tvqmInfo[IWL_MAX_TVQM_QUEUES];
    };
    
    //Data realted to data path?
    struct iwl_mvm_sta* stationData[IWL_MVM_STATION_COUNT];
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
