/******************************************************************************
 *
 * This file is provided under a dual BSD/GPLv2 license.  When using or
 * redistributing this file, you may do so under either license.
 *
 * GPL LICENSE SUMMARY
 *
 * Copyright(c) 2003 - 2015 Intel Corporation. All rights reserved.
 * Copyright(c) 2013 - 2015 Intel Mobile Communications GmbH
 * Copyright(c) 2016 - 2017 Intel Deutschland GmbH
 * Copyright(c) 2018 - 2019 Intel Corporation
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of version 2 of the GNU General Public License as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
 * more details.
 *
 * The full GNU General Public License is included in this distribution in the
 * file called COPYING.
 *
 * Contact Information:
 *  Intel Linux Wireless <linuxwifi@intel.com>
 * Intel Corporation, 5200 N.E. Elam Young Parkway, Hillsboro, OR 97124-6497
 *
 * BSD LICENSE
 *
 * Copyright(c) 2003 - 2015 Intel Corporation. All rights reserved.
 * Copyright(c) 2013 - 2015 Intel Mobile Communications GmbH
 * Copyright(c) 2016 - 2017 Intel Deutschland GmbH
 * Copyright(c) 2018 - 2019 Intel Corporation
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 *  * Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 *  * Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in
 *    the documentation and/or other materials provided with the
 *    distribution.
 *  * Neither the name Intel Corporation nor the names of its
 *    contributors may be used to endorse or promote products derived
 *    from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 *****************************************************************************/

#ifndef internals_h
#define internals_h

//#include <linux/spinlock.h>
//#include <linux/interrupt.h>
//#include <linux/skbuff.h>
//#include <linux/wait.h>
//#include <linux/pci.h>
//#include <linux/timer.h>
//#include <linux/cpu.h>

#include "linux-porting.h"
#include "iwl-fh.h"
#include <sys/kernel_types.h>
#include <sys/queue.h>

//#include "iwl-fh.h"
//#include "iwl-csr.h"
//#include "iwl-trans.h"
//#include "iwl-debug.h"
//#include "iwl-io.h"
//#include "iwl-op-mode.h"
//#include "iwl-drv.h"

/* We need 2 entries for the TX command and header, and another one might
 * be needed for potential data in the SKB's head. The remaining ones can
 * be used for frags.
 */
#define IWL_PCIE_MAX_FRAGS(x) (x->max_tbs - 3)

/*
 * RX related structures and functions
 */
#define RX_NUM_QUEUES 1
#define RX_POST_REQ_ALLOC 2
#define RX_CLAIM_REQ_ALLOC 8
#define RX_PENDING_WATERMARK 16
#define FIRST_RX_QUEUE 512

struct iwl_host_cmd;

/*This file includes the declaration that are internal to the
 * trans_pcie layer */

/**
 * struct iwl_rx_mem_buffer
 * @page_dma: bus address of rxb page
 * @page: driver's pointer to the rxb page
 * @invalid: rxb is in driver ownership - not owned by HW
 * @vid: index of this rxb in the global table
 */
struct iwl_rx_mem_buffer {
    dma_addr_t page_dma;
    struct page *page;
    u16 vid;
    bool invalid;
    LIST_ENTRY(iwl_rx_mem_buffer) list;
};

///**
// * struct isr_statistics - interrupt statistics
// *
// */
//struct isr_statistics {
//    u32 hw;
//    u32 sw;
//    u32 err_code;
//    u32 sch;
//    u32 alive;
//    u32 rfkill;
//    u32 ctkill;
//    u32 wakeup;
//    u32 rx;
//    u32 tx;
//    u32 unhandled;
//};

/**
 * struct iwl_rx_transfer_desc - transfer descriptor
 * @addr: ptr to free buffer start address
 * @rbid: unique tag of the buffer
 * @reserved: reserved
 */
struct iwl_rx_transfer_desc {
    __le16 rbid;
    __le16 reserved[3];
    __le64 addr;
} __packed;

#define IWL_RX_CD_FLAGS_FRAGMENTED    BIT(0)

