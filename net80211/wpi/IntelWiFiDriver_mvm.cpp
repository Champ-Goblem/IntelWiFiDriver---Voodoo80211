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
    if (deviceProps.status.FWError) {
        //Already beed set before by another interrupt
        return;
    }
    deviceProps.status.FWError = true;
    if (!deviceProps.status.deviceNotAvailable) {
        //We should print out NIC error log so long as we still have
        //a communication channel with the NIC
        dumpNICErrorLog();
    }
    //Restart the NIC to reset it
    forceNICRestart(true);
}

void IntelWiFiDriver::forceNICRestart(bool firmwareError) {
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
