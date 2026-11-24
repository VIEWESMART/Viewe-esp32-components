# API：UES31S043H800V480C-U

包含 `bsp/esp-bsp.h`。函数名与 [ESP-BSP API 约定](https://github.com/espressif/esp-bsp/blob/master/docu/BSP_development_guide.md) 一致。英文版见 [API.md](API.md)。

显示和触摸的硬件初始化（`bsp/display.h`、`bsp/touch.h`）不需要 LVGL。LVGL 函数只在关闭 `CONFIG_BSP_NO_GRAPHIC_LIB` 时存在。

## 能力宏

已定义：`BSP_CAPS_DISPLAY`、`BSP_CAPS_TOUCH`、`BSP_CAPS_BUTTONS`、`BSP_CAPS_AUDIO`、`BSP_CAPS_AUDIO_SPEAKER`、`BSP_CAPS_AUDIO_MIC`、`BSP_CAPS_SDCARD`、`BSP_CAPS_LED`、`BSP_CAPS_BUZZER`、`BSP_CAPS_USB`、`BSP_CAPS_UART1`、`BSP_CAPS_RS485`、`BSP_CAPS_CAN`、`BSP_CAPS_WIFI`、`BSP_CAPS_BLE`、`BSP_CAPS_BT_CLASSIC`。`BSP_CAPS_IMU` 为 0。

## 引脚

| 分组 | 引脚 |
| --- | --- |
| I2C | SCL `GPIO1`，SDA `GPIO0` |
| I2S0 | MCLK `GPIO48`，BCK `GPIO49`，WS `GPIO50`，DOUT `GPIO51`，DIN `GPIO52`，功放 `GPIO47` |
| RGB | PCLK `GPIO40`，HSYNC `GPIO44`，VSYNC `GPIO45`，DE `GPIO43`，背光 `GPIO39` |
| RGB 数据 | R2–R7 为 `GPIO2–7`，G2–G7 为 `GPIO8–13`，B2–B7 为 `GPIO14–19`。R0/R1、G0/G1、B0/B1 为 `GPIO_NUM_NC` |
| SDMMC | D0–D3 为 `GPIO20–23`，CMD `GPIO25`，CLK `GPIO24`。初始版开发板不靠 GPIO 给卡上电。原理图 V1.1 用 `GPIO57`（`SD_CTRL`，低电平有效） |
| ADC 按键 | `GPIO42`（SW3–SW6） |
| WS2812 / 蜂鸣器 | `GPIO37` / `GPIO46` |
| UART1 | TX `GPIO33`，RX `GPIO34` |
| RS485 | TX `GPIO35`，RX `GPIO36` |
| CAN | TX `GPIO53`，RX `GPIO54` |
| BOOT | `GPIO61` |

## I2C

`bsp_i2c_init()` 可以多次调用。`bsp_i2c_deinit()` 释放总线。`bsp_i2c_get_handle()` 返回 `i2c_master_bus_handle_t`。

## 显示

| 函数 | 作用 |
| --- | --- |
| `bsp_display_new()` | RGB888 屏。配置为 `NULL` 时使用 2 块 PSRAM 帧缓冲和 20 行 bounce |
| `bsp_display_get_panel()` | `bsp_display_new()` 之后的面板句柄 |
| `bsp_display_get_framebuffers()` | 两块帧缓冲的指针 |
| `bsp_display_brightness_init()` | LEDC 背光 |
| `bsp_display_brightness_set()` | 背光 0–100% |
| `bsp_display_backlight_on()` / `off()` | 背光开关 |
| `bsp_display_start()` | 屏幕 + GT911 + `esp_lvgl_port` direct 模式，VSYNC 交换 |
| `bsp_display_start_with_config()` | 同上，使用 `bsp_display_cfg_t` |
| `bsp_display_lock()` / `unlock()` | LVGL 不是线程安全的 |
| `bsp_display_get_input_dev()` | LVGL 触摸输入设备 |
| `bsp_display_rotate()` | LVGL 旋转 |
| `bsp_display_enter_sleep()` / `exit_sleep()` | 屏幕休眠 |

## 触摸

`bsp_touch_new()` 创建 GT911 句柄。重复调用返回同一个句柄。

## SD 卡

`bsp_sdcard_mount()` / `bsp_sdcard_unmount()`。挂载点是 `CONFIG_BSP_SD_MOUNT_POINT`。`bsp_sdcard_get_handle()` 返回 `sdmmc_card_t *`。

## 音频

`bsp_audio_init()` 打开 I2S0 全双工（TX 和 RX，`bclk_div=8`）。`bsp_audio_codec_speaker_init()` 和 `bsp_audio_codec_microphone_init()` 返回 `esp_codec_dev` 句柄。`bsp_audio_poweramp_enable()` 控制 NS4150B。`bsp_i2s_get_handles()` 返回 TX 和 RX 通道。

## 按键、LED、蜂鸣器

`bsp_iot_button_create()` 用 `espressif/button` 的 ADC 设备填入 `bsp_button_t` 句柄（`BSP_BUTTON_SW3` 到 `SW6`）。

`bsp_led_rgb_init()` / `bsp_led_rgb_set()` 驱动 WS2812。`bsp_buzzer_init()`、`bsp_buzzer_set()`、`bsp_buzzer_beep()` 驱动 GPIO46。

## UART、RS485、CAN

`bsp_uart1_init(baud)`。`bsp_rs485_init(baud)` 和 `bsp_rs485_write()`。在定义了 `CONFIG_SOC_TWAI_SUPPORTED` 时使用 `bsp_can_init(bitrate, self_test, &node)`。`self_test` 为真时，发送不需要 ACK，并回环到本机接收。

## USB、Wi-Fi、蓝牙

`bsp_usb_init()` 只确认 USB 2.0 高速 PHY 在专用管脚上。应用程序自己安装 TinyUSB。

`bsp_wifi_init_sta(ssid, password)` 启动 STA。`bsp_bt_nvs_init()` 为射频准备 NVS。同一份固件里不能同时打开 BLE 和经典蓝牙。

## ADC

`bsp_adc_initialize()`、`bsp_adc_read_raw()`、`bsp_adc_read_mv()` 读取 GPIO42 上的按键分压。
