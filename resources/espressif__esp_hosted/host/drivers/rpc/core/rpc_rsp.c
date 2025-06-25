// Copyright 2015-2022 Espressif Systems (Shanghai) PTE LTD
/* SPDX-License-Identifier: GPL-2.0 OR Apache-2.0 */

#include "rpc_core.h"
#include "rpc_slave_if.h"
#include "esp_log.h"
#include "esp_hosted_wifi_config.h"
#include "esp_hosted_transport.h"
#include "esp_hosted_bitmasks.h"
#include "esp_idf_version.h"
#include "esp_hosted_config.h"

DEFINE_LOG_TAG(rpc_rsp);

/* RPC response is result of remote function invokation at slave from host
 * The response will contain the return values of the RPC procedure
 * Return values typically will be simple integer return value of rpc call
 * for simple procedures. For function call with return value as a parameter,
 * RPC will contain full structure returned for that parameter and wrapper
 * level above will return these in expected pointer
 *
 * Responses will typically have two levels:
 * 1. protobuf level response received
 * 2. parse the response so that Ctrl_cmd_t app structure will be populated
 * or parsed from protobuf level response.
 *
 * For new RPC request, add up switch case for your message
 * For altogether new RPC function addition, please check
 * esp_hosted_fg/common/proto/esp_hosted_config.proto as a start point
 */

#define RPC_ERR_IN_RESP(msGparaM)                                             \
    if (rpc_msg->msGparaM->resp) {                                            \
        app_resp->resp_event_status = rpc_msg->msGparaM->resp;                \
        ESP_LOGW(TAG, "Hosted RPC_Resp [0x%"PRIx16"], uid [%"PRIu32"], resp code [%"PRIi32"]", \
                app_resp->msg_id, app_resp->uid, app_resp->resp_event_status); \
        goto fail_parse_rpc_msg;                                              \
    }


#define RPC_RSP_COPY_BYTES(dst,src) {                                         \
    if (src.data && src.len) {                                                \
        g_h.funcs->_h_memcpy(dst, src.data, src.len);                         \
    }                                                                         \
}

