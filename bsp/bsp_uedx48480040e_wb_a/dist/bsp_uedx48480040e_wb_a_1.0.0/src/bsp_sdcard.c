/*
 * SPDX-FileCopyrightText: 2026
 * SPDX-License-Identifier: Apache-2.0
 */

#include "bsp/uedx48480040e_wb_a.h"

#include "driver/gpio.h"
#include "driver/spi_common.h"
#include "esp_check.h"
#include "esp_log.h"
#include "esp_vfs_fat.h"
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include "freertos/task.h"
#include "rom/ets_sys.h"
#include "sdmmc_cmd.h"

#include <assert.h>
#include <sys/stat.h>

static const char *TAG = "bsp_sdcard";

sdmmc_card_t *bsp_sdcard = NULL;

static bool s_spi_bus_ready = false;
static SemaphoreHandle_t s_sd_mutex = NULL;

static void bsp_sdcard_ensure_mutex(void)
{
    if (s_sd_mutex == NULL) {
        s_sd_mutex = xSemaphoreCreateMutex();
    }
}

void bsp_sdcard_lock(void)
{
    bsp_sdcard_ensure_mutex();
    xSemaphoreTake(s_sd_mutex, portMAX_DELAY);
}

void bsp_sdcard_unlock(void)
{
    if (s_sd_mutex != NULL) {
        xSemaphoreGive(s_sd_mutex);
    }
}

static void bsp_sdcard_spi_idle_clocks(void)
{
    gpio_set_level(BSP_SD_SPI_CS, 1);
    for (int i = 0; i < 16; i++) {
        gpio_set_level(BSP_SD_SPI_CLK, 0);
        ets_delay_us(10);
        gpio_set_level(BSP_SD_SPI_CLK, 1);
        ets_delay_us(10);
    }
    gpio_set_level(BSP_SD_SPI_CLK, 0);
}

static void bsp_sdcard_gpio_prepare(void)
{
    /* LCD 3-wire SDA may have left GPIO47 held after panel init. */
    gpio_hold_dis(BSP_SD_SPI_CS);
    gpio_hold_dis(BSP_SD_SPI_MOSI);
    gpio_hold_dis(BSP_SD_SPI_MISO);
    gpio_hold_dis(BSP_SD_SPI_CLK);

    gpio_reset_pin(BSP_SD_SPI_CS);
    gpio_reset_pin(BSP_SD_SPI_MOSI);
    gpio_reset_pin(BSP_SD_SPI_MISO);
    gpio_reset_pin(BSP_SD_SPI_CLK);

    const gpio_config_t out_cfg = {
        .pin_bit_mask = (1ULL << BSP_SD_SPI_CS) | (1ULL << BSP_SD_SPI_MOSI) |
                        (1ULL << BSP_SD_SPI_CLK),
        .mode = GPIO_MODE_OUTPUT,
        .pull_up_en = GPIO_PULLUP_ENABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_DISABLE,
    };
    gpio_config(&out_cfg);

    const gpio_config_t in_cfg = {
        .pin_bit_mask = (1ULL << BSP_SD_SPI_MISO),
        .mode = GPIO_MODE_INPUT,
        .pull_up_en = GPIO_PULLUP_ENABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_DISABLE,
    };
    gpio_config(&in_cfg);

    gpio_set_level(BSP_SD_SPI_CS, 1);
    gpio_set_level(BSP_SD_SPI_CLK, 0);
    gpio_set_level(BSP_SD_SPI_MOSI, 1);
    bsp_sdcard_spi_idle_clocks();
    vTaskDelay(pdMS_TO_TICKS(20));
}

void bsp_sdcard_get_sdspi_host(const int slot, sdmmc_host_t *config)
{
    assert(config);
    sdmmc_host_t host = SDSPI_HOST_DEFAULT();
    host.slot = slot;
    *config = host;
}

void bsp_sdcard_sdspi_get_slot(const spi_host_device_t spi_host, sdspi_device_config_t *config)
{
    assert(config);
    sdspi_device_config_t slot_config = SDSPI_DEVICE_CONFIG_DEFAULT();
    slot_config.gpio_cs = BSP_SD_SPI_CS;
    slot_config.gpio_cd = GPIO_NUM_NC;
    slot_config.gpio_wp = GPIO_NUM_NC;
    slot_config.host_id = spi_host;
    *config = slot_config;
}

