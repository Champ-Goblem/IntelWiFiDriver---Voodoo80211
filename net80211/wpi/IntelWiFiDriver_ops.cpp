//
//  IntelWiFiDriver_opps.cpp
//  net80211
//
//  Created by Administrator on 09/01/2020.
//

#include "IntelWiFiDriver.hpp"
#include "iwlwifi_headers/iwl-fh.h"
#include "IntelWiFiDriver_ops.hpp"

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
    busClearBit(WPI_GP_CNTRL, deviceProps.deviceConfig->csr->flag_mac_access_req);
    
    //Stop the device and put it in a low power state
    apmStopG2(false);
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
    
    bzero(deviceProps.txq_stopped, sizeof(deviceProps.txq_stopped));
    bzero(deviceProps.txq_used, sizeof(deviceProps.txq_used));
    
    for (int txqID = 0; txqID < ARRAY_SIZE(deviceProps.txQueues); txqID++) {
        if (!deviceProps.txQueues[txqID]) continue;
        unmapTxQG2(txqID);
    }
    
}

void IntelWiFiDriver::apmStopG2(bool opModeLeave) {
    //iwl_pcie_gen2_apm_stop
    if (DEBUG) printf("%s: Stopping card, entering low power state\n", DRVNAME);
    
    if (opModeLeave) {
        if (!deviceProps.status.deviceEnabled) apmInitG2();
        
        busSetBits(WPI_DBG_LINK_PWR_MGMT_REG, CSR_RESET_LINK_PWR_MGMT_DISABLED);
        busSetBits(WPI_HW_IF_CONFIG, CSR_HW_IF_CONFIG_REG_PREPARE | CSR_HW_IF_CONFIG_REG_ENABLE_PME);
        IOSleep(1);
        busClearBits(WPI_DBG_LINK_PWR_MGMT_REG, CSR_RESET_LINK_PWR_MGMT_DISABLED);
        IOSleep(5);
    }
    
    deviceProps.status.deviceEnabled = false;
    
    apmStopMaster();
    
    resetDevice();
    
    busClearBit(WPI_GP_CNTRL, deviceProps.deviceConfig->csr->flag_init_done);
}

void IntelWiFiDriver::unmapTxQG2(int txqID) {
    //iwl_pcie_gen2_txq_unmap
    struct iwl_txq *txq = deviceProps.txQueues[txqID];
    
    IOSimpleLockLock(txq->lock);
    while (txq->write_ptr != txq->read_ptr) {
        if (DEBUG) printf("%s: txq %d freed %d\n", DRVNAME, txqID, txq->read_ptr);
        
        if (txqID != deviceProps.commandQueue) {
            int idx = getCommandIndex(txq, txq->read_ptr);
            mbuf_t skb = txq->entries[idx].skb;
            
            if (!skb) LOG_ERROR("%s: skb not allocates, id: %d, idx: %d\n", DRVNAME, txqID, idx);
            freeTSOPage(skb);
        }
        
        freeTFDG2(txq);
        txq->read_ptr = queueIncWrap(txq->read_ptr);
        
        if (txq->read_ptr == txq->write_ptr) {
            IOInterruptState flags = IOSimpleLockLockDisableInterrupt(deviceProps.NICAccessLock);
            if (txqID != deviceProps.commandQueue) {
                if (DEBUG) printf("%s: txq %d, last tx freed\n", DRVNAME, txqID);
                unref();
            } else if (deviceProps.comandInFlight) {
                deviceProps.comandInFlight = false;
                if (DEBUG) printf("%s: cleared command in flight status\n", DRVNAME);
                unref();
            }
            IOSimpleLockUnlockEnableInterrupt(deviceProps.NICAccessLock, flags);
        }
    }
    
    //While loop here calls iwl_op_mode_free_skb, we dont need to free each skb
    //as we can just free the whole lot
    mbuf_freem_list(txq->overflow_q);
    
    IOSimpleLockUnlock(txq->lock);
    
    wakeQueue(txq);
}

