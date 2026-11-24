/*
 * SPDX-FileCopyrightText: 2024-2026 Shenzhen VIEWE TECHNOLOGY Co., LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

/**
 * @file
 * @brief ESP LCD touch: CHSC6540
 */

#pragma once

#include "esp_lcd_touch.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Create a new CHSC6540 touch driver
 *
 * @note  The I2C communication should be initialized before use this function.
 *
 * @param io LCD panel IO handle, it should be created by `esp_lcd_new_panel_io_i2c()`
 * @param config Touch panel configuration
 * @param tp Touch panel handle
 * @return
 *      - ESP_OK: on success
 */
esp_err_t esp_lcd_touch_new_i2c_chsc6540(const esp_lcd_panel_io_handle_t io,
                                         const esp_lcd_touch_config_t *config,
                                         esp_lcd_touch_handle_t *tp);

/**
 * @brief I2C address of the CHSC6540 controller
 */
#define ESP_LCD_TOUCH_IO_I2C_CHSC6540_ADDRESS    (0x2E)

/**
 * @brief Touch IO configuration structure
 */
#define ESP_LCD_TOUCH_IO_I2C_CHSC6540_CONFIG()                 \
    {                                                          \
        .dev_addr = ESP_LCD_TOUCH_IO_I2C_CHSC6540_ADDRESS,     \
        .scl_speed_hz = 400000,                                \
        .on_color_trans_done = 0,                              \
        .user_ctx = 0,                                         \
        .control_phase_bytes = 1,                              \
        .dc_bit_offset = 0,                                    \
        .lcd_cmd_bits = 8,                                     \
        .lcd_param_bits = 0,                                   \
        .flags =                                               \
        {                                                      \
            .dc_low_on_data = 0,                               \
            .disable_control_phase = 1,                        \
        },                                                     \
    }

#ifdef __cplusplus
}
#endif
