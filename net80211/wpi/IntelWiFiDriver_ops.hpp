//
//  IntelWiFiDriver-ops.hpp
//  net80211
//
//  Created by Administrator on 09/02/2020.
//

#ifndef IntelWiFiDriver_ops_h
#define IntelWiFiDriver_ops_h
/*
 * Causes for the FH register interrupts
 */
enum msix_fh_int_causes {
    MSIX_FH_INT_CAUSES_Q0            = BIT(0),
    MSIX_FH_INT_CAUSES_Q1            = BIT(1),
    MSIX_FH_INT_CAUSES_D2S_CH0_NUM        = BIT(16),
    MSIX_FH_INT_CAUSES_D2S_CH1_NUM        = BIT(17),
    MSIX_FH_INT_CAUSES_S2D            = BIT(19),
    MSIX_FH_INT_CAUSES_FH_ERR        = BIT(21),
};

/*
 * Causes for the HW register interrupts
 */
enum msix_hw_int_causes {
    MSIX_HW_INT_CAUSES_REG_ALIVE        = BIT(0),
    MSIX_HW_INT_CAUSES_REG_WAKEUP        = BIT(1),
    MSIX_HW_INT_CAUSES_REG_IPC        = BIT(1),
    MSIX_HW_INT_CAUSES_REG_IML              = BIT(2),
    MSIX_HW_INT_CAUSES_REG_SW_ERR_V2    = BIT(5),
    MSIX_HW_INT_CAUSES_REG_CT_KILL        = BIT(6),
    MSIX_HW_INT_CAUSES_REG_RF_KILL        = BIT(7),
    MSIX_HW_INT_CAUSES_REG_PERIODIC        = BIT(8),
    MSIX_HW_INT_CAUSES_REG_SW_ERR        = BIT(25),
    MSIX_HW_INT_CAUSES_REG_SCD        = BIT(26),
    MSIX_HW_INT_CAUSES_REG_FH_TX        = BIT(27),
    MSIX_HW_INT_CAUSES_REG_HW_ERR        = BIT(29),
    MSIX_HW_INT_CAUSES_REG_HAP        = BIT(30),
};

#define MSIX_MIN_INTERRUPT_VECTORS        2
#define MSIX_AUTO_CLEAR_CAUSE            0
#define MSIX_NON_AUTO_CLEAR_CAUSE        BIT(7)

struct iwl_causes_list {
    u32 cause_num;
    u32 mask_reg;
    u8 addr;
};

static struct iwl_causes_list causes_list[] = {
    {MSIX_FH_INT_CAUSES_D2S_CH0_NUM,    CSR_MSIX_FH_INT_MASK_AD, 0},
    {MSIX_FH_INT_CAUSES_D2S_CH1_NUM,    CSR_MSIX_FH_INT_MASK_AD, 0x1},
    {MSIX_FH_INT_CAUSES_S2D,        CSR_MSIX_FH_INT_MASK_AD, 0x3},
    {MSIX_FH_INT_CAUSES_FH_ERR,        CSR_MSIX_FH_INT_MASK_AD, 0x5},
    {MSIX_HW_INT_CAUSES_REG_ALIVE,        CSR_MSIX_HW_INT_MASK_AD, 0x10},
    {MSIX_HW_INT_CAUSES_REG_WAKEUP,        CSR_MSIX_HW_INT_MASK_AD, 0x11},
    {MSIX_HW_INT_CAUSES_REG_IML,            CSR_MSIX_HW_INT_MASK_AD, 0x12},
    {MSIX_HW_INT_CAUSES_REG_CT_KILL,    CSR_MSIX_HW_INT_MASK_AD, 0x16},
    {MSIX_HW_INT_CAUSES_REG_RF_KILL,    CSR_MSIX_HW_INT_MASK_AD, 0x17},
    {MSIX_HW_INT_CAUSES_REG_PERIODIC,    CSR_MSIX_HW_INT_MASK_AD, 0x18},
    {MSIX_HW_INT_CAUSES_REG_SW_ERR,        CSR_MSIX_HW_INT_MASK_AD, 0x29},
    {MSIX_HW_INT_CAUSES_REG_SCD,        CSR_MSIX_HW_INT_MASK_AD, 0x2A},
    {MSIX_HW_INT_CAUSES_REG_FH_TX,        CSR_MSIX_HW_INT_MASK_AD, 0x2B},
    {MSIX_HW_INT_CAUSES_REG_HW_ERR,        CSR_MSIX_HW_INT_MASK_AD, 0x2D},
    {MSIX_HW_INT_CAUSES_REG_HAP,        CSR_MSIX_HW_INT_MASK_AD, 0x2E},
};

static struct iwl_causes_list causes_list_v2[] = {
    {MSIX_FH_INT_CAUSES_D2S_CH0_NUM,    CSR_MSIX_FH_INT_MASK_AD, 0},
    {MSIX_FH_INT_CAUSES_D2S_CH1_NUM,    CSR_MSIX_FH_INT_MASK_AD, 0x1},
    {MSIX_FH_INT_CAUSES_S2D,        CSR_MSIX_FH_INT_MASK_AD, 0x3},
    {MSIX_FH_INT_CAUSES_FH_ERR,        CSR_MSIX_FH_INT_MASK_AD, 0x5},
    {MSIX_HW_INT_CAUSES_REG_ALIVE,        CSR_MSIX_HW_INT_MASK_AD, 0x10},
    {MSIX_HW_INT_CAUSES_REG_IPC,        CSR_MSIX_HW_INT_MASK_AD, 0x11},
    {MSIX_HW_INT_CAUSES_REG_SW_ERR_V2,    CSR_MSIX_HW_INT_MASK_AD, 0x15},
    {MSIX_HW_INT_CAUSES_REG_CT_KILL,    CSR_MSIX_HW_INT_MASK_AD, 0x16},
    {MSIX_HW_INT_CAUSES_REG_RF_KILL,    CSR_MSIX_HW_INT_MASK_AD, 0x17},
    {MSIX_HW_INT_CAUSES_REG_PERIODIC,    CSR_MSIX_HW_INT_MASK_AD, 0x18},
    {MSIX_HW_INT_CAUSES_REG_SCD,        CSR_MSIX_HW_INT_MASK_AD, 0x2A},
    {MSIX_HW_INT_CAUSES_REG_FH_TX,        CSR_MSIX_HW_INT_MASK_AD, 0x2B},
    {MSIX_HW_INT_CAUSES_REG_HW_ERR,        CSR_MSIX_HW_INT_MASK_AD, 0x2D},
    {MSIX_HW_INT_CAUSES_REG_HAP,        CSR_MSIX_HW_INT_MASK_AD, 0x2E},
};

#endif /* IntelWiFiDriver_ops_h */
