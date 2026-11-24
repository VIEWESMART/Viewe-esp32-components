/*
 * SPDX-FileCopyrightText: 2026 VIEWE
 * SPDX-License-Identifier: Apache-2.0
 */

/**
 * @file
 * @brief VIEWE DeskNob touch (CST826), hardware init without LVGL.
 */

#pragma once

#include "esp_err.h"
#include "esp_lcd_touch.h"

#ifdef __cplusplus
extern "C" {
#endif

/** @defgroup g05_touch Touch
 *  @{
 */

/**
 * @brief Touch configuration. Reserved for future flags; pass NULL.
 */
typedef struct {
    int dummy;
} bsp_touch_config_t;

/**
 * @brief Create the CST826 touch controller.
 *
 * The touch I2C bus is initialized on first call and can be called again.
 *
 * @param[in]  config    Touch configuration. NULL selects the board default.
 * @param[out] ret_touch Touch handle.
 */
esp_err_t bsp_touch_new(const bsp_touch_config_t *config, esp_lcd_touch_handle_t *ret_touch);

/** @} */

#ifdef __cplusplus
}
#endif
