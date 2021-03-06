//
//  IntelWiFiDriver_debug.cpp
//  net80211
//
//  Created by Administrator on 04/01/2020.
//

#include "IntelWiFiDriver.hpp"

void IntelWiFiDriver::updateHardwareDebugStatistics(enum hardwareDebugStatistics updateStat, uint32_t value) {
    if (!DEBUG) {
        return;
    }
    
    //If debugging is enabled for the kext we will update the stats of the driver + card
    //We arent really bothered about using this if debugging is not enabled
    switch (updateStat) {
        case hardwareError:
            hwStats.hardwareError++;
            break;
        case softwareError:
            hwStats.softwareError++;
            break;
        case schedulerFired:
            hwStats.schedulerFired++;
            break;
        case aliveRecieved:
            hwStats.aliveRecieved++;
            break;
        case rfKillToggledOn:
            hwStats.rfKillToggledOn++;
            break;
        case ctKill:
            hwStats.ctKill++;
            break;
        case wakeup:
            hwStats.wakeup++;
            break;
        case rxRecieved:
            hwStats.rx++;
            break;
        case txRecieved:
            hwStats.tx++;
            break;
    }
}

void IntelWiFiDriver::dumpHardwareRegisters() {
    //iwl_trans_pcie_dump_regs
    if (!DEBUG) return;
    
    //TODO:Get registers from NIC and write out to log buffer
}


void IntelWiFiDriver::dumpNICErrorLog() {
    //iwl_mvm_dump_nic_error_log
    //TODO: Dump the error log from the NIC
}

void IntelWiFiDriver::collectFirmwareErrorDetails() {
    //iwl_fw_error_correct
    //TODO: Implement collecting the error log from the firmware
}

void IntelWiFiDriver::firmwareDebugStopRecording() {
    //iwl_fw_dbg_stop_recording
    //TODO: Implement
}
