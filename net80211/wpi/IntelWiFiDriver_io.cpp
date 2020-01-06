//
//  IntelWiFiDriver_io.c
//  net80211
//
//  Created by Administrator on 06/01/2020.
//

#include "IntelWiFiDriver.hpp"
//==================================
//         Wrapper functions
//==================================
#define MASKBIT(b) (1UL << (b))
//Wrappers around IOPCIDevice->writex and IOPCIDevice->readx so we dont have to keep supplying memory map
void IntelWiFiDriver::busWrite32(uint32_t offset, uint32_t value) {
    deviceProps.device->ioWrite32(offset, value, deviceProps.deviceMemoryMap);
}

//Reads uint32 at offset and sets the bit at position bitPosition to 1
void IntelWiFiDriver::busSetBit(uint32_t offset, uint8_t bitPosition) {
    uint32_t value = busRead32(offset);
    value |= MASKBIT(bitPosition);
    busWrite32(offset, value);
}

void IntelWiFiDriver::busClearBit(uint32_t offset, uint8_t bitPosition) {
    uint32_t value = busRead32(offset);
    value &= ~MASKBIT(bitPosition);
    busWrite32(offset, value);
}

uint32_t IntelWiFiDriver::busRead32(uint32_t offset) {
    return deviceProps.device->ioRead32(offset, deviceProps.deviceMemoryMap);
}
//==================================

uint32_t IntelWiFiDriver::readPRHP(uint32_t offset) {
    u_long flags;
    uint32_t value = 0x5a5a5a5a;
    
    return 0;
}
