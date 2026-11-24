/*
 * SPDX-FileCopyrightText: 2015-2023 Espressif Systems (Shanghai) CO LTD
 * SPDX-FileCopyrightText: 2026 VIEWE
 *
 * SPDX-License-Identifier: Apache-2.0
 *
 * CST826 notes (vs CST816S):
 * - Chip type is at 0xAA (=0x11); register 0xA7 returns 0.
 * - Firmware version is at 0xA6.
 * - Deep sleep command is write 0x03 to 0xA5; wake by RST.
 * - Touch report layout (0x02 count, 0x03..0x06 XY) is compatible with CST816 family.
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

#include "esp_lcd_touch_cst826.h"

#define POINT_NUM_MAX           (1)

#define DATA_START_REG          (0x02)
#define CHIP_ID_REG_816         (0xA7)  /* CST816x family; CST826 returns 0 here */
#define CHIP_TYPE_REG           (0xAA)  /* CST826 / CST830 / CST836U */
#define FW_VERSION_REG          (0xA6)
#define SLEEP_REG               (0xA5)
#define SLEEP_CMD               (0x03)

#define CST826_CHIP_TYPE        (0x11)

static const char *TAG = "CST826";

static esp_err_t read_data(esp_lcd_touch_handle_t tp);
static bool get_xy(esp_lcd_touch_handle_t tp, uint16_t *x, uint16_t *y, uint16_t *strength, uint8_t *point_num, uint8_t max_point_num);
static esp_err_t del(esp_lcd_touch_handle_t tp);
static esp_err_t enter_sleep(esp_lcd_touch_handle_t tp);
static esp_err_t exit_sleep(esp_lcd_touch_handle_t tp);

static esp_err_t i2c_read_bytes(esp_lcd_touch_handle_t tp, uint16_t reg, uint8_t *data, uint8_t len);
static esp_err_t i2c_write_byte(esp_lcd_touch_handle_t tp, uint16_t reg, uint8_t data);

static esp_err_t reset(esp_lcd_touch_handle_t tp);
static esp_err_t read_id(esp_lcd_touch_handle_t tp);

esp_err_t esp_lcd_touch_new_i2c_cst826(const esp_lcd_panel_io_handle_t io, const esp_lcd_touch_config_t *config, esp_lcd_touch_handle_t *tp)
{
    ESP_RETURN_ON_FALSE(io, ESP_ERR_INVALID_ARG, TAG, "Invalid io");
    ESP_RETURN_ON_FALSE(config, ESP_ERR_INVALID_ARG, TAG, "Invalid config");
    ESP_RETURN_ON_FALSE(tp, ESP_ERR_INVALID_ARG, TAG, "Invalid touch handle");

    esp_err_t ret = ESP_OK;
    esp_lcd_touch_handle_t cst826 = calloc(1, sizeof(esp_lcd_touch_t));
    ESP_GOTO_ON_FALSE(cst826, ESP_ERR_NO_MEM, err, TAG, "Touch handle malloc failed");

    cst826->io = io;
    cst826->read_data = read_data;
    cst826->get_xy = get_xy;
    cst826->del = del;
    cst826->enter_sleep = enter_sleep;
    cst826->exit_sleep = exit_sleep;
    cst826->data.lock.owner = portMUX_FREE_VAL;
    memcpy(&cst826->config, config, sizeof(esp_lcd_touch_config_t));

    if (cst826->config.int_gpio_num != GPIO_NUM_NC) {
        const gpio_config_t int_gpio_config = {
            .mode = GPIO_MODE_INPUT,
            .intr_type = (cst826->config.levels.interrupt ? GPIO_INTR_POSEDGE : GPIO_INTR_NEGEDGE),
            .pin_bit_mask = BIT64(cst826->config.int_gpio_num)
        };
        ESP_GOTO_ON_ERROR(gpio_config(&int_gpio_config), err, TAG, "GPIO intr config failed");

        if (cst826->config.interrupt_callback) {
            esp_lcd_touch_register_interrupt_callback(cst826, cst826->config.interrupt_callback);
        }
    }

    if (cst826->config.rst_gpio_num != GPIO_NUM_NC) {
        const gpio_config_t rst_gpio_config = {
            .mode = GPIO_MODE_OUTPUT,
            .pin_bit_mask = BIT64(cst826->config.rst_gpio_num)
        };
        ESP_GOTO_ON_ERROR(gpio_config(&rst_gpio_config), err, TAG, "GPIO reset config failed");
    }

    ESP_GOTO_ON_ERROR(reset(cst826), err, TAG, "Reset failed");
    /* Soft-fail: some boards only answer after first touch if RST is unused */
    if (read_id(cst826) != ESP_OK) {
        ESP_LOGW(TAG, "Chip ID probe failed (will continue); ensure RST is wired or touch once");
    }

    *tp = cst826;
    return ESP_OK;

err:
    if (cst826) {
        del(cst826);
    }
    ESP_LOGE(TAG, "Initialization failed!");
    return ret;
}

