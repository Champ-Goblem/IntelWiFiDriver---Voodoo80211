//
//  IntelWifiDriver_intrpts.cpp
//  net80211
//
//  Created by Administrator on 22/10/2019.
//

#include "IntelWiFiDriver.hpp"

//Questions:
//1. When is MSIX used over ICT and why, or vice-versa?
//2. When is ICT enabled?

int IntelWiFiDriver::interruptHandler(OSObject* owner, IOInterruptEventSource* sender, int count) {
    //We are going to start off by using INTA register but to quote the iwlwifi source:
    /* "interrupt handler using ict table, with this interrupt driver will
     * stop using INTA register to get device's interrupt, reading this register
     * is expensive, device will write interrupts in ICT dram table, increment
     * index then will fire interrupt to driver, driver will OR all ICT table
     * entries from current index up to table entry with 0 value. the result is
     * the interrupt we need to service, driver will set the entries back to 0 and
     * set index."
     */
    uint32_t inta;
    Boolean receivedFHTX, receivedRFKill, recievedAlive_FHRX;
    //Disable interrupts
//    bus_space_write_4(NULL, deviceBusMap, WPI_MASK, 0);
    busWrite32(WPI_MASK, 0);
    
    //Read INTA register
//    inta = bus_space_read_4(NULL, deviceBusMap, WPI_INT);
    inta = busRead32(WPI_INT);
    //VoodooIntel3945 uses r2 value too, not sure this relates to interrupts?
//    intb = bus_space_read_4(NULL, deviceBusPointer, WPI_FH_INT);
    
    if (inta == 0){
        //Interupt was not for us
        //Re-enable as we dont have anything to service
        if (deviceProps.status.interruptsEnabled) enableInterrupts();
        return 0;
    }
    
    if (inta == 0xFFFFFFFF || (inta & 0xFFFFFFF0 ) == 0xa5a5a5a0) {
        //Hardware has disappeared time to reset
        printf("%s: Harware gone INTA=0x%08x\n", DRVNAME, inta);
        return 0;
    }
    
    //Acknowledge interrupt recieved
//    bus_space_write_4(NULL, deviceBusMap, WPI_INT, inta | ~deviceProps.intaBitMask);
    busWrite32(WPI_INT, inta | ~deviceProps.intaBitMask);
//    bus_space_write_4(NULL, deviceBusPointer, WPI_FH_INT, intb);
    
    if (inta & WPI_INT_HW_ERR) {
        //Hardware error
        printf("%s: Hardware error INTA=0x%08x\n", DRVNAME, inta);
        
        //Completely disable interrupts and log it to the driver stats
        disableInterrupts();
        updateHardwareDebugStatistics(hardwareError, 0);
        
        //Pass off to our handler function
        handleHardwareErrorINT();
        return 0;
    }
    
    if (inta & WPI_INT_SCD) {
        //Apparently not used:
        /* "NIC fires this, but we don't use it, redundant with WAKEUP" */
        //Just update hardware stats for debug
        updateHardwareDebugStatistics(schedulerFired, 0);
    }
    
    if (inta & WPI_INT_ALIVE) {
        //Device calling back alive
        updateHardwareDebugStatistics(aliveRecieved, 0);
        if (deviceProps.deviceConfig->gen2) {
            rxMultiqueueRestock();
        }
    }
    
//    //These have been processed
//    inta &= ~(WPI_INT_ALIVE | WPI_INT_SCD)
    
    if (inta & WPI_INT_RF_TOGGLED) {
        //RF Kill switch has been toggled
        handleRFKillINT();
    }
    
    if (inta & WPI_INT_CT_KILL) {
        //Hardware overheated
        printf("%s: Hardware has stopped itself due to overheat INTA=0x%08x", DRVNAME, inta);
        updateHardwareDebugStatistics(ctKill, 0);
    }
    
    if (inta & WPI_INT_SW_ERR) {
        //uCode detected a software error
        printf("%s: Hardware detected software error INTA=0x%08x", DRVNAME, inta);
        updateHardwareDebugStatistics(softwareError, 0);
        
        handleHardwareErrorINT();
    }
    
    if (inta & WPI_INT_WAKEUP) {
        //Wakeup after "power-down" sleep
        updateHardwareDebugStatistics(wakeup, 0);
        
        handleWakeupINT();
    }
    
    if (inta & (WPI_INT_FH_RX | WPI_INT_SW_RX | WPI_INT_RX_PERIODIC)) {
        //All microcode command responses
        
        if (inta & (WPI_INT_FH_RX | WPI_INT_SW_RX)) {
//            bus_space_write_4(NULL, deviceBusMap, WPI_FH_INT, WPI_FH_INT_RX_MASK);
            busWrite32(WPI_FH_INT, WPI_FH_INT_RX_MASK);
        }
        
        //TODO: If using ICT
        //1. Write CSR_INT_BIT_RX_PERIODIC to WPI_INT
        //2. Disable periodic interrupt using write8
        //3. Something about enabling in 8msec? nothing seems to make sure its 8msec
        
        updateHardwareDebugStatistics(rxRecieved, 0);
        handleRxINT();
    }
    
    if (inta & WPI_INT_FH_TX) {
        //Tx channel for microcode load complete
//        bus_space_write_4(NULL, deviceBusMap, WPI_FH_INT, WPI_FH_INT_TX_MASK);
        busWrite32(WPI_FH_INT, WPI_FH_INT_TX_MASK);
        
        updateHardwareDebugStatistics(txRecieved, 0);
        
        //Notify any waiting locks that we have finally loaded the microcode
        //Not sure the lock should be locked by us here?
        IOLockLock(deviceProps.ucodeWriteWaitLock);
        deviceProps.ucodeWriteComplete = true;
        IOLockWakeup(deviceProps.ucodeWriteWaitLock, &deviceProps.ucodeWriteComplete, true);
        IOLockUnlock(deviceProps.ucodeWriteWaitLock);
        receivedFHTX = true;
    }
    
    if (!deviceProps.status.interruptsEnabled) {
        enableInterrupts();
    } else if (receivedFHTX) {
        enableFirmwareLoadINT();
    } else if (receivedRFKill) {
        enableRFKillINT();
    } else if (recievedAlive_FHRX) {
        enableCTXInfoINT();
    }
    
    return 0;
}

