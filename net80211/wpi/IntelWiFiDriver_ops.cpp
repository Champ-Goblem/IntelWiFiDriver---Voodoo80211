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
    //iwl_pcie_rxmq_restock
    struct iwl_rx_mem_buffer* rxmb;
    struct iwl_rxq* rxq = deviceProps.rxq;
    if (!deviceProps.status.deviceEnabled) {
        return;
    }
    
    IOSimpleLockLock(rxq->lock);
    rxmb = TAILQ_FIRST(rxq->rx_free);
    while (rxq->free_count) {
        //Remove the current element
        TAILQ_REMOVE(rxq->rx_free, rxmb, list);
        
        rxmb->invalid = false;
        //Check that 12 first bits are expected to be empty
        //Might be worth adding some sort of reference here to trace this back
        if (!(rxmb->page_dma & DMA_BIT_MASK(12))) printf("%s: rx_mem_buffer page_dma first 12 bits not empty\n", DRVNAME);
        //"Point to Rx buffer ia next RBD in circular buffer"
        restockRBD(rxmb);
        rxq->write = (rxq->write + 1) & MQ_RX_TABLE_MASK;
        rxq->free_count--;
        //Get the next element
        rxmb = TAILQ_NEXT(rxmb, list);
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
//===================================
//      Gen2 device operations
//===================================
//Stop a gen2 device or put it in a low power state
void IntelWiFiDriver::stopDeviceG2(bool setLowPowerState) {
    //_iwl_trans_pcie_gen2_stop_device
    //TODO: Implement
    if (deviceProps.isDown) return;
    deviceProps.isDown = true;
    
    //Call stop dbgc
    firmwareDebugStopRecording();
    
    //Disable interrupts for the device
    disableInterrupts();
    
    //TODO: if using ICT call to ICT stop here
    
    //If the device hasn't already been stopped or not started
    //yet then we need to remove both the rx and tx queues
    if (deviceProps.status.deviceEnabled) {
        deviceProps.status.deviceEnabled = false;
        txStopG2();
        rxStop();
    }
    
    if (deviceProps.deviceConfig->device_family >= IWL_DEVICE_FAMILY_22560) {
        ctxtInfoFreeG3();
    } else {
        ctxtInfoFree();
    }
    
    //Make sure that we have released the request for the device to stay awake
    busClearBit(WPI_GP_CNTRL, BIT(deviceProps.deviceConfig->csr->flag_mac_access_req));
    
    //Stop the device and put it in a low power state
    apmStopG2();
    resetDevice();
    
    //From iwlwifi:
    /*
     * Upon stop, the IVAR table gets erased, so msi-x won't
     * work. This causes a bug in RF-KILL flows, since the interrupt
     * that enables radio won't fire on the correct irq, and the
     * driver won't be able to handle the interrupt.
     * Configure the IVAR table again after reset.
     */
    configureMSIX();
    
    //From iwlwifi:
    /*
     * Upon stop, the APM issues an interrupt if HW RF kill is set.
     * This is a bug in certain verions of the hardware.
     * Certain devices also keep sending HW RF kill interrupt all
     * the time, unless the interrupt is ACKed even if the interrupt
     * should be masked. Re-ACK all the interrupts here.
     */
    disableInterrupts();
    
    //Reset some of the statuses associated with the card
    deviceProps.status.syncHCMDActive = false;
    deviceProps.status.interruptsEnabled = false;
    deviceProps.status.PMI_TPower = false;
    
    //Even if the device is stopped, we still want it to be able to respond
    //to the RF kill interrupt
    enableRFKillINT();
    
    //iwlwifi talkes about maintaining ownership to the device, but I think
    //we will still have ownership so long as this driver is not unloaded
    //but implementing below should confirm either yes or no
    //The hardware will still need to be prepared anyway so might as well call it
    prepareCardHardware();
}

void IntelWiFiDriver::txStopG2() {
    //iwl_pcie_gen2_tx_stop
    //TODO: Implement
    
}

void IntelWiFiDriver::apmStopG2() {
    //iwl_pcie_gen2_apm_stop
    //TODO: Implement
}
//===================================


//===================================
//       Gen1 device operations
//===================================

//Stop a gen1 device or put it in a low power state
void IntelWiFiDriver::stopDeviceG1(bool setLowPowerState) {
    //_iwl_trans_pcie_stop_device
    //TODO: Implement
}
//===================================

void IntelWiFiDriver::rxStop() {
    //iwl_pcie_rx_stop
    //TODO: Implement
}

void IntelWiFiDriver::configureMSIX() {
    //iwl_pcie_conf_msix_hw
    //TODO: Implement
}
