/*
 * SPDX-FileCopyrightText: 2026 VIEWE
 * SPDX-License-Identifier: Apache-2.0
 *
 * USB 2.0 High Speed mass-storage device on the ESP32-S31 internal UTMI PHY.
 * D+ / D- are the dedicated USB_DP / USB_DM pads.
 */

#include "bsp/viewe_desknob.h"

#include <stdio.h>
#include <string.h>

#include "bsp_err_check.h"
#include "esp_check.h"
#include "esp_log.h"
#include "esp_partition.h"
#include "driver/sdmmc_host.h"
#include "sdmmc_cmd.h"
#include "tinyusb.h"
#include "tinyusb_default_config.h"
#include "tinyusb_msc.h"
#include "wear_levelling.h"

static const char *TAG = "viewe_desknob.usb";

#define BSP_USB_BASE_PATH   "/udisk"
#define EPNUM_MSC           1
#define TUSB_DESC_TOTAL_LEN (TUD_CONFIG_DESC_LEN + TUD_MSC_DESC_LEN)

enum {
    ITF_NUM_MSC = 0,
    ITF_NUM_TOTAL
};

enum {
    EDPT_MSC_OUT = 0x01,
    EDPT_MSC_IN  = 0x81,
};

static wl_handle_t s_wl = WL_INVALID_HANDLE;
static tinyusb_msc_storage_handle_t s_storage;

static const tusb_desc_device_t s_device_desc = {
    .bLength = sizeof(tusb_desc_device_t),
    .bDescriptorType = TUSB_DESC_DEVICE,
    .bcdUSB = 0x0200,
    .bDeviceClass = TUSB_CLASS_MISC,
    .bDeviceSubClass = MISC_SUBCLASS_COMMON,
    .bDeviceProtocol = MISC_PROTOCOL_IAD,
    .bMaxPacketSize0 = CFG_TUD_ENDPOINT0_SIZE,
    .idVendor = 0x303A,
    .idProduct = 0x4002,
    .bcdDevice = 0x0100,
    .iManufacturer = 0x01,
    .iProduct = 0x02,
    .iSerialNumber = 0x03,
    .bNumConfigurations = 0x01,
};

static const uint8_t s_fs_cfg_desc[] = {
    TUD_CONFIG_DESCRIPTOR(1, ITF_NUM_TOTAL, 0, TUSB_DESC_TOTAL_LEN, TUSB_DESC_CONFIG_ATT_REMOTE_WAKEUP, 100),
    TUD_MSC_DESCRIPTOR(ITF_NUM_MSC, 4, EDPT_MSC_OUT, EDPT_MSC_IN, 64),
};

static const tusb_desc_device_qualifier_t s_qualifier = {
    .bLength = sizeof(tusb_desc_device_qualifier_t),
    .bDescriptorType = TUSB_DESC_DEVICE_QUALIFIER,
    .bcdUSB = 0x0200,
    .bDeviceClass = TUSB_CLASS_MISC,
    .bDeviceSubClass = MISC_SUBCLASS_COMMON,
    .bDeviceProtocol = MISC_PROTOCOL_IAD,
    .bMaxPacketSize0 = CFG_TUD_ENDPOINT0_SIZE,
    .bNumConfigurations = 0x01,
    .bReserved = 0,
};

static const uint8_t s_hs_cfg_desc[] = {
    TUD_CONFIG_DESCRIPTOR(1, ITF_NUM_TOTAL, 0, TUSB_DESC_TOTAL_LEN, TUSB_DESC_CONFIG_ATT_REMOTE_WAKEUP, 100),
    TUD_MSC_DESCRIPTOR(ITF_NUM_MSC, 4, EDPT_MSC_OUT, EDPT_MSC_IN, 512),
};

static const char *s_strings[] = {
    (const char[]) { 0x09, 0x04 },
    "VIEWE",
    "DeskNob USB Disk",
    "DESKNOB",
    "U-disk",
};

static const char *s_sd_strings[] = {
    (const char[]) { 0x09, 0x04 },
    "VIEWE",
    "DeskNob SD Card",
    "DESKNOB",
    "SD card",
};

static sdmmc_card_t s_sd_card;
static bool s_sd_active;
static uint64_t s_sd_bytes;

static esp_err_t bsp_usb_driver_install(const char **strings, unsigned count)
{
    tinyusb_config_t tusb_cfg = TINYUSB_DEFAULT_CONFIG();
    tusb_cfg.descriptor.device = &s_device_desc;
    tusb_cfg.descriptor.full_speed_config = s_fs_cfg_desc;
    tusb_cfg.descriptor.high_speed_config = s_hs_cfg_desc;
    tusb_cfg.descriptor.qualifier = &s_qualifier;
    tusb_cfg.descriptor.string = strings;
    tusb_cfg.descriptor.string_count = count;
    return tinyusb_driver_install(&tusb_cfg);
}

static void bsp_usb_seed_file(void)
{
    char path[48];
    snprintf(path, sizeof(path), BSP_USB_BASE_PATH "/README.TXT");
    FILE *existing = fopen(path, "r");
    if (existing) {
        fclose(existing);
        return;
    }
    FILE *file = fopen(path, "w");
    if (!file) {
        ESP_LOGW(TAG, "Could not create %s", path);
        return;
    }
    fputs("VIEWE DeskNob USB 2.0 disk\r\n", file);
    fputs("This file was created by the board.\r\n", file);
    fclose(file);
    ESP_LOGI(TAG, "Created %s", path);
}