static esp_err_t read_data(esp_lcd_touch_handle_t tp)
{
    uint8_t buf[5] = {0};
    uint16_t x = 0;
    uint16_t y = 0;
    uint8_t point_num = 0;
    uint8_t point_id = 0;

    /* 0x02: FingerNum, 0x03..0x06: first point XH/XL/YH/YL */
    ESP_RETURN_ON_ERROR(i2c_read_bytes(tp, DATA_START_REG, buf, sizeof(buf)), TAG, "I2C read failed");

    point_num = buf[0] & 0x0F;
    point_id = buf[3] >> 4;

    portENTER_CRITICAL(&tp->data.lock);
    tp->data.points = 0;
    tp->data.coords[0].x = 0;
    tp->data.coords[0].y = 0;

    /* 0x0F touch ID means invalid / lifted on Hynitron self-cap family */
    if (point_num > 0 && point_num <= POINT_NUM_MAX && point_id != 0x0F) {
        x = (((uint16_t)(buf[1] & 0x0F)) << 8) | buf[2];
        y = (((uint16_t)(buf[3] & 0x0F)) << 8) | buf[4];

        /* Accept (0,0): round panels can report the origin as a valid point. */
        if (x < 4096 && y < 4096) {
            tp->data.points = 1;
            tp->data.coords[0].x = x;
            tp->data.coords[0].y = y;
            tp->data.coords[0].strength = 50;
        }
    }
    portEXIT_CRITICAL(&tp->data.lock);

    if (tp->data.points > 0) {
        ESP_LOGD(TAG, "Touch: id=%d x=%d y=%d", point_id, x, y);
    }

    return ESP_OK;
}

static bool get_xy(esp_lcd_touch_handle_t tp, uint16_t *x, uint16_t *y, uint16_t *strength, uint8_t *point_num, uint8_t max_point_num)
{
    portENTER_CRITICAL(&tp->data.lock);
    *point_num = (tp->data.points > max_point_num ? max_point_num : tp->data.points);

    if (*point_num > 0) {
        for (size_t i = 0; i < *point_num; i++) {
            x[i] = tp->data.coords[i].x;
            y[i] = tp->data.coords[i].y;
            if (strength) {
                strength[i] = tp->data.coords[i].strength;
            }
        }
    }

    tp->data.points = 0;
    portEXIT_CRITICAL(&tp->data.lock);

    return (*point_num > 0);
}

static esp_err_t del(esp_lcd_touch_handle_t tp)
{
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

static esp_err_t reset(esp_lcd_touch_handle_t tp)
{
    if (tp->config.rst_gpio_num != GPIO_NUM_NC) {
        ESP_RETURN_ON_ERROR(gpio_set_level(tp->config.rst_gpio_num, tp->config.levels.reset), TAG, "GPIO set level failed");
        vTaskDelay(pdMS_TO_TICKS(10));
        ESP_RETURN_ON_ERROR(gpio_set_level(tp->config.rst_gpio_num, !tp->config.levels.reset), TAG, "GPIO set level failed");
        /* Vendor: >30 ms after RST before I2C is reliable */
        vTaskDelay(pdMS_TO_TICKS(50));
    }

    return ESP_OK;
}

static esp_err_t enter_sleep(esp_lcd_touch_handle_t tp)
{
    /* CST826 / CST716 / CST836U: sleep cmd at 0xA5 (not 0xE5 used by CST816S) */
    return i2c_write_byte(tp, SLEEP_REG, SLEEP_CMD);
}

static esp_err_t exit_sleep(esp_lcd_touch_handle_t tp)
{
    /* Wake CST826 by hardware reset */
    return reset(tp);
}

static esp_err_t read_id(esp_lcd_touch_handle_t tp)
{
    uint8_t id_a7 = 0;
    uint8_t chip_type = 0;
    uint8_t fw_ver = 0;

    ESP_RETURN_ON_ERROR(i2c_read_bytes(tp, CHIP_ID_REG_816, &id_a7, 1), TAG, "I2C read 0xA7 failed");
    ESP_RETURN_ON_ERROR(i2c_read_bytes(tp, CHIP_TYPE_REG, &chip_type, 1), TAG, "I2C read 0xAA failed");
    (void)i2c_read_bytes(tp, FW_VERSION_REG, &fw_ver, 1);

    /*
     * CST826: 0xA7==0, chip type at 0xAA==0x11.
     * If 0xA7 is non-zero this is likely a CST816x wired by mistake.
     */
    if (chip_type != CST826_CHIP_TYPE) {
        ESP_LOGE(TAG, "Unexpected chip type 0x%02X (0xA7=0x%02X), expected CST826 0x%02X",
                 chip_type, id_a7, CST826_CHIP_TYPE);
        return ESP_ERR_NOT_FOUND;
    }

    ESP_LOGI(TAG, "CST826 ready (chip_type=0x%02X, fw=0x%02X)", chip_type, fw_ver);
    return ESP_OK;
}

static esp_err_t i2c_read_bytes(esp_lcd_touch_handle_t tp, uint16_t reg, uint8_t *data, uint8_t len)
{
    ESP_RETURN_ON_FALSE(data, ESP_ERR_INVALID_ARG, TAG, "Invalid data");
    return esp_lcd_panel_io_rx_param(tp->io, reg, data, len);
}

static esp_err_t i2c_write_byte(esp_lcd_touch_handle_t tp, uint16_t reg, uint8_t data)
{
    return esp_lcd_panel_io_tx_param(tp->io, reg, (uint8_t[]) {
        data
    }, 1);
}
