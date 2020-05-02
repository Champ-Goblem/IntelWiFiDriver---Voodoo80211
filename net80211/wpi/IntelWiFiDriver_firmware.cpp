//
//  IntelWiFiDriver_firmware.cpp
//  net80211
//
//  Created by Administrator on 01/05/2020.
//

#include "IntelWiFiDriver.hpp"

bool IntelWiFiDriver::checkFWCapabilities(iwl_ucode_tlv_capa capabilities) {
    //fw_has_capa
    return test_bit(capabilities, deviceProps.mvm.fw->ucode_capa._capa);
}