esp_err_t bsp_usb_msc_init(void)
{
    if (s_storage) {
        return ESP_OK;
    }

    const esp_partition_t *part = esp_partition_find_first(ESP_PARTITION_TYPE_DATA,
                                                           ESP_PARTITION_SUBTYPE_DATA_FAT,
                                                           CONFIG_BSP_USB_MSC_PARTITION_LABEL);
    if (!part) {
        ESP_LOGE(TAG, "FAT partition '%s' not found", CONFIG_BSP_USB_MSC_PARTITION_LABEL);
        return ESP_ERR_NOT_FOUND;
    }

    BSP_ERROR_CHECK_RETURN_ERR(wl_mount(part, &s_wl));

    static char base_path[] = BSP_USB_BASE_PATH;
    tinyusb_msc_storage_config_t storage_cfg = {
        .mount_point = TINYUSB_MSC_STORAGE_MOUNT_APP,
        .medium.wl_handle = s_wl,
        .fat_fs = {
            .base_path = base_path,
            .do_not_format = false,
            .format_flags = 0,
            .config = {
                .format_if_mount_failed = true,
                .max_files = 4,
                .allocation_unit_size = CONFIG_WL_SECTOR_SIZE,
            },
        },
    };
    esp_err_t err = tinyusb_msc_new_storage_spiflash(&storage_cfg, &s_storage);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "MSC storage init failed: %s", esp_err_to_name(err));
        if (s_wl != WL_INVALID_HANDLE) {
            wl_unmount(s_wl);
            s_wl = WL_INVALID_HANDLE;
        }
        return err;
    }

    bsp_usb_seed_file();
    BSP_ERROR_CHECK_RETURN_ERR(tinyusb_msc_set_storage_mount_point(s_storage, TINYUSB_MSC_STORAGE_MOUNT_USB));
    BSP_ERROR_CHECK_RETURN_ERR(bsp_usb_driver_install(s_strings, sizeof(s_strings) / sizeof(s_strings[0])));

    ESP_LOGI(TAG, "USB 2.0 HS disk ready on USB_DP/USB_DM, partition %s (%u bytes)",
             CONFIG_BSP_USB_MSC_PARTITION_LABEL, (unsigned)part->size);
    return ESP_OK;
}

static void bsp_usb_sd_host_deinit(sdmmc_host_t *host)
{
    if (host->flags & SDMMC_HOST_FLAG_DEINIT_ARG) {
        host->deinit_p(host->slot);
    } else {
        host->deinit();
    }
}

static esp_err_t bsp_usb_sd_card_init(sdmmc_card_t *card)
{
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

    esp_err_t err = host.init();
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "SD host init failed: %s", esp_err_to_name(err));
        return err;
    }
    err = sdmmc_host_init_slot(host.slot, &slot);
    if (err != ESP_OK) {
        bsp_usb_sd_host_deinit(&host);
        ESP_LOGE(TAG, "SD slot init failed: %s", esp_err_to_name(err));
        return err;
    }
    err = sdmmc_card_init(&host, card);
    if (err != ESP_OK) {
        bsp_usb_sd_host_deinit(&host);
        ESP_LOGE(TAG, "SD card init failed: %s", esp_err_to_name(err));
        return err;
    }
    sdmmc_card_print_info(stdout, card);
    return ESP_OK;
}

esp_err_t bsp_usb_msc_sdcard_init(uint64_t *capacity_bytes)
{
    if (s_storage) {
        if (!s_sd_active) {
            return ESP_ERR_INVALID_STATE;
        }
        if (capacity_bytes) {
            *capacity_bytes = s_sd_bytes;
        }
        return ESP_OK;
    }

    esp_err_t err = bsp_usb_sd_card_init(&s_sd_card);
    if (err != ESP_OK) {
        return err;
    }

    s_sd_bytes = (uint64_t)s_sd_card.csd.capacity * s_sd_card.csd.sector_size;
    tinyusb_msc_storage_config_t storage_cfg = {
        .mount_point = TINYUSB_MSC_STORAGE_MOUNT_USB,
        .medium.card = &s_sd_card,
        .fat_fs = {
            .base_path = NULL,
            .do_not_format = true,
            .format_flags = 0,
            .config = {
                .format_if_mount_failed = false,
                .max_files = 4,
                .allocation_unit_size = 16 * 1024,
            },
        },
    };
    err = tinyusb_msc_new_storage_sdmmc(&storage_cfg, &s_storage);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "SD MSC init failed: %s", esp_err_to_name(err));
        bsp_usb_sd_host_deinit(&s_sd_card.host);
        return err;
    }

    err = bsp_usb_driver_install(s_sd_strings, sizeof(s_sd_strings) / sizeof(s_sd_strings[0]));
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "USB driver install failed: %s", esp_err_to_name(err));
        if (s_storage) {
            tinyusb_msc_delete_storage(s_storage);
            s_storage = NULL;
        }
        bsp_usb_sd_host_deinit(&s_sd_card.host);
        return err;
    }

    s_sd_active = true;
    if (capacity_bytes) {
        *capacity_bytes = s_sd_bytes;
    }
    ESP_LOGI(TAG, "USB 2.0 HS SD card ready, %llu bytes, not formatted",
             (unsigned long long)s_sd_bytes);
    return ESP_OK;
}
