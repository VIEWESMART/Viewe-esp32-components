/*
 * SPDX-FileCopyrightText: 2026 VIEWE
 * SPDX-License-Identifier: Apache-2.0
 */

#include "bsp/viewe_desknob.h"

#include <string.h>
#include "bsp_err_check.h"
#include "esp_bt.h"
#include "esp_bt_main.h"
#include "esp_event.h"
#if CONFIG_BTDM_CTRL_MODE_BLE_ONLY || CONFIG_BTDM_CTRL_MODE_BTDM
#include "esp_gap_ble_api.h"
#define BSP_HAS_BLE 1
#else
#define BSP_HAS_BLE 0
#endif
#if CONFIG_BTDM_CTRL_MODE_BR_EDR_ONLY || CONFIG_BTDM_CTRL_MODE_BTDM
#include "esp_gap_bt_api.h"
#define BSP_HAS_CLASSIC 1
#else
#define BSP_HAS_CLASSIC 0
#endif
#include "esp_log.h"
#include "esp_netif.h"
#include "esp_wifi.h"
#include "nvs_flash.h"

static const char *TAG = "viewe_desknob.radio";

static bool s_nvs_ready;
static bool s_netif_ready;
static bool s_wifi_started;
static bool s_bt_ready;
static bsp_bt_mode_t s_bt_mode;
static char s_bt_name[32] = "VIEWE-DeskNob";

static esp_err_t bsp_nvs_init(void)
{
    if (s_nvs_ready) {
        return ESP_OK;
    }
    esp_err_t err = nvs_flash_init();
    if (err == ESP_ERR_NVS_NO_FREE_PAGES || err == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        BSP_ERROR_CHECK_RETURN_ERR(nvs_flash_erase());
        err = nvs_flash_init();
    }
    BSP_ERROR_CHECK_RETURN_ERR(err);
    s_nvs_ready = true;
    return ESP_OK;
}

static esp_err_t bsp_netif_init(void)
{
    if (s_netif_ready) {
        return ESP_OK;
    }
    esp_err_t err = esp_netif_init();
    if (err != ESP_OK && err != ESP_ERR_INVALID_STATE) {
        return err;
    }
    err = esp_event_loop_create_default();
    if (err != ESP_OK && err != ESP_ERR_INVALID_STATE) {
        return err;
    }
    s_netif_ready = true;
    return ESP_OK;
}

esp_err_t bsp_wifi_init(void)
{
    if (s_wifi_started) {
        return ESP_OK;
    }
    BSP_ERROR_CHECK_RETURN_ERR(bsp_nvs_init());
    BSP_ERROR_CHECK_RETURN_ERR(bsp_netif_init());
    if (!esp_netif_get_handle_from_ifkey("WIFI_STA_DEF")) {
        esp_netif_create_default_wifi_sta();
    }

    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    BSP_ERROR_CHECK_RETURN_ERR(esp_wifi_init(&cfg));
    BSP_ERROR_CHECK_RETURN_ERR(esp_wifi_set_mode(WIFI_MODE_STA));
    BSP_ERROR_CHECK_RETURN_ERR(esp_wifi_start());
    s_wifi_started = true;
    ESP_LOGI(TAG, "Wi-Fi STA started");
    return ESP_OK;
}

esp_err_t bsp_wifi_scan(uint16_t *ap_count)
{
    BSP_ERROR_CHECK_RETURN_ERR(bsp_wifi_init());
    wifi_scan_config_t scan = { 0 };
    BSP_ERROR_CHECK_RETURN_ERR(esp_wifi_scan_start(&scan, true));
    uint16_t found = 0;
    BSP_ERROR_CHECK_RETURN_ERR(esp_wifi_scan_get_ap_num(&found));
    if (ap_count) {
        *ap_count = found;
    }
    ESP_LOGI(TAG, "Wi-Fi scan found %u AP(s)", found);
    return ESP_OK;
}

#if BSP_HAS_BLE

static uint8_t s_adv_raw[31];
static uint8_t s_adv_raw_len;

static void bsp_ble_build_legacy_payload(void)
{
    uint8_t flags = ESP_BLE_ADV_FLAG_GEN_DISC;
    if (s_bt_mode == BSP_BT_MODE_BTDM) {
        flags |= ESP_BLE_ADV_FLAG_DMT_CONTROLLER_SPT | ESP_BLE_ADV_FLAG_DMT_HOST_SPT;
    } else {
        flags |= ESP_BLE_ADV_FLAG_BREDR_NOT_SPT;
    }
    size_t name_len = strlen(s_bt_name);
    if (name_len > 20) {
        name_len = 20;
    }
    s_adv_raw[0] = 0x02;
    s_adv_raw[1] = 0x01;
    s_adv_raw[2] = flags;
    s_adv_raw[3] = (uint8_t)(name_len + 1);
    s_adv_raw[4] = 0x09;
    memcpy(&s_adv_raw[5], s_bt_name, name_len);
    s_adv_raw_len = (uint8_t)(5 + name_len);
}

