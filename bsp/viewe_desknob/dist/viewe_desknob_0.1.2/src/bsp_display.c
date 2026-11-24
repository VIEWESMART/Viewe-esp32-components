/*
 * SPDX-FileCopyrightText: 2026 VIEWE
 * SPDX-License-Identifier: Apache-2.0
 */

#include "bsp/viewe_desknob.h"

#include "bsp_err_check.h"
#include "driver/ledc.h"
#include "driver/spi_master.h"
#include "esp_lcd_gc9b72.h"
#include "esp_lcd_panel_ops.h"
#include "esp_lcd_touch_cst826.h"
#include "esp_log.h"

static const char *TAG = "viewe_desknob.disp";

static esp_lcd_panel_handle_t s_panel;
static esp_lcd_panel_io_handle_t s_panel_io;
static bool s_spi_bus_ready;
static i2c_master_bus_handle_t s_touch_bus;
static esp_lcd_panel_io_handle_t s_touch_io;
static esp_lcd_touch_handle_t s_touch;
static lv_display_t *s_disp;
static lv_indev_t *s_indev;
static bool s_brightness_ready;

static esp_err_t bsp_touch_i2c_init(void)
{
    if (s_touch_bus) {
        return ESP_OK;
    }
    const i2c_master_bus_config_t cfg = {
        .clk_source = I2C_CLK_SRC_DEFAULT,
        .i2c_port = BSP_TOUCH_I2C_NUM,
        .scl_io_num = BSP_TOUCH_I2C_SCL,
        .sda_io_num = BSP_TOUCH_I2C_SDA,
        .glitch_ignore_cnt = 7,
        .flags.enable_internal_pullup = true,
    };
    BSP_ERROR_CHECK_RETURN_ERR(i2c_new_master_bus(&cfg, &s_touch_bus));
    ESP_LOGI(TAG, "Touch I2C port=%d SCL=%d SDA=%d", (int)BSP_TOUCH_I2C_NUM,
             BSP_TOUCH_I2C_SCL, BSP_TOUCH_I2C_SDA);
    return ESP_OK;
}

static void bsp_display_cleanup_temp(esp_lcd_panel_handle_t panel, esp_lcd_panel_io_handle_t io, bool free_bus)
{
    if (panel) {
        esp_lcd_panel_del(panel);
    }
    if (io) {
        esp_lcd_panel_io_del(io);
    }
    if (free_bus) {
        spi_bus_free(BSP_LCD_SPI_NUM);
        s_spi_bus_ready = false;
    }
}

esp_err_t bsp_display_brightness_init(void)
{
    if (s_brightness_ready) {
        return ESP_OK;
    }

    const ledc_timer_config_t timer = {
        .speed_mode = LEDC_LOW_SPEED_MODE,
        .duty_resolution = LEDC_TIMER_10_BIT,
        .timer_num = LEDC_TIMER_0,
        .freq_hz = 5000,
        .clk_cfg = LEDC_AUTO_CLK,
    };
    BSP_ERROR_CHECK_RETURN_ERR(ledc_timer_config(&timer));

    const ledc_channel_config_t channel = {
        .gpio_num = BSP_LCD_BACKLIGHT,
        .speed_mode = LEDC_LOW_SPEED_MODE,
        .channel = CONFIG_BSP_LCD_BRIGHTNESS_LEDC_CH,
        .intr_type = LEDC_INTR_DISABLE,
        .timer_sel = LEDC_TIMER_0,
        .duty = 0,
        .hpoint = 0,
    };
    BSP_ERROR_CHECK_RETURN_ERR(ledc_channel_config(&channel));
    s_brightness_ready = true;
    return ESP_OK;
}

esp_err_t bsp_display_brightness_set(int brightness_percent)
{
    if (brightness_percent < 0) {
        brightness_percent = 0;
    } else if (brightness_percent > 100) {
        brightness_percent = 100;
    }
    BSP_ERROR_CHECK_RETURN_ERR(bsp_display_brightness_init());

    const uint32_t duty = (1023 * (uint32_t)brightness_percent) / 100;
    BSP_ERROR_CHECK_RETURN_ERR(ledc_set_duty(LEDC_LOW_SPEED_MODE, CONFIG_BSP_LCD_BRIGHTNESS_LEDC_CH, duty));
    return ledc_update_duty(LEDC_LOW_SPEED_MODE, CONFIG_BSP_LCD_BRIGHTNESS_LEDC_CH);
}

esp_err_t bsp_display_backlight_on(void)
{
    return bsp_display_brightness_set(100);
}

esp_err_t bsp_display_backlight_off(void)
{
    return bsp_display_brightness_set(0);
}

