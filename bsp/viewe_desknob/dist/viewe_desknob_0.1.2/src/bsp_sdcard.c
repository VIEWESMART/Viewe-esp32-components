/*
 * SPDX-FileCopyrightText: 2026 VIEWE
 * SPDX-License-Identifier: Apache-2.0
 */

#include "bsp/viewe_desknob.h"

#include "bsp_err_check.h"
#include "driver/sdmmc_host.h"
#include "esp_log.h"
#include "esp_vfs_fat.h"
#include "sdmmc_cmd.h"

static const char *TAG = "viewe_desknob.sd";

static sdmmc_card_t *s_card;

esp_err_t bsp_sdcard_mount(void)
{
    if (s_card) {
        return ESP_OK;
    }

    sdmmc_host_t host = SDMMC_HOST_DEFAULT();
    host.slot = SDMMC_HOST_SLOT_0;
    host.max_freq_khz = SDMMC_FREQ_HIGHSPEED;
    host.flags &= ~(SDMMC_HOST_FLAG_1BIT | SDMMC_HOST_FLAG_4BIT | SDMMC_HOST_FLAG_8BIT);
    host.flags |= SDMMC_HOST_FLAG_4BIT;

    sdmmc_slot_config_t slot = SDMMC_SLOT_CONFIG_DEFAULT();
    slot.width = 4;
    slot.clk = BSP_SD_CLK;
    slot.cmd = BSP_SD_CMD;
    slot.d0 = BSP_SD_D0;
    slot.d1 = BSP_SD_D1;
    slot.d2 = BSP_SD_D2;
    slot.d3 = BSP_SD_D3;
    slot.flags |= SDMMC_SLOT_FLAG_INTERNAL_PULLUP;

    const esp_vfs_fat_sdmmc_mount_config_t mount_cfg = {
        .format_if_mount_failed = false,
        .max_files = 5,
        .allocation_unit_size = 16 * 1024,
    };

    esp_err_t err = esp_vfs_fat_sdmmc_mount(BSP_SD_MOUNT_POINT, &host, &slot, &mount_cfg, &s_card);
    if (err != ESP_OK) {
        ESP_LOGW(TAG, "SD mount failed: %s", esp_err_to_name(err));
        s_card = NULL;
        return err;
    }
    sdmmc_card_print_info(stdout, s_card);
    return ESP_OK;
}

esp_err_t bsp_sdcard_unmount(void)
{
    if (!s_card) {
        return ESP_OK;
    }
    esp_err_t err = esp_vfs_fat_sdcard_unmount(BSP_SD_MOUNT_POINT, s_card);
    s_card = NULL;
    return err;
}
