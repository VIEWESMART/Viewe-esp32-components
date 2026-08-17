/*
 * SPDX-FileCopyrightText: 2026
 * SPDX-License-Identifier: Apache-2.0
 */
#pragma once

#include "esp_err.h"
#include "esp_wifi_types.h"

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define BSP_WIFI_SSID_MAX 32
#define BSP_WIFI_SCAN_MAX 12

typedef struct {
    char ssid[BSP_WIFI_SSID_MAX + 1];
    int8_t rssi;
    wifi_auth_mode_t auth;
} bsp_wifi_ap_info_t;

esp_err_t bsp_wifi_init(void);
bool bsp_wifi_is_initialized(void);
bool bsp_wifi_is_connected(void);
bool bsp_wifi_is_scanning(void);
int8_t bsp_wifi_get_rssi(void);
const char *bsp_wifi_status_text(void);
void bsp_wifi_copy_status(char *out, size_t out_len);

esp_err_t bsp_wifi_scan(bsp_wifi_ap_info_t *results, uint16_t *count, uint16_t max_count);
esp_err_t bsp_wifi_connect(const char *ssid, const char *password);

#ifdef __cplusplus
}
#endif