esp_err_t bsp_display_new(const bsp_display_config_t *config, esp_lcd_panel_handle_t *ret_panel,
                          esp_lcd_panel_io_handle_t *ret_io)
{
    BSP_ERROR_CHECK_RETURN_ERR(ret_panel ? ESP_OK : ESP_ERR_INVALID_ARG);
    if (s_panel) {
        *ret_panel = s_panel;
        if (ret_io) {
            *ret_io = s_panel_io;
        }
        return ESP_OK;
    }

    BSP_ERROR_CHECK_RETURN_ERR(bsp_display_brightness_set(0));

    const int max_transfer = (config && config->max_transfer_sz > 0)
                             ? config->max_transfer_sz
                             : BSP_LCD_SPI_MAX_TRANSFER_SZ;

    esp_lcd_panel_io_handle_t io = NULL;
    esp_lcd_panel_handle_t panel = NULL;
    bool bus_owned = false;
    esp_err_t err = ESP_OK;

    if (!s_spi_bus_ready) {
        const spi_bus_config_t buscfg = GC9B72_PANEL_BUS_QSPI_CONFIG(
            BSP_LCD_PCLK, BSP_LCD_DATA0, BSP_LCD_DATA1, BSP_LCD_DATA2, BSP_LCD_DATA3, max_transfer);
        err = spi_bus_initialize(BSP_LCD_SPI_NUM, &buscfg, SPI_DMA_CH_AUTO);
        if (err != ESP_OK) {
            return err;
        }
        s_spi_bus_ready = true;
        bus_owned = true;
    }

    const esp_lcd_panel_io_spi_config_t io_cfg = GC9B72_PANEL_IO_QSPI_CONFIG(BSP_LCD_CS, NULL, NULL);
    err = esp_lcd_new_panel_io_spi((esp_lcd_spi_bus_handle_t)BSP_LCD_SPI_NUM, &io_cfg, &io);
    if (err != ESP_OK) {
        bsp_display_cleanup_temp(NULL, NULL, bus_owned);
        return err;
    }

    gc9b72_vendor_config_t vendor = {
        .init_cmds = NULL,
        .init_cmds_size = 0,
        .flags.use_qspi_interface = 1,
    };
    const esp_lcd_panel_dev_config_t panel_cfg = {
        .reset_gpio_num = BSP_LCD_RST,
        .rgb_ele_order = LCD_RGB_ELEMENT_ORDER_RGB,
        .bits_per_pixel = BSP_LCD_BITS_PER_PIXEL,
        .vendor_config = &vendor,
    };
    err = esp_lcd_new_panel_gc9b72(io, &panel_cfg, &panel);
    if (err != ESP_OK) {
        bsp_display_cleanup_temp(NULL, io, bus_owned);
        return err;
    }
    err = esp_lcd_panel_reset(panel);
    if (err != ESP_OK) {
        bsp_display_cleanup_temp(panel, io, bus_owned);
        return err;
    }
    err = esp_lcd_panel_init(panel);
    if (err != ESP_OK) {
        bsp_display_cleanup_temp(panel, io, bus_owned);
        return err;
    }
    err = esp_lcd_panel_disp_on_off(panel, true);
    if (err != ESP_OK) {
        bsp_display_cleanup_temp(panel, io, bus_owned);
        return err;
    }

    s_panel = panel;
    s_panel_io = io;
    *ret_panel = s_panel;
    if (ret_io) {
        *ret_io = s_panel_io;
    }
    ESP_LOGI(TAG, "GC9B72 %dx%d QSPI ready (max_transfer=%d)", BSP_LCD_H_RES, BSP_LCD_V_RES, max_transfer);
    return ESP_OK;
}

esp_err_t bsp_touch_new(const bsp_touch_config_t *config, esp_lcd_touch_handle_t *ret_touch)
{
    (void)config;
    BSP_ERROR_CHECK_RETURN_ERR(ret_touch ? ESP_OK : ESP_ERR_INVALID_ARG);
    if (s_touch) {
        *ret_touch = s_touch;
        return ESP_OK;
    }

    BSP_ERROR_CHECK_RETURN_ERR(bsp_touch_i2c_init());

    const esp_lcd_touch_config_t touch_cfg = {
        .x_max = BSP_LCD_H_RES,
        .y_max = BSP_LCD_V_RES,
        .rst_gpio_num = BSP_LCD_TOUCH_RST,
        .int_gpio_num = BSP_LCD_TOUCH_INT,
        .levels.reset = 0,
        .levels.interrupt = 0,
        .flags.swap_xy = 0,
        .flags.mirror_x = 1,
        .flags.mirror_y = 0,
    };
    esp_lcd_panel_io_i2c_config_t io_cfg = ESP_LCD_TOUCH_IO_I2C_CST826_CONFIG();
    io_cfg.scl_speed_hz = 400000;

    esp_lcd_panel_io_handle_t touch_io = NULL;
    esp_lcd_touch_handle_t touch = NULL;
    esp_err_t err = esp_lcd_new_panel_io_i2c(s_touch_bus, &io_cfg, &touch_io);
    if (err != ESP_OK) {
        return err;
    }
    err = esp_lcd_touch_new_i2c_cst826(touch_io, &touch_cfg, &touch);
    if (err != ESP_OK) {
        esp_lcd_panel_io_del(touch_io);
        return err;
    }

    s_touch_io = touch_io;
    s_touch = touch;
    *ret_touch = s_touch;
    ESP_LOGI(TAG, "CST826 ready");
    return ESP_OK;
}

