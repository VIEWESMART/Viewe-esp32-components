# API: UES31S043H800V480C-U

[中文](API_CN.md)

Include `bsp/esp-bsp.h`. Names match the [ESP-BSP API conventions](https://github.com/espressif/esp-bsp/blob/master/docu/BSP_development_guide.md).

Display and touch hardware init (`bsp/display.h`, `bsp/touch.h`) does not need LVGL. The LVGL functions exist only when `CONFIG_BSP_NO_GRAPHIC_LIB` is off.

## Capabilities

`BSP_CAPS_DISPLAY`, `BSP_CAPS_TOUCH`, `BSP_CAPS_BUTTONS`, `BSP_CAPS_AUDIO`, `BSP_CAPS_AUDIO_SPEAKER`, `BSP_CAPS_AUDIO_MIC`, `BSP_CAPS_SDCARD`, `BSP_CAPS_LED`, `BSP_CAPS_BUZZER`, `BSP_CAPS_USB`, `BSP_CAPS_UART1`, `BSP_CAPS_RS485`, `BSP_CAPS_CAN`, `BSP_CAPS_WIFI`, `BSP_CAPS_BLE`, `BSP_CAPS_BT_CLASSIC`. `BSP_CAPS_IMU` is 0.

## Pinout

| Group | Pins |
| --- | --- |
| I2C | SCL `GPIO1`, SDA `GPIO0` |
| I2S0 | MCLK `GPIO48`, BCK `GPIO49`, WS `GPIO50`, DOUT `GPIO51`, DIN `GPIO52`, PA `GPIO47` |
| RGB | PCLK `GPIO40`, HSYNC `GPIO44`, VSYNC `GPIO45`, DE `GPIO43`, backlight `GPIO39` |
| RGB data | R2–R7 `GPIO2–7`, G2–G7 `GPIO8–13`, B2–B7 `GPIO14–19`. R0/R1, G0/G1, B0/B1 are `GPIO_NUM_NC` |
| SDMMC | D0–D3 `GPIO20–23`, CMD `GPIO25`, CLK `GPIO24`. Initial boards power the card without a GPIO. Schematic V1.1 uses `GPIO57` (`SD_CTRL`, active low) |
| ADC keys | `GPIO42` (SW3–SW6) |
| WS2812 / buzzer | `GPIO37` / `GPIO46` |
| UART1 | TX `GPIO33`, RX `GPIO34` |
| RS485 | TX `GPIO35`, RX `GPIO36` |
| CAN | TX `GPIO53`, RX `GPIO54` |
| BOOT | `GPIO61` |

## I2C

`bsp_i2c_init()` may be called more than once. `bsp_i2c_deinit()` releases the bus. `bsp_i2c_get_handle()` returns the `i2c_master_bus_handle_t`.

## Display

| Function | Role |
| --- | --- |
| `bsp_display_new()` | RGB888 panel. `NULL` config uses 2 PSRAM framebuffers and bounce 20 |
| `bsp_display_get_panel()` | Panel handle after `bsp_display_new()` |
| `bsp_display_get_framebuffers()` | Pointers to the two framebuffers |
| `bsp_display_brightness_init()` | LEDC backlight |
| `bsp_display_brightness_set()` | Backlight 0–100% |
| `bsp_display_backlight_on()` / `off()` | Backlight switch |
| `bsp_display_start()` | Panel + GT911 + `esp_lvgl_port` direct mode, VSYNC swap |
| `bsp_display_start_with_config()` | Same, with `bsp_display_cfg_t` |
| `bsp_display_lock()` / `unlock()` | LVGL is not thread-safe |
| `bsp_display_get_input_dev()` | LVGL touch input device |
| `bsp_display_rotate()` | LVGL rotation |
| `bsp_display_enter_sleep()` / `exit_sleep()` | Panel sleep |

## Touch

`bsp_touch_new()` creates the GT911 handle. The call is idempotent: a later call returns the same handle.

## SD card

`bsp_sdcard_mount()` / `bsp_sdcard_unmount()`. Mount point is `CONFIG_BSP_SD_MOUNT_POINT`. `bsp_sdcard_get_handle()` returns `sdmmc_card_t *`.

## Audio

`bsp_audio_init()` opens I2S0 full duplex (TX and RX, `bclk_div=8`). `bsp_audio_codec_speaker_init()` and `bsp_audio_codec_microphone_init()` return `esp_codec_dev` handles. `bsp_audio_poweramp_enable()` drives the NS4150B. `bsp_i2s_get_handles()` returns the TX and RX channels.

## Buttons, LED, buzzer

`bsp_iot_button_create()` fills `bsp_button_t` handles (`BSP_BUTTON_SW3` … `SW6`) using `espressif/button` ADC devices.

`bsp_led_rgb_init()` / `bsp_led_rgb_set()` drive the WS2812. `bsp_buzzer_init()`, `bsp_buzzer_set()`, and `bsp_buzzer_beep()` drive GPIO46.

## UART, RS485, CAN

`bsp_uart1_init(baud)`. `bsp_rs485_init(baud)` and `bsp_rs485_write()`. `bsp_can_init(bitrate, self_test, &node)` when `CONFIG_SOC_TWAI_SUPPORTED` is set. `self_test` loops TX back to RX without an ACK.

## USB, Wi-Fi, Bluetooth

`bsp_usb_init()` only checks that the USB 2.0 HS PHY is on the dedicated pads. The application installs TinyUSB.

`bsp_wifi_init_sta(ssid, password)` starts a station. `bsp_bt_nvs_init()` prepares NVS for the radio. BLE and Classic Bluetooth cannot be enabled in the same firmware.

## ADC

`bsp_adc_initialize()`, `bsp_adc_read_raw()`, `bsp_adc_read_mv()` read the key resistor ladder on GPIO42.