#pragma mark Interrupt enabler/disabler functions

void IntelWiFiDriver::enableInterrupts() {
    //Enable NIC interrupts
    deviceProps.status.interruptsEnabled = true;
    if (deviceProps.msixEnabled) {
        //TODO: if using MSIX enable this
//        deviceProps.msixFHMask = deviceProps.msixFHInitMask;
//        deviceProps.msixHWMask = deviceProps.msixHWInitMask;
//        busWrite32(CSR_MSIX_FH_INT_MASK_AD, deviceProps.msixFHMask);
//        busWrite32(CSR_MSIX_HW_INT_MASK_AD, deviceProps.msixHWMask);
        printf("%s: enableInterrupts MSIX exec path not setup", DRVNAME);
    } else {
        deviceProps.intaBitMask = WPI_INT_MASK_ALL;
        busWrite32(WPI_MASK, WPI_INT_MASK_ALL);
    }
    
    if(DEBUG) printf("%s: Enabled interrupts", DRVNAME);
    return;
}

void IntelWiFiDriver::disableInterrupts() {
    //Disable NIC interrupts and reset
    deviceProps.status.interruptsEnabled = false;

    if (deviceProps.msixEnabled) {
        //TODO: if using MSIX enable this
//        busWrite32(CSR_MSIX_FH_INT_MASK_AD, deviceProps.msixFHInitMask);
//        busWrite32(CSR_MSIX_HW_INT_MASK_AD, deviceProps.msixHWInitMask);
        printf("%s: disableInterrupts MSIX exec path not setup", DRVNAME);
    } else {
        busWrite32(WPI_MASK, 0);
        busWrite32(WPI_INT, 0xffffffff);
        busWrite32(WPI_FH_INT, 0xffffffff);
    }
    
    if (DEBUG) printf("%s: Disabled interrupts", DRVNAME);
    return;
}

void IntelWiFiDriver::enableFirmwareLoadINT() {
    //Enable the firmware loaded interrupt by CSR_INT_MASK
    if (deviceProps.msixEnabled) {
        //TODO: if using MSIX set up here
        printf("%s: enableFirmareLoadINT MSIX exec path not setup", DRVNAME);
    } else {
        deviceProps.intaBitMask = WPI_INT_FH_TX;
        busWrite32(WPI_MASK, WPI_INT_FH_TX);
    }
    
    if (DEBUG) printf("%s: Enabled firmware load interrupt", DRVNAME);
    return;
}