// copy the rpc record info to the wifi record info
static int rpc_copy_ap_record(wifi_ap_record_t *ap_record, WifiApRecord *rpc_ap_record)
{
	RPC_RSP_COPY_BYTES(ap_record->ssid, rpc_ap_record->ssid);
	RPC_RSP_COPY_BYTES(ap_record->bssid, rpc_ap_record->bssid);

	ap_record->primary         = rpc_ap_record->primary;
	ap_record->second          = rpc_ap_record->second;
	ap_record->rssi            = rpc_ap_record->rssi;
	ap_record->authmode        = rpc_ap_record->authmode;
	ap_record->pairwise_cipher = rpc_ap_record->pairwise_cipher;
	ap_record->group_cipher    = rpc_ap_record->group_cipher;
	ap_record->ant             = rpc_ap_record->ant;

	ap_record->phy_11b       = H_GET_BIT(WIFI_SCAN_AP_REC_phy_11b_BIT, rpc_ap_record->bitmask);
	ap_record->phy_11g       = H_GET_BIT(WIFI_SCAN_AP_REC_phy_11g_BIT, rpc_ap_record->bitmask);
	ap_record->phy_11n       = H_GET_BIT(WIFI_SCAN_AP_REC_phy_11n_BIT, rpc_ap_record->bitmask);
	ap_record->phy_lr        = H_GET_BIT(WIFI_SCAN_AP_REC_phy_lr_BIT, rpc_ap_record->bitmask);
	ap_record->phy_11a       = H_GET_BIT(WIFI_SCAN_AP_REC_phy_11a_BIT, rpc_ap_record->bitmask);
	ap_record->phy_11ac      = H_GET_BIT(WIFI_SCAN_AP_REC_phy_11ac_BIT, rpc_ap_record->bitmask);
	ap_record->phy_11ax      = H_GET_BIT(WIFI_SCAN_AP_REC_phy_11ax_BIT, rpc_ap_record->bitmask);
	ap_record->wps           = H_GET_BIT(WIFI_SCAN_AP_REC_wps_BIT, rpc_ap_record->bitmask);
	ap_record->ftm_responder = H_GET_BIT(WIFI_SCAN_AP_REC_ftm_responder_BIT, rpc_ap_record->bitmask);
	ap_record->ftm_initiator = H_GET_BIT(WIFI_SCAN_AP_REC_ftm_initiator_BIT, rpc_ap_record->bitmask);
	ap_record->reserved      = WIFI_SCAN_AP_GET_RESERVED_VAL(rpc_ap_record->bitmask);

	RPC_RSP_COPY_BYTES(ap_record->country.cc, rpc_ap_record->country->cc);
	ap_record->country.schan        = rpc_ap_record->country->schan;
	ap_record->country.nchan        = rpc_ap_record->country->nchan;
	ap_record->country.max_tx_power = rpc_ap_record->country->max_tx_power;
	ap_record->country.policy       = rpc_ap_record->country->policy;

	ESP_LOGD(TAG, "SSID: %s BSSid: " MACSTR, ap_record->ssid, MAC2STR(ap_record->bssid));
	ESP_LOGD(TAG, "Primary: %u Second: %u RSSI: %d Authmode: %u",
			ap_record->primary, ap_record->second,
			ap_record->rssi, ap_record->authmode
			);
	ESP_LOGD(TAG, "PairwiseCipher: %u Groupcipher: %u Ant: %u",
			ap_record->pairwise_cipher, ap_record->group_cipher,
			ap_record->ant
			);
	ESP_LOGD(TAG, "Bitmask: 11b:%u g:%u n:%u ax: %u lr:%u wps:%u ftm_resp:%u ftm_ini:%u res: %u",
			ap_record->phy_11b, ap_record->phy_11g,
			ap_record->phy_11n, ap_record->phy_11ax, ap_record->phy_lr,
			ap_record->wps, ap_record->ftm_responder,
			ap_record->ftm_initiator, ap_record->reserved
			);
	ESP_LOGD(TAG, "Country cc:%c%c schan: %u nchan: %u max_tx_pow: %d policy: %u",
			ap_record->country.cc[0], ap_record->country.cc[1], ap_record->country.schan,
			ap_record->country.nchan, ap_record->country.max_tx_power,
			ap_record->country.policy);

	WifiHeApInfo *p_c_he_ap = rpc_ap_record->he_ap;
	wifi_he_ap_info_t *p_a_he_ap = &ap_record->he_ap;
	// six bits
	p_a_he_ap->bss_color = p_c_he_ap->bitmask & 0x3F;
	p_a_he_ap->partial_bss_color = H_GET_BIT(WIFI_HE_AP_INFO_partial_bss_color_BIT, p_c_he_ap->bitmask);
	p_a_he_ap->bss_color_disabled = H_GET_BIT(WIFI_HE_AP_INFO_bss_color_disabled_BIT, p_c_he_ap->bitmask);

	ESP_LOGD(TAG, "HE_AP: bss_color %d, partial_bss_color %d, bss_color_disabled %d",
			p_a_he_ap->bss_color, p_a_he_ap->bss_color_disabled, p_a_he_ap->bss_color_disabled);

	ap_record->bandwidth    = rpc_ap_record->bandwidth;
	ap_record->vht_ch_freq1 = rpc_ap_record->vht_ch_freq1;
	ap_record->vht_ch_freq2 = rpc_ap_record->vht_ch_freq2;

	return 0;
}

/* This will copy rpc response from `Rpc` into
 * application structure `ctrl_cmd_t`
 * This function is called after protobuf decoding is successful
 **/
