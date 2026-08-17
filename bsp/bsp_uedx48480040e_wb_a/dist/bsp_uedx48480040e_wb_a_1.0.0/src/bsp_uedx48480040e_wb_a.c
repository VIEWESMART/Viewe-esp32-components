/*
 * SPDX-FileCopyrightText: 2026
 * SPDX-License-Identifier: Apache-2.0
 */

#include "esp_check.h"
#include "esp_err.h"
#include "esp_log.h"
#include "esp_lcd_gc9503.h"
#include "esp_lcd_panel_io_additions.h"
#include "esp_lcd_panel_ops.h"
#include "esp_lcd_panel_rgb.h"
#include "esp_lcd_touch_ft5x06.h"
#include "driver/gpio.h"
#include "driver/i2c_master.h"
#include "driver/ledc.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "sdkconfig.h"

#include "bsp/uedx48480040e_wb_a.h"
#include "bsp_err_check.h"

#if (BSP_CONFIG_NO_GRAPHIC_LIB == 0)
#include "esp_lv_adapter.h"
#include "esp_lv_adapter_display.h"
#include "esp_lv_adapter_input.h"
#endif

#if ((BSP_LCD_H_RES * BSP_LCD_V_RES) % \
     (BSP_LCD_H_RES * CONFIG_BSP_LCD_RGB_BOUNCE_BUFFER_HEIGHT)) != 0
#error "CONFIG_BSP_LCD_RGB_BOUNCE_BUFFER_HEIGHT must divide the full frame"
#endif

static const char *TAG = "BSP-UEDX48480040E-WB-A";

static bool s_i2c_initialized = false;
static i2c_master_bus_handle_t s_i2c_bus = NULL;
static bool s_brightness_initialized = false;

/**************************************************************************************************
 *
 * I2C
 *
 **************************************************************************************************/
esp_err_t bsp_i2c_init(void)
{
    if (s_i2c_initialized) {
        return ESP_OK;
    }

    const i2c_master_bus_config_t bus_cfg = {
        .i2c_port = BSP_I2C_NUM,
        .sda_io_num = BSP_I2C_SDA,
        .scl_io_num = BSP_I2C_SCL,
        .clk_source = I2C_CLK_SRC_DEFAULT,
        .glitch_ignore_cnt = 7,
        .flags.enable_internal_pullup = true,
    };
    BSP_ERROR_CHECK_RETURN_ERR(i2c_new_master_bus(&bus_cfg, &s_i2c_bus));
    s_i2c_initialized = true;
    ESP_LOGD(TAG, "I2C initialized (SDA=%d SCL=%d)", BSP_I2C_SDA, BSP_I2C_SCL);
    return ESP_OK;
}

esp_err_t bsp_i2c_deinit(void)
{
    if (!s_i2c_initialized) {
        return ESP_OK;
    }
    BSP_ERROR_CHECK_RETURN_ERR(i2c_del_master_bus(s_i2c_bus));
    s_i2c_bus = NULL;
    s_i2c_initialized = false;
    return ESP_OK;
}

i2c_master_bus_handle_t bsp_i2c_get_handle(void)
{
    bsp_i2c_init();
    return s_i2c_bus;
}

/**************************************************************************************************
 *
 * Backlight (LEDC PWM)
 *
 **************************************************************************************************/
esp_err_t bsp_display_brightness_init(void)
{
    if (s_brightness_initialized) {
        return ESP_OK;
    }
    if (BSP_LCD_BACKLIGHT == GPIO_NUM_NC) {
        ESP_LOGW(TAG, "Backlight GPIO is NC");
        return ESP_ERR_NOT_SUPPORTED;
    }

    const ledc_timer_config_t timer_cfg = {
        .speed_mode = LEDC_LOW_SPEED_MODE,
        .duty_resolution = LEDC_TIMER_10_BIT,
        .timer_num = LEDC_TIMER_0,
        .freq_hz = 5000,
        .clk_cfg = LEDC_AUTO_CLK,
    };
    BSP_ERROR_CHECK_RETURN_ERR(ledc_timer_config(&timer_cfg));

    const ledc_channel_config_t ch_cfg = {
        .gpio_num = BSP_LCD_BACKLIGHT,
        .speed_mode = LEDC_LOW_SPEED_MODE,
        .channel = CONFIG_BSP_DISPLAY_BRIGHTNESS_LEDC_CH,
        .intr_type = LEDC_INTR_DISABLE,
        .timer_sel = LEDC_TIMER_0,
        .duty = 0,
        .hpoint = 0,
    };
    BSP_ERROR_CHECK_RETURN_ERR(ledc_channel_config(&ch_cfg));
    s_brightness_initialized = true;
    return ESP_OK;
}