//Startup the NICs basic functionailty after reset
//This does not load uCode or start embedded processor
int IntelWiFiDriver::apmInitG2() {
    //iwl_pcie_gen2_apm_init
    if (DEBUG) printf("%s: APM init, starting cards basic functions\n", DRVNAME);
    
    /*
     * Use "set_bit" below rather than "write", to preserve any hardware
     * bits already set by default after reset.
     */
    
    /*
     * Disable L0s without affecting L1;
     * don't wait for ICH L0s (ICH bug W/A)
     */
    busSetBits(WPI_GIO_CHICKEN, WPI_GIO_CHICKEN_L1A_NO_L0S_RX);
    
    //Set FH wait thresholf to maximum (HW error during stress W/A)
    busSetBits(WPI_DBG_HPET_MEM_REG, CSR_DBG_HPET_MEM_REG_VAL);
    
    /*
     * Enable HAP INTA (interrupt from management bus) to
     * wake device's PCI Express link L1a -> L0s
     */
    busSetBits(WPI_HW_IF_CONFIG, CSR_HW_IF_CONFIG_REG_BIT_HAP_WAKE_L1A);
    
    apmConfig();
    
    int ret = finishNICInit();
    if (ret) return ret;
    
    deviceProps.status.deviceEnabled = true;
    
    return 0;
}

void IntelWiFiDriver::freeTFDG2(struct iwl_txq* txq) {
    //iwl_pcie_gen2_free_tfd

    //rd_prt is bounded by TFD_QUEUE_SIZE_MAX and
    //idx is bounded by n_widow
    int idx = getCommandIndex(txq, txq->id);
    
    unmapTFDG2(&txq->entries[idx].meta, (iwl_tfh_tfd*)getTFD(txq, idx));
    
    if (txq->entries) {
        mbuf_t skb = txq->entries[idx].skb;
        
        if (skb) {
            mbuf_freem_list(skb);
            txq->entries[idx].skb = NULL;
        }
    }
}

void IntelWiFiDriver::unmapTFDG2(struct iwl_cmd_meta* meta, struct iwl_tfh_tfd* tfd) {
    //iwl_pcie_gen2_tfd_unmap
    //TODO: Implement
    
    int numTBS = getNumTBSG2(tfd);
    
    if(numTBS > deviceProps.maxTBS) {
        LOG_ERROR("%s: Too many chunks: %i\n", DRVNAME, numTBS);
        return;
    }
    
    for (int i = 1; i < numTBS; i++) {
        if (meta->tbs & BIT(i)) {
            //TODO: Come back to here once DMA is implemented
        }
    }
}

int IntelWiFiDriver::getNumTBSG2(struct iwl_tfh_tfd* tfd) {
    //iwl_pcie_gen2_get_num_tbs
    return le16_to_cpu(tfd->num_tbs) & 0x1f;
}
//===================================


//===================================
//       Gen1 device operations
//===================================

//Stop a gen1 device or put it in a low power state
void IntelWiFiDriver::stopDeviceG1(bool setLowPowerState) {
    //_iwl_trans_pcie_stop_device
    if (deviceProps.isDown) return;
    deviceProps.isDown = true;
    
    //Stop debug recording
    firmwareDebugStopRecording();
    
    //disable interrupts as we are putting device in sleep
    disableInterrupts();
    
    //From iwlwifi
    /*
     * If a HW restart happens during firmware loading,
     * then the firmware loading might call this function
     * and later it might be called again due to the
     * restart. So don't process again if the device is
     * already dead.
     */
    if (deviceProps.status.deviceEnabled) {
        deviceProps.status.deviceEnabled = false;
        txStopG1();
        rxStop();
        
        //Power down device's busmaster DMA checks
        if (!deviceProps.deviceConfig->apmg_not_supported) {
            writePRPH(WPI_APMG_CLK_DIS, WPI_APMG_CLK_DMA_CLK_RQT);
            udelay(5);
        }
    }
    
    //Ensure we release our request to stay awake
    busClearBit(WPI_GP_CNTRL, deviceProps.deviceConfig->csr->flag_mac_access_req);
    
    //Put the device into a lower power state
    apmStopG1(false);
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
    
    //Re-enable the rf kill interrupt so we can still service the card
    //being enabled
    enableRFKillINT();
    
    //iwlwifi talkes about maintaining ownership to the device, but I think
    //we will still have ownership so long as this driver is not unloaded
    //but implementing below should confirm either yes or no
    //The hardware will still need to be prepared anyway so might as well call it
    prepareCardHardware();
}