int rpc_parse_rsp(Rpc *rpc_msg, ctrl_cmd_t *app_resp)
{
	uint16_t i = 0;

	/* 1. Check non NULL */
	if (!rpc_msg || !app_resp) {
		ESP_LOGE(TAG, "NULL rpc resp or NULL App Resp");
		goto fail_parse_rpc_msg;
	}

	/* 2. update basic fields */
	app_resp->msg_type = RPC_TYPE__Resp;
	app_resp->msg_id = rpc_msg->msg_id;
	app_resp->uid = rpc_msg->uid;
	ESP_LOGI(TAG, " --> RPC_Resp [0x%x], uid %ld", app_resp->msg_id, app_resp->uid);

	/* 3. parse Rpc into ctrl_cmd_t */
	switch (rpc_msg->msg_id) {

	case RPC_ID__Resp_GetMACAddress : {
		RPC_FAIL_ON_NULL(resp_get_mac_address);
		RPC_ERR_IN_RESP(resp_get_mac_address);
		RPC_FAIL_ON_NULL(resp_get_mac_address->mac.data);

		RPC_RSP_COPY_BYTES(app_resp->u.wifi_mac.mac, rpc_msg->resp_get_mac_address->mac);
		ESP_LOGD(TAG, "Mac addr: "MACSTR, MAC2STR(app_resp->u.wifi_mac.mac));
		break;
	} case RPC_ID__Resp_SetMacAddress : {
		RPC_FAIL_ON_NULL(resp_set_mac_address);
		RPC_ERR_IN_RESP(resp_set_mac_address);
		break;
	} case RPC_ID__Resp_GetWifiMode : {
		RPC_FAIL_ON_NULL(resp_get_wifi_mode);
		RPC_ERR_IN_RESP(resp_get_wifi_mode);

		app_resp->u.wifi_mode.mode = rpc_msg->resp_get_wifi_mode->mode;
		break;
	} case RPC_ID__Resp_SetWifiMode : {
		RPC_FAIL_ON_NULL(resp_set_wifi_mode);
		RPC_ERR_IN_RESP(resp_set_wifi_mode);
		break;
	} case RPC_ID__Resp_WifiSetPs: {
		RPC_FAIL_ON_NULL(resp_wifi_set_ps);
		RPC_ERR_IN_RESP(resp_wifi_set_ps);
		break;
	} case RPC_ID__Resp_WifiGetPs : {
		RPC_FAIL_ON_NULL(resp_wifi_get_ps);
		RPC_ERR_IN_RESP(resp_wifi_get_ps);
		app_resp->u.wifi_ps.ps_mode = rpc_msg->resp_wifi_get_ps->type;
		break;
	} case RPC_ID__Resp_OTABegin : {
		RPC_FAIL_ON_NULL(resp_ota_begin);
		RPC_ERR_IN_RESP(resp_ota_begin);
		if (rpc_msg->resp_ota_begin->resp) {
			ESP_LOGE(TAG, "OTA Begin Failed");
			goto fail_parse_rpc_msg;
		}
		break;
	} case RPC_ID__Resp_OTAWrite : {
		RPC_FAIL_ON_NULL(resp_ota_write);
		RPC_ERR_IN_RESP(resp_ota_write);
		if (rpc_msg->resp_ota_write->resp) {
			ESP_LOGE(TAG, "OTA write failed");
			goto fail_parse_rpc_msg;
		}
		break;
	} case RPC_ID__Resp_OTAEnd: {
		RPC_FAIL_ON_NULL(resp_ota_end);
		if (rpc_msg->resp_ota_end->resp) {
			ESP_LOGE(TAG, "OTA write failed");
			goto fail_parse_rpc_msg;
		}
		break;
	} case RPC_ID__Resp_WifiSetMaxTxPower: {
		RPC_FAIL_ON_NULL(resp_set_wifi_max_tx_power);
		RPC_ERR_IN_RESP(resp_set_wifi_max_tx_power);
		break;
	} case RPC_ID__Resp_WifiGetMaxTxPower: {
		RPC_FAIL_ON_NULL(resp_get_wifi_max_tx_power);
		RPC_ERR_IN_RESP(resp_get_wifi_max_tx_power);
		app_resp->u.wifi_tx_power.power =
			rpc_msg->resp_get_wifi_max_tx_power->power;
		break;
	} case RPC_ID__Resp_ConfigHeartbeat: {
		RPC_FAIL_ON_NULL(resp_config_heartbeat);
		RPC_ERR_IN_RESP(resp_config_heartbeat);
		break;
	} case RPC_ID__Resp_WifiInit: {
		RPC_FAIL_ON_NULL(resp_wifi_init);
		RPC_ERR_IN_RESP(resp_wifi_init);
		break;
	} case RPC_ID__Resp_WifiDeinit: {
		RPC_FAIL_ON_NULL(resp_wifi_deinit);
		RPC_ERR_IN_RESP(resp_wifi_deinit);
		break;
	} case RPC_ID__Resp_WifiStart: {
		RPC_FAIL_ON_NULL(resp_wifi_start);
		RPC_ERR_IN_RESP(resp_wifi_start);
		break;
	} case RPC_ID__Resp_WifiStop: {
		RPC_FAIL_ON_NULL(resp_wifi_stop);
		RPC_ERR_IN_RESP(resp_wifi_stop);
		break;
	} case RPC_ID__Resp_WifiConnect: {
		RPC_FAIL_ON_NULL(resp_wifi_connect);
		RPC_ERR_IN_RESP(resp_wifi_connect);
		break;
	} case RPC_ID__Resp_WifiDisconnect: {
		RPC_FAIL_ON_NULL(resp_wifi_disconnect);
		RPC_ERR_IN_RESP(resp_wifi_disconnect);
		break;
	} case RPC_ID__Resp_WifiSetConfig: {
		RPC_FAIL_ON_NULL(resp_wifi_set_config);
		RPC_ERR_IN_RESP(resp_wifi_set_config);
		break;
	} case RPC_ID__Resp_WifiGetConfig: {
		RPC_FAIL_ON_NULL(resp_wifi_set_config);
		RPC_ERR_IN_RESP(resp_wifi_set_config);

		app_resp->u.wifi_config.iface = rpc_msg->resp_wifi_get_config->iface;

		switch (app_resp->u.wifi_config.iface) {

		case WIFI_IF_STA: {
			wifi_sta_config_t * p_a_sta = &(app_resp->u.wifi_config.u.sta);
			WifiStaConfig * p_c_sta = rpc_msg->resp_wifi_get_config->cfg->sta;
			RPC_RSP_COPY_BYTES(p_a_sta->ssid, p_c_sta->ssid);
			RPC_RSP_COPY_BYTES(p_a_sta->password, p_c_sta->password);
			p_a_sta->scan_method = p_c_sta->scan_method;
			p_a_sta->bssid_set = p_c_sta->bssid_set;

			if (p_a_sta->bssid_set)
				RPC_RSP_COPY_BYTES(p_a_sta->bssid, p_c_sta->bssid);

			p_a_sta->channel = p_c_sta->channel;
			p_a_sta->listen_interval = p_c_sta->listen_interval;
			p_a_sta->sort_method = p_c_sta->sort_method;
			p_a_sta->threshold.rssi = p_c_sta->threshold->rssi;
			p_a_sta->threshold.authmode = p_c_sta->threshold->authmode;
			//p_a_sta->ssid_hidden = p_c_sta->ssid_hidden;
			//p_a_sta->max_connections = p_c_sta->max_connections;
			p_a_sta->pmf_cfg.capable = p_c_sta->pmf_cfg->capable;
			p_a_sta->pmf_cfg.required = p_c_sta->pmf_cfg->required;

			p_a_sta->rm_enabled = H_GET_BIT(WIFI_STA_CONFIG_1_rm_enabled, p_c_sta->bitmask);
			p_a_sta->btm_enabled = H_GET_BIT(WIFI_STA_CONFIG_1_btm_enabled, p_c_sta->bitmask);
			p_a_sta->mbo_enabled = H_GET_BIT(WIFI_STA_CONFIG_1_mbo_enabled, p_c_sta->bitmask);
			p_a_sta->ft_enabled = H_GET_BIT(WIFI_STA_CONFIG_1_ft_enabled, p_c_sta->bitmask);
			p_a_sta->owe_enabled = H_GET_BIT(WIFI_STA_CONFIG_1_owe_enabled, p_c_sta->bitmask);
			p_a_sta->transition_disable = H_GET_BIT(WIFI_STA_CONFIG_1_transition_disable, p_c_sta->bitmask);

#if H_DECODE_WIFI_RESERVED_FIELD
  #if H_WIFI_NEW_RESERVED_FIELD_NAMES
			p_a_sta->reserved1 = WIFI_STA_CONFIG_1_GET_RESERVED_VAL(p_c_sta->bitmask);
  #else
			p_a_sta->reserved = WIFI_STA_CONFIG_1_GET_RESERVED_VAL(p_c_sta->bitmask);
  #endif
#endif

			p_a_sta->sae_pwe_h2e = p_c_sta->sae_pwe_h2e;
			p_a_sta->failure_retry_cnt = p_c_sta->failure_retry_cnt;

			p_a_sta->he_dcm_set = H_GET_BIT(WIFI_STA_CONFIG_2_he_dcm_set_BIT, p_c_sta->he_bitmask);

			// WIFI_HE_STA_CONFIG_he_dcm_max_constellation_tx is two bits wide
			p_a_sta->he_dcm_max_constellation_tx = (p_c_sta->he_bitmask >> WIFI_STA_CONFIG_2_he_dcm_max_constellation_tx_BITS) & 0x03;
			// WIFI_HE_STA_CONFIG_he_dcm_max_constellation_rx is two bits wide
			p_a_sta->he_dcm_max_constellation_rx = (p_c_sta->he_bitmask >> WIFI_STA_CONFIG_2_he_dcm_max_constellation_rx_BITS) & 0x03;
			p_a_sta->he_mcs9_enabled = H_GET_BIT(WIFI_STA_CONFIG_2_he_mcs9_enabled_BIT, p_c_sta->he_bitmask);
			p_a_sta->he_su_beamformee_disabled = H_GET_BIT(WIFI_STA_CONFIG_2_he_su_beamformee_disabled_BIT, p_c_sta->he_bitmask);
			p_a_sta->he_trig_su_bmforming_feedback_disabled = H_GET_BIT(WIFI_STA_CONFIG_2_he_trig_su_bmforming_feedback_disabled_BIT, p_c_sta->bitmask);
			p_a_sta->he_trig_mu_bmforming_partial_feedback_disabled = H_GET_BIT(WIFI_STA_CONFIG_2_he_trig_mu_bmforming_partial_feedback_disabled_BIT, p_c_sta->bitmask);
			p_a_sta->he_trig_cqi_feedback_disabled = H_GET_BIT(WIFI_STA_CONFIG_2_he_trig_cqi_feedback_disabled_BIT, p_c_sta->bitmask);

#if H_WIFI_VHT_FIELDS_AVAILABLE
			p_a_sta->vht_su_beamformee_disabled = H_GET_BIT(WIFI_STA_CONFIG_2_vht_su_beamformee_disabled, p_c_sta->he_bitmask);
			p_a_sta->vht_mu_beamformee_disabled = H_GET_BIT(WIFI_STA_CONFIG_2_vht_mu_beamformee_disabled, p_c_sta->he_bitmask);
			p_a_sta->vht_mcs8_enabled = H_GET_BIT(WIFI_STA_CONFIG_2_vht_mcs8_enabled, p_c_sta->he_bitmask);
#endif

#if H_DECODE_WIFI_RESERVED_FIELD
  #if H_WIFI_NEW_RESERVED_FIELD_NAMES
			p_a_sta->reserved2 = WIFI_STA_CONFIG_2_GET_RESERVED_VAL(p_c_sta->he_bitmask);
  #else
			p_a_sta->he_reserved = WIFI_STA_CONFIG_2_GET_RESERVED_VAL(p_c_sta->he_bitmask);
  #endif
#endif

			break;
		}
		case WIFI_IF_AP: {
			wifi_ap_config_t * p_a_ap = &(app_resp->u.wifi_config.u.ap);
			WifiApConfig * p_c_ap = rpc_msg->resp_wifi_get_config->cfg->ap;

			RPC_RSP_COPY_BYTES(p_a_ap->ssid, p_c_ap->ssid);
			RPC_RSP_COPY_BYTES(p_a_ap->password, p_c_ap->password);
			p_a_ap->ssid_len = p_c_ap->ssid_len;
			p_a_ap->channel = p_c_ap->channel;
			p_a_ap->authmode = p_c_ap->authmode;
			p_a_ap->ssid_hidden = p_c_ap->ssid_hidden;
			p_a_ap->max_connection = p_c_ap->max_connection;
			p_a_ap->beacon_interval = p_c_ap->beacon_interval;
			p_a_ap->pairwise_cipher = p_c_ap->pairwise_cipher;
			p_a_ap->ftm_responder = p_c_ap->ftm_responder;
			p_a_ap->pmf_cfg.capable = p_c_ap->pmf_cfg->capable;
			p_a_ap->pmf_cfg.required = p_c_ap->pmf_cfg->required;
			break;
		}
		default:
			ESP_LOGE(TAG, "Unsupported WiFi interface[%u]", app_resp->u.wifi_config.iface);
		} //switch

		break;

	} case RPC_ID__Resp_WifiScanStart: {
		RPC_FAIL_ON_NULL(resp_wifi_scan_start);
		RPC_ERR_IN_RESP(resp_wifi_scan_start);
		break;
	} case RPC_ID__Resp_WifiScanStop: {
		RPC_FAIL_ON_NULL(resp_wifi_scan_stop);
		RPC_ERR_IN_RESP(resp_wifi_scan_stop);
		break;
	} case RPC_ID__Resp_WifiScanGetApNum: {
		wifi_scan_ap_list_t *p_a = &(app_resp->u.wifi_scan_ap_list);
		RPC_FAIL_ON_NULL(resp_wifi_scan_get_ap_num);
		RPC_ERR_IN_RESP(resp_wifi_scan_get_ap_num);

		p_a->number = rpc_msg->resp_wifi_scan_get_ap_num->number;
		break;
	} case RPC_ID__Resp_WifiScanGetApRecord: {
		RPC_FAIL_ON_NULL(resp_wifi_scan_get_ap_records);
		RPC_ERR_IN_RESP(resp_wifi_scan_get_ap_records);

		rpc_copy_ap_record(&(app_resp->u.wifi_ap_record),
						   rpc_msg->resp_wifi_scan_get_ap_record->ap_record);
		break;
	} case RPC_ID__Resp_WifiScanGetApRecords: {
		wifi_scan_ap_list_t *p_a = &(app_resp->u.wifi_scan_ap_list);
		wifi_ap_record_t *list = NULL;
		WifiApRecord **p_c_list = NULL;

		RPC_FAIL_ON_NULL(resp_wifi_scan_get_ap_records);
		RPC_ERR_IN_RESP(resp_wifi_scan_get_ap_records);
		p_c_list = rpc_msg->resp_wifi_scan_get_ap_records->ap_records;

		p_a->number = rpc_msg->resp_wifi_scan_get_ap_records->number;

		if (!p_a->number) {
			ESP_LOGI(TAG, "No AP found");
			goto fail_parse_rpc_msg;
		}
		ESP_LOGD(TAG, "Num AP records: %u",
				app_resp->u.wifi_scan_ap_list.number);

		RPC_FAIL_ON_NULL(resp_wifi_scan_get_ap_records->ap_records);

		list = (wifi_ap_record_t*)g_h.funcs->_h_calloc(p_a->number,
				sizeof(wifi_ap_record_t));
		p_a->out_list = list;

		RPC_FAIL_ON_NULL_PRINT(list, "Malloc Failed");

		app_resp->app_free_buff_func = g_h.funcs->_h_free;
		app_resp->app_free_buff_hdl = list;

		ESP_LOGD(TAG, "Number of available APs is %d", p_a->number);
		for (i=0; i<p_a->number; i++) {
			rpc_copy_ap_record(&list[i], p_c_list[i]);
		}
		break;
	} case RPC_ID__Resp_WifiStaGetApInfo: {
		WifiApRecord *p_c = NULL;
		wifi_ap_record_t *ap_info = NULL;
		wifi_scan_ap_list_t *p_a = &(app_resp->u.wifi_scan_ap_list);

		RPC_FAIL_ON_NULL(resp_wifi_sta_get_ap_info);
		RPC_ERR_IN_RESP(resp_wifi_sta_get_ap_info);
		p_c = rpc_msg->resp_wifi_sta_get_ap_info->ap_record;

		p_a->number = 1;

		RPC_FAIL_ON_NULL(resp_wifi_sta_get_ap_info->ap_record);

		ap_info = (wifi_ap_record_t*)g_h.funcs->_h_calloc(p_a->number,
				sizeof(wifi_ap_record_t));
		p_a->out_list = ap_info;

		RPC_FAIL_ON_NULL_PRINT(ap_info, "Malloc Failed");

		app_resp->app_free_buff_func = g_h.funcs->_h_free;
		app_resp->app_free_buff_hdl = ap_info;

		rpc_copy_ap_record(ap_info, p_c);
		break;
	} case RPC_ID__Resp_WifiClearApList: {
		RPC_FAIL_ON_NULL(resp_wifi_clear_ap_list);
		RPC_ERR_IN_RESP(resp_wifi_clear_ap_list);
		break;
	} case RPC_ID__Resp_WifiRestore: {
		RPC_FAIL_ON_NULL(resp_wifi_restore);
		RPC_ERR_IN_RESP(resp_wifi_restore);
		break;
	} case RPC_ID__Resp_WifiClearFastConnect: {
		RPC_FAIL_ON_NULL(resp_wifi_clear_fast_connect);
		RPC_ERR_IN_RESP(resp_wifi_clear_fast_connect);
		break;
	} case RPC_ID__Resp_WifiDeauthSta: {
		RPC_FAIL_ON_NULL(resp_wifi_deauth_sta);
		RPC_ERR_IN_RESP(resp_wifi_deauth_sta);
		break;
	} case RPC_ID__Resp_WifiSetStorage: {
		RPC_FAIL_ON_NULL(resp_wifi_set_storage);
		RPC_ERR_IN_RESP(resp_wifi_set_storage);
		break;
	} case RPC_ID__Resp_WifiSetBandwidth: {
		RPC_FAIL_ON_NULL(resp_wifi_set_bandwidth);
		RPC_ERR_IN_RESP(resp_wifi_set_bandwidth);
		break;
	} case RPC_ID__Resp_WifiGetBandwidth: {
		RPC_FAIL_ON_NULL(resp_wifi_get_bandwidth);
		RPC_ERR_IN_RESP(resp_wifi_get_bandwidth);
		app_resp->u.wifi_bandwidth.bw =
			rpc_msg->resp_wifi_get_bandwidth->bw;
		break;
	} case RPC_ID__Resp_WifiSetChannel: {
		RPC_FAIL_ON_NULL(resp_wifi_set_channel);
		RPC_ERR_IN_RESP(resp_wifi_set_channel);
		break;
	} case RPC_ID__Resp_WifiGetChannel: {
		RPC_FAIL_ON_NULL(resp_wifi_get_channel);
		RPC_ERR_IN_RESP(resp_wifi_get_channel);
		app_resp->u.wifi_channel.primary =
			rpc_msg->resp_wifi_get_channel->primary;
		app_resp->u.wifi_channel.second =
			rpc_msg->resp_wifi_get_channel->second;
		break;
	} case RPC_ID__Resp_WifiSetCountryCode: {
		RPC_FAIL_ON_NULL(resp_wifi_set_country_code);
		RPC_ERR_IN_RESP(resp_wifi_set_country_code);
		break;
	} case RPC_ID__Resp_WifiGetCountryCode: {
		RPC_FAIL_ON_NULL(resp_wifi_get_country_code);
		RPC_ERR_IN_RESP(resp_wifi_get_country_code);

		RPC_RSP_COPY_BYTES(&app_resp->u.wifi_country_code.cc[0],
				rpc_msg->resp_wifi_get_country_code->country);
		break;
	} case RPC_ID__Resp_WifiSetCountry: {
		RPC_FAIL_ON_NULL(resp_wifi_set_country);
		RPC_ERR_IN_RESP(resp_wifi_set_country);
		break;
	} case RPC_ID__Resp_WifiGetCountry: {
		RPC_FAIL_ON_NULL(resp_wifi_get_country);
		RPC_ERR_IN_RESP(resp_wifi_get_country);

		RPC_RSP_COPY_BYTES(&app_resp->u.wifi_country.cc[0],
				rpc_msg->resp_wifi_get_country->country->cc);
		app_resp->u.wifi_country.schan        = rpc_msg->resp_wifi_get_country->country->schan;
		app_resp->u.wifi_country.nchan        = rpc_msg->resp_wifi_get_country->country->nchan;
		app_resp->u.wifi_country.max_tx_power = rpc_msg->resp_wifi_get_country->country->max_tx_power;
		app_resp->u.wifi_country.policy       = rpc_msg->resp_wifi_get_country->country->policy;
		break;
	} case RPC_ID__Resp_WifiApGetStaList: {
		RPC_FAIL_ON_NULL(resp_wifi_ap_get_sta_list);
		RPC_ERR_IN_RESP(resp_wifi_ap_get_sta_list);

		// handle case where slave's num is bigger than our ESP_WIFI_MAX_CONN_NUM
		uint32_t num_stations = rpc_msg->resp_wifi_ap_get_sta_list->sta_list->num;
		if (num_stations > ESP_WIFI_MAX_CONN_NUM) {
			ESP_LOGW(TAG, "Slave returned %ld connected stations, but we can only accept %d items", num_stations, ESP_WIFI_MAX_CONN_NUM);
			num_stations = ESP_WIFI_MAX_CONN_NUM;
		}

		WifiStaInfo ** p_c_sta_list = rpc_msg->resp_wifi_ap_get_sta_list->sta_list->sta;

		for (int i = 0; i < num_stations; i++) {
			wifi_sta_info_t * p_a_sta = &app_resp->u.wifi_ap_sta_list.sta[i];

			RPC_RSP_COPY_BYTES(p_a_sta->mac, p_c_sta_list[i]->mac);
			p_a_sta->rssi = p_c_sta_list[i]->rssi;

			p_a_sta->phy_11b = H_GET_BIT(WIFI_STA_INFO_phy_11b_BIT, p_c_sta_list[i]->bitmask);
			p_a_sta->phy_11g = H_GET_BIT(WIFI_STA_INFO_phy_11g_BIT, p_c_sta_list[i]->bitmask);
			p_a_sta->phy_11n = H_GET_BIT(WIFI_STA_INFO_phy_11n_BIT, p_c_sta_list[i]->bitmask);
			p_a_sta->phy_lr = H_GET_BIT(WIFI_STA_INFO_phy_lr_BIT, p_c_sta_list[i]->bitmask);
			p_a_sta->phy_11ax = H_GET_BIT(WIFI_STA_INFO_phy_11ax_BIT, p_c_sta_list[i]->bitmask);
			p_a_sta->is_mesh_child = H_GET_BIT(WIFI_STA_INFO_is_mesh_child_BIT, p_c_sta_list[i]->bitmask);
			p_a_sta->reserved = WIFI_STA_INFO_GET_RESERVED_VAL(p_c_sta_list[i]->bitmask);
		}

		app_resp->u.wifi_ap_sta_list.num = rpc_msg->resp_wifi_ap_get_sta_list->sta_list->num;
		break;
	} case RPC_ID__Resp_WifiApGetStaAid: {
		RPC_FAIL_ON_NULL(resp_wifi_ap_get_sta_aid);
		RPC_ERR_IN_RESP(resp_wifi_ap_get_sta_aid);

		app_resp->u.wifi_ap_get_sta_aid.aid = rpc_msg->resp_wifi_ap_get_sta_aid->aid;
		break;
	} case RPC_ID__Resp_WifiStaGetRssi: {
		RPC_FAIL_ON_NULL(resp_wifi_sta_get_rssi);
		RPC_ERR_IN_RESP(resp_wifi_sta_get_rssi);

		app_resp->u.wifi_sta_get_rssi.rssi = rpc_msg->resp_wifi_sta_get_rssi->rssi;
		break;
	} case RPC_ID__Resp_WifiSetProtocol: {
		RPC_FAIL_ON_NULL(resp_wifi_set_protocol);
		RPC_ERR_IN_RESP(resp_wifi_set_protocol);
		break;
	} case RPC_ID__Resp_WifiGetProtocol: {
		RPC_FAIL_ON_NULL(resp_wifi_get_protocol);
		RPC_ERR_IN_RESP(resp_wifi_get_protocol);
		app_resp->u.wifi_protocol.protocol_bitmap =
			rpc_msg->resp_wifi_get_protocol->protocol_bitmap;
		break;
	} case RPC_ID__Resp_WifiStaGetNegotiatedPhymode: {
		RPC_FAIL_ON_NULL(resp_wifi_sta_get_negotiated_phymode);
		RPC_ERR_IN_RESP(resp_wifi_sta_get_negotiated_phymode);
		app_resp->u.wifi_sta_get_negotiated_phymode.phymode =
			rpc_msg->resp_wifi_sta_get_negotiated_phymode->phymode;
		break;
	} case RPC_ID__Resp_WifiStaGetAid: {
		RPC_FAIL_ON_NULL(resp_wifi_sta_get_aid);
		RPC_ERR_IN_RESP(resp_wifi_sta_get_aid);
		app_resp->u.wifi_sta_get_aid.aid =
			rpc_msg->resp_wifi_sta_get_aid->aid;
		break;
	} case RPC_ID__Resp_GetCoprocessorFwVersion: {
		RPC_FAIL_ON_NULL(resp_get_coprocessor_fwversion);
		RPC_ERR_IN_RESP(resp_get_coprocessor_fwversion);
		app_resp->u.coprocessor_fwversion.major1 =
			rpc_msg->resp_get_coprocessor_fwversion->major1;
		app_resp->u.coprocessor_fwversion.minor1 =
			rpc_msg->resp_get_coprocessor_fwversion->minor1;
		app_resp->u.coprocessor_fwversion.patch1 =
			rpc_msg->resp_get_coprocessor_fwversion->patch1;
		break;
#if H_WIFI_DUALBAND_SUPPORT
	} case RPC_ID__Resp_WifiSetProtocols: {
		RPC_FAIL_ON_NULL(resp_wifi_set_protocols);
		RPC_ERR_IN_RESP(resp_wifi_set_protocols);
		app_resp->u.wifi_protocols.ifx =
			rpc_msg->resp_wifi_set_protocols->ifx;
		break;
	} case RPC_ID__Resp_WifiGetProtocols: {
		RPC_FAIL_ON_NULL(resp_wifi_get_protocols);
		RPC_ERR_IN_RESP(resp_wifi_get_protocols);
		app_resp->u.wifi_protocols.ifx =
			rpc_msg->resp_wifi_get_protocols->ifx;
		app_resp->u.wifi_protocols.ghz_2g =
			rpc_msg->resp_wifi_get_protocols->protocols->ghz_2g;
		app_resp->u.wifi_protocols.ghz_5g =
			rpc_msg->resp_wifi_get_protocols->protocols->ghz_5g;
		break;
	} case RPC_ID__Resp_WifiSetBandwidths: {
		RPC_FAIL_ON_NULL(resp_wifi_set_bandwidths);
		RPC_ERR_IN_RESP(resp_wifi_set_bandwidths);
		app_resp->u.wifi_bandwidths.ifx =
			rpc_msg->resp_wifi_set_bandwidths->ifx;
		break;
	} case RPC_ID__Resp_WifiGetBandwidths: {
		RPC_FAIL_ON_NULL(resp_wifi_get_bandwidths);
		RPC_ERR_IN_RESP(resp_wifi_get_bandwidths);
		app_resp->u.wifi_bandwidths.ifx =
			rpc_msg->resp_wifi_get_bandwidths->ifx;
		app_resp->u.wifi_bandwidths.ghz_2g =
			rpc_msg->resp_wifi_get_bandwidths->bandwidths->ghz_2g;
		app_resp->u.wifi_bandwidths.ghz_5g =
			rpc_msg->resp_wifi_get_bandwidths->bandwidths->ghz_5g;
		break;
	} case RPC_ID__Resp_WifiSetBand: {
		RPC_FAIL_ON_NULL(resp_wifi_set_country_code);
		RPC_ERR_IN_RESP(resp_wifi_set_country_code);
		break;
	} case RPC_ID__Resp_WifiGetBand: {
		RPC_FAIL_ON_NULL(resp_wifi_get_band);
		RPC_ERR_IN_RESP(resp_wifi_get_band);
		app_resp->u.wifi_band =
			rpc_msg->resp_wifi_get_band->band;
		break;
	} case RPC_ID__Resp_WifiSetBandMode: {
		RPC_FAIL_ON_NULL(resp_wifi_set_country_code);
		RPC_ERR_IN_RESP(resp_wifi_set_country_code);
		break;
	} case RPC_ID__Resp_WifiGetBandMode: {
		RPC_FAIL_ON_NULL(resp_wifi_get_bandmode);
		RPC_ERR_IN_RESP(resp_wifi_get_bandmode);
		app_resp->u.wifi_band_mode =
			rpc_msg->resp_wifi_get_bandmode->bandmode;
		break;
#endif
	} default: {
		ESP_LOGE(TAG, "Unsupported rpc Resp[%u]", rpc_msg->msg_id);
		goto fail_parse_rpc_msg;
		break;
	}

	}

	app_resp->resp_event_status = SUCCESS;
	return SUCCESS;

	/* 5. Free up buffers in failure cases */
fail_parse_rpc_msg:
	return SUCCESS;
}
