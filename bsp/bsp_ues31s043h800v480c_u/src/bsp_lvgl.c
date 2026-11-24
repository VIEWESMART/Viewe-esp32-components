/*
 * SPDX-FileCopyrightText: 2026 VIEWE
 * SPDX-License-Identifier: Apache-2.0
 *
 * Locked LVGL path: 2 PSRAM FBs, direct mode, swap on VSYNC.
 * Bounce 20 and 22 MHz PCLK. A panel opened with 1 FB falls back to
 * 40-line partial buffers.
 */

#include "sdkconfig.h"
#include "esp_check.h"
#include "esp_log.h"
#include "bsp/esp-bsp.h"
#include "bsp_err_check.h"

static const char *TAG = "BSP-LVGL";

static lv_display_t *s_disp;
static lv_indev_t *s_indev;

static bool bsp_panel_has_two_fbs(void)
{
    void *fb0 = NULL;
    void *fb1 = NULL;
    if (bsp_display_get_framebuffers(&fb0, &fb1) != ESP_OK) {
        return false;
    }
    return fb0 != NULL && fb1 != NULL;
}

static lv_display_t *bsp_display_lcd_init(const bsp_display_cfg_t *cfg)
{
    esp_lcd_panel_handle_t panel = bsp_display_get_panel();
    if (panel == NULL) {
        /* Two FBs: LVGL paints the hidden one, RGB scans the other, swap on VSYNC. */
        const bsp_display_config_t hw_cfg = {
            .num_fbs = BSP_LCD_FB_COUNT,
            .bounce_lines = BSP_LCD_BOUNCE_BUFFER_HEIGHT,
        };
        BSP_ERROR_CHECK_RETURN_NULL(bsp_display_new(&hw_cfg, &panel, NULL));
    }
    const bool avoid_tear = bsp_panel_has_two_fbs();

    lvgl_port_display_cfg_t disp_cfg = {
        .io_handle = NULL,
        .panel_handle = panel,
        .buffer_size = cfg->buffer_size,
        .double_buffer = cfg->double_buffer,
        .hres = BSP_LCD_H_RES,
        .vres = BSP_LCD_V_RES,
        .monochrome = false,
        .rotation = {
            .swap_xy = false,
            .mirror_x = false,
            .mirror_y = false,
        },
#if LVGL_VERSION_MAJOR >= 9
        .color_format = LV_COLOR_FORMAT_RGB888,
#endif
        .flags = {
            .buff_dma = cfg->flags.buff_dma,
            .buff_spiram = cfg->flags.buff_spiram,
            .direct_mode = avoid_tear,
        },
    };
    const lvgl_port_display_rgb_cfg_t rgb_cfg = {
        .flags = {
            .bb_mode = BSP_LCD_BOUNCE_BUFFER_HEIGHT > 0,
            .avoid_tearing = avoid_tear,
        },
    };
    lv_display_t *disp = lvgl_port_add_disp_rgb(&disp_cfg, &rgb_cfg);
    if (disp) {
        ESP_LOGI(TAG, "LVGL port RGB888 bounce, %s, %d-line draw buf",
                 avoid_tear ? "2 FB direct, VSYNC swap" : "1 FB partial",
                 CONFIG_BSP_LCD_DRAW_BUF_HEIGHT);
    }
    return disp;
}

static lv_indev_t *bsp_display_indev_init(lv_display_t *disp)
{
    esp_lcd_touch_handle_t touch = NULL;
    BSP_ERROR_CHECK_RETURN_NULL(bsp_touch_new(NULL, &touch));
    const lvgl_port_touch_cfg_t touch_cfg = {
        .disp = disp,
        .handle = touch,
    };
    return lvgl_port_add_touch(&touch_cfg);
}

lv_display_t *bsp_display_start(void)
{
    bsp_display_cfg_t cfg = {
        .lvgl_port_cfg = ESP_LVGL_PORT_INIT_CONFIG(),
        .buffer_size = BSP_LCD_H_RES * CONFIG_BSP_LCD_DRAW_BUF_HEIGHT,
        .double_buffer = CONFIG_BSP_LCD_DRAW_BUF_DOUBLE,
        .flags = {
            .buff_dma = false,
            .buff_spiram = true,
        },
    };
    cfg.lvgl_port_cfg.task_stack = 24576;
    cfg.lvgl_port_cfg.task_affinity = 0;
    return bsp_display_start_with_config(&cfg);
}

lv_display_t *bsp_display_start_with_config(const bsp_display_cfg_t *cfg)
{
    BSP_NULL_CHECK(cfg, NULL);
    if (s_disp) {
        return s_disp;
    }

    BSP_ERROR_CHECK_RETURN_NULL(lvgl_port_init(&cfg->lvgl_port_cfg));
    s_disp = bsp_display_lcd_init(cfg);
    BSP_NULL_CHECK(s_disp, NULL);
    s_indev = bsp_display_indev_init(s_disp);
    if (s_indev == NULL) {
        ESP_LOGW(TAG, "LVGL touch indev not added");
    }

    BSP_ERROR_CHECK_RETURN_NULL(bsp_display_backlight_on());
    return s_disp;
}

lv_indev_t *bsp_display_get_input_dev(void)
{
    return s_indev;
}

bool bsp_display_lock(uint32_t timeout_ms)
{
    return lvgl_port_lock(timeout_ms);
}

void bsp_display_unlock(void)
{
    lvgl_port_unlock();
}

esp_err_t bsp_display_enter_sleep(void)
{
    return bsp_display_backlight_off();
}

esp_err_t bsp_display_exit_sleep(void)
{
    return bsp_display_backlight_on();
}

void bsp_display_rotate(lv_display_t *disp, lv_display_rotation_t rotation)
{
    if (disp == NULL) {
        disp = s_disp;
    }
    if (disp == NULL) {
        return;
    }
    if (bsp_display_lock(0)) {
        lv_display_set_rotation(disp, rotation);
        bsp_display_unlock();
    }
}