static void bsp_ble_gap_cb(esp_gap_ble_cb_event_t event, esp_ble_gap_cb_param_t *param)
{
    (void)param;
    if (event == ESP_GAP_BLE_EXT_ADV_SET_PARAMS_COMPLETE_EVT) {
        esp_ble_gap_config_ext_adv_data_raw(0, s_adv_raw_len, s_adv_raw);
    } else if (event == ESP_GAP_BLE_EXT_ADV_DATA_SET_COMPLETE_EVT) {
        const esp_ble_gap_ext_adv_t adv = {
            .instance = 0,
            .duration = 0,
            .max_events = 0,
        };
        esp_ble_gap_ext_adv_start(1, &adv);
    }
}

#endif

static bool bsp_mode_allowed(bsp_bt_mode_t mode)
{
    if (mode == BSP_BT_MODE_CLASSIC) {
        return BSP_HAS_CLASSIC;
    }
    if (mode == BSP_BT_MODE_BLE) {
        return BSP_HAS_BLE;
    }
    return BSP_HAS_CLASSIC && BSP_HAS_BLE;
}

static esp_bt_mode_t bsp_to_idf_mode(bsp_bt_mode_t mode)
{
    if (mode == BSP_BT_MODE_BLE) {
        return ESP_BT_MODE_BLE;
    }
    if (mode == BSP_BT_MODE_CLASSIC) {
        return ESP_BT_MODE_CLASSIC_BT;
    }
    return ESP_BT_MODE_BTDM;
}

esp_err_t bsp_bt_init(bsp_bt_mode_t mode, const char *device_name)
{
    if (s_bt_ready) {
        return ESP_OK;
    }
    if (!bsp_mode_allowed(mode)) {
        ESP_LOGE(TAG, "Requested BT mode is not enabled in menuconfig");
        return ESP_ERR_NOT_SUPPORTED;
    }

    BSP_ERROR_CHECK_RETURN_ERR(bsp_nvs_init());
    if (device_name && device_name[0]) {
        strncpy(s_bt_name, device_name, sizeof(s_bt_name) - 1);
        s_bt_name[sizeof(s_bt_name) - 1] = '\0';
    }

    esp_bt_controller_config_t cfg = BT_CONTROLLER_INIT_CONFIG_DEFAULT();
    BSP_ERROR_CHECK_RETURN_ERR(esp_bt_controller_init(&cfg));
    BSP_ERROR_CHECK_RETURN_ERR(esp_bt_controller_enable(bsp_to_idf_mode(mode)));
    BSP_ERROR_CHECK_RETURN_ERR(esp_bluedroid_init());
    BSP_ERROR_CHECK_RETURN_ERR(esp_bluedroid_enable());

    s_bt_mode = mode;
    s_bt_ready = true;

#if BSP_HAS_CLASSIC
    if (mode != BSP_BT_MODE_BLE) {
        BSP_ERROR_CHECK_RETURN_ERR(esp_bt_gap_set_device_name(s_bt_name));
    }
#endif
#if BSP_HAS_BLE
    if (mode != BSP_BT_MODE_CLASSIC) {
        BSP_ERROR_CHECK_RETURN_ERR(esp_ble_gap_set_device_name(s_bt_name));
    }
#endif
    ESP_LOGI(TAG, "Bluetooth ready, mode=%d name=%s", (int)mode, s_bt_name);
    return ESP_OK;
}

esp_err_t bsp_bt_classic_set_discoverable(bool enable)
{
#if !BSP_HAS_CLASSIC
    (void)enable;
    return ESP_ERR_NOT_SUPPORTED;
#else
    if (!s_bt_ready || s_bt_mode == BSP_BT_MODE_BLE) {
        return ESP_ERR_INVALID_STATE;
    }
    if (enable) {
        return esp_bt_gap_set_scan_mode(ESP_BT_CONNECTABLE, ESP_BT_GENERAL_DISCOVERABLE);
    }
    return esp_bt_gap_set_scan_mode(ESP_BT_NON_CONNECTABLE, ESP_BT_NON_DISCOVERABLE);
#endif
}

esp_err_t bsp_bt_ble_start_advertising(void)
{
#if !BSP_HAS_BLE
    return ESP_ERR_NOT_SUPPORTED;
#else
    if (!s_bt_ready || s_bt_mode == BSP_BT_MODE_CLASSIC) {
        return ESP_ERR_INVALID_STATE;
    }
    bsp_ble_build_legacy_payload();
    BSP_ERROR_CHECK_RETURN_ERR(esp_ble_gap_register_callback(bsp_ble_gap_cb));
    const esp_ble_gap_ext_adv_params_t params = {
        .type = ESP_BLE_GAP_SET_EXT_ADV_PROP_LEGACY_IND,
        .interval_min = 0x20,
        .interval_max = 0x40,
        .channel_map = ADV_CHNL_ALL,
        .own_addr_type = BLE_ADDR_TYPE_PUBLIC,
        .filter_policy = ADV_FILTER_ALLOW_SCAN_ANY_CON_ANY,
        .tx_power = EXT_ADV_TX_PWR_NO_PREFERENCE,
        .primary_phy = ESP_BLE_GAP_PRI_PHY_1M,
        .max_skip = 0,
        .secondary_phy = ESP_BLE_GAP_PHY_1M,
        .sid = 0,
        .scan_req_notif = false,
    };
    return esp_ble_gap_ext_adv_set_params(0, &params);
#endif
}
