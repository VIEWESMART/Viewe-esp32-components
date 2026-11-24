# BSP：VIEWE DeskNob

[![Component Registry](https://components.espressif.com/components/viewesmart/viewe_desknob/badge.svg)](https://components.espressif.com/components/viewesmart/viewe_desknob)

| [English](README.md) | [变更记录](CHANGELOG.md) |
| --- | --- |

## 概述

VIEWE **DeskNob** 板级支持包（ESP32-S31，2.1 寸圆屏 **360×360** GC9B72 QSPI + CST826 + ES8311）。

公开 API 遵循 [ESP-BSP 开发指南](https://github.com/espressif/esp-bsp/blob/master/docu/BSP_development_guide.md)。应用程序只包含一个头文件：

```c
#include "bsp/esp-bsp.h"
```

`bsp/display.h` / `bsp/touch.h` 可在无 LVGL 时初始化屏和 CST826；`bsp_display_start()` 通过 `esp_lvgl_adapter` 再启动 LVGL。

## 能力与依赖

依赖只写版本约束。下次编译时，IDF 组件管理器会从 [ESP 组件注册表](https://components.espressif.com) 下载到应用工程的 `managed_components`。本组件不内嵌驱动源码。

| 支持 | 能力 | 控制器 | 组件 | 版本 |
| --- | --- | --- | --- | --- |
| ✔ | 显示 DISPLAY | GC9B72 QSPI RGB565 360×360 | [viewesmart/esp_lcd_gc9b72](https://components.espressif.com/components/viewesmart/esp_lcd_gc9b72) | ^1.0.0 |
| ✔ | LVGL | | [espressif/esp_lvgl_adapter](https://components.espressif.com/components/espressif/esp_lvgl_adapter) | ^0.5.0 |
| ✔ | 触摸 TOUCH | CST826 | [viewesmart/esp_lcd_touch_cst826](https://components.espressif.com/components/viewesmart/esp_lcd_touch_cst826) | ^1.0.0 |
| ✔ | 按键 BUTTONS | GPIO 左右侧开关 | [espressif/button](https://components.espressif.com/components/espressif/button) | ^4 |
| ✔ | 音频 AUDIO | | [espressif/esp_codec_dev](https://components.espressif.com/components/espressif/esp_codec_dev) | ~1.5 |
| ✔ | 扬声器 / 麦克风 | ES8311 + PA | | |
| ✔ | SD 卡 | SDMMC 4 线 | idf | >=6.1 |
| ✔ | LED | WS2812 ×12 | [espressif/led_strip](https://components.espressif.com/components/espressif/led_strip) | ^3 |
| ✔ | 电池采样 | GPIO51 VCC_OUT | idf | >=6.1 |
| ✔ | Wi-Fi / 蓝牙 | STA 扫描、经典蓝牙 + BLE | idf | >=6.1 |
| ○ | USB MSC | 可选 FAT U 盘 | [espressif/esp_tinyusb](https://components.espressif.com/components/espressif/esp_tinyusb) | ^2.0.1 |

默认 LVGL 使用内部 SRAM 的 **10 行** DMA bounce（`SPI_WITHOUT_PSRAM`），`max_transfer_sz = 360×2×10`。

## 添加到工程

```
idf.py add-dependency "viewesmart/viewe_desknob^0.1.2"
```

或写在 `idf_component.yml`：

```yaml
dependencies:
  viewesmart/viewe_desknob: "^0.1.2"
```

目标芯片需为 `esp32s31`：

```
idf.py set-target esp32s31
```

## 最小用法

```c
#include "bsp/esp-bsp.h"

void app_main(void)
{
    lv_display_t *disp = bsp_display_start();
    bsp_display_backlight_on();

    if (bsp_display_lock(0)) {
        lv_obj_t *label = lv_label_create(lv_screen_active());
        lv_label_set_text(label, "DeskNob");
        lv_obj_center(label);
        bsp_display_unlock();
    }
}
```

## 引脚摘要

| 功能 | 引脚 |
| --- | --- |
| LCD QSPI | CS=17 PCLK=18 D0..D3=16/15/14/13 RST=12 TE=19 BL=7 |
| 触摸 I2C | SDA=10 SCL=9 RST=8 INT=11 |
| Codec I2C / I2S | SCL=35 SDA=40；BCK=43 WS=45 DO=46 DI=44；PA=47 |
| SDMMC | CLK=24 CMD=25 D0..D3=20..23 |
| 侧键 | CCW=GPIO3，CW=GPIO2（低有效） |
| RGB 灯串 | GPIO61，12× WS2812 |
| 电池 ADC | GPIO51（VCC_OUT 15 kΩ / 10 kΩ） |

## 许可证

Apache-2.0 — 见 [`LICENSE`](LICENSE)。
