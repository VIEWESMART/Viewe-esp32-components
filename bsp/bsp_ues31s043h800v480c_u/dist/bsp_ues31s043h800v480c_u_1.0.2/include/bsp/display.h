/*
 * SPDX-FileCopyrightText: 2026 VIEWE
 * SPDX-License-Identifier: Apache-2.0
 *
 * Hardware display API without LVGL. Use bsp_display_start() from
 * bsp/esp-bsp.h when a graphic library is enabled.
 */
#pragma once

#include "esp_err.h"
#include "esp_lcd_types.h"

#define ESP_LCD_COLOR_FORMAT_RGB565 (1)
#define ESP_LCD_COLOR_FORMAT_RGB888 (2)

#define BSP_LCD_COLOR_FORMAT        (ESP_LCD_COLOR_FORMAT_RGB888)
#define BSP_LCD_BIGENDIAN           (1)
#define BSP_LCD_BITS_PER_PIXEL      (24)
#define BSP_LCD_COLOR_SPACE         (LCD_RGB_ELEMENT_ORDER_RGB)
#define BSP_LCD_H_RES               (800)
#define BSP_LCD_V_RES               (480)
/* RGB888. Keep current porch: 22 MHz PCLK, H/V 4/8/8, 2 FBs, bounce 20.
 * LVGL paints the hidden FB and swaps on VSYNC. */
#define BSP_LCD_PIXEL_CLOCK_HZ      (22 * 1000 * 1000)
#define BSP_LCD_HSYNC_PULSE_WIDTH   (4)
#define BSP_LCD_HSYNC_BACK_PORCH    (8)
#define BSP_LCD_HSYNC_FRONT_PORCH   (8)
#define BSP_LCD_VSYNC_PULSE_WIDTH   (4)
#define BSP_LCD_VSYNC_BACK_PORCH    (8)
#define BSP_LCD_VSYNC_FRONT_PORCH   (8)
#define BSP_LCD_BOUNCE_BUFFER_HEIGHT (20)
#define BSP_LCD_FB_COUNT            (2)
#define BSP_LCD_FB_SIZE             (BSP_LCD_H_RES * BSP_LCD_V_RES * (BSP_LCD_BITS_PER_PIXEL / 8))

#if BSP_LCD_BOUNCE_BUFFER_HEIGHT > 0
_Static_assert((BSP_LCD_V_RES % BSP_LCD_BOUNCE_BUFFER_HEIGHT) == 0,
               "RGB bounce height must divide V_RES");
_Static_assert(((BSP_LCD_V_RES / BSP_LCD_BOUNCE_BUFFER_HEIGHT) % 2) == 0,
               "frame size / bounce size must be even");
#endif

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    int max_transfer_sz; /*!< Unused for RGB panels; kept for API compatibility. */
    uint8_t num_fbs;     /*!< RGB framebuffers, 0 = BSP_LCD_FB_COUNT (max 3). */
    uint16_t bounce_lines; /*!< RGB bounce height in lines, 0 = BSP default. */
} bsp_display_config_t;

esp_err_t bsp_display_new(const bsp_display_config_t *config,
                          esp_lcd_panel_handle_t *ret_panel,
                          esp_lcd_panel_io_handle_t *ret_io);

esp_err_t bsp_display_get_framebuffers(void **fb0, void **fb1);
esp_lcd_panel_handle_t bsp_display_get_panel(void);

esp_err_t bsp_display_brightness_init(void);
esp_err_t bsp_display_brightness_set(int brightness_percent);
esp_err_t bsp_display_backlight_on(void);
esp_err_t bsp_display_backlight_off(void);

#ifdef __cplusplus
}
#endif