void IntelWiFiDriver::enableRFKillINT() {
    //Enable the RF toggle interrupt
    if (deviceProps.msixEnabled) {
        //TODO: if using MSIX set up here
        printf("%s: enableRFKillINT MSIX exec path not setup", DRVNAME);
    } else {
        deviceProps.intaBitMask = WPI_INT_RF_TOGGLED;
        busWrite32(WPI_MASK, WPI_INT_RF_TOGGLED);
    }
    
    if(DEBUG) printf("%s: Enabled RF toggle interrupt", DRVNAME);
    return;
}

void IntelWiFiDriver::enableCTXInfoINT() {
    //Enable the ALIVE interrupt only
    if (deviceProps.msixEnabled) {
        //TODO: if using MSIX set up here
        printf("%s: enableCTXInfoINT MSIX exec path not setup", DRVNAME);
    } else {
        deviceProps.intaBitMask = WPI_INT_ALIVE | WPI_INT_FH_RX;
        busWrite32(WPI_MASK, deviceProps.intaBitMask);
    }
    
    if (DEBUG) printf("%s: Enabled CTX Info interrupt", DRVNAME);
    return;
}

#pragma mark Different handler functions for interrupt bits
void IntelWiFiDriver::handleHardwareErrorINT() {
    //Handles any hardware errors reported by the NIC
    
    if(deviceProps.deviceConfig->internal_wimax_coex && //Enabled 5150/5350_agn/6050/6150 series NICs
       !deviceProps.deviceConfig->apmg_not_supported && //False for 22000/8000/9000 series NICs
       (!(readPRPH(WPI_APMG_CLK_CTRL_REG) & WPI_APMG_CLK_MRB_FUNC_MODE) ||
        (readPRPH(WPI_APMG_PS) & WPI_APMG_PS_CTRL_VAL_RESET_REQ))) {
           deviceProps.status.syncHCMDActive = false;
           //No need to worry about here for now enabling WIMAX and that as we are
           //only supporting MVM cards for the time being, leaving this here for future
           //DVM update or removal
           //TODO: Will need to complete here for DVM cards
       }
    
    for (int i = 0; i < deviceProps.deviceConfig->base_params->num_of_queues; i++) {
        if (deviceProps.txQueues[i]) {
            //Inactive the timers for each txq
            //We have a choice of disabling or cancelling but intuitively I would say cancel
            deviceProps.txQueues[i]->stuck_timer.cancelTimeout();
        }
    }
    
    receivedNICError();
    deviceProps.status.syncHCMDActive = false;
    IOLockLock(deviceProps.waitCommandQueue);
    IOLockWakeup(deviceProps.waitCommandQueue, &deviceProps.status.syncHCMDActive, true);
    IOLockUnlock(deviceProps.waitCommandQueue);
    return;
}

void IntelWiFiDriver::handleRFKillINT() {
    //Remember update hwstats
    bool ret, prev, rfKillSet;
    
    IOSimpleLockLock(deviceProps.mutex);
    prev = deviceProps.status.RFKillOpmodeEnabled;
    rfKillSet = isRFKillSet();
    if (rfKillSet) {
        deviceProps.status.RFKillOpmodeEnabled = true;
        deviceProps.status.RFKillHardwareEnabled = true;
    }
    
    if (deviceProps.opmodeDown) {
        ret = rfKillSet;
    } else {
        ret = deviceProps.status.RFKillOpmodeEnabled;
    }
    
    if (DEBUG) printf("%s: RF Kill toggled %s\n", DRVNAME, rfKillSet ? "on" : "off");
    
    updateHardwareDebugStatistics(rfKillToggledOn, 0);
    
    if (prev != ret) {
        setHardwareRFKillState(ret);
    }
    IOSimpleLockUnlock(deviceProps.mutex);
    
    if (rfKillSet) {
        if (deviceProps.status.syncHCMDActive) {
            deviceProps.status.syncHCMDActive = false;
            if (DEBUG) printf("RFKill while SYNC HCMD in flight\n");
            IOLockWakeup(deviceProps.waitCommandQueue, &deviceProps.status.syncHCMDActive, true);
        } else {
            deviceProps.status.RFKillHardwareEnabled = false;
            if (deviceProps.opmodeDown) {
                deviceProps.status.RFKillOpmodeEnabled = false;
            }
        }
    }
}

void IntelWiFiDriver::handleWakeupINT() {
    //TODO: Implement handling wakeup interrupt
    //Check rxQueue and txQueue
}

void IntelWiFiDriver::handleRxINT() {
    //TODO: Implement handling Rx interrupt
}
