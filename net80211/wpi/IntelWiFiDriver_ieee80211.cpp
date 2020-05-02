//
//  IntelWiFiDriver_ieee80211.cpp
//  net80211
//
//  Created by Administrator on 02/05/2020.
//

#include "IntelWiFiDriver.hpp"
#include "../ieee80211_proto.h"

int IntelWiFiDriver::ieee80211_newstate(struct ieee80211com *ic, enum ieee80211_state nstate, int mgt) {
    //From if_wpi
    //TODO: Implement
//    switch (nstate) {
//        case IEEE80211_S_SCAN:
//            <#statements#>
//            break;
//            
//        default:
//            break;
//    }
    
    return Voodoo80211Device::ieee80211_newstate(ic, nstate, mgt);
}
