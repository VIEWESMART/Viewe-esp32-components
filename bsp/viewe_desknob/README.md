# BSP: VIEWE DeskNob

[![Component Registry](https://components.espressif.com/components/viewesmart/viewe_desknob/badge.svg)](https://components.espressif.com/components/viewesmart/viewe_desknob)

| [中文](README_CN.md) | [Changelog](CHANGELOG.md) |
| --- | --- |

## Overview

Board Support Package for the VIEWE **DeskNob** (ESP32-S31, 2.1" round **360×360** GC9B72 QSPI + CST826 + ES8311).

The public API follows the [ESP-BSP development guide](https://github.com/espressif/esp-bsp/blob/master/docu/BSP_development_guide.md). Applications include one header:

```c
#include "bsp/esp-bsp.h"
```

`bsp/display.h` and `bsp/touch.h` initialize the panel and CST826 without LVGL. `bsp_display_start()` adds LVGL through `esp_lvgl_adapter`.

## Capabilities and dependencies

Dependencies are version constraints only. The IDF Component Manager downloads them from the [ESP Component Registry](https://components.espressif.com) into the application's `managed_components` directory. This component does not vendor driver source.

| Available | Capability | Controller | Component | Version |
| --- | --- | --- | --- | --- |
| ✔ | DISPLAY | GC9B72 QSPI RGB565 360×360 | [viewesmart/esp_lcd_gc9b72](https://components.espressif.com/components/viewesmart/esp_lcd_gc9b72) | ^1.0.0 |
| ✔ | LVGL | | [espressif/esp_lvgl_adapter](https://components.espressif.com/components/espressif/esp_lvgl_adapter) | ^0.5.0 |
| ✔ | TOUCH | CST826 | [viewesmart/esp_lcd_touch_cst826](https://components.espressif.com/components/viewesmart/esp_lcd_touch_cst826) | ^1.0.0 |
| ✔ | BUTTONS | GPIO CCW/CW | [espressif/button](https://components.espressif.com/components/espressif/button) | ^4 |
| ✔ | AUDIO | | [espressif/esp_codec_dev](https://components.espressif.com/components/espressif/esp_codec_dev) | ~1.5 |
| ✔ | AUDIO_SPEAKER | ES8311 + PA | | |
| ✔ | AUDIO_MIC | ES8311 | | |
| ✔ | SDCARD | SDMMC 4-bit | idf | >=6.1 |
| ✔ | LED | WS2812 ×12 | [espressif/led_strip](https://components.espressif.com/components/espressif/led_strip) | ^3 |
| ✔ | BATTERY | GPIO51 VCC_OUT sense | idf | >=6.1 |
| ✔ | WIFI / BT | STA scan, Classic + BLE | idf | >=6.1 |
| ○ | USB_MSC | Optional FAT U-disk | [espressif/esp_tinyusb](https://components.espressif.com/components/espressif/esp_tinyusb) | ^2.0.1 |

Default LVGL path uses a **10-line** DMA bounce buffer in internal SRAM (`SPI_WITHOUT_PSRAM`), `max_transfer_sz = 360×2×10`.

## Add to project

```
idf.py add-dependency "viewesmart/viewe_desknob^0.1.2"
```

Or in `idf_component.yml`:

```yaml
dependencies:
  viewesmart/viewe_desknob: "^0.1.2"
```

Target must be `esp32s31`:

```
idf.py set-target esp32s31
```

## Minimal usage

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

## Pin map (summary)

| Function | Pins |
| --- | --- |
| LCD QSPI | CS=17 PCLK=18 D0..D3=16/15/14/13 RST=12 TE=19 BL=7 |
| Touch I2C | SDA=10 SCL=9 RST=8 INT=11 |
| Codec I2C / I2S | SCL=35 SDA=40; BCK=43 WS=45 DO=46 DI=44; PA=47 |
| SDMMC | CLK=24 CMD=25 D0..D3=20..23 |
| Side switches | CCW=GPIO3, CW=GPIO2 (active low) |
| RGB strip | GPIO61, 12× WS2812 |
| Battery ADC | GPIO51 (15 kΩ / 10 kΩ on VCC_OUT) |

## License

Apache-2.0 — see [`LICENSE`](LICENSE).
