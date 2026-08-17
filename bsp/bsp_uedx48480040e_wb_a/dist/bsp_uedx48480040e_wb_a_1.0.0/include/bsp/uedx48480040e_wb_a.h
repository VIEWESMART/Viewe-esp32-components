/*
 * SPDX-FileCopyrightText: 2026
 * SPDX-License-Identifier: Apache-2.0
 */

/**
 * @file
 * @brief ESP BSP: VIEWE UEDX48480040E-WB-A (4" 480x480 GC9503 RGB + FT5x06)
 */

#pragma once

#include "sdkconfig.h"

#ifndef CONFIG_BSP_I2C_NUM
#define CONFIG_BSP_I2C_NUM 0
#endif
#ifndef CONFIG_BSP_I2C_CLK_SPEED_HZ
#define CONFIG_BSP_I2C_CLK_SPEED_HZ 400000
#endif
#ifndef CONFIG_BSP_DISPLAY_BRIGHTNESS_LEDC_CH
#define CONFIG_BSP_DISPLAY_BRIGHTNESS_LEDC_CH 0
#endif
#ifndef CONFIG_BSP_LCD_RGB_BUFFER_NUMS
#define CONFIG_BSP_LCD_RGB_BUFFER_NUMS 3
#endif
#ifndef CONFIG_BSP_LCD_RGB_BOUNCE_BUFFER_HEIGHT
#define CONFIG_BSP_LCD_RGB_BOUNCE_BUFFER_HEIGHT 20
#endif
#ifndef CONFIG_BSP_SD_MOUNT_POINT
#define CONFIG_BSP_SD_MOUNT_POINT "/sdcard"
#endif

#include "driver/gpio.h"
#include "driver/i2c_master.h"
#include "driver/sdmmc_host.h"
#include "driver/sdspi_host.h"
#include "driver/spi_common.h"
#include "esp_err.h"
#include "esp_vfs_fat.h"
#include "sdmmc_cmd.h"
#include "bsp/config.h"
#include "bsp/display.h"
#include "bsp/touch.h"
#include "bsp/wifi.h"