/**
 * struct iwl_rx_completion_desc - completion descriptor
 * @reserved1: reserved
 * @rbid: unique tag of the received buffer
 * @flags: flags (0: fragmented, all others: reserved)
 * @reserved2: reserved
 */
struct iwl_rx_completion_desc {
    __le32 reserved1;
    __le16 rbid;
    u8 flags;
    u8 reserved2[25];
} __packed;

/**
 * struct iwl_rxq - Rx queue
 * @id: queue index
 * @bd: driver's pointer to buffer of receive buffer descriptors (rbd).
 *    Address size is 32 bit in pre-9000 devices and 64 bit in 9000 devices.
 *    In 22560 devices it is a pointer to a list of iwl_rx_transfer_desc's
 * @bd_dma: bus address of buffer of receive buffer descriptors (rbd)
 * @ubd: driver's pointer to buffer of used receive buffer descriptors (rbd)
 * @ubd_dma: physical address of buffer of used receive buffer descriptors (rbd)
 * @tr_tail: driver's pointer to the transmission ring tail buffer
 * @tr_tail_dma: physical address of the buffer for the transmission ring tail
 * @cr_tail: driver's pointer to the completion ring tail buffer
 * @cr_tail_dma: physical address of the buffer for the completion ring tail
 * @read: Shared index to newest available Rx buffer
 * @write: Shared index to oldest written Rx packet
 * @free_count: Number of pre-allocated buffers in rx_free
 * @used_count: Number of RBDs handled to allocator to use for allocation
 * @write_actual:
 * @rx_free: list of RBDs with allocated RB ready for use
 * @rx_used: list of RBDs with no RB attached
 * @need_update: flag to indicate we need to update read/write index
 * @rb_stts: driver's pointer to receive buffer status
 * @rb_stts_dma: bus address of receive buffer status
 * @lock:
 * @queue: actual rx queue. Not used for multi-rx queue.
 *
 * NOTE:  rx_free and rx_used are used as a FIFO for iwl_rx_mem_buffers
 */
struct iwl_rxq {
    int id;
    void *bd;
    dma_addr_t bd_dma;
    union {
        void *used_bd;
        __le32 *bd_32;
        struct iwl_rx_completion_desc *cd;
    };
    dma_addr_t used_bd_dma;
    __le16 *tr_tail;
    dma_addr_t tr_tail_dma;
    __le16 *cr_tail;
    dma_addr_t cr_tail_dma;
    u32 read;
    u32 write;
    u32 free_count;
    u32 used_count;
    u32 write_actual;
    u32 queue_size;
    TAILQ_ENTRY(iwl_rxq) rx_free;
    TAILQ_ENTRY(iwl_rxq) rx_used;
    bool need_update;
    void *rb_stts;
    dma_addr_t rb_stts_dma;
    spinlock_t lock;
//    struct napi_struct napi;
    struct iwl_rx_mem_buffer *queue[RX_QUEUE_SIZE];
};

/**
 * struct iwl_rb_allocator - Rx allocator
 * @req_pending: number of requests the allcator had not processed yet
 * @req_ready: number of requests honored and ready for claiming
 * @rbd_allocated: RBDs with pages allocated and ready to be handled to
 *    the queue. This is a list of &struct iwl_rx_mem_buffer
 * @rbd_empty: RBDs with no page attached for allocator use. This is a list
 *    of &struct iwl_rx_mem_buffer
 * @lock: protects the rbd_allocated and rbd_empty lists
 * @alloc_wq: work queue for background calls
 * @rx_alloc: work struct for background calls
 */
struct iwl_rb_allocator {
    atomic_t req_pending;
    atomic_t req_ready;
    struct list_head rbd_allocated;
    struct list_head rbd_empty;
    spinlock_t lock;
    struct workqueue_struct *alloc_wq;
    struct work_struct rx_alloc;
};

struct iwl_dma_ptr {
    dma_addr_t dma;
    void *addr;
    size_t size;
};

