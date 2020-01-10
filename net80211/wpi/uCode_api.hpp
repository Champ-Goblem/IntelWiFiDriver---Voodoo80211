//
//  uCode_api.h
//  net80211
//
//  Created by Administrator on 09/01/2020.
//

#ifndef uCode_api_h
#define uCode_api_h

/**
 * enum iwl_ucode_tlv_api - ucode api
 * @IWL_UCODE_TLV_API_FRAGMENTED_SCAN: This ucode supports active dwell time
 *    longer than the passive one, which is essential for fragmented scan.
 * @IWL_UCODE_TLV_API_WIFI_MCC_UPDATE: ucode supports MCC updates with source.
 * @IWL_UCODE_TLV_API_LQ_SS_PARAMS: Configure STBC/BFER via LQ CMD ss_params
 * @IWL_UCODE_TLV_API_NEW_VERSION: new versioning format
 * @IWL_UCODE_TLV_API_SCAN_TSF_REPORT: Scan start time reported in scan
 *    iteration complete notification, and the timestamp reported for RX
 *    received during scan, are reported in TSF of the mac specified in the
 *    scan request.
 * @IWL_UCODE_TLV_API_TKIP_MIC_KEYS: This ucode supports version 2 of
 *    ADD_MODIFY_STA_KEY_API_S_VER_2.
 * @IWL_UCODE_TLV_API_STA_TYPE: This ucode supports station type assignement.
 * @IWL_UCODE_TLV_API_NAN2_VER2: This ucode supports NAN API version 2
 * @IWL_UCODE_TLV_API_NEW_RX_STATS: should new RX STATISTICS API be used
 * @IWL_UCODE_TLV_API_QUOTA_LOW_LATENCY: Quota command includes a field
 *    indicating low latency direction.
 * @IWL_UCODE_TLV_API_DEPRECATE_TTAK: RX status flag TTAK ok (bit 7) is
 *    deprecated.
 * @IWL_UCODE_TLV_API_ADAPTIVE_DWELL_V2: This ucode supports version 8
 *    of scan request: SCAN_REQUEST_CMD_UMAC_API_S_VER_8
 * @IWL_UCODE_TLV_API_FRAG_EBS: This ucode supports fragmented EBS
 * @IWL_UCODE_TLV_API_REDUCE_TX_POWER: This ucode supports v5 of
 *    the REDUCE_TX_POWER_CMD.
 * @IWL_UCODE_TLV_API_SHORT_BEACON_NOTIF: This ucode supports the short
 *    version of the beacon notification.
 * @IWL_UCODE_TLV_API_BEACON_FILTER_V4: This ucode supports v4 of
 *    BEACON_FILTER_CONFIG_API_S_VER_4.
 * @IWL_UCODE_TLV_API_REGULATORY_NVM_INFO: This ucode supports v4 of
 *    REGULATORY_NVM_GET_INFO_RSP_API_S.
 * @IWL_UCODE_TLV_API_FTM_NEW_RANGE_REQ: This ucode supports v7 of
 *    LOCATION_RANGE_REQ_CMD_API_S and v6 of LOCATION_RANGE_RESP_NTFY_API_S.
 * @IWL_UCODE_TLV_API_SCAN_OFFLOAD_CHANS: This ucode supports v2 of
 *    SCAN_OFFLOAD_PROFILE_MATCH_RESULTS_S and v3 of
 *    SCAN_OFFLOAD_PROFILES_QUERY_RSP_S.
 * @IWL_UCODE_TLV_API_MBSSID_HE: This ucode supports v2 of
 *    STA_CONTEXT_DOT11AX_API_S
 * @IWL_UCODE_TLV_CAPA_SAR_TABLE_VER: This ucode supports different sar
 *    version tables.
 *
 * @NUM_IWL_UCODE_TLV_API: number of bits used
 */
