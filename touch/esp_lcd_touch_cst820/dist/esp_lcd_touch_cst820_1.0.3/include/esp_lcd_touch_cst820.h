/*
 * SPDX-FileCopyrightText: 2023-2026 VIEWE TECHNOLOGY CO.,LTD
 * SPDX-License-Identifier: Apache-2.0
 */

/**
 * @file   esp_lcd_touch_cst820.h
 * @brief  ESP LCD touch: CST820
 * @author long liu(Ayang)
 * @version 1.0.3
 */

#pragma once

#include "esp_lcd_touch.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Create a new CST820 touch driver
 *
 * @note Create an I2C master bus (`i2c_new_master_bus`) and panel IO
 *       (`esp_lcd_new_panel_io_i2c`) before calling this function.
 *
 * @param io     Panel IO from `esp_lcd_new_panel_io_i2c()`
 * @param config Touch configuration
 * @param tp     Output touch handle
 * @return
 *      - ESP_OK on success
 */
esp_err_t esp_lcd_touch_new_i2c_cst820(const esp_lcd_panel_io_handle_t io,
                                       const esp_lcd_touch_config_t *config,
                                       esp_lcd_touch_handle_t *tp);

/** I2C address of the CST820 controller */
#define ESP_LCD_TOUCH_IO_I2C_CST820_ADDRESS    (0x15)

/**
 * @brief Touch IO configuration for new I2C master + `esp_lcd_new_panel_io_i2c()`
 */
#define ESP_LCD_TOUCH_IO_I2C_CST820_CONFIG()               \
    {                                                      \
        .dev_addr = ESP_LCD_TOUCH_IO_I2C_CST820_ADDRESS,   \
        .scl_speed_hz = 400000,                            \
        .control_phase_bytes = 1,                          \
        .dc_bit_offset = 0,                                \
        .lcd_cmd_bits = 8,                                 \
        .flags =                                           \
        {                                                  \
            .disable_control_phase = 1,                    \
        },                                                 \
    }

#ifdef __cplusplus
}
#endif
