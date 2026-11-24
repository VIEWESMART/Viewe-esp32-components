/*
 * SPDX-FileCopyrightText: 2024-2026 Shenzhen VIEWE TECHNOLOGY Co., LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <inttypes.h>
#include <stdio.h>
#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/gpio.h"
#include "esp_system.h"
#include "esp_err.h"
#include "esp_log.h"
#include "esp_check.h"
#include "esp_lcd_panel_io.h"
#include "esp_lcd_touch.h"

#define POINT_NUM_MAX       (1)

/* CHSC6540 touch data starts at register 0x00 */
#define DATA_START_REG      (0x00)
#define CHIP_ID_REG         (0xA7)
#define TOUCH_DATA_LEN      (7)

static const char *TAG = "CHSC6540";

static esp_err_t esp_lcd_touch_chsc6540_read_data(esp_lcd_touch_handle_t tp);
static bool esp_lcd_touch_chsc6540_get_xy(esp_lcd_touch_handle_t tp, uint16_t *x, uint16_t *y,
        uint16_t *strength, uint8_t *point_num, uint8_t max_point_num);
static esp_err_t esp_lcd_touch_chsc6540_del(esp_lcd_touch_handle_t tp);

static esp_err_t touch_chsc6540_i2c_read(esp_lcd_touch_handle_t tp, uint16_t reg, uint8_t *data, uint8_t len);
static esp_err_t touch_chsc6540_reset(esp_lcd_touch_handle_t tp);
#ifndef CONFIG_ESP_LCD_TOUCH_CHSC6540_DISABLE_READ_ID
static esp_err_t touch_chsc6540_read_id(esp_lcd_touch_handle_t tp);
#endif

esp_err_t esp_lcd_touch_new_i2c_chsc6540(const esp_lcd_panel_io_handle_t io,
                                         const esp_lcd_touch_config_t *config,
                                         esp_lcd_touch_handle_t *tp)
{
    ESP_RETURN_ON_FALSE(io != NULL, ESP_ERR_INVALID_ARG, TAG, "Touch controller io handle can't be NULL");
    ESP_RETURN_ON_FALSE(config != NULL, ESP_ERR_INVALID_ARG, TAG,
                        "Pointer to the touch controller configuration can't be NULL");
    ESP_RETURN_ON_FALSE(tp != NULL, ESP_ERR_INVALID_ARG, TAG, "Pointer to the touch controller handle can't be NULL");

    esp_err_t ret = ESP_OK;
    esp_lcd_touch_handle_t chsc6540 = calloc(1, sizeof(esp_lcd_touch_t));
    ESP_GOTO_ON_FALSE(chsc6540, ESP_ERR_NO_MEM, err, TAG, "Touch handle malloc failed");

    chsc6540->io = io;
    chsc6540->read_data = esp_lcd_touch_chsc6540_read_data;
    chsc6540->get_xy = esp_lcd_touch_chsc6540_get_xy;
    chsc6540->del = esp_lcd_touch_chsc6540_del;
    chsc6540->data.lock.owner = portMUX_FREE_VAL;
    memcpy(&chsc6540->config, config, sizeof(esp_lcd_touch_config_t));

    if (chsc6540->config.int_gpio_num != GPIO_NUM_NC) {
        const gpio_config_t int_gpio_config = {
            .mode = GPIO_MODE_INPUT,
            .intr_type = (chsc6540->config.levels.interrupt ? GPIO_INTR_POSEDGE : GPIO_INTR_NEGEDGE),
            .pin_bit_mask = BIT64(chsc6540->config.int_gpio_num)
        };
        ESP_GOTO_ON_ERROR(gpio_config(&int_gpio_config), err, TAG, "GPIO intr config failed");

        if (chsc6540->config.interrupt_callback) {
            esp_lcd_touch_register_interrupt_callback(chsc6540, chsc6540->config.interrupt_callback);
        }
    }

    if (chsc6540->config.rst_gpio_num != GPIO_NUM_NC) {
        const gpio_config_t rst_gpio_config = {
            .mode = GPIO_MODE_OUTPUT,
            .pin_bit_mask = BIT64(chsc6540->config.rst_gpio_num)
        };
        ESP_GOTO_ON_ERROR(gpio_config(&rst_gpio_config), err, TAG, "GPIO reset config failed");
    }

    ESP_GOTO_ON_ERROR(touch_chsc6540_reset(chsc6540), err, TAG, "Reset failed");
#ifdef CONFIG_ESP_LCD_TOUCH_CHSC6540_DISABLE_READ_ID
    ESP_LOGI(TAG, "Read ID disabled");
#else
    ESP_GOTO_ON_ERROR(touch_chsc6540_read_id(chsc6540), err, TAG, "Read ID failed");
#endif
    *tp = chsc6540;
    return ESP_OK;

err:
    if (chsc6540) {
        esp_lcd_touch_chsc6540_del(chsc6540);
    }
    ESP_LOGE(TAG, "Initialization failed!");
    return ret;
}

