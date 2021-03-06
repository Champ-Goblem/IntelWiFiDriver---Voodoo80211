//
//  IntelWiFiDriver_mvm.c
//  net80211
//
//  Created by Administrator on 08/01/2020.
//

#include "IntelWiFiDriver.hpp"
#include "IntelWiFiDriver_mvm.hpp"
#include "../ieee80211_proto.h"
//===================================
//      MVM only operations
//===================================

//The NIC has encountered an error during operation, log and restart
void IntelWiFiDriver::receivedNICError() {
    //iwl_mvm_nic_error
    if (deviceProps.status.FWError) {
        //Already beed set before by another interrupt
        return;
    }
    deviceProps.status.FWError = true;
    if (deviceProps.status.deviceEnabled) {
        //We should print out NIC error log so long as we still have
        //a communication channel with the NIC
        dumpNICErrorLog();
    }
    //Restart the NIC to reset it
    forceNICRestart(true);
}

void IntelWiFiDriver::forceNICRestart(bool firmwareError) {
    //iwl_mvm_nic_restart
    MVMSpecificConfig* mvmConfig = &deviceProps.mvmConfig;
    abortNotificationWaits();
    //Cancel the periodic dump trigger as we are restarting it
    mvmConfig->fwRuntimeData.dump.periodicTrigger->cancelTimeout();
    
    //To quote from iwlwifi:
    /*
     * This is a bit racy, but worst case we tell mac80211 about
     * a stopped/aborted scan when that was already done which
     * is not a problem. It is necessary to abort any os scan
     * here because mac80211 requires having the scan cleared
     * before restarting.
     * We'll reset the scan_status to NONE in restart cleanup in
     * the next start() call from mac80211. If restart isn't called
     * (no fw restart) scan status will stay busy.
     */
    reportScanAborted();
    
    if (!mvmConfig->firmwareRestart && firmwareError) {
        collectFirmwareErrorDetails();
    } else if (mvmConfig->status.HWRestart) {
        LOG_ERROR("%s: Firmware error during reconfiguration, kext needs reloading", DRVNAME);
        //Basically iwlwifi forces the driver to be unloaded and schedules the probe operation
        //again, not sure if this is possible in OSx, needs more research
        //TODO: Research forcing kext unloading from within
    } else if (mvmConfig->status.HWRestartRequested) {
        printf("%s: Restart request has already been requested", DRVNAME);
    } else if (mvmConfig->fwRuntimeData.microcodeType == REGULAR &&
               mvmConfig->harwareRegistered &&
               !deviceProps.status.connectionClosed) {
        //iwlwifi starts by calling iwl_mvm_ref which translates to a kernel call pm_runtime_get
        //this function is to do with retaining the device, so as long as we dont release our ref
        //to IOPCIDevice we should be fine
        
        if (mvmConfig->fwData.uCodeCapabilities.errorLogSize) {
            uint32_t srcSize = mvmConfig->fwData.uCodeCapabilities.errorLogSize;
            uint32_t srcAddress = mvmConfig->fwData.uCodeCapabilities.errorLogAddress;
            uint8_t* errorLogBuffer = (uint8_t*)IOMalloc(srcSize);
            
            if (!errorLogBuffer) {
                printf("%s: Failed to allocate recovery buffer", DRVNAME);
            } else {
                bzero(errorLogBuffer, srcSize);
                mvmConfig->errorRecoveryBuffer = errorLogBuffer;
                
                //Read the entire error log from io memory into our buffer
                readIOMemToBuffer(srcAddress, errorLogBuffer, srcSize);
            }
            
            collectFirmwareErrorDetails();
            
            if (firmwareError && mvmConfig->firmwareRestart > 0) {
                mvmConfig->firmwareRestart--;
            }
            mvmConfig->status.HWRestartRequested = true;
            restartHardware();
        }
    }
}

void IntelWiFiDriver::setHardwareRFKillState(bool status) {
    //iwl_trans_pcie_rf_kill and iwl_mvm_set_hw_rfkill_state combined
    
    //iwlwifi reads the value of RFKillSafeInitDone with READ_ONCE,
    //this is to detect and thus eliminate race conditions
    //I am unsure whether a similar thing exists in xnu
    //TODO: read normally for now
    bool rfKillSafe = deviceProps.mvmConfig.RFKillSafeInitDone;
    bool unifiedUCode = deviceHasUnifiedUCode(deviceProps.deviceConfig);
    bool ret;
    //Update our local status value
    deviceProps.mvmConfig.status.HWRFKill = status;
    
    setRFKillState(status);
    
    if (rfKillSafe) {
        abortNotificationWaits();
    }
    
    if (unifiedUCode) {
        ret =  false;
    } else {
        ret = status && (deviceProps.mvmConfig.fwRuntimeData.microcodeType != INIT ||
                         rfKillSafe);
    }
    
    if (ret) {
        if (deviceProps.deviceConfig->gen2) {
            stopDeviceG2(true);
        } else {
            stopDeviceG1(true);
        }
    }
}

