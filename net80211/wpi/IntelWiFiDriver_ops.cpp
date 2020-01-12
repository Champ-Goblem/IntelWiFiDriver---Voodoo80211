//
//  IntelWiFiDriver_opps.cpp
//  net80211
//
//  Created by Administrator on 09/01/2020.
//

#include "IntelWiFiDriver.hpp"
#include "iwlwifi_headers/iwl-fh.h"

//===================================
//      MVM only card operations
//===================================
void IntelWiFiDriver::reportScanAborted() {
    //iwl_mvm_report_scan_aborted
    //TODO: Implement reporting scan abborted
}
//===================================

void IntelWiFiDriver::rxMultiqueueRestock() {
    struct iwl_rx_mem_buffer* rxmb;
    struct iwl_rxq* rxq = deviceProps.rxq;
    if (!deviceProps.status.deviceNotAvailable) {
        return;
    }
    
    IOSimpleLockLock(rxq->lock);
    rxmb = TAILQ_FIRST(rxq->rx_free);
    while (rxq->free_count) {
        //Get next element and remove
        rxmb = TAILQ_NEXT(rxmb, list);
        TAILQ_REMOVE(rxq->rx_free, rxmb, list);
        
        rxmb->invalid = false;
        //Check that 12 first bits are expected to be empty
        //Might be worth adding some sort of reference here to trace this back
        if (!(rxmb->page_dma & DMA_BIT_MASK(12))) printf("%s: rx_mem_buffer page_dma first 12 bits not empty\n", DRVNAME);
        //"Point to Rx buffer ia next RBD in circular buffer"
        restockRBD(rxmb);
        rxq->write = (rxq->write + 1) & MQ_RX_TABLE_MASK;
        rxq->free_count--;
    }
    IOSimpleLockUnlock(rxq->lock);
    
    //Tell the device if we have added more space for firmware to place data
    //Increment write pointer in multiples of 8
    if (rxq->write_actual != (rxq->write & ~0x7)) {
        IOSimpleLockLock(rxq->lock);
        rxQueueIncrementWritePointer();
        IOSimpleLockUnlock(rxq->lock);
    }
}

void IntelWiFiDriver::restockRBD(struct iwl_rx_mem_buffer* rxmb) {
    //iwl_pcie_restock_bd
    struct iwl_rxq* rxq = deviceProps.rxq;
    if (deviceProps.deviceConfig->device_family >= IWL_DEVICE_FAMILY_22560) {
        struct iwl_rx_transfer_desc* bd = (iwl_rx_transfer_desc*)rxq->bd;
        
        //iwlwifi uses BUILD_BUG_ON to check sizing of bd
        
        bd[rxq->write].addr = cpu_to_le64(rxmb->page_dma);
        bd[rxq->write].rbid = cpu_to_le16(rxmb->vid);
    } else {
        unsigned long long* bd = (unsigned long long*)rxq->bd;
        
        bd[rxq->write] = cpu_to_le64(rxmb->page_dma | rxmb->vid);
    }
    
    if (DEBUG) printf("%s: Assigned virtual RB ID %u to queue %d index %d\n", DRVNAME,
                      (uint32_t)rxmb->vid, rxq->id, rxq->write);
}

void IntelWiFiDriver::rxQueueIncrementWritePointer() {
    //iwl_pcie_rxq_inc_wr_ptr
    
    if (!deviceProps.deviceConfig->base_params->shadow_reg_enable &&
        deviceProps.status.deviceAsleep) {
        uint32_t reg = busRead32(WPI_UCODE_DRV_GP1);
        
        //To quote iwlwifi:
        /*
         *"explicitly wake up the NIC if:
         * 1. shadow registers aren't enabled
         * 2. there is a chance that the NIC is asleep"
         */
        if (reg & WPI_UCODE_DRV_GP1_BIT_MAC_SLEEP) {
            if (DEBUG) printf("%s: RX queue requesting wakeup, GP1=0x%x\n", DRVNAME, reg);
            busSetBit(WPI_GP_CNTRL, deviceProps.deviceConfig->csr->flag_mac_access_req);
            deviceProps.rxq->need_update = true;
            return;
        }
        
        deviceProps.rxq->write_actual = round_down(deviceProps.rxq->write, 8);
        if (deviceProps.deviceConfig->device_family == IWL_DEVICE_FAMILY_22560) {
            busWrite32(WPI_HBUS_TARG_WRPTR, deviceProps.rxq->write_actual |
                       ((FIRST_RX_QUEUE + deviceProps.rxq->id) << 16));
        } else if (deviceProps.deviceConfig->mq_rx_supported){
            busWrite32(RFH_Q_FRBDCB_WIDX_TRG(deviceProps.rxq->id), deviceProps.rxq->write_actual);
        } else {
            busWrite32(FH_RSCSR_CHNL0_WPTR, deviceProps.rxq->write_actual);
        }
    }
}
