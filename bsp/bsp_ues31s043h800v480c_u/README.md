# BSP: UES31S043H800V480C-U

| [中文](README_CN.md) | [HOW TO USE API](API.md) | [EXAMPLES](#examples) |
| --- | --- | --- |

## Overview

Board Support Package for the VIEWE **UES31S043H800V480C-U** (ESP32-S31, 4.3" 800×480 ST7262 + GT911 + ES8389).

The public API follows the [ESP-BSP development guide](https://github.com/espressif/esp-bsp/blob/master/docu/BSP_development_guide.md). Applications include one header:

```c
#include "bsp/esp-bsp.h"
```

`bsp/display.h` and `bsp/touch.h` initialize the panel and GT911 without LVGL (AVI, MP4, USB display). `bsp_display_start()` adds LVGL through `esp_lvgl_port`.

## Capabilities and dependencies

Dependencies are version constraints only. The IDF Component Manager downloads them from the [ESP Component Registry](https://components.espressif.com) into the application's `managed_components` directory on the next build. This component does not vendor driver source.

| Available | Capability | Controller | Component | Version |
| --- | --- | --- | --- | --- |
| ✔ | DISPLAY | ST7262 RGB666 as RGB888 | idf `esp_lcd` | >=5.5 |
| ✔ | LVGL_PORT | | [espressif/esp_lvgl_port](https://components.espressif.com/components/espressif/esp_lvgl_port) | ~2.9 |
| ✔ | TOUCH | GT911 | [espressif/esp_lcd_touch_gt911](https://components.espressif.com/components/espressif/esp_lcd_touch_gt911) | ^1 |
| ✔ | BUTTONS | ADC SW3–SW6 | [espressif/button](https://components.espressif.com/components/espressif/button) | >=4.2.1 |
| ✔ | AUDIO | | [espressif/esp_codec_dev](https://components.espressif.com/components/espressif/esp_codec_dev) | ~1.5 |
| ✔ | AUDIO_SPEAKER | ES8389 + NS4150B | | |
| ✔ | AUDIO_MIC | ES8389 | | |
| ✔ | SDCARD | SDMMC 4-bit | idf | >=5.5 |
| ✔ | LED | WS2812 | [espressif/led_strip](https://components.espressif.com/components/espressif/led_strip) | ^3 |
| ✔ | BUZZER | GPIO46 | idf | >=5.5 |
| ✔ | UART1 | GPIO33/34 | idf | >=5.5 |
| ✔ | RS485 | UART2, hardware auto DE | idf | >=5.5 |
| ✔ | CAN | TWAI GPIO53/54 | idf | >=5.5 |
| ✔ | USB | USB 2.0 HS dedicated PHY | idf | >=5.5 |

`espressif/button` **4.2.1** is the minimum. That release accepts targets that do not define `SOC_ADC_CHANNEL_NUM`, which includes ESP32-S31. LVGL and `esp_lvgl_port` are skipped when `CONFIG_BSP_NO_GRAPHIC_LIB=y`.

## Verified RGB + LVGL

This panel is electrically RGB666. On ESP32-S31 the RGB driver must use **RGB888** (`data_width=24`, unused LSBs `GPIO_NUM_NC`).

Current timing: PCLK 22 MHz, porch H 4/8/8 and V 4/8/8, refresh about 54 Hz. Two framebuffers in PSRAM swap on VSYNC. The bounce buffer is 20 lines.

| Item | Value |
| --- | --- |
| PCLK | 22 MHz |
| Porch | H 4/8/8, V 4/8/8 |
| RGB FB | **2** in PSRAM, swap on VSYNC |
| Bounce | **20** lines (16000 px) |
| LVGL | `esp_lvgl_port`, direct mode, swap the 2 FBs on VSYNC |
| Color | `CONFIG_LV_COLOR_DEPTH_16`, display format RGB888 |
| Tear | bounce on, avoid-tearing on, direct mode |
| PSRAM | 200 MHz OCT + XIP, `MALLOC_ALWAYSINTERNAL=0` |
| RGB ISR | `CONFIG_LCD_RGB_ISR_IRAM_SAFE` off |
| I2S | I2S0 TX+RX, MCLK `GPIO48`, `bclk_div=8` |

`bsp_display_new()` defaults to **2 framebuffers**. `bsp_display_start()` swaps them on VSYNC.

Recommended `sdkconfig` for LVGL + audio apps:

```
CONFIG_LV_COLOR_DEPTH_16=y
CONFIG_LV_OS_NONE=y
CONFIG_SPIRAM_MODE_OCT=y
CONFIG_SPIRAM_SPEED_200M=y
CONFIG_SPIRAM_XIP_FROM_PSRAM=y
CONFIG_SPIRAM_MALLOC_ALWAYSINTERNAL=0
# CONFIG_LCD_RGB_ISR_IRAM_SAFE is not set
```

## Add to a project

From the ESP Component Registry:

```yaml
# main/idf_component.yml
dependencies:
  viewesmart/bsp_ues31s043h800v480c_u: "^1.0.2"
```

Source is [VIEWESMART/Viewe-esp32-components](https://github.com/VIEWESMART/Viewe-esp32-components/tree/main/bsp/bsp_ues31s043h800v480c_u). While this tree is still a local checkout, point at the directory instead:

```yaml
dependencies:
  bsp_ues31s043h800v480c_u:
    path: ../../bsp_ues31s043h800v480c_u
```

Build with the preview target:

```
idf.py --preview set-target esp32s31
idf.py --preview build
```

## Examples

Verification apps live in `../bsp_verify/`. Do not modify the original examples next to this component.

| Example | What it checks |
| --- | --- |
| `01_display_touch` | RGB panel and GT911 |
| `02_audio_speaker` / `03_audio_mic` | ES8389 speaker and microphone |
| `04_sdcard` | SDMMC mount |
| `05_adc_buttons` | ADC keys via `espressif/button` |
| `14_avi_player` / `15_mp4_player` | Video without LVGL (`BSP_NO_GRAPHIC_LIB`) |
| `16_sd_music` / `17_bt_audio` / `18_lvgl` | Locked 2-FB LVGL path |

## Typical calls

```c
#include "bsp/esp-bsp.h"

/* LVGL: display + touch + esp_lvgl_port */
lv_display_t *disp = bsp_display_start();
if (bsp_display_lock(0)) {
    lv_obj_t *label = lv_label_create(lv_screen_active());
    lv_label_set_text(label, "Hello");
    lv_obj_center(label);
    bsp_display_unlock();
}

/* Raw LCD (AVI / MP4 / USB): no LVGL */
bsp_display_new(NULL, &panel, NULL);
bsp_display_backlight_on();
bsp_touch_new(NULL, &touch);
bsp_sdcard_mount();

esp_codec_dev_handle_t spk = bsp_audio_codec_speaker_init();
esp_codec_dev_open(spk, &fs);
bsp_audio_poweramp_enable(true);
```

BOOT is **GPIO61**. GPIO0 is I2C SDA. Pinout and the full function list are in [API.md](API.md).