void IntelWiFiDriver::setRFKillState(bool status) {
    //iwl_mvm_set_rfkill_state
    bool currentStatus = isRadioKilled(deviceProps.mvmConfig.status);
    
    if (currentStatus) {
        //TODO: Check this when implementing rxSyncWaitQ
//        IOLockLock(deviceProps.mvmConfig.rxSyncWaitQueue);
//        IOLockWakeup(deviceProps.mvmConfig.rxSyncWaitQueue, NULL, true);
//        IOLockUnlock(deviceProps.mvmConfig.rxSyncWaitQueue);
        deviceProps.mvmConfig.rxSyncWaitQueue->commandWakeup((void*)(deviceProps.mvm.queueSyncCounter == 0 ||
                                                             isRadioKilled(deviceProps.mvm.status)));
    }
    //iwlwifi here makes a call to wiphy_rfkill_set_hw_state
    //which internally makes a call to rfkill_set_hw_state
    //The call then schedules &rdev->rfkill_sync
    //We need to find some replacement for these in the apple kernel
    //TODO: Find a replacement
    
//    disable(deviceProps.networkInterface);
//    getInterface()->setLinkState(kIO80211NetworkLinkDown, 0);
    ieee80211_set_link_state(deviceProps.bsdIEEEStruct, kIO80211NetworkLinkDown);
    
}

void IntelWiFiDriver::reportScanAborted() {
    //iwl_mvm_report_scan_aborted
    //TODO: Check this when implementing scanning
    
    if (checkFWCapabilities(IWL_UCODE_TLV_CAPA_UMAC_SCAN)){
        int uid = mvmScanUIDByStatus(IWL_MVM_SCAN_REGULAR);
        if (uid >= 0) {
            //Reset the scan
            deviceProps.bsdIEEEStruct->ic_scan_lock = IEEE80211_SCAN_UNLOCKED;
            ieee80211_newstate(deviceProps.bsdIEEEStruct, IEEE80211_S_INIT, -1);
        }
        
        uid = mvmScanUIDByStatus(IWL_MVM_SCAN_SCHED);
        if (uid >= 0 && !deviceProps.mvm.firmwareRestart) {
            //Reset the scan
            deviceProps.bsdIEEEStruct->ic_scan_lock = IEEE80211_SCAN_UNLOCKED;
            ieee80211_newstate(deviceProps.bsdIEEEStruct, IEEE80211_S_INIT, -1);
        }
        
        for (int i = 0; i < deviceProps.mvm.maxScans; i++) {
            if (deviceProps.mvm.scanUIDStatus[i]) {
                LOG_ERROR("%s: UMAC scan UID %d status not cleared\n", DRVNAME, i);
                deviceProps.mvm.scanUIDStatus[i] = 0;
            }
        }
    } else {
        if (deviceProps.mvm.scanStatus & IWL_MVM_SCAN_REGULAR) {
            //Reset the scan
            deviceProps.bsdIEEEStruct->ic_scan_lock = IEEE80211_SCAN_UNLOCKED;
            ieee80211_newstate(deviceProps.bsdIEEEStruct, IEEE80211_S_INIT, -1);
        }
        
        if ((deviceProps.mvm.scanStatus & IWL_MVM_SCAN_SCHED) && !deviceProps.mvm.firmwareRestart) {
            //Reset the scan
            deviceProps.bsdIEEEStruct->ic_scan_lock = IEEE80211_SCAN_UNLOCKED;
            ieee80211_newstate(deviceProps.bsdIEEEStruct, IEEE80211_S_INIT, -1);
            deviceProps.mvm.schedScanPassAll = SCHED_SCAN_PASS_ALL_DISABLED;
        }
    }
}

void IntelWiFiDriver::freeSKB(mbuf_t skb) {
    //iwl_mvm_free_skb

    //I dont think this is acutally needed as we free mbufs with methods from
    //kpi_mbuf.h
}

void IntelWiFiDriver::mvmWakeSWQueue(int txqID) {
    //iwl_mvm_wake_sw_queue ->
    //iwl_mvm_queue_state_change
    if (DEBUG) printf("%s: Waking sw queue %d\n", DRVNAME, txqID);
    mvmQueueStateChange(txqID, true);
}

int IntelWiFiDriver::mvmScanUIDByStatus(int status) {
    //iwl_mvm_scan_uid_by_status
    for (int i = 0; i < deviceProps.mvm.maxScans; i++) {
        if (deviceProps.mvm.scanUIDStatus[i] == status) {
            return i;
        }
    }
    return -ENOENT;
}

void IntelWiFiDriver::mvmQueueStateChange(int hwQueue, bool start) {
    //iwl_mvm_queue_state_change
    //TODO: Implement
    //TODO: Fix station mode, currently only using our driver based data structures
    //for station mode, at some point in the future we need to fix this
    
    uint8_t stationID = mvmHasNewTxAPI() ? deviceProps.mvm.tvqmInfo[hwQueue].sta_id : deviceProps.mvm.queueInfo[hwQueue].ra_sta_id;
    if (stationID >= ARRAY_SIZE(deviceProps.mvm.stationData)) {
        return;
    }
    
    struct iwl_mvm_sta* sta = deviceProps.mvm.stationData[stationID];
    if (sta == NULL) {
        return;
    }
    
    //Ive got no fucking clue how we are going to change this over
    //TODO: Get station back
    if (mvmIsStaticQueue(hwQueue)) {
        if (!start) {
            LOG_ERROR("%s: We need to get station mode features back into bsd, I think they have been removed, should be starting device here\n", DRVNAME);
        } else {
            LOG_ERROR("%s: We need to get station mode features back into bsd, I think they have been removed\n", DRVNAME);
        }
        return;
    }
    
}

bool IntelWiFiDriver::mvmHasNewTxAPI() {
    //iwl_mvm_has_new_tx_api
    return deviceProps.deviceConfig->use_tfh;
}

int IntelWiFiDriver::mvmIsStaticQueue(int queue) {
    //iwl_mvm_is_static_queue
    //TODO: Implement
}