void IntelWiFiDriver::txStopG1() {
    //iwl_pcie_tx_stop
    //iwl_scd_deactivate_fifos
    //Turn off all Tx DMA fifos
    writePRPH(SCD_TXFACT, 0);
    
    //[iwl_pcie_tx_stop_fh START]
    //Spinlock irq_lock
    IOInterruptState flags;
    uint32_t mask;
    if (!grabNICAccess(flags)) {
        return;
    }
    
    for (int ch = 0; ch < FH_TCSR_CHNL_NUM; ch++) {
        busWrite32(FH_TCSR_CHNL_TX_CONFIG_REG(ch), 0);
        mask |= FH_TSSR_TX_STATUS_REG_MSK_CHNL_IDLE(ch);
    }
    
    int ret = pollBit(FH_TSSR_TX_STATUS_REG, mask, mask, 5000);
    if (ret < 0 ) {
        LOG_ERROR("%s: Failing on timeout while stopping DMA channel [0x%08x]\n", DRVNAME, busRead32(FH_TSSR_TX_STATUS_REG));
    }
    
    releaseNICAccess(flags);
    
    //Spinunlock irq_lock
    //[iwl_pcie_tx_stop_fh END]
    
    //Mark the queues as stopped since we stopped Tx altogether
    bzero(&deviceProps.txq_used, sizeof(deviceProps.txq_used));
    bzero(&deviceProps.txq_stopped, sizeof(deviceProps.txq_stopped));
    
    //If device was started then immediately stopped, txqs will not be ready
    if (!deviceProps.txqAllocated) return;
    
    //Unmap DMAs from host system and free skb's
    for (int txqID = 0; txqID < deviceProps.deviceConfig->base_params->num_of_queues; txqID++) {
        unmapTxQ(txqID);
    }
    
}

void IntelWiFiDriver::apmStopG1(bool opModeLeave) {
    //iwl_pcie_apm_stop
    if (DEBUG) printf("%s: Stopping card, putting into low power state\n", DRVNAME);
    
    if (opModeLeave) {
        if (!deviceProps.status.deviceEnabled) {
            apmInitG1();
        }
        
        //Inform ME that we are leaving
        if (deviceProps.deviceConfig->device_family == IWL_DEVICE_FAMILY_7000) {
            setBitsPRPH(APMG_PCIDEV_STT_REG, APMG_PCIDEV_STT_VAL_WAKE_ME);
        } else if (deviceProps.deviceConfig->device_family == IWL_DEVICE_FAMILY_8000) {
            busSetBits(WPI_DBG_LINK_PWR_MGMT_REG, CSR_RESET_LINK_PWR_MGMT_DISABLED);
            busSetBits(WPI_HW_IF_CONFIG, CSR_HW_IF_CONFIG_REG_PREPARE | CSR_HW_IF_CONFIG_REG_ENABLE_PME);
            IOSleep(1);
            busClearBits(WPI_DBG_LINK_PWR_MGMT_REG, CSR_RESET_LINK_PWR_MGMT_DISABLED);
        }
        IOSleep(5);
    }
    
    deviceProps.status.deviceEnabled = false;
    
    apmStopMaster();
    
    if (deviceProps.deviceConfig->lp_xtal_workaround) {
        apm_LP_XTAL_Enable();
        return;
    }
    
    resetDevice();
    
    /*
     * Clear "initialization complete" bit to move adapter from
     * D0A* (powered-up Active) --> D0U* (Uninitialized) state.
     */
    busClearBit(WPI_GP_CNTRL, deviceProps.deviceConfig->csr->flag_init_done);
}

