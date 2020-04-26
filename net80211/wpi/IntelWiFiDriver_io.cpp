//
//  IntelWiFiDriver_io.c
//  net80211
//
//  Created by Administrator on 06/01/2020.
//

#include "IntelWiFiDriver.hpp"

const struct iwl_csr_params iwl_csr_v1 = {
    .flag_mac_clock_ready = 0,
    .flag_val_mac_access_en = 0,
    .flag_init_done = 2,
    .flag_mac_access_req = 3,
    .flag_sw_reset = 7,
    .flag_master_dis = 8,
    .flag_stop_master = 9,
    .addr_sw_reset = WPI_HW_IF_CONFIG + 0x020,
    .mac_addr0_otp = 0x380,
    .mac_addr1_otp = 0x384,
    .mac_addr0_strap = 0x388,
    .mac_addr1_strap = 0x38C
};

const struct iwl_csr_params iwl_csr_v2 = {
    .flag_init_done = 6,
    .flag_mac_clock_ready = 20,
    .flag_val_mac_access_en = 20,
    .flag_mac_access_req = 21,
    .flag_master_dis = 28,
    .flag_stop_master = 29,
    .flag_sw_reset = 31,
    .addr_sw_reset = WPI_HW_IF_CONFIG + 0x024,
    .mac_addr0_otp = 0x30,
    .mac_addr1_otp = 0x34,
    .mac_addr0_strap = 0x38,
    .mac_addr1_strap = 0x3C
};

//==================================
//         Wrapper functions
//==================================
//Wrappers around IOPCIDevice->writex and IOPCIDevice->readx so we dont have to keep supplying memory map
void IntelWiFiDriver::busWrite32(uint32_t offset, uint32_t value) {
    deviceProps.device->ioWrite32(offset, value, deviceProps.deviceMemoryMap);
}

void IntelWiFiDriver::busWrite8(uint16_t offest, uint8_t value) {
    deviceProps.device->ioWrite8(offest, value, deviceProps.deviceMemoryMap);
}

uint32_t IntelWiFiDriver::busRead32(uint32_t offset) {
    return deviceProps.device->ioRead32(offset, deviceProps.deviceMemoryMap);
}

uint32_t IntelWiFiDriver::busReadShr(uint32_t address) {
    busWrite32(HEEP_CTRL_WRD_PCIEX_CRTL_REG, ((address & 0x0000ffff) | (2 << 28)));
    return busRead32(HEEP_CTRL_WRD_PCIEX_DATA_REG);
}

void IntelWiFiDriver::busWriteShr(uint32_t address, uint32_t value) {
    busWrite32(HEEP_CTRL_WRD_PCIEX_DATA_REG, value);
    busWrite32(HEEP_CTRL_WRD_PCIEX_CRTL_REG, ((address & 0x0000ffff) | (3 << 28)));
}
//==================================

//==================================
//          IO Bit opertaions
//==================================
//Reads uint32 at offset and sets the bit at position bitPosition to 1, bitPosition specifies how much
//to leftshift 1ul by
void IntelWiFiDriver::busSetBit(uint32_t offset, uint8_t bitPosition) {
    uint32_t value = busRead32(offset);
    value |= BIT(bitPosition);
    busWrite32(offset, value);
}

//Reads value at offset and sets a number of bits, determined by bitPositions to 1, bitPositions has already been
//shifted (more for use with the flags set in if_wpireg.h
void IntelWiFiDriver::busSetBits(uint32_t offset, uint32_t bitPositions) {
    uint32_t value = busRead32(offset);
    value |= bitPositions;
    busWrite32(offset, value);
}

//Reads value at offset and clears that bit, bitPosition specifies how much
//to leftshift 1ul by
void IntelWiFiDriver::busClearBit(uint32_t offset, uint8_t bitPosition) {
    uint32_t value = busRead32(offset);
    value &= ~BIT(bitPosition);
    busWrite32(offset, value);
}

//Reads value at offset and clears the bits at bitPostions, bitPositions has already been
//shifted (more for use with the flags set in if_wpireg.h
void IntelWiFiDriver::busClearBits(uint32_t offset, uint32_t bitPositions) {
    uint32_t value = busRead32(offset);
    value &= ~bitPositions;
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


//===================================
//          PRPH Related
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

void IntelWiFiDriver::writePRPH(uint32_t offset, uint32_t value) {
    //iwl_write_prph
    IOInterruptState flags;
    if (grabNICAccess(flags)) {
        //Makes a call to iwl_write_prph_no_grap which calls a debug trace function
        //then calls tans->ops->write_prph which is set to iwl_trans_pcie_write_prph
        //iwl_trans_pcie_write_prph:
        uint32_t mask = getPRPHMask();
        busWrite32(WPI_PRPH_WADDR, (offset & mask) | (3 << 24));
        busWrite32(WPI_PRPH_WDATA, value);
        releaseNICAccess(flags);
    }
}

uint32_t IntelWiFiDriver::getPRPHMask() {
    //iwl_trans_pcie_prph_msk
    if (deviceProps.deviceConfig->device_family >= IWL_DEVICE_FAMILY_22560) {
        return 0x00ffffff;
    }
    return 0x000fffff;
}

void IntelWiFiDriver::writeUMAC_PRPH(uint32_t offset, uint32_t value) {
    //iwl_write_umac_prph
    writePRPH(offset + deviceProps.deviceConfig->umac_prph_offset, value);
}

void IntelWiFiDriver::setBitsPRPH(uint32_t offset, uint32_t mask) {
    //iwl_set_bits_prph
    //TODO: Implement
}

void IntelWiFiDriver::clearBitsPRPH(uint32_t offset, uint32_t mask) {
    //iwl_clear_bits_prph
    //TODO: Implement
}

//===================================

int IntelWiFiDriver::readIOMemToBuffer(uint32_t address, void* buffer, int dwords) {
    IOInterruptState flags;
    
    if (grabNICAccess(flags)) {
        //Might seem as though reading from WPI_MEM_RDATA would keep returning the same
        //values, but once WPI_MEM_RADDR is set, each time WPI_MEM_RDATA is read the
        //value is incremented by one dword
        busWrite32(WPI_MEM_RADDR, address);
        uint32_t* buff = (uint32_t*)buffer;
        for (int offsets = 0; offsets < dwords; offsets++) {
            buff[offsets] = busRead32(WPI_MEM_RDATA);
        }
        releaseNICAccess(flags);
    } else {
        return -EBUSY;
    }
    return 0;
}
