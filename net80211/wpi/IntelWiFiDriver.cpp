//
//  IntelWiFiDriver.cpp
//  net80211
//
//  Created by Administrator on 09/09/2019.
//

#include "IntelWiFiDriver.hpp"
//#include "IntelWiFiDriver_cfg.h"
OSDefineMetaClassAndStructors(IntelWiFiDriver, Voodoo80211Device)

#pragma mark Setup
bool IntelWiFiDriver::device_attach(void *aux) {
    IO_LOG("%s: Device Attach", DRVNAME);
    //Get and set our device
    struct pci_attach_args *pa = (struct pci_attach_args*)aux;
    //IOPCIDevice* dev = OSDynamicCast(IOPCIDevice, pa->pa_tag);
    IOPCIDevice* dev = pa->pa_tag;
    if (!dev) {
        LOG_ERROR("%s: Failed to get initial setup\n", DRVNAME);
        return false;
    }
    deviceProps = new PCIDevice;
    deviceProps->device = dev;
    
    //Read vendor and device ID from both subsystem and system
    uint16_t vendorID = dev->configRead16(kIOPCIConfigVendorID);
    uint16_t deviceID = dev->configRead16(kIOPCIConfigDeviceID);
    uint16_t ss_vendorID = dev->configRead16(kIOPCIConfigSubSystemVendorID);
    uint16_t ss_deviceID = dev->configRead16(kIOPCIConfigSubSystemID);
    
    //Check vendor IDs and error accordingly
    if (vendorID != PCI_VENDOR_ID_INTEL) {
        LOG_ERROR("%s: Provided device not compatible with driver, wrong vendor ID (vid:0x%04x, did:0x%04x)\n", \
                  DRVNAME, vendorID, deviceID);
        return false;
    }
    
    if (ss_vendorID != PCI_VENDOR_ID_INTEL) {
        LOG_ERROR("%s: Found matching device with vendor ID 8086 but wrong subsytem vendor ID (did:0x%04x ss_vid:0x%04x ss_did:0x%04x)\n", \
                  DRVNAME, deviceID, ss_vendorID, ss_deviceID);
        return false;
    }
    
    IO_LOG("%s: Got device (did:0x%04x, ss_did:0x%04x)\n", DRVNAME, deviceID, ss_deviceID);
    
    //Get the PCI Capability structure offset and store it
    int error = pci_get_capability(NULL, dev, PCI_CAP_PCIEXPRESS, &deviceProps->capabilitiesStructOffset, NULL);
    if (error == 0) {
        LOG_ERROR("%s: PCIe capability structure not found!\n", DRVNAME);
        return false;
    }
    
    //Set our devices iwl_cfg structure
    error = setDeviceCFG(deviceID, ss_deviceID);
    if (error) {
        return false;
    }
    
    //Allocate our device memory and store the pointer to it in our structure
    deviceProps->deviceMemoryMap = dev->mapDeviceMemoryWithRegister(kIOPCIConfigBaseAddress0);
    if (!deviceProps->deviceMemoryMap) {
        LOG_ERROR("%s: Could not get memory map for device\n", DRVNAME);
        return false;
    }
    IO_LOG("%s: Mapped device memory at vmAddr:0x%llx, size:%llu\n", DRVNAME, deviceProps->deviceMemoryMap->getAddress(), \
           deviceProps->deviceMemoryMap->getSize());    
    
    //Install our interrupt handler
    
    return true;
}

int IntelWiFiDriver::setDeviceCFG(uint16_t deviceID, uint16_t ss_deviceID) {
    UInt32 i;
    const struct iwl_cfg* devCfg;
    for(i=0; i<sizeof(wifi_card_ids) / sizeof(wifi_card); i++) {
        if(wifi_card_ids[i].device == deviceID && wifi_card_ids[i].subdevice == ss_deviceID)
            devCfg = wifi_card_ids[i].config;
    }
    
    if (!devCfg) {
        LOG_ERROR("%s: Failed to get cards config, might not be an MVM compatible card!", DRVNAME);
        return 1;
    }
    IO_LOG("%s: Got cards config (%s)", DRVNAME, devCfg->name);
    deviceProps->deviceConfig = devCfg;
    return 0;
}

#pragma mark PCIDevice detach and clearing
int IntelWiFiDriver::device_detach(int flags) {
    releaseDeviceAllocs();
    return 0;
}

void IntelWiFiDriver::releaseDeviceAllocs() {
    if (deviceProps->deviceMemoryMap) {
        IO_LOG("releasing memory map");
        deviceProps->deviceMemoryMap->release();
    }
    deviceProps->deviceMemoryMap = 0;
    deviceProps->deviceConfig = 0;
    deviceProps->workLoop = 0;
    deviceProps->device = 0;
    deviceProps->capabilitiesStructOffset = 0;
    deviceProps = 0;
    IO_LOG("%s: Released all items in device struct", DRVNAME);
}