int IntelWiFiDriver::apmInitG1() {
    //iwl_pcie_apm_init
    
    if (DEBUG) printf("%s: APM init, preparing cards basic functions\n", DRVNAME);
    
    /*
     * Use "set_bit" below rather than "write", to preserve any hardware
     * bits already set by default after reset.
     */
    
    /* Disable L0S exit timer (platform NMI Work/Around) */
    if (deviceProps.deviceConfig->device_family < IWL_DEVICE_FAMILY_8000){
        busSetBits(WPI_GIO_CHICKEN, CSR_GIO_CHICKEN_BITS_REG_BIT_DIS_L0S_EXIT_TIMER);
    }
    
    /*
     * Disable L0s without affecting L1;
     *  don't wait for ICH L0s (ICH bug W/A)
     */
    busSetBits(WPI_GIO_CHICKEN, WPI_GIO_CHICKEN_L1A_NO_L0S_RX);
    
    //Set FH wait thresh to max (HW error during stress W/A)
    busSetBits(WPI_DBG_HPET_MEM_REG, CSR_DBG_HPET_MEM_REG_VAL);
    
    /*
     * Enable HAP INTA (interrupt from management bus) to
     * wake device's PCI Express link L1a -> L0s
     */
    busSetBits(WPI_HW_IF_CONFIG, CSR_HW_IF_CONFIG_REG_BIT_HAP_WAKE_L1A);
    
    apmConfig();
    
    /* Configure analog phase-lock-loop before activating to D0A */
    if (deviceProps.deviceConfig->base_params->pll_cfg) {
        busSetBits(WPI_ANA_PLL, CSR50_ANA_PLL_CFG_VAL);
    }
    
    int ret = finishNICInit();
    if (ret) return ret;
    
    if (deviceProps.deviceConfig->host_interrupt_operation_mode) {
        //From iwlwifi:
        /*
         * This is a bit of an abuse - This is needed for 7260 / 3160
         * only check host_interrupt_operation_mode even if this is
         * not related to host_interrupt_operation_mode.
         *
         * Enable the oscillator to count wake up time for L1 exit. This
         * consumes slightly more power (100uA) - but allows to be sure
         * that we wake up from L1 on time.
         *
         * This looks weird: read twice the same register, discard the
         * value, set a bit, and yet again, read that same register
         * just to discard the value. But that's the way the hardware
         * seems to like it.
         */
        readPRPH(OSC_CLK);
        readPRPH(OSC_CLK);
        setBitsPRPH(OSC_CLK, OSC_CLK_FORCE_CONTROL);
        readPRPH(OSC_CLK);
        readPRPH(OSC_CLK);
    }
    
    /*
     * Enable DMA clock and wait for it to stabilize.
     *
     * Write to "CLK_EN_REG"; "1" bits enable clocks, while "0"
     * bits do not disable clocks.  This preserves any hardware
     * bits already set by default in "CLK_CTRL_REG" after reset.
     */
    if (!deviceProps.deviceConfig->apmg_not_supported) {
        writePRPH(APMG_CLK_EN_REG, APMG_CLK_VAL_DMA_CLK_RQT);
        IOSleep(1);
        
        //Disable L1 active
        setBitsPRPH(APMG_PCIDEV_STT_REG, APMG_PCIDEV_STT_VAL_L1_ACT_DIS);
        
        //Clear the interrupt in APMG if the NIC is in RFKILL
        writePRPH(APMG_RTC_INT_STT_REG, APMG_RTC_INT_STT_RFKILL);
    }
    
    deviceProps.status.deviceEnabled = true;
    
    return 0;
}

/*
 * Enable LP XTAL to avoid HW bug where device may consume much power if
 * FW is not loaded after device reset. LP XTAL is disabled by default
 * after device HW reset. Do it only if XTAL is fed by internal source.
 * Configure device's "persistence" mode to avoid resetting XTAL again when
 * SHRD_HW_RST occurs in S3.
 */
