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
#include "DrvStructs.h"
#include "iwlwifi_headers/deviceConfigs.h"
#include <os/log.h>

#define DRVNAME "net80211"
#define PCI_VENDOR_ID_INTEL 0x8086
#define LOG_ERROR(string...) os_log_error(OS_LOG_DEFAULT,string)
#define IO_LOG(string...) IOLog(string)

class IntelWiFiDriver : public Voodoo80211Device {
    OSDeclareDefaultStructors(IntelWiFiDriver)
protected:
    virtual bool device_attach(void *aux);
    virtual int device_detach(int);
private:
     PCIDevice* deviceProps;
    
    void releaseDeviceAllocs();
    int interruptHandler(OSObject* owner, IOInterruptEventSource* sender);
    int setDeviceCFG(uint16_t deviceID, uint16_t ss_deviceID);
};

#endif /* IntelWiFiDriver_hpp */
