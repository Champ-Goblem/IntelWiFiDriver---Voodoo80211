//
//  Firmware.hpp
//  net80211
//
//  Created by Administrator on 09/01/2020.
//

#ifndef Firmware_h
#define Firmware_h
#include "uCode_api.hpp"
enum UCodeType {
    REGULAR,            //IWL_UCODE_REGUALR
    INIT,               //IWL_UCODE_INIT
    WOWLAN,             //IWL_UCODE_WOWLAN
    REGULAR_USNIFFER,   //IWL_UCODE_REGULAR_USNIFFER
    TYPE_MAX            //IWL_UCODE_TYPE_MAX
};

/**
 * struct FirmwareCommandVersion - firmware command version entry
 * @cmd: command ID
 * @group: group ID
 * @cmd_ver: command version
 * @notif_ver: notification version
 */
struct FirmwareCommandVersion {
    uint8_t cmd;
    uint8_t group;
    uint8_t cmd_ver;
    uint8_t notif_ver;
} __packed;

struct UCodeCapabilities {
    uint32_t maxProbeLenght;
    uint32_t scanChannelsCount;
    uint32_t standardPhyCalibrationSize;
    uint32_t flags;
    uint32_t errorLogAddress; //Could probably be changed to a pointer or something similar
    uint32_t errorLogSize;
    u_long   api[BITS_TO_LONGS(NUM_UCODE_TLV_API)];
    u_long   capa[BITS_TO_LONGS(NUM_UCODE_TLV_CAPA)];
    
    struct FirmwareCommandVersion commandVersions;
    uint32_t commandVersionsCount;
};

struct FirmwareData {
    struct UCodeCapabilities uCodeCapabilities;
};

struct FirmwareRuntimeData {
    enum UCodeType microcodeType;
    
    struct {
        IOTimerEventSource* periodicTrigger; //Seems to be similar to timer_list
    } dump;
};
#endif /* Firmware_h */