static esp_err_t esp_lcd_touch_chsc6540_read_data(esp_lcd_touch_handle_t tp)
{
    ESP_RETURN_ON_FALSE(tp != NULL, ESP_ERR_INVALID_ARG, TAG, "Touch controller handle can't be NULL");

    uint8_t buf[TOUCH_DATA_LEN] = {0};
    ESP_RETURN_ON_ERROR(touch_chsc6540_i2c_read(tp, DATA_START_REG, buf, sizeof(buf)), TAG, "I2C read failed");

    /* Register map (from reg 0x00):
     *  buf[2]       : touch point count
     *  buf[3][7:4]  : event / gesture nibble
     *  buf[3][3:0]|buf[4] : X
     *  buf[5][3:0]|buf[6] : Y
     */
    uint8_t points = buf[2];
    uint16_t x = (uint16_t)(((buf[3] & 0x0F) << 8) | buf[4]);
    uint16_t y = (uint16_t)(((buf[5] & 0x0F) << 8) | buf[6]);

    portENTER_CRITICAL(&tp->data.lock);
    points = (points > POINT_NUM_MAX) ? POINT_NUM_MAX : points;
    tp->data.points = points;
    for (int i = 0; i < points; i++) {
        tp->data.coords[i].x = x;
        tp->data.coords[i].y = y;
    }
    portEXIT_CRITICAL(&tp->data.lock);

    return ESP_OK;
}

static bool esp_lcd_touch_chsc6540_get_xy(esp_lcd_touch_handle_t tp, uint16_t *x, uint16_t *y,
        uint16_t *strength, uint8_t *point_num, uint8_t max_point_num)
{
    ESP_RETURN_ON_FALSE(tp != NULL, false, TAG, "Touch controller handle can't be NULL");
    ESP_RETURN_ON_FALSE(x != NULL, false, TAG, "Pointer to the x coordinates array can't be NULL");
    ESP_RETURN_ON_FALSE(y != NULL, false, TAG, "Pointer to the y coordinates array can't be NULL");
    ESP_RETURN_ON_FALSE(point_num != NULL, false, TAG, "Pointer to number of touch points can't be NULL");
    ESP_RETURN_ON_FALSE(max_point_num > 0, false, TAG, "Array size must be equal or larger than 1");

    portENTER_CRITICAL(&tp->data.lock);
    *point_num = (tp->data.points > max_point_num ? max_point_num : tp->data.points);
    for (size_t i = 0; i < *point_num; i++) {
        x[i] = tp->data.coords[i].x;
        y[i] = tp->data.coords[i].y;
        if (strength) {
            strength[i] = tp->data.coords[i].strength;
        }
    }
    tp->data.points = 0;
    portEXIT_CRITICAL(&tp->data.lock);

    return (*point_num > 0);
}

static esp_err_t esp_lcd_touch_chsc6540_del(esp_lcd_touch_handle_t tp)
{
    ESP_RETURN_ON_FALSE(tp != NULL, ESP_ERR_INVALID_ARG, TAG, "Touch controller handle can't be NULL");

    if (tp->config.int_gpio_num != GPIO_NUM_NC) {
        gpio_reset_pin(tp->config.int_gpio_num);
        if (tp->config.interrupt_callback) {
            gpio_isr_handler_remove(tp->config.int_gpio_num);
        }
    }
    if (tp->config.rst_gpio_num != GPIO_NUM_NC) {
        gpio_reset_pin(tp->config.rst_gpio_num);
    }
    free(tp);
    return ESP_OK;
}

static esp_err_t touch_chsc6540_reset(esp_lcd_touch_handle_t tp)
{
    ESP_RETURN_ON_FALSE(tp != NULL, ESP_ERR_INVALID_ARG, TAG, "Touch controller handle can't be NULL");

    if (tp->config.rst_gpio_num != GPIO_NUM_NC) {
        ESP_RETURN_ON_ERROR(gpio_set_level(tp->config.rst_gpio_num, tp->config.levels.reset), TAG, "GPIO set level failed");
        vTaskDelay(pdMS_TO_TICKS(200));
        ESP_RETURN_ON_ERROR(gpio_set_level(tp->config.rst_gpio_num, !tp->config.levels.reset), TAG, "GPIO set level failed");
        vTaskDelay(pdMS_TO_TICKS(200));
    }
    return ESP_OK;
}

#ifndef CONFIG_ESP_LCD_TOUCH_CHSC6540_DISABLE_READ_ID
static esp_err_t touch_chsc6540_read_id(esp_lcd_touch_handle_t tp)
{
    ESP_RETURN_ON_FALSE(tp != NULL, ESP_ERR_INVALID_ARG, TAG, "Touch controller handle can't be NULL");

    uint8_t id = 0;
    ESP_RETURN_ON_ERROR(touch_chsc6540_i2c_read(tp, CHIP_ID_REG, &id, 1), TAG, "I2C read failed");
    ESP_LOGI(TAG, "IC id: 0x%02X", id);
    return ESP_OK;
}
#endif

static esp_err_t touch_chsc6540_i2c_read(esp_lcd_touch_handle_t tp, uint16_t reg, uint8_t *data, uint8_t len)
{
    ESP_RETURN_ON_FALSE(tp != NULL, ESP_ERR_INVALID_ARG, TAG, "Touch controller handle can't be NULL");
    ESP_RETURN_ON_FALSE(data != NULL, ESP_ERR_INVALID_ARG, TAG, "Pointer to the data array can't be NULL");

    return esp_lcd_panel_io_rx_param(tp->io, reg, data, len);
}
