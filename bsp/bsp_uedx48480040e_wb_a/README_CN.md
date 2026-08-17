# BSP: UEDX48480040E-WB-A

| [如何使用 API](#如何使用-api) | [![Component Registry](https://components.espressif.com/components/viewesmart/bsp_uedx48480040e_wb_a/badge.svg)](https://components.espressif.com/components/viewesmart/bsp_uedx48480040e_wb_a) |
| --- | --- |

[English](./README.md)

## 概述

**UEDX48480040E-WB-A** 板级支持包（BSP）：基于 ESP32-S3 的 4 寸 480×480 RGB 屏模组。公开 API 对齐 [Espressif ESP-BSP](https://github.com/espressif/esp-bsp)。

* MCU：ESP32-S3（建议 16 MB Flash + Octal PSRAM）
* 显示：GC9503，RGB565 + 3-wire SPI 初始化
* 分辨率：480 × 480
* 触摸：FT5x06，I2C（`0x38`）
* 背光：GPIO38，LEDC PWM
* SD 卡：SDSPI（CS 与 LCD 3-wire SDA 共用 GPIO47）
* GUI：`esp_lvgl_adapter`，支持 LVGL 8.x 或 9.x

| 项目 | 数值 |
| ---- | ---- |
| LCD 接口 | RGB565 + 3-wire SPI 初始化 |
| 触摸接口 | I2C（`I2C_NUM_0`，SDA=GPIO40，SCL=GPIO41） |
| ESP-IDF | >= 5.5 |
| Target | esp32s3 |

> [!NOTE]
> * GPIO47 在面板初始化期间是 LCD 3-wire SDA，随后由 `auto_del_panel_io` 释放并复用为 SD CS。必须先完成 `bsp_display_new()` / `bsp_display_start()` 再挂载 SD。
> * 硬件 RGB 数据线相对 ESP RGB565 位序存在 R/B 对调，BSP 引脚表已处理。
> * 请在应用的 `idf_component.yml` 中锁定 LVGL 主版本（`^8` 或 `^9`），以匹配 UI。

## 能力与依赖

| 可用 | 能力 | 控制器 | 组件 | 版本 |
| :--: | ---- | ------ | ---- | ---- |
| :heavy_check_mark: | DISPLAY | GC9503 | [espressif/esp_lcd_gc9503](https://components.espressif.com/components/espressif/esp_lcd_gc9503) | ^3 |
| :heavy_check_mark: | LVGL_PORT | | [espressif/esp_lvgl_adapter](https://components.espressif.com/components/espressif/esp_lvgl_adapter) | ^0.5.3 |
| :heavy_check_mark: | TOUCH | FT5x06 | [espressif/esp_lcd_touch_ft5x06](https://components.espressif.com/components/espressif/esp_lcd_touch_ft5x06) | ^1 |
| :heavy_check_mark: | SDCARD | SDSPI | IDF `fatfs` / `sdmmc` | >=5.5 |
| :heavy_check_mark: | WIFI | | IDF `esp_wifi` | >=5.5 |
| :x: | AUDIO | | | |
| :x: | BUTTONS | | | |
| :x: | IMU | | | |

## 添加到工程

```
idf.py add-dependency "viewesmart/bsp_uedx48480040e_wb_a^1.0.0"
```

或在 `main/idf_component.yml` 中：

```yaml
dependencies:
  viewesmart/bsp_uedx48480040e_wb_a: "^1.0.0"
  lvgl/lvgl:
    version: "^9"
    public: true
```

更多信息见 [IDF Component Manager](https://docs.espressif.com/projects/esp-idf/zh_CN/latest/esp32/api-guides/tools/idf-component-manager.html)。

## 如何使用 API

```c
#include "bsp/esp-bsp.h"
```

### 显示、触摸与 LVGL

简单方式：

```c
lv_display_t *disp = bsp_display_start();
```

带配置（在 LCD 3-wire IO 释放后挂载 SD）：

```c
bsp_display_cfg_t cfg = {0};
cfg.flags.mount_sd = 1;
lv_display_t *disp = bsp_display_start_with_config(&cfg);
```

所有 LVGL API 调用须放在 lock / unlock 之间：

```c
bsp_display_lock(0);
lv_obj_t *label = lv_label_create(lv_screen_active()); /* LVGL 8 用 lv_scr_act() */
lv_label_set_text(label, "Hello");
lv_obj_center(label);
bsp_display_unlock();
```

```c
lv_indev_t *touch = bsp_display_get_input_dev();
```

### 背光

```c
bsp_display_backlight_on();
bsp_display_brightness_set(50);   /* 0~100 */
bsp_display_backlight_off();
```

### 仅硬件、不启动 LVGL

```c
esp_lcd_panel_handle_t panel;
esp_lcd_panel_io_handle_t io;
esp_lcd_touch_handle_t tp;

bsp_display_new(NULL, &panel, &io);
bsp_display_backlight_on();
bsp_touch_new(NULL, &tp);
```

之后可直接使用 [ESP-LCD](https://docs.espressif.com/projects/esp-idf/zh_CN/latest/esp32/api-reference/peripherals/lcd/index.html) 与触摸 API。

### SD 卡

CS 与 LCD 3-wire SDA 共用 GPIO47，必须先完成面板初始化：

```c
ESP_ERROR_CHECK(bsp_sdcard_mount());
FILE *f = fopen(BSP_SD_MOUNT_POINT "/hello.txt", "w");
```

### I2C

触摸初始化时会自动打开 I2C，也可自行初始化并复用总线：

```c
bsp_i2c_init();
i2c_master_bus_handle_t i2c = bsp_i2c_get_handle();
```

### Wi-Fi

```c
ESP_ERROR_CHECK(bsp_wifi_init());
ESP_ERROR_CHECK(bsp_wifi_connect("ssid", "password"));
```

### API 摘要

| 函数 | 说明 |
| ---- | ---- |
| `bsp_display_start()` / `bsp_display_start_with_config()` | 初始化 LCD、可选 SD、触摸和 LVGL |
| `bsp_display_lock(timeout_ms)` / `bsp_display_unlock()` | LVGL 线程安全锁（`0` 永久等待） |
| `bsp_display_get_input_dev()` | LVGL 触摸 indev |
| `bsp_display_new()` | 仅创建 GC9503 RGB 面板，不启动 LVGL |
| `bsp_display_backlight_on()` / `off()` / `bsp_display_brightness_set()` | 背光 PWM |
| `bsp_touch_new()` | 创建 FT5x06 触摸 |
| `bsp_sdcard_mount()` / `unmount()` | SDSPI FAT 挂载 |
| `bsp_i2c_init()` / `bsp_i2c_get_handle()` | 触摸 I2C 总线 |
| `bsp_wifi_init()` / `scan()` / `connect()` | Wi-Fi 辅助接口 |

## 引脚

定义于 `include/bsp/uedx48480040e_wb_a.h`：

| 功能 | GPIO |
| ---- | ---- |
| LCD 3-wire CS | 39 |
| LCD 3-wire SCK | 48 |
| LCD 3-wire SDA（随后作 SD CS） | 47 |
| LCD RGB PCLK / VSYNC / HSYNC / DE | 21 / 17 / 16 / 18 |
| LCD 背光 | 38 |
| 触摸 SCL / SDA | 41 / 40 |
| SD MOSI / CLK / MISO | 42 / 45 / 46 |

RGB565 数据线为 `BSP_LCD_DATA0`–`BSP_LCD_DATA15`（R0–R4，G0–G5，B0–B4）。

## 配置

`menuconfig` → **Board Support Package (UEDX48480040E-WB-A)**：

* `BSP_LCD_RGB_BUFFER_NUMS`：RGB 帧缓冲个数（默认 3）
* `BSP_LCD_RGB_BOUNCE_BUFFER_HEIGHT`：bounce buffer 行高（默认 20）
* `BSP_SD_MOUNT_POINT`：SD 挂载点（默认 `/sdcard`）

建议的 `sdkconfig.defaults`：

```text
CONFIG_IDF_TARGET="esp32s3"
CONFIG_ESPTOOLPY_FLASHSIZE_16MB=y
CONFIG_SPIRAM=y
CONFIG_SPIRAM_MODE_OCT=y
CONFIG_SPIRAM_SPEED_80M=y
CONFIG_SPIRAM_USE_MALLOC=y
CONFIG_LV_COLOR_DEPTH_16=y
```

## 排障

| 现象 | 排查 |
| ---- | ---- |
| 黑屏 | 打开 Octal PSRAM；调用 `bsp_display_backlight_on()`；保持 BSP RGB 时序 |
| SD 挂载失败 | 先初始化 LCD 以释放 GPIO47；使用 FAT32 卡 |
| 颜色异常 | 不要自行重排 RGB 数据线；BSP 已处理硬件 R/B 对调 |
| 触摸 / I2C 异常 | FT5x06 地址为 `0x38`，SDA/SCL 为 GPIO40/41 |
| LVGL API / 链接错误 | 应用 LVGL 主版本须与 UI 一致 |

## 许可证

Apache License 2.0，见 [LICENSE](./LICENSE)。