lv_display_t *bsp_display_start(void)
{
    return bsp_display_start_with_config(NULL);
}

lv_display_t *bsp_display_start_with_config(const bsp_display_cfg_t *cfg)
{
    if (s_disp) {
        return s_disp;
    }

    /* Official Brookesia/SPI path: DMA stripe in internal SRAM (no PSRAM bounce). */
    bsp_display_cfg_t local = {
        .lvgl_port_cfg = ESP_LV_ADAPTER_DEFAULT_CONFIG(),
        .buffer_size = BSP_LCD_DRAW_BUFF_SIZE,
        .double_buffer = false,
        .flags.buff_dma = true,
        .flags.buff_spiram = false,
    };
    if (cfg) {
        local = *cfg;
        if (local.buffer_size == 0) {
            local.buffer_size = BSP_LCD_DRAW_BUFF_SIZE;
        }
    }

    const uint16_t buffer_height = (uint16_t)(local.buffer_size / BSP_LCD_H_RES);
    const int max_transfer = (int)(local.buffer_size * (BSP_LCD_BITS_PER_PIXEL / 8));
    const bsp_display_config_t hw_cfg = {
        .max_transfer_sz = max_transfer > 0 ? max_transfer : BSP_LCD_SPI_MAX_TRANSFER_SZ,
    };

    esp_lcd_panel_handle_t panel = NULL;
    esp_lcd_panel_io_handle_t panel_io = NULL;
    if (bsp_display_new(&hw_cfg, &panel, &panel_io) != ESP_OK) {
        return NULL;
    }
    if (!esp_lv_adapter_is_initialized() && esp_lv_adapter_init(&local.lvgl_port_cfg) != ESP_OK) {
        ESP_LOGE(TAG, "LVGL adapter init failed");
        return NULL;
    }

    esp_lv_adapter_display_config_t display_cfg;
    if (local.flags.buff_spiram) {
        display_cfg = ESP_LV_ADAPTER_DISPLAY_SPI_WITH_PSRAM_DEFAULT_CONFIG(
                panel, panel_io, BSP_LCD_H_RES, BSP_LCD_V_RES, ESP_LV_ADAPTER_ROTATE_0);
    } else {
        display_cfg = ESP_LV_ADAPTER_DISPLAY_SPI_WITHOUT_PSRAM_DEFAULT_CONFIG(
                panel, panel_io, BSP_LCD_H_RES, BSP_LCD_V_RES, ESP_LV_ADAPTER_ROTATE_0);
    }
    display_cfg.profile.buffer_height = buffer_height ? buffer_height : BSP_LCD_DRAW_BUFF_HEIGHT;
    display_cfg.profile.use_psram = local.flags.buff_spiram;
    display_cfg.profile.require_double_buffer = local.double_buffer;

    lv_display_t *disp = esp_lv_adapter_register_display(&display_cfg);
    if (!disp) {
        ESP_LOGE(TAG, "register display failed");
        return NULL;
    }

    if (bsp_touch_new(NULL, &s_touch) == ESP_OK) {
        esp_lv_adapter_touch_config_t touch_cfg = ESP_LV_ADAPTER_TOUCH_DEFAULT_CONFIG(disp, s_touch);
        s_indev = esp_lv_adapter_register_touch(&touch_cfg);
        if (!s_indev) {
            ESP_LOGW(TAG, "touch was not registered with LVGL");
        }
    } else {
        ESP_LOGW(TAG, "touch init failed, display continues");
    }

    if (esp_lv_adapter_start() != ESP_OK) {
        ESP_LOGE(TAG, "LVGL adapter start failed");
        s_disp = NULL;
        s_indev = NULL;
        return NULL;
    }

    s_disp = disp;
    ESP_LOGI(TAG, "LVGL ready: buffer_height=%u use_psram=%u max_transfer=%d",
             display_cfg.profile.buffer_height, display_cfg.profile.use_psram ? 1U : 0U, hw_cfg.max_transfer_sz);
    return s_disp;
}

lv_indev_t *bsp_display_get_input_dev(void)
{
    return s_indev;
}

bool bsp_display_lock(uint32_t timeout_ms)
{
    const int32_t wait = (timeout_ms == 0) ? -1 : (int32_t)timeout_ms;
    return esp_lv_adapter_lock(wait) == ESP_OK;
}

void bsp_display_unlock(void)
{
    esp_lv_adapter_unlock();
}

void bsp_display_rotate(lv_display_t *disp, lv_display_rotation_t rotation)
{
    if (!disp) {
        return;
    }
    bsp_display_lock(0);
    lv_display_set_rotation(disp, rotation);
    bsp_display_unlock();
}
