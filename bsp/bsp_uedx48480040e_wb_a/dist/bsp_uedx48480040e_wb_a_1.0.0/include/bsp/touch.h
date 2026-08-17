/*
 * SPDX-FileCopyrightText: 2026
 * SPDX-License-Identifier: Apache-2.0
 */

/**
 * @file
 * @brief BSP touchscreen (without LVGL)
 */

#pragma once

#include "esp_err.h"
#include "esp_lcd_touch.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    void *dummy;
} bsp_touch_config_t;

/**
 * @brief Create FT5x06 touch controller
 *
 * Initializes I2C if needed.
 *
 * @param[in]  config    Touch config, may be NULL
 * @param[out] ret_touch Touch handle
 */
esp_err_t bsp_touch_new(const bsp_touch_config_t *config, esp_lcd_touch_handle_t *ret_touch);

#ifdef __cplusplus
}
#endif
