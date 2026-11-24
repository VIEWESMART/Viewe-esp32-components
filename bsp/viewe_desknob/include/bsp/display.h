/*
 * SPDX-FileCopyrightText: 2026 VIEWE
 * SPDX-License-Identifier: Apache-2.0
 */

/**
 * @file
 * @brief VIEWE DeskNob display (GC9B72 QSPI), hardware init without LVGL.
 */

#pragma once

#include "esp_err.h"
#include "esp_lcd_types.h"

#ifdef __cplusplus
extern "C" {
#endif

/** @defgroup g04_display Display
 *  @{
 */

#define BSP_LCD_H_RES              (360)
#define BSP_LCD_V_RES              (360)
#define BSP_LCD_BITS_PER_PIXEL     (16)
#define BSP_LCD_PIXEL_CLOCK_HZ     (40 * 1000 * 1000)
/* Align with Espressif SPI/QSPI BSP boards (vocat/korvo/waveshare): width * 2 * 10 */
#define BSP_LCD_DRAW_BUFF_HEIGHT   (10)
#define BSP_LCD_DRAW_BUFF_SIZE     (BSP_LCD_H_RES * BSP_LCD_DRAW_BUFF_HEIGHT)
#define BSP_LCD_SPI_MAX_TRANSFER_SZ \
    (BSP_LCD_H_RES * BSP_LCD_DRAW_BUFF_HEIGHT * (BSP_LCD_BITS_PER_PIXEL / 8))

/**
 * @brief Display hardware configuration.
 */
typedef struct {
    int max_transfer_sz; /*!< Maximum SPI transfer size in bytes. 0 selects BSP_LCD_SPI_MAX_TRANSFER_SZ. */
} bsp_display_config_t;

/**
 * @brief Create the GC9B72 panel and its QSPI panel IO.
 *
 * Backlight stays off. Call bsp_display_brightness_set() or bsp_display_backlight_on() afterwards.
 *
 * @param[in]  config    Display configuration. NULL selects the default.
 * @param[out] ret_panel LCD panel handle.
 * @param[out] ret_io    LCD panel IO handle. Can be NULL.
 */
esp_err_t bsp_display_new(const bsp_display_config_t *config, esp_lcd_panel_handle_t *ret_panel,
                          esp_lcd_panel_io_handle_t *ret_io);

/**
 * @brief Initialize backlight LEDC PWM.
 */
esp_err_t bsp_display_brightness_init(void);

/**
 * @brief Set backlight brightness.
 *
 * @param[in] brightness_percent 0..100. 0 turns the backlight off.
 */
esp_err_t bsp_display_brightness_set(int brightness_percent);

/**
 * @brief Turn the backlight on (100%).
 */
esp_err_t bsp_display_backlight_on(void);

/**
 * @brief Turn the backlight off.
 */
esp_err_t bsp_display_backlight_off(void);

/** @} */

#ifdef __cplusplus
}
#endif
