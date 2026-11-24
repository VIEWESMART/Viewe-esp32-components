# BSP：UES31S043H800V480C-U

| [English](README.md) | [API 用法](API_CN.md) | [示例](#示例) |
| --- | --- | --- |

## 概述

VIEWE **UES31S043H800V480C-U** 的板级支持包（ESP32-S31，4.3 寸 800×480 ST7262 + GT911 + ES8389）。

公开 API 遵循 [ESP-BSP 开发指南](https://github.com/espressif/esp-bsp/blob/master/docu/BSP_development_guide.md)。应用程序只包含一个头文件：

```c
#include "bsp/esp-bsp.h"
```

`bsp/display.h` 和 `bsp/touch.h` 在不使用 LVGL 的情况下初始化屏幕和 GT911（AVI、MP4、USB 显示）。`bsp_display_start()` 通过 `esp_lvgl_port` 再启动 LVGL。

## 能力与依赖

依赖只写版本约束。下次编译时，IDF 组件管理器会从 [ESP 组件注册表](https://components.espressif.com) 下载到应用工程的 `managed_components` 目录。本组件不在目录里保存驱动源码。

| 支持 | 能力 | 控制器 | 组件 | 版本 |
| --- | --- | --- | --- | --- |
| ✔ | 显示 DISPLAY | ST7262，RGB666 按 RGB888 驱动 | idf `esp_lcd` | >=5.5 |
| ✔ | LVGL_PORT | | [espressif/esp_lvgl_port](https://components.espressif.com/components/espressif/esp_lvgl_port) | ~2.9 |
| ✔ | 触摸 TOUCH | GT911 | [espressif/esp_lcd_touch_gt911](https://components.espressif.com/components/espressif/esp_lcd_touch_gt911) | ^1 |
| ✔ | 按键 BUTTONS | ADC，SW3–SW6 | [espressif/button](https://components.espressif.com/components/espressif/button) | >=4.2.1 |
| ✔ | 音频 AUDIO | | [espressif/esp_codec_dev](https://components.espressif.com/components/espressif/esp_codec_dev) | ~1.5 |
| ✔ | 扬声器 AUDIO_SPEAKER | ES8389 + NS4150B | | |
| ✔ | 麦克风 AUDIO_MIC | ES8389 | | |
| ✔ | SD 卡 SDCARD | SDMMC 4 线 | idf | >=5.5 |
| ✔ | LED | WS2812 | [espressif/led_strip](https://components.espressif.com/components/espressif/led_strip) | ^3 |
| ✔ | 蜂鸣器 BUZZER | GPIO46 | idf | >=5.5 |
| ✔ | UART1 | GPIO33/34 | idf | >=5.5 |
| ✔ | RS485 | UART2，硬件自动 DE | idf | >=5.5 |
| ✔ | CAN | TWAI，GPIO53/54 | idf | >=5.5 |
| ✔ | USB | USB 2.0 高速，专用 PHY | idf | >=5.5 |

`espressif/button` 最低 **4.2.1**。这一版支持没有 `SOC_ADC_CHANNEL_NUM` 宏的芯片，其中包括 ESP32-S31。`CONFIG_BSP_NO_GRAPHIC_LIB=y` 时不拉取 LVGL 和 `esp_lvgl_port`。

## 已验证的 RGB 与 LVGL

这块屏电气上是 RGB666。ESP32-S31 的 RGB 驱动必须按 **RGB888** 使用（`data_width=24`，未接线的低位为 `GPIO_NUM_NC`）。

当前时序：PCLK 22 MHz，消隐水平 4/8/8、垂直 4/8/8，刷新约 54 Hz。PSRAM 中两块帧缓冲在 VSYNC 时交换。Bounce 为 20 行。

| 项目 | 取值 |
| --- | --- |
| PCLK | 22 MHz |
| 消隐 | 水平 4/8/8，垂直 4/8/8 |
| RGB 帧缓冲 | PSRAM 中 **2** 块，VSYNC 时交换 |
| Bounce | **20** 行（16000 像素） |
| LVGL | `esp_lvgl_port`，direct 模式，VSYNC 时交换两块帧缓冲 |
| 颜色 | `CONFIG_LV_COLOR_DEPTH_16`，显示格式 RGB888 |
| 撕裂 | 开启 bounce、avoid-tearing、direct 模式 |
| PSRAM | 200 MHz OCT + XIP，`MALLOC_ALWAYSINTERNAL=0` |
| RGB 中断 | 关闭 `CONFIG_LCD_RGB_ISR_IRAM_SAFE` |
| I2S | I2S0 全双工 TX+RX，MCLK `GPIO48`，`bclk_div=8` |

`bsp_display_new()` 默认 **2 块帧缓冲**。`bsp_display_start()` 在 VSYNC 时交换它们。

带 LVGL 和音频的应用建议使用下面的 `sdkconfig`：

```
CONFIG_LV_COLOR_DEPTH_16=y
CONFIG_LV_OS_NONE=y
CONFIG_SPIRAM_MODE_OCT=y
CONFIG_SPIRAM_SPEED_200M=y
CONFIG_SPIRAM_XIP_FROM_PSRAM=y
CONFIG_SPIRAM_MALLOC_ALWAYSINTERNAL=0
# CONFIG_LCD_RGB_ISR_IRAM_SAFE is not set
```

## 加入工程

从 ESP 组件注册表加入：

```yaml
# main/idf_component.yml
dependencies:
  viewesmart/bsp_ues31s043h800v480c_u: "^1.0.2"
```

源码在 [VIEWESMART/Viewe-esp32-components](https://github.com/VIEWESMART/Viewe-esp32-components/tree/main/bsp/bsp_ues31s043h800v480c_u)。还在本地目录开发时，改用路径：

```yaml
dependencies:
  bsp_ues31s043h800v480c_u:
    path: ../../bsp_ues31s043h800v480c_u
```

使用 preview 目标编译：

```
idf.py --preview set-target esp32s31
idf.py --preview build
```

## 示例

验证工程在 `../bsp_verify/`。不要修改本组件旁边的原始示例。

| 示例 | 检查内容 |
| --- | --- |
| `01_display_touch` | RGB 屏和 GT911 |
| `02_audio_speaker` / `03_audio_mic` | ES8389 扬声器和麦克风 |
| `04_sdcard` | SDMMC 挂载 |
| `05_adc_buttons` | 通过 `espressif/button` 使用 ADC 按键 |
| `14_avi_player` / `15_mp4_player` | 不使用 LVGL 的视频（`BSP_NO_GRAPHIC_LIB`） |
| `16_sd_music` / `17_bt_audio` / `18_lvgl` | 已锁定的双帧缓冲 LVGL 路径 |

## 典型调用

```c
#include "bsp/esp-bsp.h"

/* LVGL：显示 + 触摸 + esp_lvgl_port */
lv_display_t *disp = bsp_display_start();
if (bsp_display_lock(0)) {
    lv_obj_t *label = lv_label_create(lv_screen_active());
    lv_label_set_text(label, "Hello");
    lv_obj_center(label);
    bsp_display_unlock();
}

/* 裸 LCD（AVI / MP4 / USB）：不使用 LVGL */
bsp_display_new(NULL, &panel, NULL);
bsp_display_backlight_on();
bsp_touch_new(NULL, &touch);
bsp_sdcard_mount();

esp_codec_dev_handle_t spk = bsp_audio_codec_speaker_init();
esp_codec_dev_open(spk, &fs);
bsp_audio_poweramp_enable(true);
```

BOOT 是 **GPIO61**。GPIO0 是 I2C SDA。引脚和全部函数见 [API_CN.md](API_CN.md)。
