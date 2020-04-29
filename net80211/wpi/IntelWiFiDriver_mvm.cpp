//
//  IntelWiFiDriver_mvm.c
//  net80211
//
//  Created by Administrator on 08/01/2020.
//

#include "IntelWiFiDriver.hpp"
#include "IntelWiFiDriver_mvm.hpp"
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
        IOLockLock(deviceProps.mvmConfig.rxSyncWaitQueue);
        IOLockWakeup(deviceProps.mvmConfig.rxSyncWaitQueue, NULL, true);
        IOLockUnlock(deviceProps.mvmConfig.rxSyncWaitQueue);
    }
    //iwlwifi here makes a call to wiphy_rfkill_set_hw_state
    //which internally makes a call to rfkill_set_hw_state
    //The call then schedules &rdev->rfkill_sync
    //We need to find some replacement for these in the apple kernel
    //TODO: Find a replacement
    
}

void IntelWiFiDriver::reportScanAborted() {
    //iwl_mvm_report_scan_aborted
    //TODO: Implement reporting scan abborted
}

void IntelWiFiDriver::freeSKB(mbuf_t skb) {
    //iwl_mvm_free_skb

    //I dont think this is acutally needed as we free mbufs with methods from
    //kpi_mbuf.h
}

void IntelWiFiDriver::mvmWakeSWQueue(int txqID) {
    //iwl_mvm_wake_sw_queue ->
    //iwl_mvm_queue_state_change
    //TODO: Implement
}