enum UCodeTLV_API {
    /* API Set 0 */
    UCODE_TLV_API_FRAGMENTED_SCAN       = 8,
    UCODE_TLV_API_WIFI_MCC_UPDATE       = 9,
    UCODE_TLV_API_LQ_SS_PARAMS          = 18,
    UCODE_TLV_API_NEW_VERSION           = 20,
    UCODE_TLV_API_SCAN_TSF_REPORT       = 28,
    UCODE_TLV_API_TKIP_MIC_KEYS         = 29,
    UCODE_TLV_API_STA_TYPE              = 30,
    UCODE_TLV_API_NAN2_VER2             = 31,
    /* API Set 1 */
    UCODE_TLV_API_ADAPTIVE_DWELL        = 32,
    UCODE_TLV_API_OCE                   = 33,
    UCODE_TLV_API_NEW_BEACON_TEMPLATE   = 34,
    UCODE_TLV_API_NEW_RX_STATS          = 35,
    UCODE_TLV_API_WOWLAN_KEY_MATERIAL   = 36,
    UCODE_TLV_API_QUOTA_LOW_LATENCY     = 38,
    UCODE_TLV_API_DEPRECATE_TTAK        = 41,
    UCODE_TLV_API_ADAPTIVE_DWELL_V2     = 42,
    UCODE_TLV_API_FRAG_EBS              = 44,
    UCODE_TLV_API_REDUCE_TX_POWER       = 45,
    UCODE_TLV_API_SHORT_BEACON_NOTIF    = 46,
    UCODE_TLV_API_BEACON_FILTER_V4      = 47,
    UCODE_TLV_API_REGULATORY_NVM_INFO   = 48,
    UCODE_TLV_API_FTM_NEW_RANGE_REQ     = 49,
    UCODE_TLV_API_SCAN_OFFLOAD_CHANS    = 50,
    UCODE_TLV_API_MBSSID_HE             = 52,
    UCODE_TLV_API_WOWLAN_TCP_SYN_WAKE   = 53,
    UCODE_TLV_API_FTM_RTT_ACCURACY      = 54,
    UCODE_TLV_API_SAR_TABLE_VER         = 55,
    UCODE_TLV_API_ADWELL_HB_DEF_N_AP    = 57,
    
    NUM_UCODE_TLV_API
};

