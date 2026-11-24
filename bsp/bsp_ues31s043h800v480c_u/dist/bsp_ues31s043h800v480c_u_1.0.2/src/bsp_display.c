/*
 * SPDX-FileCopyrightText: 2026 VIEWE
 * SPDX-License-Identifier: Apache-2.0
 */

#include <inttypes.h>
#include <stdio.h>
#include <string.h>
#include "esp_check.h"
#include "esp_log.h"
#include "esp_lcd_panel_ops.h"
#include "esp_lcd_panel_rgb.h"
#include "esp_lcd_panel_io.h"
#include "esp_lcd_touch_gt911.h"
#include "driver/ledc.h"
#include "driver/gpio.h"
#include "bsp/esp-bsp.h"
#include "bsp_err_check.h"

static const char *TAG = "BSP-DISPLAY";

static bool s_pwm_inited;
static esp_lcd_panel_handle_t s_panel;
static esp_lcd_touch_handle_t s_touch;
static void *s_fb[3];
static uint8_t s_fb_count;

esp_err_t bsp_display_brightness_init(void)
{
    if (s_pwm_inited) {
        return ESP_OK;
    }

    const ledc_timer_config_t timer = {
        .speed_mode = LEDC_LOW_SPEED_MODE,
        .duty_resolution = LEDC_TIMER_10_BIT,
        .timer_num = CONFIG_BSP_DISPLAY_BRIGHTNESS_LEDC_TIMER,
        .freq_hz = 25000,
        .clk_cfg = LEDC_AUTO_CLK,
    };
    BSP_ERROR_CHECK_RETURN_ERR(ledc_timer_config(&timer));

    const ledc_channel_config_t ch = {
        .gpio_num = BSP_LCD_BACKLIGHT,
        .speed_mode = LEDC_LOW_SPEED_MODE,
        .channel = CONFIG_BSP_DISPLAY_BRIGHTNESS_LEDC_CH,
        .timer_sel = CONFIG_BSP_DISPLAY_BRIGHTNESS_LEDC_TIMER,
        .duty = 0,
        .hpoint = 0,
    };
    BSP_ERROR_CHECK_RETURN_ERR(ledc_channel_config(&ch));
    s_pwm_inited = true;
    return ESP_OK;
}

esp_err_t bsp_display_brightness_set(int brightness_percent)
{
    if (brightness_percent > 100) {
        brightness_percent = 100;
    }
    if (brightness_percent < 0) {
        brightness_percent = 0;
    }

    BSP_ERROR_CHECK_RETURN_ERR(bsp_display_brightness_init());
    uint32_t duty = (1023 * (uint32_t)brightness_percent) / 100;
    BSP_ERROR_CHECK_RETURN_ERR(ledc_set_duty(LEDC_LOW_SPEED_MODE,
                                             CONFIG_BSP_DISPLAY_BRIGHTNESS_LEDC_CH, duty));
    BSP_ERROR_CHECK_RETURN_ERR(ledc_update_duty(LEDC_LOW_SPEED_MODE,
                                                CONFIG_BSP_DISPLAY_BRIGHTNESS_LEDC_CH));
    ESP_LOGD(TAG, "backlight %d%% duty=%" PRIu32, brightness_percent, duty);
    return ESP_OK;
}

esp_err_t bsp_display_backlight_on(void)
{
    return bsp_display_brightness_set(100);
}

esp_err_t bsp_display_backlight_off(void)
{
    return bsp_display_brightness_set(0);
}

