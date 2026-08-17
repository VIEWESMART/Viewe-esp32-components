/*
 * SPDX-FileCopyrightText: 2026
 * SPDX-License-Identifier: Apache-2.0
 */

/**
 * @file
 * @brief BSP LCD (GC9503 RGB) and LVGL port via esp_lvgl_adapter
 */

#pragma once

#include <stdbool.h>
#include "esp_err.h"
#include "esp_lcd_types.h"
#include "sdkconfig.h"
#include "bsp/config.h"

#if (BSP_CONFIG_NO_GRAPHIC_LIB == 0)
#include "esp_lv_adapter.h"
#include "lvgl.h"
#endif

#ifdef __cplusplus
extern "C" {
#endif

#define ESP_LCD_COLOR_FORMAT_RGB565    (1)
#define ESP_LCD_COLOR_FORMAT_RGB888    (2)

#define BSP_LCD_COLOR_FORMAT        (ESP_LCD_COLOR_FORMAT_RGB565)
#define BSP_LCD_BIGENDIAN           (0)
#define BSP_LCD_BITS_PER_PIXEL      (16)
#define BSP_LCD_COLOR_SPACE         (LCD_RGB_ELEMENT_ORDER_BGR)
#define BSP_LCD_H_RES               (480)
#define BSP_LCD_V_RES               (480)
/** GC9503 COLMOD; RGB bus remains 16-bit RGB565. */
#define BSP_LCD_PANEL_BPP           (18)

typedef struct {
    int max_transfer_sz;    /*!< Unused on RGB panels; kept for ESP-BSP API compatibility. */
} bsp_display_config_t;

/**
 * @brief Create GC9503 RGB panel
 *
 * Performs reset and vendor init. Backlight is not turned on.
 * 3-wire SPI IO is released after init (`auto_del_panel_io`) so GPIO47
 * can be reused as SD CS.
 *
 * @param[in]  config    Display config, may be NULL
 * @param[out] ret_panel Panel handle
 * @param[out] ret_io    Always set to NULL after SPI IO is deleted
 */
esp_err_t bsp_display_new(const bsp_display_config_t *config, esp_lcd_panel_handle_t *ret_panel,
                          esp_lcd_panel_io_handle_t *ret_io);

esp_err_t bsp_display_brightness_init(void);
esp_err_t bsp_display_brightness_set(int brightness_percent);
esp_err_t bsp_display_backlight_on(void);
esp_err_t bsp_display_backlight_off(void);

#if (BSP_CONFIG_NO_GRAPHIC_LIB == 0)

/**
 * @brief LVGL display start configuration
 *
 * LVGL is ported with espressif/esp_lvgl_adapter (not esp_lvgl_port).
 */
typedef struct {
    esp_lv_adapter_config_t lv_adapter_cfg; /*!< Task / tick configuration */
    struct {
        unsigned int mount_sd : 1;          /*!< Mount SDSPI after panel IO is released */
    } flags;
} bsp_display_cfg_t;

/**
 * @brief Initialize LCD, optional SD, touch, and LVGL
 *
 * Uses board-default adapter settings (32 KB stack, priority 7, core 0).
 *
 * @return LVGL display or NULL on failure
 */
lv_display_t *bsp_display_start(void);

/**
 * @brief Initialize LCD, optional SD, touch, and LVGL with config
 *
 * @param[in] cfg Display / adapter config, may be NULL (same as bsp_display_start)
 * @return LVGL display or NULL on failure
 */
lv_display_t *bsp_display_start_with_config(const bsp_display_cfg_t *cfg);

/**
 * @brief Get LVGL input device (touch)
 *
 * Valid after a successful bsp_display_start().
 */
lv_indev_t *bsp_display_get_input_dev(void);

/**
 * @brief Take LVGL mutex
 *
 * @param timeout_ms Timeout in ms. 0 waits forever.
 * @return true if the lock was taken
 */
bool bsp_display_lock(uint32_t timeout_ms);

/**
 * @brief Give LVGL mutex
 */
void bsp_display_unlock(void);

#endif /* BSP_CONFIG_NO_GRAPHIC_LIB == 0 */

#ifdef __cplusplus
}
#endif