/**
 * enum iwl_ucode_tlv_capa - ucode capabilities
 * @IWL_UCODE_TLV_CAPA_D0I3_SUPPORT: supports D0i3
 * @IWL_UCODE_TLV_CAPA_LAR_SUPPORT: supports Location Aware Regulatory
 * @IWL_UCODE_TLV_CAPA_UMAC_SCAN: supports UMAC scan.
 * @IWL_UCODE_TLV_CAPA_BEAMFORMER: supports Beamformer
 * @IWL_UCODE_TLV_CAPA_TDLS_SUPPORT: support basic TDLS functionality
 * @IWL_UCODE_TLV_CAPA_TXPOWER_INSERTION_SUPPORT: supports insertion of current
 *    tx power value into TPC Report action frame and Link Measurement Report
 *    action frame
 * @IWL_UCODE_TLV_CAPA_DS_PARAM_SET_IE_SUPPORT: supports updating current
 *    channel in DS parameter set element in probe requests.
 * @IWL_UCODE_TLV_CAPA_WFA_TPC_REP_IE_SUPPORT: supports adding TPC Report IE in
 *    probe requests.
 * @IWL_UCODE_TLV_CAPA_QUIET_PERIOD_SUPPORT: supports Quiet Period requests
 * @IWL_UCODE_TLV_CAPA_DQA_SUPPORT: supports dynamic queue allocation (DQA),
 *    which also implies support for the scheduler configuration command
 * @IWL_UCODE_TLV_CAPA_TDLS_CHANNEL_SWITCH: supports TDLS channel switching
 * @IWL_UCODE_TLV_CAPA_CNSLDTD_D3_D0_IMG: Consolidated D3-D0 image
 * @IWL_UCODE_TLV_CAPA_HOTSPOT_SUPPORT: supports Hot Spot Command
 * @IWL_UCODE_TLV_CAPA_DC2DC_SUPPORT: supports DC2DC Command
 * @IWL_UCODE_TLV_CAPA_CSUM_SUPPORT: supports TCP Checksum Offload
 * @IWL_UCODE_TLV_CAPA_RADIO_BEACON_STATS: support radio and beacon statistics
 * @IWL_UCODE_TLV_CAPA_P2P_SCM_UAPSD: supports U-APSD on p2p interface when it
 *    is standalone or with a BSS station interface in the same binding.
 * @IWL_UCODE_TLV_CAPA_BT_COEX_PLCR: enabled BT Coex packet level co-running
 * @IWL_UCODE_TLV_CAPA_LAR_MULTI_MCC: ucode supports LAR updates with different
 *    sources for the MCC. This TLV bit is a future replacement to
 *    IWL_UCODE_TLV_API_WIFI_MCC_UPDATE. When either is set, multi-source LAR
 *    is supported.
 * @IWL_UCODE_TLV_CAPA_BT_COEX_RRC: supports BT Coex RRC
 * @IWL_UCODE_TLV_CAPA_GSCAN_SUPPORT: supports gscan (no longer used)
 * @IWL_UCODE_TLV_CAPA_STA_PM_NOTIF: firmware will send STA PM notification
 * @IWL_UCODE_TLV_CAPA_TLC_OFFLOAD: firmware implements rate scaling algorithm
 * @IWL_UCODE_TLV_CAPA_DYNAMIC_QUOTA: firmware implements quota related
 * @IWL_UCODE_TLV_CAPA_COEX_SCHEMA_2: firmware implements Coex Schema 2
 * IWL_UCODE_TLV_CAPA_CHANNEL_SWITCH_CMD: firmware supports CSA command
 * @IWL_UCODE_TLV_CAPA_ULTRA_HB_CHANNELS: firmware supports ultra high band
 *    (6 GHz).
 * @IWL_UCODE_TLV_CAPA_CS_MODIFY: firmware supports modify action CSA command
 * @IWL_UCODE_TLV_CAPA_EXTENDED_DTS_MEASURE: extended DTS measurement
 * @IWL_UCODE_TLV_CAPA_SHORT_PM_TIMEOUTS: supports short PM timeouts
 * @IWL_UCODE_TLV_CAPA_BT_MPLUT_SUPPORT: supports bt-coex Multi-priority LUT
 * @IWL_UCODE_TLV_CAPA_CSA_AND_TBTT_OFFLOAD: the firmware supports CSA
 *    countdown offloading. Beacon notifications are not sent to the host.
 *    The fw also offloads TBTT alignment.
 * @IWL_UCODE_TLV_CAPA_BEACON_ANT_SELECTION: firmware will decide on what
 *    antenna the beacon should be transmitted
 * @IWL_UCODE_TLV_CAPA_BEACON_STORING: firmware will store the latest beacon
 *    from AP and will send it upon d0i3 exit.
 * @IWL_UCODE_TLV_CAPA_LAR_SUPPORT_V3: support LAR API V3
 * @IWL_UCODE_TLV_CAPA_CT_KILL_BY_FW: firmware responsible for CT-kill
 * @IWL_UCODE_TLV_CAPA_TEMP_THS_REPORT_SUPPORT: supports temperature
 *    thresholds reporting
 * @IWL_UCODE_TLV_CAPA_CTDP_SUPPORT: supports cTDP command
 * @IWL_UCODE_TLV_CAPA_USNIFFER_UNIFIED: supports usniffer enabled in
 *    regular image.
 * @IWL_UCODE_TLV_CAPA_EXTEND_SHARED_MEM_CFG: support getting more shared
 *    memory addresses from the firmware.
 * @IWL_UCODE_TLV_CAPA_LQM_SUPPORT: supports Link Quality Measurement
 * @IWL_UCODE_TLV_CAPA_TX_POWER_ACK: reduced TX power API has larger
 *    command size (command version 4) that supports toggling ACK TX
 *    power reduction.
 * @IWL_UCODE_TLV_CAPA_D3_DEBUG: supports debug recording during D3
 * @IWL_UCODE_TLV_CAPA_MCC_UPDATE_11AX_SUPPORT: MCC response support 11ax
 *    capability.
 * @IWL_UCODE_TLV_CAPA_CSI_REPORTING: firmware is capable of being configured
 *    to report the CSI information with (certain) RX frames
 * @IWL_UCODE_TLV_CAPA_FTM_CALIBRATED: has FTM calibrated and thus supports both
 *    initiator and responder
 *
 * @IWL_UCODE_TLV_CAPA_MLME_OFFLOAD: supports MLME offload
 *
 * @NUM_IWL_UCODE_TLV_CAPA: number of bits used
 */
