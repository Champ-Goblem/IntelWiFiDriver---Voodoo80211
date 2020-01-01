//
//  IntelWifiDriver_intrpts.cpp
//  net80211
//
//  Created by Administrator on 22/10/2019.
//

#include "IntelWiFiDriver.hpp"

//bool IntelWiFiDriver::interruptFilter(IOFilterInterruptEventSource* source) {
//
//}

int IntelWiFiDriver::interruptHandler(OSObject* owner, IOInterruptEventSource* sender, int count) {
    uint32_t inta, intb;
    inta = bus_space_read_4(NULL, deviceProps.deviceMemoryMapVAddr, WPI_INT);
    intb = bus_space_read_4(NULL, deviceProps.deviceMemoryMapVAddr, WPI_FH_INT);
    
    
}
