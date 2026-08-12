#pragma once

#include "esp_err.h"
#include "esp_lcd_panel_ops.h"
#include "esp_lcd_panel_io.h"
#include "esp_lcd_touch.h"
#include "esp_lv_adapter.h"
#include "bsp_knob_15_md50et_board.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    lv_display_t *disp;
    lv_indev_t *touch;
    esp_lcd_panel_handle_t panel;
    esp_lcd_panel_io_handle_t panel_io;
    esp_lcd_touch_handle_t tp;
} bsp_knob_15_md50et_handles_t;

/** 旋钮事件回调（参数为 knob_event_t 强转的 void*，与 iot_knob 一致） */
typedef void (*bsp_knob_15_md50et_knob_cb_t)(void *event);

/** 按键事件回调（参数为 button_event_t 强转的 void*，与 iot_button 一致） */
typedef void (*bsp_knob_15_md50et_button_cb_t)(void *event);

/**
 * 初始化 1.5" 旋钮屏板级资源：
 * CO5300 QSPI LCD + CST820 触摸 + esp_lvgl_adapter（兼容 LVGL 8 / 9）+ 旋钮/按键。
 * 成功后 LVGL 任务已启动，可直接在 lock 下绘制。
 *
 * LVGL 主版本由工程 `idf_component.yml` 中的 `lvgl/lvgl` 约束决定（^8 或 ^9）。
 */
esp_err_t bsp_knob_15_md50et_init(bsp_knob_15_md50et_handles_t *out_handles);

esp_err_t bsp_knob_15_md50et_lock(int timeout_ms);
void bsp_knob_15_md50et_unlock(void);

void bsp_knob_15_md50et_backlight_on(void);
void bsp_knob_15_md50et_backlight_off(void);

/** 设置面板亮度 0~100（经 CO5300 驱动，QSPI 安全） */
esp_err_t bsp_knob_15_md50et_set_brightness(uint8_t percent);

void bsp_knob_15_md50et_register_knob_cb(bsp_knob_15_md50et_knob_cb_t cb);
void bsp_knob_15_md50et_register_button_cb(bsp_knob_15_md50et_button_cb_t cb);

#ifdef __cplusplus
}
#endif