static esp_err_t bsp_sdcard_mount_once(int freq_khz, sdmmc_card_t **card_out)
{
    sdmmc_host_t host;
    bsp_sdcard_get_sdspi_host(BSP_SDSPI_HOST, &host);
    host.max_freq_khz = freq_khz;

    if (!s_spi_bus_ready) {
        const spi_bus_config_t bus_cfg = {
            .mosi_io_num = BSP_SD_SPI_MOSI,
            .miso_io_num = BSP_SD_SPI_MISO,
            .sclk_io_num = BSP_SD_SPI_CLK,
            .quadwp_io_num = -1,
            .quadhd_io_num = -1,
            .max_transfer_sz = 32 * 1024,
        };

        esp_err_t ret = spi_bus_initialize((spi_host_device_t)host.slot, &bus_cfg,
                                           SDSPI_DEFAULT_DMA);
        if (ret != ESP_OK && ret != ESP_ERR_INVALID_STATE) {
            ESP_LOGE(TAG, "SPI bus init failed: %s", esp_err_to_name(ret));
            return ret;
        }
        s_spi_bus_ready = true;
    }

    esp_vfs_fat_sdmmc_mount_config_t mount_config = {
#ifdef CONFIG_BSP_SD_FORMAT_ON_MOUNT_FAIL
        .format_if_mount_failed = CONFIG_BSP_SD_FORMAT_ON_MOUNT_FAIL,
#else
        .format_if_mount_failed = false,
#endif
        .max_files = 8,
        .allocation_unit_size = 16 * 1024,
    };

    sdspi_device_config_t slot_config;
    bsp_sdcard_sdspi_get_slot((spi_host_device_t)host.slot, &slot_config);

    return esp_vfs_fat_sdspi_mount(BSP_SD_MOUNT_POINT, &host, &slot_config,
                                   &mount_config, card_out);
}

esp_err_t bsp_sdcard_sdspi_mount(bsp_sdcard_cfg_t *cfg)
{
    if (bsp_sdcard != NULL) {
        return ESP_ERR_INVALID_STATE;
    }

    if (cfg == NULL || cfg->mount == NULL || cfg->host == NULL || cfg->slot.sdspi == NULL) {
        return bsp_sdcard_mount();
    }

    if (!s_spi_bus_ready) {
        const spi_bus_config_t bus_cfg = {
            .mosi_io_num = BSP_SD_SPI_MOSI,
            .miso_io_num = BSP_SD_SPI_MISO,
            .sclk_io_num = BSP_SD_SPI_CLK,
            .quadwp_io_num = -1,
            .quadhd_io_num = -1,
            .max_transfer_sz = 32 * 1024,
        };
        esp_err_t ret = spi_bus_initialize((spi_host_device_t)cfg->host->slot, &bus_cfg,
                                           SDSPI_DEFAULT_DMA);
        if (ret != ESP_OK && ret != ESP_ERR_INVALID_STATE) {
            return ret;
        }
        s_spi_bus_ready = true;
    }

    return esp_vfs_fat_sdspi_mount(BSP_SD_MOUNT_POINT, cfg->host, cfg->slot.sdspi,
                                   cfg->mount, &bsp_sdcard);
}

esp_err_t bsp_sdcard_mount(void)
{
    if (bsp_sdcard != NULL) {
        return ESP_OK;
    }

    static const int freqs[] = {SDMMC_FREQ_DEFAULT, SDMMC_FREQ_PROBING};
    sdmmc_card_t *card = NULL;
    esp_err_t ret = ESP_FAIL;

    for (int round = 0; round < 4 && bsp_sdcard == NULL; round++) {
        bsp_sdcard_gpio_prepare();

        for (size_t i = 0; i < sizeof(freqs) / sizeof(freqs[0]); i++) {
            ESP_LOGI(TAG, "SD mount try %d freq=%d kHz", round + 1, freqs[i]);
            ret = bsp_sdcard_mount_once(freqs[i], &card);
            if (ret == ESP_OK) {
                break;
            }
            ESP_LOGW(TAG, "SD mount failed: %s", esp_err_to_name(ret));
            vTaskDelay(pdMS_TO_TICKS(50));
        }

        if (ret == ESP_OK) {
            break;
        }
        vTaskDelay(pdMS_TO_TICKS(200));
    }

    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "SD mount failed after retries: %s", esp_err_to_name(ret));
        return ret;
    }

    bsp_sdcard = card;
    sdmmc_card_print_info(stdout, bsp_sdcard);
    mkdir(BSP_SD_PHOTO_DIR, 0775);
    mkdir(BSP_SD_ANIM_DIR, 0775);
    ESP_LOGI(TAG, "SD mounted at %s", BSP_SD_MOUNT_POINT);
    return ESP_OK;
}

esp_err_t bsp_sdcard_unmount(void)
{
    if (bsp_sdcard == NULL) {
        return ESP_ERR_INVALID_STATE;
    }

    esp_err_t ret = esp_vfs_fat_sdcard_unmount(BSP_SD_MOUNT_POINT, bsp_sdcard);
    bsp_sdcard = NULL;

    if (s_spi_bus_ready) {
        spi_bus_free(BSP_SDSPI_HOST);
        s_spi_bus_ready = false;
    }
    return ret;
}

sdmmc_card_t *bsp_sdcard_get_handle(void)
{
    return bsp_sdcard;
}
