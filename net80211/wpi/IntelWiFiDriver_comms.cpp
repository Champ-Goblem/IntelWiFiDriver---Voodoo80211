//
//  IntelWiFiDriver_comms.c
//  net80211
//
//  Created by Administrator on 06/01/2020.
//

#include "IntelWiFiDriver.hpp"

//Keep the NIC awake while we read a value from its registers
bool IntelWiFiDriver::grabNICAccess(IOInterruptState flags) {
    //Setup the lock and disbale interrupts
    flags = IOSimpleLockLockDisableInterrupt(deviceProps.NICAccessLock);
    
    //Check if device is currently already being held awake
    if (deviceProps.holdNICAwake) return true;
    
    busSetBit(WPI_GP_CNTRL, deviceProps.deviceConfig->csr->flag_mac_access_req);
    if (deviceProps.deviceConfig->device_family >= IWL_DEVICE_FAMILY_8000) udelay(2);
    
    int ret = pollBit(WPI_GP_CNTRL, (uint32_t)BIT(deviceProps.deviceConfig->csr->flag_val_mac_access_en),
                      (uint32_t)(BIT(deviceProps.deviceConfig->csr->flag_mac_clock_ready) |
                      WPI_GP_CNTRL_REG_FLAG_GOING_TO_SLEEP), 15000);
    if (ret < 0) {
        uint32_t ctrlReg = busRead32(WPI_GP_CNTRL);
        
        //Log the error to the console and print registers if we have DEBUG status
        LOG_ERROR("%s: Timeout waiting for hardware access CSR_GP_CNTRL=0x%08x", DRVNAME, ctrlReg);
        dumpHardwareRegisters();
        
        //In iwlwifi they initiate removal functions if the device detected has been removed
        //Simply here we will set conncetion closed status to true and the pci nub should do all the heavy lifting
        //Otherwise we shal set the reset flag
        if (ctrlReg == ~0U) {
            printf("%s: Detected device was removed", DRVNAME);
            deviceProps.status.connectionClosed = true;
        } else {
            busWrite32(WPI_RESET, WPI_RESET_FORCE_NMI);
        }
        
        IOSimpleLockUnlockEnableInterrupt(deviceProps.NICAccessLock, flags);
        return false;
    }
    return true;
}

//Release the NIC from keeping itself awake
void IntelWiFiDriver::releaseNICAccess(IOInterruptState flags) {
    if (!deviceProps.holdNICAwake) {
        busClearBit(WPI_GP_CNTRL, deviceProps.deviceConfig->csr->flag_mac_access_req);
    }
    
    IOSimpleLockUnlockEnableInterrupt(deviceProps.NICAccessLock, flags);
}

void IntelWiFiDriver::restartHardware() {
    //ieee80211_restart_hw
    //TODO: Implement restarting hardware
    
    //In order to restart hardware we probably need to do some paperwork first
    //with the kernel to alleviate any issues but basically I think we can put
    //the device in powered off state (D0i3) then power it back up to reset
    
    //We will also need a way to completely reset the status of the driver too
    //and then call start to initialise again
    
    //Might also be worth keeping track of how many times hardware has been restarted
    //due to fatal error
}

bool IntelWiFiDriver::isRFKillSet() {
    //iwlwifi asserts that the lock deviceProps.mutex is held
    if (deviceProps.debugRFKill) return true;
    
    return !(busRead32(WPI_GP_CNTRL) & WPI_GP_CNTRL_RFKILL);
}

void IntelWiFiDriver::resetDevice() {
    //iwl_trans_pcie_sw_reset
    //TODO: Implement
}

bool IntelWiFiDriver::prepareCardHardware() {
    //iwl_pcie_prepare_card_hw
    
    if (DEBUG) printf("%s: preparing card hw", DRVNAME);
    
    int ret = setHardwareReady();
    if (ret >= 0) return true;
    
    busSetBit(WPI_DBG_LINK_PWR_MGMT_REG, CSR_RESET_LINK_PWR_MGMT_DISABLED);
    IOSleep(2000);
    
    for (int c = 0; c < 10; c++) {
        //If hw not ready prepare conditions to check again
        busSetBit(WPI_HW_IF_CONFIG, CSR_HW_IF_CONFIG_REG_PREPARE);
        int t = 0;
        do {
            ret = setHardwareReady();
            if (ret >= 0) return true;
            IOSleep(1000);
            t += 200;
        } while (t < 150000);
        IOSleep(25);
    }
    
    LOG_ERROR("%s: Could not prepare card hw", DRVNAME);
    return false;
}

int IntelWiFiDriver::setHardwareReady() {
    //iwl_pcie_set_hw_ready
    //TODO: Implement
    
    busSetBit(WPI_HW_IF_CONFIG, CSR_HW_IF_CONFIG_REG_BIT_NIC_READY);
    
    int ret = pollBit(WPI_HW_IF_CONFIG, CSR_HW_IF_CONFIG_REG_BIT_NIC_READY, CSR_HW_IF_CONFIG_REG_BIT_NIC_READY, HW_READY_TIMEOUT);
    if (ret >= 0) busSetBit(WPI_MBOX_SET, CSR_MBOX_SET_REG_OS_ALIVE);
    
    if (DEBUG) printf("%s: harware%s read\n", DRVNAME, ret < 0 ? " not" : "");
    return ret;
}

void IntelWiFiDriver::unref(){
    //iwl_trans_pcie_unref
    //TODO: find replacement
    //Makes a call to pm_runtime_mark_last_busy then pm_runtime_put_autosuspend
    //which decrements the reference counter of the device, not sure if we need
    //this or if we will need a workaround
    
    //The function also checks the D0i3_disable flag to see if the power level 3
    //(full power off) function is available.
    
    //The value of D0i3_disable is set to false by default, but is registered via
    //MODULE_PARAM_NAMED and thus the value can be adjusted via modprobe or kernel
    //command line arguments
    
    //[INFO]: iwl_trans_(ref/unref) command removed in iwlwifi-next tree
}