///**
// * iwl_queue_inc_wrap - increment queue index, wrap back to beginning
// * @index -- current index
// */
//static inline int iwl_queue_inc_wrap(struct iwl_trans *trans, int index)
//{
//    return ++index & (trans->cfg->base_params->max_tfd_queue_size - 1);
//}
//
///**
// * iwl_get_closed_rb_stts - get closed rb stts from different structs
// * @rxq - the rxq to get the rb stts from
// */
//static inline __le16 iwl_get_closed_rb_stts(struct iwl_trans *trans,
//                                            struct iwl_rxq *rxq)
//{
//    if (trans->cfg->device_family >= IWL_DEVICE_FAMILY_22560) {
//        __le16 *rb_stts = rxq->rb_stts;
//        
//        return READ_ONCE(*rb_stts);
//    } else {
//        struct iwl_rb_status *rb_stts = rxq->rb_stts;
//        
//        return READ_ONCE(rb_stts->closed_rb_num);
//    }
//}
//
///**
// * iwl_queue_dec_wrap - decrement queue index, wrap back to end
// * @index -- current index
// */
//static inline int iwl_queue_dec_wrap(struct iwl_trans *trans, int index)
//{
//    return --index & (trans->cfg->base_params->max_tfd_queue_size - 1);
//}

struct iwl_cmd_meta {
    /* only for SYNC commands, iff the reply skb is wanted */
    struct iwl_host_cmd *source;
    u32 flags;
    u32 tbs;
};

/*
 * The FH will write back to the first TB only, so we need to copy some data
 * into the buffer regardless of whether it should be mapped or not.
 * This indicates how big the first TB must be to include the scratch buffer
 * and the assigned PN.
 * Since PN location is 8 bytes at offset 12, it's 20 now.
 * If we make it bigger then allocations will be bigger and copy slower, so
 * that's probably not useful.
 */
#define IWL_FIRST_TB_SIZE    20
#define IWL_FIRST_TB_SIZE_ALIGN ALIGN(IWL_FIRST_TB_SIZE, 64)

struct iwl_pcie_txq_entry {
    struct iwl_device_cmd *cmd;
    struct sk_buff *skb;
    /* buffer to free after command completes */
    const void *free_buf;
    struct iwl_cmd_meta meta;
};

struct iwl_pcie_first_tb_buf {
    u8 buf[IWL_FIRST_TB_SIZE_ALIGN];
};

/**
 * struct iwl_txq - Tx Queue for DMA
 * @q: generic Rx/Tx queue descriptor
 * @tfds: transmit frame descriptors (DMA memory)
 * @first_tb_bufs: start of command headers, including scratch buffers, for
 *    the writeback -- this is DMA memory and an array holding one buffer
 *    for each command on the queue
 * @first_tb_dma: DMA address for the first_tb_bufs start
 * @entries: transmit entries (driver state)
 * @lock: queue lock
 * @stuck_timer: timer that fires if queue gets stuck
 * @trans_pcie: pointer back to transport (for timer)
 * @need_update: indicates need to update read/write index
 * @ampdu: true if this queue is an ampdu queue for an specific RA/TID
 * @wd_timeout: queue watchdog timeout (jiffies) - per queue
 * @frozen: tx stuck queue timer is frozen
 * @frozen_expiry_remainder: remember how long until the timer fires
 * @bc_tbl: byte count table of the queue (relevant only for gen2 transport)
 * @write_ptr: 1-st empty entry (index) host_w
 * @read_ptr: last used entry (index) host_r
 * @dma_addr:  physical addr for BD's
 * @n_window: safe queue window
 * @id: queue id
 * @low_mark: low watermark, resume queue if free space more than this
 * @high_mark: high watermark, stop queue if free space less than this
 *
 * A Tx queue consists of circular buffer of BDs (a.k.a. TFDs, transmit frame
 * descriptors) and required locking structures.
 *
 * Note the difference between TFD_QUEUE_SIZE_MAX and n_window: the hardware
 * always assumes 256 descriptors, so TFD_QUEUE_SIZE_MAX is always 256 (unless
 * there might be HW changes in the future). For the normal TX
 * queues, n_window, which is the size of the software queue data
 * is also 256; however, for the command queue, n_window is only
 * 32 since we don't need so many commands pending. Since the HW
 * still uses 256 BDs for DMA though, TFD_QUEUE_SIZE_MAX stays 256.
 * This means that we end up with the following:
 *  HW entries: | 0 | ... | N * 32 | ... | N * 32 + 31 | ... | 255 |
 *  SW entries:           | 0      | ... | 31          |
 * where N is a number between 0 and 7. This means that the SW
 * data is a window overlayed over the HW queue.
 */
