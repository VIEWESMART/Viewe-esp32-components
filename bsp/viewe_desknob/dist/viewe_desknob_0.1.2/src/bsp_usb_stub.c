/*
 * SPDX-FileCopyrightText: 2026 VIEWE
 * SPDX-License-Identifier: Apache-2.0
 *
 * USB MSC stubs when CONFIG_BSP_USB_MSC is disabled.
 */

#include "bsp/viewe_desknob.h"

#include "esp_log.h"

static const char *TAG = "viewe_desknob.usb";

esp_err_t bsp_usb_msc_init(void)
{
    ESP_LOGW(TAG, "USB MSC disabled (enable CONFIG_BSP_USB_MSC)");
    return ESP_ERR_NOT_SUPPORTED;
}

esp_err_t bsp_usb_msc_sdcard_init(uint64_t *capacity_bytes)
{
    (void)capacity_bytes;
    ESP_LOGW(TAG, "USB MSC disabled (enable CONFIG_BSP_USB_MSC)");
    return ESP_ERR_NOT_SUPPORTED;
}