esp_err_t bsp_display_new(const bsp_display_config_t *config,
                          esp_lcd_panel_handle_t *ret_panel,
                          esp_lcd_panel_io_handle_t *ret_io)
{
    BSP_NULL_CHECK(ret_panel, ESP_ERR_INVALID_ARG);

    BSP_ERROR_CHECK_RETURN_ERR(bsp_display_brightness_init());

    uint8_t num_fbs = BSP_LCD_FB_COUNT;
    uint16_t bounce_lines = BSP_LCD_BOUNCE_BUFFER_HEIGHT;
    if (config && config->num_fbs > 0) {
        num_fbs = config->num_fbs;
    }
    if (config && config->bounce_lines > 0) {
        bounce_lines = config->bounce_lines;
        if (bounce_lines > 48) {
            bounce_lines = 48;
        }
    }
    if (num_fbs > 3) {
        num_fbs = 3;
    }
    s_fb_count = num_fbs;

    /* RGB666 on S31 as RGB888. Timing: 22 MHz, H/V porch 4/8/8, bounce 20, 2 FBs. */
    const esp_lcd_rgb_panel_config_t panel_conf = {
        .clk_src = LCD_CLK_SRC_DEFAULT,
        .dma_burst_size = 128,
        .data_width = 24,
        .in_color_format = LCD_COLOR_FMT_RGB888,
        .out_color_format = LCD_COLOR_FMT_RGB888,
        .de_gpio_num = BSP_LCD_DE,
        .pclk_gpio_num = BSP_LCD_PCLK,
        .vsync_gpio_num = BSP_LCD_VSYNC,
        .hsync_gpio_num = BSP_LCD_HSYNC,
        .disp_gpio_num = BSP_LCD_DISP,
        .data_gpio_nums = {
            BSP_LCD_DATA0,  BSP_LCD_DATA1,  BSP_LCD_DATA2,  BSP_LCD_DATA3,
            BSP_LCD_DATA4,  BSP_LCD_DATA5,  BSP_LCD_DATA6,  BSP_LCD_DATA7,
            BSP_LCD_DATA8,  BSP_LCD_DATA9,  BSP_LCD_DATA10, BSP_LCD_DATA11,
            BSP_LCD_DATA12, BSP_LCD_DATA13, BSP_LCD_DATA14, BSP_LCD_DATA15,
            BSP_LCD_DATA16, BSP_LCD_DATA17, BSP_LCD_DATA18, BSP_LCD_DATA19,
            BSP_LCD_DATA20, BSP_LCD_DATA21, BSP_LCD_DATA22, BSP_LCD_DATA23,
        },
        .timings = {
            .pclk_hz = BSP_LCD_PIXEL_CLOCK_HZ,
            .h_res = BSP_LCD_H_RES,
            .v_res = BSP_LCD_V_RES,
            .hsync_pulse_width = BSP_LCD_HSYNC_PULSE_WIDTH,
            .hsync_back_porch = BSP_LCD_HSYNC_BACK_PORCH,
            .hsync_front_porch = BSP_LCD_HSYNC_FRONT_PORCH,
            .vsync_pulse_width = BSP_LCD_VSYNC_PULSE_WIDTH,
            .vsync_back_porch = BSP_LCD_VSYNC_BACK_PORCH,
            .vsync_front_porch = BSP_LCD_VSYNC_FRONT_PORCH,
            .flags = {
                .hsync_idle_low = false,
                .vsync_idle_low = false,
                .de_idle_high = false,
                .pclk_active_neg = true,
                .pclk_idle_high = false,
            },
        },
        .num_fbs = num_fbs,
        .bounce_buffer_size_px = BSP_LCD_H_RES * bounce_lines,
        .flags.fb_in_psram = true,
    };

    BSP_ERROR_CHECK_RETURN_ERR(esp_lcd_new_rgb_panel(&panel_conf, &s_panel));
    BSP_ERROR_CHECK_RETURN_ERR(esp_lcd_panel_reset(s_panel));
    BSP_ERROR_CHECK_RETURN_ERR(esp_lcd_panel_init(s_panel));
    if (num_fbs >= 3) {
        BSP_ERROR_CHECK_RETURN_ERR(esp_lcd_rgb_panel_get_frame_buffer(s_panel, 3, &s_fb[0], &s_fb[1], &s_fb[2]));
    } else if (num_fbs == 2) {
        BSP_ERROR_CHECK_RETURN_ERR(esp_lcd_rgb_panel_get_frame_buffer(s_panel, 2, &s_fb[0], &s_fb[1]));
    } else {
        BSP_ERROR_CHECK_RETURN_ERR(esp_lcd_rgb_panel_get_frame_buffer(s_panel, 1, &s_fb[0]));
    }
    for (uint8_t i = 0; i < num_fbs; i++) {
        if (s_fb[i]) {
            memset(s_fb[i], 0, BSP_LCD_FB_SIZE);
        }
    }

    *ret_panel = s_panel;
    if (ret_io) {
        *ret_io = NULL; /* RGB panel has no SPI/I80 panel IO */
    }
    ESP_LOGI(TAG, "RGB LCD %dx%d RGB888 ready (%u FB, pclk %u MHz, bounce %u)",
             BSP_LCD_H_RES, BSP_LCD_V_RES, (unsigned)s_fb_count,
             (unsigned)(BSP_LCD_PIXEL_CLOCK_HZ / 1000000), (unsigned)bounce_lines);
    return ESP_OK;
}

