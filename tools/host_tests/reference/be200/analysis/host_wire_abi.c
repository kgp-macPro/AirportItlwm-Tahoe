/* Static host ABI fixture. Struct bodies copied verbatim from pinned GPL sources.
   No driver is built. References and line numbers: HOST_ABI_PROVENANCE.json. */
#include <stdint.h>
#include <stddef.h>
#include <stdio.h>
#define __packed __attribute__((packed))
#define ETHER_ADDR_LEN 6
#define ETH_ALEN 6
#define IWX_AC_NUM 4
#define AC_NUM 4
#define MAX_HE_SUPP_NSS 2
#define MAX_CHANNEL_BW_INDX_API_D_VER_2 5
#define IWL_MAX_HE_SUPP_NSS 2
#define IWL_HE_PKT_EXT_BW_NUM 5
#define IWL_HE_PKT_EXT_CONSTELLATION_NUM 2
typedef uint8_t u8;
typedef uint16_t __le16;
typedef uint32_t __le32;
typedef uint64_t __le64;
struct iwx_mac_client_data {
    uint8_t is_assoc;
    uint8_t esr_transition_timeout;
    uint16_t medium_sync_delay;
    uint16_t assoc_id;
    uint16_t reserved1;
    uint16_t data_policy;
    uint16_t reserved2;
    uint32_t ctwin;
} __packed;
struct iwx_mac_config_cmd {
    uint32_t id_and_color;
    uint32_t action;
    uint32_t mac_type;
    uint8_t local_mld_addr[ETHER_ADDR_LEN];
    uint16_t reserved_for_local_mld_addr;
    uint32_t filter_flags;
    uint16_t he_support;
    uint16_t he_ap_support;
    uint32_t eht_support;
    uint32_t nic_not_ack_enabled;
    struct iwx_mac_client_data client;
} __packed;
struct iwl_mac_client_data {
	u8 is_assoc;
	u8 esr_transition_timeout;
	__le16 medium_sync_delay;

