/*
 * SPDX-FileCopyrightText: 2026 VIEWE
 * SPDX-License-Identifier: Apache-2.0
 */

#include "bsp/viewe_desknob.h"

#include "bsp_err_check.h"
#include "driver/gpio.h"
#include "esp_log.h"
#include "esp_rom_gpio.h"
#include "led_strip.h"
#include "soc/gpio_reg.h"
#include "soc/gpio_sig_map.h"

static const char *TAG = "viewe_desknob.led";

static led_strip_handle_t s_strip;
static int s_rmt_signal = -1;

static uint32_t bsp_led_out_sel_reg(gpio_num_t gpio)
{
    return GPIO_FUNC0_OUT_SEL_CFG_REG + ((uint32_t)gpio * 4);
}

static void bsp_led_claim_rmt(void)
{
    if (s_rmt_signal < 0) {
        return;
    }
    esp_rom_gpio_connect_out_signal(BSP_LED_STRIP_IO, s_rmt_signal, false, false);
}

static void bsp_led_park_boot_high(void)
{
    uint32_t sel = REG_READ(bsp_led_out_sel_reg(BSP_LED_STRIP_IO)) & GPIO_FUNC0_OUT_SEL_V;
    if (sel != SIG_GPIO_OUT_IDX) {
        s_rmt_signal = (int)sel;
    }
    esp_rom_gpio_connect_out_signal(BSP_LED_STRIP_IO, SIG_GPIO_OUT_IDX, false, false);
    gpio_set_direction(BSP_LED_STRIP_IO, GPIO_MODE_OUTPUT);
    gpio_set_level(BSP_LED_STRIP_IO, 1);
}

esp_err_t bsp_led_strip_init(void)
{
    if (s_strip) {
        return ESP_OK;
    }

    const led_strip_config_t strip_cfg = {
        .strip_gpio_num = BSP_LED_STRIP_IO,
        .max_leds = BSP_LED_STRIP_COUNT,
        .led_model = LED_MODEL_WS2812,
        .color_component_format = LED_STRIP_COLOR_COMPONENT_FMT_GRB,
        .flags.invert_out = false,
    };
    const led_strip_rmt_config_t rmt_cfg = {
        .clk_src = RMT_CLK_SRC_DEFAULT,
        .resolution_hz = 10 * 1000 * 1000,
        .mem_block_symbols = 64,
        .flags.with_dma = false,
    };
    BSP_ERROR_CHECK_RETURN_ERR(led_strip_new_rmt_device(&strip_cfg, &rmt_cfg, &s_strip));
    ESP_LOGI(TAG, "WS2812 strip GPIO%d, %d LEDs (shared with BOOT)", BSP_LED_STRIP_IO, BSP_LED_STRIP_COUNT);
    return bsp_led_clear();
}

esp_err_t bsp_led_set_rgb(int index, uint8_t red, uint8_t green, uint8_t blue)
{
    if (!s_strip) {
        return ESP_ERR_INVALID_STATE;
    }
    if (index < 0 || index >= BSP_LED_STRIP_COUNT) {
        return ESP_ERR_INVALID_ARG;
    }
    return led_strip_set_pixel(s_strip, index, red, green, blue);
}

esp_err_t bsp_led_refresh(void)
{
    if (!s_strip) {
        return ESP_ERR_INVALID_STATE;
    }
    bsp_led_claim_rmt();
    BSP_ERROR_CHECK_RETURN_ERR(led_strip_refresh(s_strip));
    bsp_led_park_boot_high();
    return ESP_OK;
}

esp_err_t bsp_led_clear(void)
{
    if (!s_strip) {
        return ESP_ERR_INVALID_STATE;
    }
    bsp_led_claim_rmt();
    BSP_ERROR_CHECK_RETURN_ERR(led_strip_clear(s_strip));
    bsp_led_park_boot_high();
    return ESP_OK;
}
