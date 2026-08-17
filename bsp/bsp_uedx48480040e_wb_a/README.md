# BSP: UEDX48480040E-WB-A

| [HOW TO USE API](#how-to-use-api) | [![Component Registry](https://components.espressif.com/components/viewesmart/bsp_uedx48480040e_wb_a/badge.svg)](https://components.espressif.com/components/viewesmart/bsp_uedx48480040e_wb_a) |
| --- | --- |

[中文](./README_CN.md)

## Overview

Board support package for **UEDX48480040E-WB-A**, a 4-inch 480×480 RGB display module based on ESP32-S3. The public API follows [Espressif ESP-BSP](https://github.com/espressif/esp-bsp).

* MCU: ESP32-S3 (16 MB Flash + Octal PSRAM recommended)
* Display: GC9503, RGB565 + 3-wire SPI init
* Resolution: 480 × 480
* Touch: FT5x06, I2C (`0x38`)
* Backlight: GPIO38, LEDC PWM
* SD card: SDSPI (CS shares GPIO47 with LCD 3-wire SDA)
* GUI: `esp_lvgl_adapter` with LVGL 8.x or 9.x

| Item | Value |
| ---- | ----- |
| LCD interface | RGB565 + 3-wire SPI init |
| Touch interface | I2C (`I2C_NUM_0`, SDA=GPIO40, SCL=GPIO41) |
| ESP-IDF | >= 5.5 |
| Target | esp32s3 |

> [!NOTE]
> * GPIO47 is LCD 3-wire SDA during panel init, then released (`auto_del_panel_io`) and reused as SD CS. Mount the SD card only after `bsp_display_new()` / `bsp_display_start()`.
> * Hardware RGB data traces swap R/B versus ESP RGB565 bit order; the BSP pin map already accounts for this.
> * Pin the LVGL major version (`^8` or `^9`) in the application `idf_component.yml` to match your UI.

## Capabilities and dependencies

| Available | Capability | Controller | Component | Version |
| :-------: | ---------- | ---------- | --------- | ------- |
| :heavy_check_mark: | DISPLAY | GC9503 | [espressif/esp_lcd_gc9503](https://components.espressif.com/components/espressif/esp_lcd_gc9503) | ^3 |
| :heavy_check_mark: | LVGL_PORT | | [espressif/esp_lvgl_adapter](https://components.espressif.com/components/espressif/esp_lvgl_adapter) | ^0.5.3 |
| :heavy_check_mark: | TOUCH | FT5x06 | [espressif/esp_lcd_touch_ft5x06](https://components.espressif.com/components/espressif/esp_lcd_touch_ft5x06) | ^1 |
| :heavy_check_mark: | SDCARD | SDSPI | IDF `fatfs` / `sdmmc` | >=5.5 |
| :heavy_check_mark: | WIFI | | IDF `esp_wifi` | >=5.5 |
| :x: | AUDIO | | | |
| :x: | BUTTONS | | | |
| :x: | IMU | | | |

## Add to project

```
idf.py add-dependency "viewesmart/bsp_uedx48480040e_wb_a^1.0.0"
```

or in `main/idf_component.yml`:

```yaml
dependencies:
  viewesmart/bsp_uedx48480040e_wb_a: "^1.0.0"
  lvgl/lvgl:
    version: "^9"
    public: true
```

More: [IDF Component Manager](https://docs.espressif.com/projects/esp-idf/en/latest/esp32/api-guides/tools/idf-component-manager.html).

## How to use API

Include the ESP-BSP header:

```c
#include "bsp/esp-bsp.h"
```

### Display, touch and LVGL

ESP-BSP provides two ways to initialize the **display**, **touch** and **LVGL**.

Simple method:

```c
lv_display_t *disp = bsp_display_start();
```

Configurable method (mount SD after LCD 3-wire IO is released):

```c
bsp_display_cfg_t cfg = {0};
cfg.flags.mount_sd = 1;
lv_display_t *disp = bsp_display_start_with_config(&cfg);
```

All LVGL API calls must be protected with lock/unlock:

```c
bsp_display_lock(0);
lv_obj_t *label = lv_label_create(lv_screen_active()); /* LVGL 8: lv_scr_act() */
lv_label_set_text(label, "Hello");
lv_obj_center(label);
bsp_display_unlock();
```

Get the LVGL touch input device after a successful `bsp_display_start()`:

```c
lv_indev_t *touch = bsp_display_get_input_dev();
```

### Brightness

```c
bsp_display_backlight_on();
bsp_display_brightness_set(50);   /* 0~100 */
bsp_display_backlight_off();
```

### Initialization without LVGL

```c
esp_lcd_panel_handle_t panel;
esp_lcd_panel_io_handle_t io;
esp_lcd_touch_handle_t tp;

bsp_display_new(NULL, &panel, &io);
bsp_display_backlight_on();
bsp_touch_new(NULL, &tp);
```

After this, use the [ESP-LCD](https://docs.espressif.com/projects/esp-idf/en/latest/esp32/api-reference/peripherals/lcd/index.html) and [ESP-LCD Touch](https://components.espressif.com/components/espressif/esp_lcd_touch) APIs.

### SD card

CS shares GPIO47 with LCD 3-wire SDA. Call `bsp_sdcard_mount()` only after `bsp_display_new()` / `bsp_display_start()`.

```c
ESP_ERROR_CHECK(bsp_sdcard_mount());
FILE *f = fopen(BSP_SD_MOUNT_POINT "/hello.txt", "w");
```

### I2C

I2C is initialized automatically with touch. You can also init it yourself and reuse the bus:

```c
bsp_i2c_init();
i2c_master_bus_handle_t i2c = bsp_i2c_get_handle();
```

### Wi-Fi

```c
ESP_ERROR_CHECK(bsp_wifi_init());
ESP_ERROR_CHECK(bsp_wifi_connect("ssid", "password"));
```

### API summary

| Function | Description |
| -------- | ----------- |
| `bsp_display_start()` / `bsp_display_start_with_config()` | Init LCD, optional SD, touch, and LVGL |
| `bsp_display_lock(timeout_ms)` / `bsp_display_unlock()` | LVGL thread-safety lock (`0` = wait forever) |
| `bsp_display_get_input_dev()` | LVGL touch indev |
| `bsp_display_new()` | Create GC9503 RGB panel without LVGL |
| `bsp_display_backlight_on()` / `off()` / `bsp_display_brightness_set()` | Backlight PWM |
| `bsp_touch_new()` | Create FT5x06 touch |
| `bsp_sdcard_mount()` / `unmount()` | SDSPI FAT mount |
| `bsp_i2c_init()` / `bsp_i2c_get_handle()` | Touch I2C bus |
| `bsp_wifi_init()` / `scan()` / `connect()` | Wi-Fi helper |

## Pinout

Defined in `include/bsp/uedx48480040e_wb_a.h`:

| Function | GPIO |
| -------- | ---- |
| LCD 3-wire CS | 39 |
| LCD 3-wire SCK | 48 |
| LCD 3-wire SDA (then SD CS) | 47 |
| LCD RGB PCLK / VSYNC / HSYNC / DE | 21 / 17 / 16 / 18 |
| LCD backlight | 38 |
| Touch SCL / SDA | 41 / 40 |
| SD MOSI / CLK / MISO | 42 / 45 / 46 |

RGB565 data pins are `BSP_LCD_DATA0`–`BSP_LCD_DATA15` (R0–R4, G0–G5, B0–B4).

## Configuration

`menuconfig` → **Board Support Package (UEDX48480040E-WB-A)**:

* `BSP_LCD_RGB_BUFFER_NUMS`: RGB frame buffer count (default 3)
* `BSP_LCD_RGB_BOUNCE_BUFFER_HEIGHT`: bounce buffer height in lines (default 20)
* `BSP_SD_MOUNT_POINT`: SD VFS mount point (default `/sdcard`)

Suggested `sdkconfig.defaults`:

```text
CONFIG_IDF_TARGET="esp32s3"
CONFIG_ESPTOOLPY_FLASHSIZE_16MB=y
CONFIG_SPIRAM=y
CONFIG_SPIRAM_MODE_OCT=y
CONFIG_SPIRAM_SPEED_80M=y
CONFIG_SPIRAM_USE_MALLOC=y
CONFIG_LV_COLOR_DEPTH_16=y
```

## Troubleshooting

| Symptom | What to check |
| ------- | ------------- |
| Black screen | Enable Octal PSRAM; call `bsp_display_backlight_on()`; keep the BSP RGB timing |
| SD mount fails | Init LCD first so GPIO47 is released; use a FAT32 card |
| Wrong colors | Do not remap RGB data pins; the BSP already handles the hardware R/B swap |
| Touch / I2C errors | Expect FT5x06 at `0x38` on GPIO40/41 |
| LVGL API / link errors | App LVGL major (`^8` or `^9`) must match your UI |

## License

Apache License 2.0, see [LICENSE](./LICENSE).