	__le16 assoc_id;
	__le16 reserved1;
	__le16 data_policy;
	__le16 reserved2;
	__le32 ctwin;
} __packed;
struct iwl_mac_p2p_dev_data {
	__le32 is_disc_extended;
} __packed;
struct iwl_mac_config_cmd {
	/* COMMON_INDEX_HDR_API_S_VER_1 */
	__le32 id_and_color;
	__le32 action;
	/* MAC_CONTEXT_TYPE_API_E */
	__le32 mac_type;
	u8 local_mld_addr[6];
	__le16 reserved_for_local_mld_addr;
	__le32 filter_flags;
	__le16 he_support;
	__le16 he_ap_support;
	__le32 eht_support;
	__le32 nic_not_ack_enabled;
	/* MAC_CONTEXT_CONFIG_SPECIFIC_DATA_API_U_VER_2 */
	union {
		struct iwl_mac_client_data client;
		struct iwl_mac_p2p_dev_data p2p_dev;
	};
} __packed;
struct iwx_ac_qos {
    uint16_t cw_min;
    uint16_t cw_max;
    uint8_t aifsn;
    uint8_t fifos_mask;
    uint16_t edca_txop;
} __packed;
struct iwx_he_backoff_conf {
    uint16_t cwmin, cwmax, aifsn, mu_time;
} __packed;
struct iwx_link_config_cmd {
    uint32_t action;
    uint32_t link_id;
    uint32_t mac_id;
    uint32_t phy_id;
    uint8_t local_link_addr[ETHER_ADDR_LEN];
    uint16_t reserved_for_local_link_addr;
    uint32_t modify_mask;
    uint32_t active;
    uint32_t listen_lmac;
    uint32_t cck_rates;
    uint32_t ofdm_rates;
    uint32_t cck_short_preamble;
    uint32_t short_slot;
    uint32_t protection_flags;
    uint32_t qos_flags;
    struct iwx_ac_qos ac[IWX_AC_NUM + 1];
    uint8_t htc_trig_based_pkt_ext;
    uint8_t rand_alloc_ecwmin;
    uint8_t rand_alloc_ecwmax;
    uint8_t ndp_fdbk_buff_th_exp;
    struct iwx_he_backoff_conf trig_based_txf[IWX_AC_NUM];
    uint32_t bi;
    uint32_t dtim_interval;
    uint16_t puncture_mask;
    uint16_t frame_time_rts_th;
    uint32_t flags;
    uint32_t flags_mask;
    uint8_t ref_bssid_addr[ETHER_ADDR_LEN];
    uint16_t reserved_for_ref_bssid_addr;
    uint8_t bssid_index;
    uint8_t bss_color;
    uint8_t spec_link_id;
    uint8_t ul_mu_data_disable;
    uint8_t ibss_bssid_addr[ETHER_ADDR_LEN];
    uint16_t reserved_for_ibss_bssid_addr;
    uint32_t reserved3[8];
} __packed;
struct iwl_ac_qos {
	__le16 cw_min;
	__le16 cw_max;
	u8 aifsn;
	u8 fifos_mask;
	__le16 edca_txop;
} __packed;
struct iwl_he_backoff_conf {
	__le16 cwmin;
	__le16 cwmax;
	__le16 aifsn;
	__le16 mu_time;
} __packed;
struct iwl_link_config_cmd {
	__le32 action;
	__le32 link_id;
	__le32 mac_id;
	__le32 phy_id;
	u8 local_link_addr[6];
	__le16 reserved_for_local_link_addr;
	__le32 modify_mask;
	__le32 active;
	union {
		__le32 listen_lmac;
		__le32 reserved1;
	};
	__le32 cck_rates;
	__le32 ofdm_rates;
	__le32 cck_short_preamble;
	__le32 short_slot;
	__le32 protection_flags;
	/* MAC_QOS_PARAM_API_S_VER_1 */
	__le32 qos_flags;
	struct iwl_ac_qos ac[AC_NUM + 1];
	u8 htc_trig_based_pkt_ext;
	u8 rand_alloc_ecwmin;
	u8 rand_alloc_ecwmax;
	u8 ndp_fdbk_buff_th_exp;
	struct iwl_he_backoff_conf trig_based_txf[AC_NUM];
	__le32 bi;
	__le32 dtim_interval;
	__le16 puncture_mask; /* removed in _VER_3 */
	__le16 frame_time_rts_th;
	__le32 flags;
	__le32 flags_mask;
	/* The below fields are for multi-bssid */
	u8 ref_bssid_addr[6];
	__le16 reserved_for_ref_bssid_addr;
	u8 bssid_index;
	u8 bss_color;
	u8 spec_link_id;
	u8 reserved2;
	u8 ibss_bssid_addr[6];
	__le16 reserved_for_ibss_bssid_addr;
	__le32 reserved3[8];
} __packed;
struct iwx_sta_cfg_cmd {
    uint32_t sta_id;
    uint32_t link_id;
    uint8_t peer_mld_address[ETHER_ADDR_LEN];
    uint16_t reserved_for_peer_mld_address;
    uint8_t peer_link_address[ETHER_ADDR_LEN];
    uint16_t reserved_for_peer_link_address;
    uint32_t station_type;
    uint32_t assoc_id;
    uint32_t beamform_flags;
    uint32_t mfp;
    uint32_t mimo;
    uint32_t mimo_protection;
    uint32_t ack_enabled;
    uint32_t trig_rnd_alloc;
    uint32_t tx_ampdu_spacing;
    uint32_t tx_ampdu_max_size;
    uint32_t sp_length;
    uint32_t uapsd_acs;
    uint8_t pkt_ext[2][5][2]; /* NSS, bandwidth index, PPET8/PPET16 */
    uint32_t htc_flags;
} __packed;
struct iwl_he_pkt_ext_v2 {
	u8 pkt_ext_qam_th[MAX_HE_SUPP_NSS][MAX_CHANNEL_BW_INDX_API_D_VER_2][2];
} __packed;
struct iwl_mvm_sta_cfg_cmd {
	__le32 sta_id;
	__le32 link_id;
	u8 peer_mld_address[ETH_ALEN];
	__le16 reserved_for_peer_mld_address;
	u8 peer_link_address[ETH_ALEN];
	__le16 reserved_for_peer_link_address;
	__le32 station_type;
	__le32 assoc_id;
	__le32 beamform_flags;
	__le32 mfp;
	__le32 mimo;
	__le32 mimo_protection;
	__le32 ack_enabled;
	__le32 trig_rnd_alloc;
	__le32 tx_ampdu_spacing;
	__le32 tx_ampdu_max_size;
	__le32 sp_length;
	__le32 uapsd_acs;
	struct iwl_he_pkt_ext_v2 pkt_ext;
	__le32 htc_flags;
} __packed;
struct iwx_sta_remove_cmd {
    uint32_t sta_id;
} __packed;
struct iwl_mvm_remove_sta_cmd {
	__le32 sta_id;
} __packed;
struct iwx_sec_key_cmd {
    uint32_t action;
    union {
        struct {
            uint32_t sta_mask;
            uint32_t key_id;
            uint32_t key_flags;
            uint8_t key[32];
            uint8_t tkip_mic_rx_key[8];
            uint8_t tkip_mic_tx_key[8];
            uint64_t rx_seq;
            uint64_t tx_seq;
        } __packed add; /* SEC_KEY_ADD_CMD_API_S_VER_1 */
        struct {
            uint32_t old_sta_mask;
            uint32_t new_sta_mask;
            uint32_t key_id;
            uint32_t key_flags;
        } __packed modify; /* SEC_KEY_MODIFY_CMD_API_S_VER_1 */
        struct {
            uint32_t sta_mask;
            uint32_t key_id;
            uint32_t key_flags;
        } __packed remove; /* SEC_KEY_REMOVE_CMD_API_S_VER_1 */
    } __packed u;
} __packed;
struct iwl_sec_key_cmd {
	__le32 action;
	union {
		struct {
			__le32 sta_mask;
			__le32 key_id;
			__le32 key_flags;
			u8 key[32];
			u8 tkip_mic_rx_key[8];
			u8 tkip_mic_tx_key[8];
			__le64 rx_seq;
			__le64 tx_seq;
		} __packed add; /* SEC_KEY_ADD_CMD_API_S_VER_1 */
		struct {
			__le32 old_sta_mask;
			__le32 new_sta_mask;
			__le32 key_id;
			__le32 key_flags;
		} __packed modify; /* SEC_KEY_MODIFY_CMD_API_S_VER_1 */
		struct {
			__le32 sta_mask;
			__le32 key_id;
			__le32 key_flags;
		} __packed remove; /* SEC_KEY_REMOVE_CMD_API_S_VER_1 */
	} __packed u; /* SEC_KEY_OPERATION_API_U_VER_1 */
} __packed;
struct iwx_scd_queue_cfg_cmd {
    uint32_t operation;
    union {
        struct {
            uint32_t sta_mask;
            uint8_t tid;
            uint8_t reserved[3];
            uint32_t flags;
            uint32_t cb_size;
            uint64_t bc_dram_addr;
            uint64_t tfdq_dram_addr;
        } __packed add; /* TX_QUEUE_CFG_CMD_ADD_API_S_VER_1 */
        struct {
            uint32_t sta_mask;
            uint32_t tid;
        } __packed remove; /* TX_QUEUE_CFG_CMD_REMOVE_API_S_VER_1 */
        struct {
            uint32_t old_sta_mask;
            uint32_t tid;
            uint32_t new_sta_mask;
        } __packed modify; /* TX_QUEUE_CFG_CMD_MODIFY_API_S_VER_1 */
    } __packed u;
} __packed;
struct iwl_scd_queue_cfg_cmd {
	__le32 operation;
	union {
		struct {
			__le32 sta_mask;
			u8 tid;
			u8 reserved[3];
			__le32 flags;
			__le32 cb_size;
			__le64 bc_dram_addr;
			__le64 tfdq_dram_addr;
		} __packed add; /* TX_QUEUE_CFG_CMD_ADD_API_S_VER_1 */
		struct {
			__le32 sta_mask;
			__le32 tid;
		} __packed remove; /* TX_QUEUE_CFG_CMD_REMOVE_API_S_VER_1 */
		struct {
			__le32 old_sta_mask;
			__le32 tid;
			__le32 new_sta_mask;
		} __packed modify; /* TX_QUEUE_CFG_CMD_MODIFY_API_S_VER_1 */
	} __packed u; /* TX_QUEUE_CFG_CMD_OPERATION_API_U_VER_1 */
} __packed;
struct iwx_rx_completion_desc_bz {
    uint16_t rbid;
    uint8_t flags;
    uint8_t reserved;
} __packed;
_Static_assert(sizeof(struct iwx_mac_config_cmd) == 52, "iwx_mac_config_cmd");
_Static_assert(sizeof(struct iwl_mac_config_cmd) == 52, "iwl_mac_config_cmd");
_Static_assert(sizeof(struct iwx_link_config_cmd) == 208, "iwx_link_config_cmd");
_Static_assert(sizeof(struct iwl_link_config_cmd) == 208, "iwl_link_config_cmd");
_Static_assert(sizeof(struct iwx_sta_cfg_cmd) == 96, "iwx_sta_cfg_cmd");
_Static_assert(sizeof(struct iwl_mvm_sta_cfg_cmd) == 96, "iwl_mvm_sta_cfg_cmd");
_Static_assert(sizeof(struct iwx_sta_remove_cmd) == 4, "iwx_sta_remove_cmd");
_Static_assert(sizeof(struct iwl_mvm_remove_sta_cmd) == 4, "iwl_mvm_remove_sta_cmd");
_Static_assert(sizeof(struct iwx_sec_key_cmd) == 80, "iwx_sec_key_cmd");
_Static_assert(sizeof(struct iwl_sec_key_cmd) == 80, "iwl_sec_key_cmd");
_Static_assert(sizeof(struct iwx_scd_queue_cfg_cmd) == 36, "iwx_scd_queue_cfg_cmd");
_Static_assert(sizeof(struct iwl_scd_queue_cfg_cmd) == 36, "iwl_scd_queue_cfg_cmd");
_Static_assert(offsetof(struct iwx_mac_config_cmd, action) == 4, "iwx_mac_config_cmd.action");
_Static_assert(offsetof(struct iwl_mac_config_cmd, action) == 4, "iwl_mac_config_cmd.action");
_Static_assert(offsetof(struct iwx_mac_config_cmd, local_mld_addr) == 12, "iwx_mac_config_cmd.local_mld_addr");
_Static_assert(offsetof(struct iwl_mac_config_cmd, local_mld_addr) == 12, "iwl_mac_config_cmd.local_mld_addr");
_Static_assert(offsetof(struct iwx_link_config_cmd, modify_mask) == 24, "iwx_link_config_cmd.modify_mask");
_Static_assert(offsetof(struct iwl_link_config_cmd, modify_mask) == 24, "iwl_link_config_cmd.modify_mask");
_Static_assert(offsetof(struct iwx_link_config_cmd, active) == 28, "iwx_link_config_cmd.active");
_Static_assert(offsetof(struct iwl_link_config_cmd, active) == 28, "iwl_link_config_cmd.active");
_Static_assert(offsetof(struct iwx_link_config_cmd, listen_lmac) == 32, "iwx_link_config_cmd.listen_lmac");
_Static_assert(offsetof(struct iwl_link_config_cmd, listen_lmac) == 32, "iwl_link_config_cmd.listen_lmac");
_Static_assert(offsetof(struct iwx_sta_cfg_cmd, peer_mld_address) == 8, "iwx_sta_cfg_cmd.peer_mld_address");
_Static_assert(offsetof(struct iwl_mvm_sta_cfg_cmd, peer_mld_address) == 8, "iwl_mvm_sta_cfg_cmd.peer_mld_address");
_Static_assert(offsetof(struct iwx_sta_cfg_cmd, peer_link_address) == 16, "iwx_sta_cfg_cmd.peer_link_address");
_Static_assert(offsetof(struct iwl_mvm_sta_cfg_cmd, peer_link_address) == 16, "iwl_mvm_sta_cfg_cmd.peer_link_address");
_Static_assert(offsetof(struct iwx_sta_cfg_cmd, mfp) == 36, "iwx_sta_cfg_cmd.mfp");
_Static_assert(offsetof(struct iwl_mvm_sta_cfg_cmd, mfp) == 36, "iwl_mvm_sta_cfg_cmd.mfp");
_Static_assert(sizeof(struct iwx_rx_completion_desc_bz) == 4, "BZ RX completion");
int main(void) {
printf("iwx_mac_config_cmd=%zu iwl_mac_config_cmd=%zu\n", sizeof(struct iwx_mac_config_cmd), sizeof(struct iwl_mac_config_cmd));
printf("iwx_link_config_cmd=%zu iwl_link_config_cmd=%zu\n", sizeof(struct iwx_link_config_cmd), sizeof(struct iwl_link_config_cmd));
printf("iwx_sta_cfg_cmd=%zu iwl_mvm_sta_cfg_cmd=%zu\n", sizeof(struct iwx_sta_cfg_cmd), sizeof(struct iwl_mvm_sta_cfg_cmd));
printf("iwx_sta_remove_cmd=%zu iwl_mvm_remove_sta_cmd=%zu\n", sizeof(struct iwx_sta_remove_cmd), sizeof(struct iwl_mvm_remove_sta_cmd));
printf("iwx_sec_key_cmd=%zu iwl_sec_key_cmd=%zu\n", sizeof(struct iwx_sec_key_cmd), sizeof(struct iwl_sec_key_cmd));
printf("iwx_scd_queue_cfg_cmd=%zu iwl_scd_queue_cfg_cmd=%zu\n", sizeof(struct iwx_scd_queue_cfg_cmd), sizeof(struct iwl_scd_queue_cfg_cmd));
return 0; }