enum ucode_tlv_capa {
    /* set 0 */
    UCODE_TLV_CAPA_D0I3_SUPPORT                 = 0,
    UCODE_TLV_CAPA_LAR_SUPPORT                  = 1,
    UCODE_TLV_CAPA_UMAC_SCAN                    = 2,
    UCODE_TLV_CAPA_BEAMFORMER                   = 3,
    UCODE_TLV_CAPA_TDLS_SUPPORT                 = 6,
    UCODE_TLV_CAPA_TXPOWER_INSERTION_SUPPORT    = 8,
    UCODE_TLV_CAPA_DS_PARAM_SET_IE_SUPPORT      = 9,
    UCODE_TLV_CAPA_WFA_TPC_REP_IE_SUPPORT       = 10,
    UCODE_TLV_CAPA_QUIET_PERIOD_SUPPORT         = 11,
    UCODE_TLV_CAPA_DQA_SUPPORT                  = 12,
    UCODE_TLV_CAPA_TDLS_CHANNEL_SWITCH          = 13,
    UCODE_TLV_CAPA_CNSLDTD_D3_D0_IMG            = 17,
    UCODE_TLV_CAPA_HOTSPOT_SUPPORT              = 18,
    UCODE_TLV_CAPA_DC2DC_CONFIG_SUPPORT         = 19,
    UCODE_TLV_CAPA_CSUM_SUPPORT                 = 21,
    UCODE_TLV_CAPA_RADIO_BEACON_STATS           = 22,
    UCODE_TLV_CAPA_P2P_SCM_UAPSD                = 26,
    UCODE_TLV_CAPA_BT_COEX_PLCR                 = 28,
    UCODE_TLV_CAPA_LAR_MULTI_MCC                = 29,
    UCODE_TLV_CAPA_BT_COEX_RRC                  = 30,
    UCODE_TLV_CAPA_GSCAN_SUPPORT                = 31,
    
    /* set 1 */
    UCODE_TLV_CAPA_STA_PM_NOTIF                 = 38,
    UCODE_TLV_CAPA_BINDING_CDB_SUPPORT          = 39,
    UCODE_TLV_CAPA_CDB_SUPPORT                  = 40,
    UCODE_TLV_CAPA_D0I3_END_FIRST               = 41,
    UCODE_TLV_CAPA_TLC_OFFLOAD                  = 43,
    UCODE_TLV_CAPA_DYNAMIC_QUOTA                = 44,
    UCODE_TLV_CAPA_COEX_SCHEMA_2                = 45,
    UCODE_TLV_CAPA_CHANNEL_SWITCH_CMD           = 46,
    UCODE_TLV_CAPA_ULTRA_HB_CHANNELS            = 48,
    UCODE_TLV_CAPA_FTM_CALIBRATED               = 47,
    UCODE_TLV_CAPA_CS_MODIFY                    = 49,
    
    /* set 2 */
    UCODE_TLV_CAPA_EXTENDED_DTS_MEASURE         = 64,
    UCODE_TLV_CAPA_SHORT_PM_TIMEOUTS            = 65,
    UCODE_TLV_CAPA_BT_MPLUT_SUPPORT             = 67,
    UCODE_TLV_CAPA_MULTI_QUEUE_RX_SUPPORT       = 68,
    UCODE_TLV_CAPA_CSA_AND_TBTT_OFFLOAD         = 70,
    UCODE_TLV_CAPA_BEACON_ANT_SELECTION         = 71,
    UCODE_TLV_CAPA_BEACON_STORING               = 72,
    UCODE_TLV_CAPA_LAR_SUPPORT_V3               = 73,
    UCODE_TLV_CAPA_CT_KILL_BY_FW                = 74,
    UCODE_TLV_CAPA_TEMP_THS_REPORT_SUPPORT      = 75,
    UCODE_TLV_CAPA_CTDP_SUPPORT                 = 76,
    UCODE_TLV_CAPA_USNIFFER_UNIFIED             = 77,
    UCODE_TLV_CAPA_EXTEND_SHARED_MEM_CFG        = 80,
    UCODE_TLV_CAPA_LQM_SUPPORT                  = 81,
    UCODE_TLV_CAPA_TX_POWER_ACK                 = 84,
    UCODE_TLV_CAPA_D3_DEBUG                     = 87,
    UCODE_TLV_CAPA_LED_CMD_SUPPORT              = 88,
    UCODE_TLV_CAPA_MCC_UPDATE_11AX_SUPPORT      = 89,
    UCODE_TLV_CAPA_CSI_REPORTING                = 90,
    
    /* set 3 */
    UCODE_TLV_CAPA_MLME_OFFLOAD                 = 96,
    
    NUM_UCODE_TLV_CAPA
};
#endif /* uCode_api_h */
