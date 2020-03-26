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
#include "IntelWiFiDriver_notif.hpp"

#include "iwlwifi_headers/deviceConfigs.h"
#include "iwlwifi_headers/internals.h"
#include "iwlwifi_headers/iwl-prph.h"
//#include "iwlwifi_headers/mvm.h"

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
    void busWrite8(uint16_t offfset, uint8_t value);
    uint32_t busRead32(uint32_t offset);
    void write32Direct(volatile void* base, uint32_t offset, uint32_t value);
    uint32_t read32Direct(volatile void* base, uint32_t offset);
    void busSetBit(uint32_t offset, uint8_t bitPosition);
    void busClearBit(uint32_t offset, uint8_t bitPosition);
    int pollBit(uint32_t offset, uint32_t bits, uint32_t mask, int timeout);
    uint32_t readPRPH(uint32_t offset);
    void writePRPH(uint32_t offset, uint32_t value);
    uint32_t getPRPHMask();
    void writeUMAC_PRPH(uint32_t offset, uint32_t value);
    int readIOMemToBuffer(uint32_t address, void* buffer, int dwords);
    
#pragma mark Device communication functions (IntelWiFiDriver_comms.cpp)
    bool grabNICAccess(IOInterruptState flags);
    void releaseNICAccess(IOInterruptState flags);
    void restartHardware();
    bool isRFKillSet();
    void resetDevice();
    bool prepareCardHardware();
    int setHardwareReady();
    void unref();
    
#pragma mark MVM only functions (IntelWiFiDriver_mvm.cpp)
    void receivedNICError();
    void forceNICRestart(bool firmwareError);
    void setHardwareRFKillState(bool status);
    void setRFKillState(bool status);
    void freeSKB(mbuf_t skb);
    
#pragma mark Notification wait helpers (IntelWiFiDriver_notif.cpp)
    void abortNotificationWaits();
    
#pragma mark NIC operations (IntelWiFiDriver_opps.cpp)
    void reportScanAborted();
    void rxMultiqueueRestock();
    void restockRBD(struct iwl_rx_mem_buffer* rxmb);
    void rxQueueIncrementWritePointer();
    void stopDeviceG2(bool setLowPowerSate);
    void stopDeviceG1(bool setLowPowerState);
    void txStopG2();
    void rxStop();
    void apmStopG2(bool opModeLeave);
    void configureMSIX();
    void txStopG1();
    void apmStopG1(bool opModeLeave);
    void unmapTxQ(int txqID);
    void freeTSOPage(mbuf_t skb);
    void txQFreeTDF(struct iwl_txq* txq);
    void unmapTFD(struct iwl_cmd_meta* meta, struct iwl_txq* txq, int index);
    void mapRxCauses();
    void mapNonRxCauses();
    void* getTFD(struct iwl_txq* txq, int index);
    uint8_t TFDGetNumberOfTBS(void* __tfd);
    bus_addr_t TFDGetTBAddress(void* __tfd, int index);
    uint16_t TFDGetTBLength(void* __tfd, int index);
    int queueIncWrap(int index);
    void clearCommandInFlight();
    
#pragma mark CTXT related stuff (IntelWiFiDriver_ctxt.cpp)
    void ctxtInfoFreePaging();
    void ctxtInfoFreeG3();
    void ctxtInfoFree();
    
#pragma mark Debugging (IntelWiFiDriver_debug.cpp)
    void printRefCounts();
    hardwareDebugStatisticsCounters hwStats;
    void updateHardwareDebugStatistics(enum hardwareDebugStatistics updateStat, uint32_t value);
    void dumpHardwareRegisters();
    void dumpNICErrorLog();
    void collectFirmwareErrorDetails();
    void firmwareDebugStopRecording();
//    void taggedRetain(const void* tag) const;
//    void taggedRelease(const void* tag) const;
};

#endif /* IntelWiFiDriver_hpp */