void IntelWiFiDriver::apm_LP_XTAL_Enable() {
    //iwl_pcie_apm_lp_xtal_enable
    //TODO: Implement
    
    busSetBits(WPI_GP_CNTRL, CSR_GP_CNTRL_REG_FLAG_XTAL_ON);
    
    resetDevice();
    
    int ret = finishNICInit();
    if (ret) {
        LOG_ERROR("%s: finish nic init failed\n", DRVNAME);
        busClearBits(WPI_GP_CNTRL, CSR_GP_CNTRL_REG_FLAG_XTAL_ON);
        return;
    }
    
    /*
     * Clear "disable persistence" to avoid LP XTAL resetting when
     * SHRD_HW_RST is applied in S3.
     */
    clearBitsPRPH(APMG_PCIDEV_STT_REG, APMG_PCIDEV_STT_VAL_PERSIST_DIS);
    
    /*
     * Force APMG XTAL to be active to prevent its disabling by HW
     * caused by APMG idle state.
     */
    uint32_t apmg_xtal_cfg_reg = busReadShr(SHR_APMG_XTAL_CFG_REG);
    busWriteShr(SHR_APMG_XTAL_CFG_REG, apmg_xtal_cfg_reg | SHR_APMG_XTAL_CFG_XTAL_ON_REQ);
    
    resetDevice();
    
    //Enable LP XTAL by indirect access through CSR
    uint32_t apmg_gp1_reg = busReadShr(SHR_APMG_GP1_REG);
    busWriteShr(SHR_APMG_GP1_REG, apmg_gp1_reg | SHR_APMG_GP1_WF_XTAL_LP_EN | SHR_APMG_GP1_CHICKEN_BIT_SELECT);
    
    //Clear delay clock line power up
    uint32_t dl_cfg_reg = busReadShr(SHR_APMG_DL_CFG_REG);
    busWriteShr(SHR_APMG_DL_CFG_REG, dl_cfg_reg & ~SHR_APMG_DL_CFG_DL_CLOCK_POWER_UP);
    
    /*
     * Enable persistence mode to avoid LP XTAL resetting when
     * SHRD_HW_RST is applied in S3.
     */
    busSetBits(WPI_HW_IF_CONFIG, CSR_HW_IF_CONFIG_REG_PERSIST_MODE);
    
    /*
     * Clear "initialization complete" bit to move adapter from
     * D0A* (powered-up Active) --> D0U* (Uninitialized) state.
     */
    busClearBit(WPI_GP_CNTRL, deviceProps.deviceConfig->csr->flag_init_done);
    
    //Activates XTAL resources monitor
    busSetBits(CSR_MONITOR_CFG_REG, CSR_MONITOR_XTAL_RESOURCES);
    
    //Release XTAL ON request
    busClearBits(WPI_GP_CNTRL, CSR_GP_CNTRL_REG_FLAG_XTAL_ON);
    
    IOSleep(1);
    
    //Release APMG XTAL
    busWriteShr(SHR_APMG_XTAL_CFG_REG, apmg_xtal_cfg_reg & ~SHR_APMG_XTAL_CFG_XTAL_ON_REQ);
}
//===================================

void IntelWiFiDriver::rxStop() {
    //iwl_pcie_rx_stop
    //TODO: Implement
}

void IntelWiFiDriver::configureMSIX() {
    //iwl_pcie_conf_msix_hw
    if (!deviceProps.msixEnabled) {
        if (deviceProps.deviceConfig->mq_rx_supported &&
            deviceProps.status.deviceEnabled) {
            writeUMAC_PRPH(UREG_CHICK, UREG_CHICK_MSI_ENABLE);
        }
        return;
    }
    
    //IVAR needs to be configure after reset but device
    //must be enabled to write to PRPH
    if (deviceProps.status.deviceEnabled) {
        writeUMAC_PRPH(UREG_CHICK, UREG_CHICK_MSI_ENABLE);
    }
    
    //From iwlwifi:
    /*
     * Each cause from the causes list above and the RX causes is
     * represented as a byte in the IVAR table. The first nibble
     * represents the bound interrupt vector of the cause, the second
     * represents no auto clear for this cause. This will be set if its
     * interrupt vector is bound to serve other causes.
     */
    mapRxCauses();
    mapNonRxCauses();
}

void IntelWiFiDriver::unmapTxQ(int txqID) {
    //iwl_pcie_txq_unmap
    //TODO: Implement
    struct iwl_txq* txq = deviceProps.txQueues[txqID];
    
    IOSimpleLockLock(txq->lock);
    while (txq->write_ptr != txq->read_ptr) {
        if (DEBUG) printf("%s: TxQ %d freed\n", DRVNAME, txqID);
        
        if (txqID != deviceProps.commandQueue) {
            mbuf_t skb = txq->entries[txq->read_ptr].skb;
            
            //We need to free the skbs, first check that it points to something
            if (!skb) {
                LOG_ERROR("%s: pointer to skb null [txqID: %d, read_ptr: %08x]\n", DRVNAME, txqID, txq->read_ptr);
            } else {
                freeTSOPage(skb);
            }
        }
        txQFreeTDF(txq);
        txq->read_ptr = queueIncWrap(txq->read_ptr);
        
        if (txq->read_ptr == txq->write_ptr) {
            IOInterruptState flags;
            flags = IOSimpleLockLockDisableInterrupt(deviceProps.NICAccessLock);
            if (txqID != deviceProps.commandQueue) {
                if (DEBUG) printf("%s: last txq freed: %d\n", DRVNAME, txq->id);
                unref();
            } else {
                clearCommandInFlight();
            }
            IOSimpleLockUnlockEnableInterrupt(deviceProps.NICAccessLock, flags);
        }
    }
}

void IntelWiFiDriver::freeTSOPage(mbuf_t skb) {
    //iwl_pcie_free_tso_page
    //TODO: Needs adapting for xnu
}

