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
    IOPCIDevice* device = deviceProps.device;
    IOMemoryMap* deviceBusMap = deviceProps.deviceMemoryMap;
    //Disable interrupts
//    bus_space_write_4(NULL, deviceBusMap, WPI_MASK, 0);
    device->ioWrite32(WPI_MASK, 0, deviceBusMap);
    
    //Read INTA register
//    inta = bus_space_read_4(NULL, deviceBusMap, WPI_INT);
    inta = device->ioRead32(WPI_INT, deviceBusMap);
    //VoodooIntel3945 uses r2 value too, not sure this relates to interrupts?
//    intb = bus_space_read_4(NULL, deviceBusPointer, WPI_FH_INT);
    
    if (inta == 0){
        //Interupt was not for us
        //Re-enable as we dont have anything to service
        if (!deviceProps.status.interruptsEnabled) enableInterrupts();
        return 0;
    }
    
    if (inta == 0xFFFFFFFF || (inta & 0xFFFFFFF0 ) == 0xa5a5a5a0) {
        //Hardware has disappeared time to reset
        printf("%s: Harware gone INTA=0x%08x\n", DRVNAME, inta);
        return 0;
    }
    
    //Acknowledge interrupt recieved
//    bus_space_write_4(NULL, deviceBusMap, WPI_INT, inta | ~deviceProps.intaBitMask);
    device->ioWrite32(WPI_INT, inta | ~deviceProps.intaBitMask);
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
        //TODO: if gen2 restock RQMX
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
            device->ioWrite32(WPI_FH_INT, WPI_FH_INT_RX_MASK);
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
        device->ioWrite32(WPI_FH_INT, WPI_FH_INT_TX_MASK);
        
        updateHardwareDebugStatistics(txRecieved, 0);
        
        //Notify any waiting locks that we have finally loaded the microcode
        //Not sure the lock should be locked by us here?
//        IOLockLock(deviceProps.ucodeWriteWaitLock);
        deviceProps.ucodeWriteComplete = true;
        IOLockWakeup(deviceProps.ucodeWriteWaitLock, &deviceProps.ucodeWriteComplete, true);
//        IOLockUnlock(deviceProps.ucodeWriteWaitLock);
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
//        deviceProps.device->ioWrite32(CSR_MSIX_FH_INT_MASK_AD, deviceProps.msixFHMask);
//        deviceProps.device->ioWrite32(CSR_MSIX_HW_INT_MASK_AD, deviceProps.msixHWMask);
        printf("%s: enableInterrupts MSIX exec path not setup", DRVNAME);
    } else {
        deviceProps.intaBitMask = WPI_INT_MASK_ALL;
        deviceProps.device->ioWrite32(WPI_MASK, WPI_INT_MASK_ALL);
    }
    
    if(DEBUG) printf("%s: Enabled interrupts", DRVNAME);
    return;
}

void IntelWiFiDriver::disableInterrupts() {
    //Disable NIC interrupts and reset
    deviceProps.status.interruptsEnabled = false;

    if (deviceProps.msixEnabled) {
        //TODO: if using MSIX enable this
//        deviceProps.device->ioWrite32(CSR_MSIX_FH_INT_MASK_AD, deviceProps.msixFHInitMask);
//        deviceProps.device->ioWrite32(CSR_MSIX_HW_INT_MASK_AD, deviceProps.msixHWInitMask);
        printf("%s: disableInterrupts MSIX exec path not setup", DRVNAME);
    } else {
        deviceProps.device->ioWrite32(WPI_MASK, 0);
        deviceProps.device->ioWrite32(WPI_INT, 0xffffffff);
        deviceProps.device->ioWrite32(WPI_FH_INT, 0xffffffff);
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
        deviceProps.device->ioWrite32(WPI_MASK, WPI_INT_FH_TX);
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
        deviceProps.device->ioWrite32(WPI_MASK, WPI_INT_RF_TOGGLED);
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
        deviceProps.device->ioWrite32(WPI_MASK, deviceProps.intaBitMask);
    }
    
    if (DEBUG) printf("%s: Enabled CTX Info interrupt", DRVNAME);
    return;
}

#pragma mark Different handler functions for interrupt bits
void IntelWiFiDriver::handleHardwareErrorINT() {
    //TODO: Implement handling hardware errors
    return;
}

void IntelWiFiDriver::handleRFKillINT() {
    //TODO: Implement handling RFKill interrupt
    //Remember update hwstats
    return;
}

void IntelWiFiDriver::handleWakeupINT() {
    //TODO: Implement handling wakeup interrupt
    //Check rxQueue and txQueue
    return;
}

void IntelWiFiDriver::handleRxINT() {
    //TODO: Implement handling Rx interrupt
    return;
}