struct iwl_txq {
    void *tfds;
    struct iwl_pcie_first_tb_buf *first_tb_bufs;
    dma_addr_t first_tb_dma;
    struct iwl_pcie_txq_entry *entries;
    spinlock_t lock;
    unsigned long frozen_expiry_remainder;
    struct timer_list stuck_timer;
    struct iwl_trans_pcie *trans_pcie;
    bool need_update;
    bool frozen;
    bool ampdu;
    int block;
    unsigned long wd_timeout;
    struct sk_buff_head overflow_q;
    struct iwl_dma_ptr bc_tbl;
    
    int write_ptr;
    int read_ptr;
    dma_addr_t dma_addr;
    int n_window;
    u32 id;
    int low_mark;
    int high_mark;
    
    bool overflow_tx;
};

//static inline dma_addr_t
//iwl_pcie_get_first_tb_dma(struct iwl_txq *txq, int idx)
//{
//    return txq->first_tb_dma +
//    sizeof(struct iwl_pcie_first_tb_buf) * idx;
//}
//
//struct iwl_tso_hdr_page {
//    struct page *page;
//    u8 *pos;
//};
//
//#ifdef CONFIG_IWLWIFI_DEBUGFS
///**
// * enum iwl_fw_mon_dbgfs_state - the different states of the monitor_data
// * debugfs file
// *
// * @IWL_FW_MON_DBGFS_STATE_CLOSED: the file is closed.
// * @IWL_FW_MON_DBGFS_STATE_OPEN: the file is open.
// * @IWL_FW_MON_DBGFS_STATE_DISABLED: the file is disabled, once this state is
// *    set the file can no longer be used.
// */
//enum iwl_fw_mon_dbgfs_state {
//    IWL_FW_MON_DBGFS_STATE_CLOSED,
//    IWL_FW_MON_DBGFS_STATE_OPEN,
//    IWL_FW_MON_DBGFS_STATE_DISABLED,
//};
//#endif
//
///**
// * enum iwl_shared_irq_flags - level of sharing for irq
// * @IWL_SHARED_IRQ_NON_RX: interrupt vector serves non rx causes.
// * @IWL_SHARED_IRQ_FIRST_RSS: interrupt vector serves first RSS queue.
// */
//enum iwl_shared_irq_flags {
//    IWL_SHARED_IRQ_NON_RX        = BIT(0),
//    IWL_SHARED_IRQ_FIRST_RSS    = BIT(1),
//};
//
///**
// * enum iwl_image_response_code - image response values
// * @IWL_IMAGE_RESP_DEF: the default value of the register
// * @IWL_IMAGE_RESP_SUCCESS: iml was read successfully
// * @IWL_IMAGE_RESP_FAIL: iml reading failed
// */
//enum iwl_image_response_code {
//    IWL_IMAGE_RESP_DEF        = 0,
//    IWL_IMAGE_RESP_SUCCESS        = 1,
//    IWL_IMAGE_RESP_FAIL        = 2,
//};
//
///**
// * struct cont_rec: continuous recording data structure
// * @prev_wr_ptr: the last address that was read in monitor_data
// *    debugfs file
// * @prev_wrap_cnt: the wrap count that was used during the last read in
// *    monitor_data debugfs file
// * @state: the state of monitor_data debugfs file as described
// *    in &iwl_fw_mon_dbgfs_state enum
// * @mutex: locked while reading from monitor_data debugfs file
// */
//#ifdef CONFIG_IWLWIFI_DEBUGFS
//struct cont_rec {
//    u32 prev_wr_ptr;
//    u32 prev_wrap_cnt;
//    u8  state;
//    /* Used to sync monitor_data debugfs file with driver unload flow */
//    struct mutex mutex;
//};
//#endif

#endif /* internals_h */
