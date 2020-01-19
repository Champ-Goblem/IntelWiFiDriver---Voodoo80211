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

void IntelWiFiDriver::prepareCardHardware() {
    //iwl_pcie_prepare_card_hw
    //TODO: Implement
}
