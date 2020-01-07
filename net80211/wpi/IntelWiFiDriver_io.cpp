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
//Wrappers around IOPCIDevice->writex and IOPCIDevice->readx so we dont have to keep supplying memory map
void IntelWiFiDriver::busWrite32(uint32_t offset, uint32_t value) {
    deviceProps.device->ioWrite32(offset, value, deviceProps.deviceMemoryMap);
}

uint32_t IntelWiFiDriver::busRead32(uint32_t offset) {
    return deviceProps.device->ioRead32(offset, deviceProps.deviceMemoryMap);
}
//==================================

//==================================
//          IO Bit opertaions
//==================================
//Reads uint32 at offset and sets the bit at position bitPosition to 1
void IntelWiFiDriver::busSetBit(uint32_t offset, uint8_t bitPosition) {
    uint32_t value = busRead32(offset);
    value |= BIT(bitPosition);
    busWrite32(offset, value);
}

void IntelWiFiDriver::busClearBit(uint32_t offset, uint8_t bitPosition) {
    uint32_t value = busRead32(offset);
    value &= ~BIT(bitPosition);
    busWrite32(offset, value);
}

int IntelWiFiDriver::pollBit(uint32_t offset, uint32_t bits, uint32_t mask, int timeout) {
#define POLL_INTERVAL 10
    int t = 0;
    do {
        if ((busRead32(offset) & mask) == (bits & mask)) {
            return t;
        }
        udelay(POLL_INTERVAL);
        t += POLL_INTERVAL;
    } while (t < timeout);
    return -ETIMEDOUT;
}
//===================================

uint32_t IntelWiFiDriver::readPRPH(uint32_t offset) {
    IOInterruptState flags;
    uint32_t value = 0x5a5a5a5a;
    
    if (grabNICAccess(flags)) {
        uint32_t mask = getPRPHMask();
        busWrite32(WPI_PRPH_RADDR, ((offset & mask) | 3 << 24));
        value = busRead32(WPI_PRPH_RDATA);
        releaseNICAccess(flags);
    }
    return value;
}

uint32_t IntelWiFiDriver::getPRPHMask() {
    if (deviceProps.deviceConfig->device_family >= IWL_DEVICE_FAMILY_22560) {
        return 0x00ffffff;
    }
    return 0x000fffff;
}