void IntelWiFiDriver::txQFreeTDF(struct iwl_txq* txq) {
    //iwl_pcie_txq_free_tfd
    int readPointer = txq->read_ptr;
    int commandIndex = getCommandIndex(txq, readPointer);
    
    unmapTFD(&txq->entries[commandIndex].meta, txq, readPointer);
    
    //free mbuf_t
    if (txq->entries) {
        mbuf_t skb;
        skb = txq->entries[commandIndex].skb;
        
        if (skb) {
            //TODO: Update for DVM
            //Using MVM version for freeing SKBs by default, will need to update
            //if adding DVM functionality
            
            freeSKB(skb);
            txq->entries[commandIndex].skb = NULL;
        }
    }
}

void IntelWiFiDriver::unmapTFD(struct iwl_cmd_meta* meta, struct iwl_txq* txq, int index) {
    //iwl_pcie_tfd_unmap
    void* tfd = getTFD(txq, index);
    int numberTBS = TFDGetNumberOfTBS(tfd);
    
    if (numberTBS > deviceProps.maxTBS) {
        LOG_ERROR("%s: Too many TBSs [index: %d, count: %d]\n", DRVNAME, index, numberTBS);
        //TODO: Issue fatal error
        return;
    }
    
    //First TB is never freed, its the bidirectional DMA data
    
    for (int i = 1; i < numberTBS; i++) {
        //TODO: Check if IOFree commands are right
        if (meta->tbs & BIT(i)) {
            IOFreePageable((void*)TFDGetTBAddress(tfd, i), TFDGetTBLength(tfd, i));
        } else {
            IOFree((void*)TFDGetTBAddress(tfd, i), TFDGetTBLength(tfd, i));
        }
    }
    
    meta->tbs = 0;
    
    if (deviceProps.deviceConfig->use_tfh) {
        struct iwl_tfh_tfd* tfd_fh = (iwl_tfh_tfd*)tfd;
        tfd_fh->num_tbs = 0;
    } else {
        struct iwl_tfd* tfd_fh = (iwl_tfd*)tfd;
        tfd_fh->num_tbs = 0;
    }
}

void* IntelWiFiDriver::getTFD(struct iwl_txq* txq, int index) {
    //iwl_pcie_get_tfd
    //Takes a different approach, in iwlwifi they do arithmetic to
    //void* which is illegal, using the code from iwl_pcie_tfd_get_num_tbs
    //we know what type to cast to for the situation and thus we can do safe
    //pointer maths
    if (deviceProps.deviceConfig->use_tfh) {
        index = getCommandIndex(txq, index);
        return (iwl_tfh_tfd*)txq->tfds + deviceProps.tfdSize * index;
    }
    return (iwl_tfd*)txq->tfds + deviceProps.tfdSize * index;
}

uint8_t IntelWiFiDriver::TFDGetNumberOfTBS(void* __tfd) {
    if (deviceProps.deviceConfig->use_tfh) {
        struct iwl_tfh_tfd* tfd = (iwl_tfh_tfd*)__tfd;
        return le16_to_cpu(tfd->num_tbs) & 0x1f;
    } else {
        struct iwl_tfd* tfd = (iwl_tfd*)__tfd;
        return tfd->num_tbs & 0x1f;
    }
}

bus_addr_t IntelWiFiDriver::TFDGetTBAddress(void* __tfd, int index) {
    //iwl_pcie_tfd_tb_get_addr
    if (deviceProps.deviceConfig->use_tfh) {
        struct iwl_tfh_tfd* tfd = (iwl_tfh_tfd*)__tfd;
        //TODO: Check here when implementing DMA
        struct iwl_tfh_tb* tb = &tfd->tbs[index];
        
        return tb->addr;
    } else {
        struct iwl_tfd* tfd = (iwl_tfd*)__tfd;
        struct iwl_tfd_tb* tb = &tfd->tbs[index];
        
        //Warning for unaligned pointer value, doesnt matter
        dma_addr_t addr = le32_to_cpus(&tb->lo);
        dma_addr_t hi_len;
        
        if (sizeof(dma_addr_t) <= sizeof(uint32_t)) {
            return addr;
        }
        hi_len = le16_to_cpu(tb->hi_n_len) & TB_HI_N_LEN_ADDR_HI_MSK;
        return addr | (hi_len << 32);
    }
}