#ifdef __cplusplus
extern "C" {
#endif

/**************************************************************************************************
 *  BSP Board Name
 **************************************************************************************************/
#define BSP_BOARD_UEDX48480040E_WB_A

/**************************************************************************************************
 *  BSP Capabilities
 **************************************************************************************************/
#define BSP_CAPS_DISPLAY        1
#define BSP_CAPS_TOUCH          1
#define BSP_CAPS_BUTTONS        0
#define BSP_CAPS_AUDIO          0
#define BSP_CAPS_AUDIO_SPEAKER  0
#define BSP_CAPS_AUDIO_MIC      0
#define BSP_CAPS_SDCARD         1
#define BSP_CAPS_WIFI           1
#define BSP_CAPS_IMU            0

/**************************************************************************************************
 *  Pinout
 **************************************************************************************************/

/* I2C (FT5x06) */
#define BSP_I2C_SCL             (GPIO_NUM_41)
#define BSP_I2C_SDA             (GPIO_NUM_40)

/* 3-wire SPI for GC9503 init (released after panel init) */
#define BSP_LCD_SPI_CS          (GPIO_NUM_39)
#define BSP_LCD_SPI_SCK         (GPIO_NUM_48)
#define BSP_LCD_SPI_MOSI        (GPIO_NUM_47)

#define BSP_LCD_BACKLIGHT       (GPIO_NUM_38)

/* RGB sync */
#define BSP_LCD_PCLK            (GPIO_NUM_21)
#define BSP_LCD_VSYNC           (GPIO_NUM_17)
#define BSP_LCD_HSYNC           (GPIO_NUM_16)
#define BSP_LCD_DE              (GPIO_NUM_18)
#define BSP_LCD_DISP            (GPIO_NUM_NC)

/*
 * RGB565 data GPIOs written as R0-R4, G0-G5, B0-B4.
 * Hardware traces swap R/B versus ESP RGB565 bit order, so R pins are
 * listed first (same mapping as the previous BOARD_SCREEN_A044 config).
 */
#define BSP_LCD_DATA0           (GPIO_NUM_4)
#define BSP_LCD_DATA1           (GPIO_NUM_3)
#define BSP_LCD_DATA2           (GPIO_NUM_2)
#define BSP_LCD_DATA3           (GPIO_NUM_1)
#define BSP_LCD_DATA4           (GPIO_NUM_0)
#define BSP_LCD_DATA5           (GPIO_NUM_10)
#define BSP_LCD_DATA6           (GPIO_NUM_9)
#define BSP_LCD_DATA7           (GPIO_NUM_8)
#define BSP_LCD_DATA8           (GPIO_NUM_7)
#define BSP_LCD_DATA9           (GPIO_NUM_6)
#define BSP_LCD_DATA10          (GPIO_NUM_5)
#define BSP_LCD_DATA11          (GPIO_NUM_15)
#define BSP_LCD_DATA12          (GPIO_NUM_14)
#define BSP_LCD_DATA13          (GPIO_NUM_13)
#define BSP_LCD_DATA14          (GPIO_NUM_12)
#define BSP_LCD_DATA15          (GPIO_NUM_11)

#define BSP_LCD_TOUCH_RST       (GPIO_NUM_NC)
#define BSP_LCD_TOUCH_INT       (GPIO_NUM_NC)

/* SD MMC/SPI: CS shares GPIO47 with LCD 3-wire SDA */
#define BSP_SD_SPI_CS           (GPIO_NUM_47)
#define BSP_SD_SPI_MOSI         (GPIO_NUM_42)
#define BSP_SD_SPI_CLK          (GPIO_NUM_45)
#define BSP_SD_SPI_MISO         (GPIO_NUM_46)

/**************************************************************************************************
 *  I2C
 **************************************************************************************************/
#define BSP_I2C_NUM             (CONFIG_BSP_I2C_NUM)

esp_err_t bsp_i2c_init(void);
esp_err_t bsp_i2c_deinit(void);
i2c_master_bus_handle_t bsp_i2c_get_handle(void);

/**************************************************************************************************
 *  SD card (SDSPI)
 *
 *  CS shares GPIO47 with LCD 3-wire SDA. Call bsp_sdcard_mount() only after
 *  bsp_display_new() has released the panel IO.
 *
 *  After mounting:
 *  \code{.c}
 *  FILE *f = fopen(BSP_SD_MOUNT_POINT "/hello.txt", "w");
 *  \endcode
 **************************************************************************************************/
#define BSP_SD_MOUNT_POINT      CONFIG_BSP_SD_MOUNT_POINT
#define BSP_SD_PHOTO_DIR        BSP_SD_MOUNT_POINT "/photos"
#define BSP_SD_ANIM_DIR         BSP_SD_MOUNT_POINT "/anim"
#define BSP_SDSPI_HOST          (SPI2_HOST)

typedef struct {
    const esp_vfs_fat_sdmmc_mount_config_t *mount;
    sdmmc_host_t *host;
    union {
        const sdmmc_slot_config_t *sdmmc;
        const sdspi_device_config_t *sdspi;
    } slot;
} bsp_sdcard_cfg_t;

extern sdmmc_card_t *bsp_sdcard;

esp_err_t bsp_sdcard_mount(void);
esp_err_t bsp_sdcard_unmount(void);
sdmmc_card_t *bsp_sdcard_get_handle(void);
void bsp_sdcard_get_sdspi_host(const int slot, sdmmc_host_t *config);
void bsp_sdcard_sdspi_get_slot(const spi_host_device_t spi_host, sdspi_device_config_t *config);
esp_err_t bsp_sdcard_sdspi_mount(bsp_sdcard_cfg_t *cfg);

/** Serialize SDSPI access (CS is shared with LCD init, and FATFS is not thread-safe). */
void bsp_sdcard_lock(void);
void bsp_sdcard_unlock(void);

/**************************************************************************************************
 *  Display timing (validated ~60 Hz on this module)
 **************************************************************************************************/
#define BSP_LCD_SPI_SCL_ACTIVE_EDGE (0)

#define BSP_LCD_480_480_PANEL_60HZ_RGB_TIMING() \
    {                                           \
        .pclk_hz = 18 * 1000 * 1000,            \
        .h_res = BSP_LCD_H_RES,                 \
        .v_res = BSP_LCD_V_RES,                 \
        .hsync_pulse_width = 32,                \
        .hsync_back_porch = 10,                 \
        .hsync_front_porch = 16,                \
        .vsync_pulse_width = 12,                \
        .vsync_back_porch = 15,                 \
        .vsync_front_porch = 3,                 \
        .flags.pclk_active_neg = 0,             \
    }

#ifdef __cplusplus
}
#endif
