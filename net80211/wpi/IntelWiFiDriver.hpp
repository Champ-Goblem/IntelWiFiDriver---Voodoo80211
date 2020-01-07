//
//  IntelWiFiDriver.hpp
//  net80211
//
//  Created by Administrator on 09/09/2019.
//

#ifndef IntelWiFiDriver_hpp
#define IntelWiFiDriver_hpp

#include "../Voodoo80211Device.h"
#include "if_wpireg.h"
#include "DrvStructs.hpp"
#include "iwlwifi_headers/deviceConfigs.h"
#include "iwlwifi_headers/internals.h"

#include <os/log.h>
#include <libkern/OSDebug.h>
#include <IOKit/system.h>
//#include <IOKit/IOFilterInterruptEventSource.h>

#define DRVNAME "net80211"
#define PCI_VENDOR_ID_INTEL 0x8086
#define LOG_ERROR(string...) os_log_error(OS_LOG_DEFAULT,string)
#define IO_LOG(string...) IOLog(string)

//Set the debug flag, might not be needed
#ifdef DEBUG
#define DEBUG 1
#else
#define DEBUG 0
#endif

class IntelWiFiDriver : public Voodoo80211Device {
    OSDeclareDefaultStructors(IntelWiFiDriver)
    
protected:
    virtual bool device_attach(void *aux);
    virtual int device_detach(int);
private:
     PCIDevice deviceProps;
    
    void releaseDeviceAllocs();
    int setDeviceCFG(uint16_t deviceID, uint16_t ss_deviceID);
    
#pragma mark Interrupt functions (IntelWiFiDriver_intrpts.cpp)
    int interruptHandler(OSObject* owner, IOInterruptEventSource* sender, int count);
    void disableInterrupts();
    void enableInterrupts();
    void enableFirmwareLoadINT();
    void enableRFKillINT();
    void enableCTXInfoINT();
    void handleHardwareErrorINT();
    void handleRFKillINT();
    void handleWakeupINT();
    void handleRxINT();
    
#pragma mark IO functions (IntelWiFiDriver_io.cpp)
    void busWrite32(uint32_t offset, uint32_t value);
    uint32_t busRead32(uint32_t offset);
    void write32Direct(volatile void* base, uint32_t offset, uint32_t value);
    uint32_t read32Direct(volatile void* base, uint32_t offset);
    void busSetBit(uint32_t offset, uint8_t bitPosition);
    void busClearBit(uint32_t offset, uint8_t bitPosition);
    int pollBit(uint32_t offset, uint32_t bits, uint32_t mask, int timeout);
    uint32_t readPRPH(uint32_t offset);
    uint32_t getPRPHMask();
    
#pragma mark Device communication functions (IntelWiFiDriver_comms.cpp)
    bool grabNICAccess(IOInterruptState flags);
    void releaseNICAccess(IOInterruptState flags);
    
#pragma mark Debugging (IntelWiFiDriver_debug.cpp)
    void printRefCounts();
    hardwareDebugStatisticsCounters hwStats;
    void updateHardwareDebugStatistics(enum hardwareDebugStatistics updateStat, uint32_t value);
    void dumpHardwareRegisters();
//    void taggedRetain(const void* tag) const;
//    void taggedRelease(const void* tag) const;
};

#endif /* IntelWiFiDriver_hpp */