uint16_t IntelWiFiDriver::TFDGetTBLength(void* __tfd, int index) {
    if (deviceProps.deviceConfig->use_tfh) {
        struct iwl_tfh_tfd* tfd = (iwl_tfh_tfd*)__tfd;
        //TODO: Check here when implementing DMA
        struct iwl_tfh_tb* tb = &tfd->tbs[index];
        
        return le16_to_cpu(tb->tb_len);
    } else {
        struct iwl_tfd* tfd = (iwl_tfd*)__tfd;
        struct iwl_tfd_tb* tb = &tfd->tbs[index];
        
        return le16_to_cpu(tb->hi_n_len) >> 4;
    }
}

void IntelWiFiDriver::mapRxCauses() {
    //iwl_pcie_map_rx_causes
    uint32_t offset = deviceProps.sharedVecMask & IWL_SHARED_IRQ_FIRST_RSS ? 1 : 0;
    
    //First Rx queue is always mapped to firs irq vector
    //for use with management frames, command responses
    //Other irqs mapped to other (N - 2) vectors
    uint32_t value = BIT(MSIX_FH_INT_CAUSES_Q(0));
    for (int index = 1; index < deviceProps.rxQCount; index++) {
        busWrite8(CSR_MSIX_IVAR(index), MSIX_FH_INT_CAUSES_Q(index - offset));
        value |= BIT(MSIX_FH_INT_CAUSES_Q(index - offset));
    }
    busWrite32(CSR_MSIX_FH_INT_MASK_AD, ~value);
    
    value = MSIX_FH_INT_CAUSES_Q(0);
    if (deviceProps.sharedVecMask & IWL_SHARED_IRQ_NON_RX) {
        value |= MSIX_NON_AUTO_CLEAR_CAUSE;
    }
    busWrite8(CSR_MSIX_RX_IVAR(0), value);
    
    if (deviceProps.sharedVecMask & IWL_SHARED_IRQ_FIRST_RSS) {
        busWrite8(CSR_MSIX_RX_IVAR(1), value);
    }
}

void IntelWiFiDriver::mapNonRxCauses() {
    //iwl_pcie_map_non_rx_causes
    int value = deviceProps.defIRQ | MSIX_AUTO_CLEAR_CAUSE;
    
    int arraySize = (deviceProps.deviceConfig->device_family != IWL_DEVICE_FAMILY_22560) ?
    ARRAY_SIZE(causes_list) : ARRAY_SIZE(causes_list_v2);
    
    //Map all non RX causes onto default IRQ
    //First interrupt vector will serve non-RX and FBQ causes
    for (int index = 0; index < arraySize; index++) {
        struct iwl_causes_list* causes = deviceProps.deviceConfig->device_family != IWL_DEVICE_FAMILY_22560 ? causes_list : causes_list_v2;
        busWrite8(CSR_MSIX_IVAR(causes[index].addr), value);
        busClearBit(causes[index].mask_reg, causes[index].cause_num);
    }
}

int IntelWiFiDriver::queueIncWrap(int index) {
    //iwl_queue_inc_wrap
    //TODO: Implement
}

void IntelWiFiDriver::clearCommandInFlight() {
    //iwl_pcie_clear_cmd_in_flight
    
    //Clear the command in flight flag and call unref on the device
    if (deviceProps.comandInFlight) {
        deviceProps.comandInFlight = false;
        if (DEBUG) printf("%s: clear commandInFlight flag\n", DRVNAME);
        unref();
    }
    
    if (!deviceProps.deviceConfig->base_params->apmg_wake_up_wa) {
        return;
    } else if (!deviceProps.holdNICAwake) {
        LOG_ERROR("%s: holNICAwake flag not set", DRVNAME);
        return;
    }
    
    deviceProps.holdNICAwake = false;
    busClearBit(WPI_GP_CNTRL, deviceProps.deviceConfig->csr->flag_mac_access_req);
}

void IntelWiFiDriver::apmStopMaster() {
    //iwl_pcie_apm_stop_master
    //TODO: Implement
}

void IntelWiFiDriver::apmConfig() {
    //iwl_pcie_apm_config
    //TODO: Implement
}

void IntelWiFiDriver::wakeQueue(iwl_txq *txq) {
    //iwl_wake_queue
    //TODO: Implement
    if (test_and_clear_bit(txq->id, deviceProps.txq_stopped)) {
        if (DEBUG) printf("%s: Wake hwq %d\n", DRVNAME, txq->id);
        mvmWakeSWQueue(txq->id);
    }
}