esp_err_t bsp_display_brightness_set(int brightness_percent)
{
    if (!s_brightness_initialized) {
        ESP_RETURN_ON_ERROR(bsp_display_brightness_init(), TAG, "brightness init");
    }
    if (brightness_percent > 100) {
        brightness_percent = 100;
    }
    if (brightness_percent < 0) {
        brightness_percent = 0;
    }

    const uint32_t duty = (1023U * (uint32_t)brightness_percent) / 100U;
    BSP_ERROR_CHECK_RETURN_ERR(ledc_set_duty(LEDC_LOW_SPEED_MODE, CONFIG_BSP_DISPLAY_BRIGHTNESS_LEDC_CH, duty));
    BSP_ERROR_CHECK_RETURN_ERR(ledc_update_duty(LEDC_LOW_SPEED_MODE, CONFIG_BSP_DISPLAY_BRIGHTNESS_LEDC_CH));
    ESP_LOGD(TAG, "Backlight %d%%", brightness_percent);
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

/**************************************************************************************************
 *
 * Display (GC9503 RGB + 3-wire SPI)
 *
 **************************************************************************************************/
esp_err_t bsp_display_new(const bsp_display_config_t *config, esp_lcd_panel_handle_t *ret_panel,
                          esp_lcd_panel_io_handle_t *ret_io)
{
    (void)config;
    BSP_NULL_CHECK(ret_panel, ESP_ERR_INVALID_ARG);

    esp_err_t ret = ESP_OK;
    esp_lcd_panel_io_handle_t io_handle = NULL;
    esp_lcd_panel_handle_t panel_handle = NULL;

    ESP_RETURN_ON_ERROR(bsp_display_brightness_init(), TAG, "brightness init");

    ESP_LOGD(TAG, "Install 3-wire SPI panel IO");
    spi_line_config_t line_config = {
        .cs_io_type = IO_TYPE_GPIO,
        .cs_gpio_num = BSP_LCD_SPI_CS,
        .scl_io_type = IO_TYPE_GPIO,
        .scl_gpio_num = BSP_LCD_SPI_SCK,
        .sda_io_type = IO_TYPE_GPIO,
        .sda_gpio_num = BSP_LCD_SPI_MOSI,
        .io_expander = NULL,
    };
    esp_lcd_panel_io_3wire_spi_config_t io_config =
        GC9503_PANEL_IO_3WIRE_SPI_CONFIG(line_config, BSP_LCD_SPI_SCL_ACTIVE_EDGE);
    ESP_GOTO_ON_ERROR(esp_lcd_new_panel_io_3wire_spi(&io_config, &io_handle), err, TAG, "3-wire SPI IO failed");

    ESP_LOGI(TAG, "Initialize GC9503 RGB panel %dx%d", BSP_LCD_H_RES, BSP_LCD_V_RES);
    esp_lcd_rgb_panel_config_t rgb_config = {
        .clk_src = LCD_CLK_SRC_DEFAULT,
        .psram_trans_align = 64,
        .data_width = 16,
        .bits_per_pixel = 16,
        .num_fbs = CONFIG_BSP_LCD_RGB_BUFFER_NUMS,
        .bounce_buffer_size_px = CONFIG_BSP_LCD_RGB_BOUNCE_BUFFER_HEIGHT * BSP_LCD_H_RES,
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
        },
        .timings = BSP_LCD_480_480_PANEL_60HZ_RGB_TIMING(),
        .flags.fb_in_psram = 1,
    };

    gc9503_vendor_config_t vendor_config = {
        .rgb_config = &rgb_config,
        .flags = {
            /* Release 3-wire SPI so GPIO47 can be reused as SD CS. */
            .auto_del_panel_io = 1,
        },
    };
    const esp_lcd_panel_dev_config_t panel_config = {
        .reset_gpio_num = GPIO_NUM_NC,
        .rgb_ele_order = BSP_LCD_COLOR_SPACE,
        .bits_per_pixel = BSP_LCD_PANEL_BPP,
        .vendor_config = &vendor_config,
    };

    ESP_GOTO_ON_ERROR(esp_lcd_new_panel_gc9503(io_handle, &panel_config, &panel_handle), err, TAG,
                      "GC9503 init failed");
    ESP_GOTO_ON_ERROR(esp_lcd_panel_reset(panel_handle), err, TAG, "panel reset failed");
    ESP_GOTO_ON_ERROR(esp_lcd_panel_init(panel_handle), err, TAG, "panel init failed");

    /* 3-wire SPI IO is deleted inside the vendor driver. */
    io_handle = NULL;
    *ret_panel = panel_handle;
    if (ret_io) {
        *ret_io = NULL;
    }
    return ESP_OK;

err:
    if (panel_handle) {
        esp_lcd_panel_del(panel_handle);
    }
    if (io_handle) {
        esp_lcd_panel_io_del(io_handle);
    }
    return ret;
}

/**************************************************************************************************
 *
 * Touch (FT5x06)
 *
 **************************************************************************************************/
