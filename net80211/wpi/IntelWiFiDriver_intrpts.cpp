//
//  IntelWifiDriver_intrpts.cpp
//  net80211
//
//  Created by Administrator on 22/10/2019.
//

#include "IntelWiFiDriver.hpp"

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
    
    //IO_LOG/IOLog not to be used in interrupt context
    
    uint32_t inta;
    bus_space_handle_t deviceBusPointer = (bus_space_handle_t)deviceProps.deviceMemoryMapVAddr;
    
    //Disable interrupts
    bus_space_write_4(NULL, deviceBusPointer, WPI_MASK, 0);
    //Read INTA register
    inta = bus_space_read_4(NULL, deviceBusPointer, WPI_INT);
    //VoodooIntel3945 uses r2 value too, not sure this relates to interrupts?
//    intb = bus_space_read_4(NULL, deviceBusPointer, WPI_FH_INT);
    
    if (inta == 0){
        //Interupt was not for us
        if (getInterface()->getFlags() & IFF_UP) bus_space_write_4(NULL, deviceBusPointer, WPI_MASK, WPI_INT_MASK);
        return 0;
    }
    
    if (inta == 0xFFFFFFFF || (inta & 0xFFFFFFF0 ) == 0xa5a5a5a0) {
        //Hardware has disappeared time to reset
        printf("%s: Harware gone INTA=0x%08x\n", DRVNAME, inta);
        return 0;
    }
    
    //Acknowledge interrupt recieved
    bus_space_write_4(NULL, deviceBusPointer, WPI_INT, inta | ~deviceProps.intaBitMask);
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
        
        
    }
    return 0;
}

void IntelWiFiDriver::disableInterrupts() {
    //TODO: Implement disabling interrupts
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