esp_err_t bsp_display_get_framebuffers(void **fb0, void **fb1)
{
    if (!s_panel) {
        return ESP_ERR_INVALID_STATE;
    }
    if (fb0) {
        *fb0 = s_fb[0];
    }
    if (fb1) {
        *fb1 = s_fb[1];
    }
    return ESP_OK;
}

esp_lcd_panel_handle_t bsp_display_get_panel(void)
{
    return s_panel;
}

esp_err_t bsp_touch_new(const bsp_touch_config_t *config, esp_lcd_touch_handle_t *ret_touch)
{
    (void)config;
    BSP_NULL_CHECK(ret_touch, ESP_ERR_INVALID_ARG);
    if (s_touch) {
        *ret_touch = s_touch;
        return ESP_OK;
    }
    BSP_ERROR_CHECK_RETURN_ERR(bsp_i2c_init());

    i2c_master_bus_handle_t bus = bsp_i2c_get_handle();
    const uint8_t candidates[] = {
        ESP_LCD_TOUCH_IO_I2C_GT911_ADDRESS,
        ESP_LCD_TOUCH_IO_I2C_GT911_ADDRESS_BACKUP,
    };
    uint8_t found = 0;
    for (size_t i = 0; i < sizeof(candidates); i++) {
        if (i2c_master_probe(bus, candidates[i], 200) == ESP_OK) {
            found = candidates[i];
            ESP_LOGI(TAG, "GT911 at 7-bit 0x%02X", found);
            break;
        }
    }
    if (found == 0) {
        ESP_LOGE(TAG, "GT911 not found");
        return ESP_ERR_NOT_FOUND;
    }

    esp_lcd_panel_io_handle_t tp_io = NULL;
    esp_lcd_panel_io_i2c_config_t io_cfg = ESP_LCD_TOUCH_IO_I2C_GT911_CONFIG();
    io_cfg.dev_addr = found;
    io_cfg.scl_speed_hz = CONFIG_BSP_I2C_CLK_SPEED_HZ;
    BSP_ERROR_CHECK_RETURN_ERR(esp_lcd_new_panel_io_i2c(bus, &io_cfg, &tp_io));

    const esp_lcd_touch_config_t tp_cfg = {
        .x_max = BSP_LCD_H_RES,
        .y_max = BSP_LCD_V_RES,
        .rst_gpio_num = BSP_LCD_TOUCH_RST,
        .int_gpio_num = BSP_LCD_TOUCH_INT,
        .levels = {
            .reset = 0,
            .interrupt = 0,
        },
        .flags = {
            .swap_xy = 0,
            .mirror_x = 0,
            .mirror_y = 0,
        },
    };
    BSP_ERROR_CHECK_RETURN_ERR(esp_lcd_touch_new_i2c_gt911(tp_io, &tp_cfg, &s_touch));
    *ret_touch = s_touch;
    return ESP_OK;
}