esp_err_t bsp_touch_new(const bsp_touch_config_t *config, esp_lcd_touch_handle_t *ret_touch)
{
    (void)config;
    BSP_NULL_CHECK(ret_touch, ESP_ERR_INVALID_ARG);

    BSP_ERROR_CHECK_RETURN_ERR(bsp_i2c_init());
    vTaskDelay(pdMS_TO_TICKS(150));

    esp_lcd_panel_io_handle_t tp_io = NULL;
    esp_lcd_panel_io_i2c_config_t tp_io_config = ESP_LCD_TOUCH_IO_I2C_FT5x06_CONFIG();
    tp_io_config.scl_speed_hz = CONFIG_BSP_I2C_CLK_SPEED_HZ;
    BSP_ERROR_CHECK_RETURN_ERR(esp_lcd_new_panel_io_i2c(s_i2c_bus, &tp_io_config, &tp_io));

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
    esp_err_t err = esp_lcd_touch_new_i2c_ft5x06(tp_io, &tp_cfg, ret_touch);
    if (err != ESP_OK) {
        esp_lcd_panel_io_del(tp_io);
        return err;
    }
    ESP_LOGI(TAG, "FT5x06 touch ready");
    return ESP_OK;
}

#if (BSP_CONFIG_NO_GRAPHIC_LIB == 0)

/**************************************************************************************************
 *
 * LVGL port (espressif/esp_lvgl_adapter)
 *
 **************************************************************************************************/
static lv_display_t *s_lv_disp = NULL;
static lv_indev_t *s_lv_indev = NULL;

static void bsp_display_adapter_defaults(esp_lv_adapter_config_t *cfg)
{
    esp_lv_adapter_config_t def = ESP_LV_ADAPTER_DEFAULT_CONFIG();
    *cfg = def;
    cfg->task_stack_size = 32 * 1024;
    cfg->task_priority = 7;
    cfg->task_core_id = 0;
    cfg->stack_in_psram = false;
}

lv_display_t *bsp_display_start(void)
{
    return bsp_display_start_with_config(NULL);
}

lv_display_t *bsp_display_start_with_config(const bsp_display_cfg_t *cfg)
{
    bsp_display_cfg_t local = {0};
    if (cfg == NULL) {
        bsp_display_adapter_defaults(&local.lv_adapter_cfg);
        cfg = &local;
    } else if (cfg->lv_adapter_cfg.task_stack_size == 0) {
        local = *cfg;
        bsp_display_adapter_defaults(&local.lv_adapter_cfg);
        cfg = &local;
    }

    if (s_lv_disp != NULL) {
        return s_lv_disp;
    }

    esp_lcd_panel_handle_t panel = NULL;
    esp_lcd_panel_io_handle_t io = NULL;
    if (bsp_display_new(NULL, &panel, &io) != ESP_OK) {
        return NULL;
    }
    (void)bsp_display_backlight_on();

    if (cfg->flags.mount_sd) {
        vTaskDelay(pdMS_TO_TICKS(150));
        esp_err_t sd_ret = bsp_sdcard_mount();
        if (sd_ret != ESP_OK) {
            ESP_LOGW(TAG, "SD mount failed: %s", esp_err_to_name(sd_ret));
        }
    }

    if (esp_lv_adapter_init(&cfg->lv_adapter_cfg) != ESP_OK) {
        ESP_LOGE(TAG, "esp_lv_adapter_init failed");
        return NULL;
    }

    esp_lv_adapter_display_config_t disp_cfg = ESP_LV_ADAPTER_DISPLAY_CONFIG(
        panel, io,
        ESP_LV_ADAPTER_DISPLAY_PROFILE_RGB_DEFAULT_CONFIG(
            BSP_LCD_H_RES, BSP_LCD_V_RES, ESP_LV_ADAPTER_ROTATE_0),
        ESP_LV_ADAPTER_TEAR_AVOID_MODE_DEFAULT_RGB, ESP_LV_ADAPTER_TE_SYNC_DISABLED());
    s_lv_disp = esp_lv_adapter_register_display(&disp_cfg);
    if (s_lv_disp == NULL) {
        ESP_LOGE(TAG, "register display failed");
        return NULL;
    }

    esp_lcd_touch_handle_t tp = NULL;
    if (bsp_touch_new(NULL, &tp) == ESP_OK) {
        const esp_lv_adapter_touch_config_t touch_cfg =
            ESP_LV_ADAPTER_TOUCH_DEFAULT_CONFIG(s_lv_disp, tp);
        s_lv_indev = esp_lv_adapter_register_touch(&touch_cfg);
        if (s_lv_indev == NULL) {
            ESP_LOGW(TAG, "Touch register failed");
        }
    } else {
        ESP_LOGW(TAG, "Continue without touch");
    }

    if (esp_lv_adapter_start() != ESP_OK) {
        ESP_LOGE(TAG, "esp_lv_adapter_start failed");
        s_lv_disp = NULL;
        s_lv_indev = NULL;
        return NULL;
    }

    ESP_LOGI(TAG, "LVGL started %dx%d (esp_lvgl_adapter)", BSP_LCD_H_RES, BSP_LCD_V_RES);
    return s_lv_disp;
}

lv_indev_t *bsp_display_get_input_dev(void)
{
    return s_lv_indev;
}

bool bsp_display_lock(uint32_t timeout_ms)
{
    const int32_t t = (timeout_ms == 0) ? -1 : (int32_t)timeout_ms;
    return esp_lv_adapter_lock(t) == ESP_OK;
}

void bsp_display_unlock(void)
{
    esp_lv_adapter_unlock();
}

#endif /* BSP_CONFIG_NO_GRAPHIC_LIB == 0 */
