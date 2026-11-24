# -*- coding: utf-8 -*-
"""Generate bsp_verify IDF projects that use the BSP and leave originals untouched."""
from __future__ import annotations

import os

ROOT = r"E:\idf-debuging\02_s31_examples\bsp_verify"
BSP_YML = """dependencies:
  idf: \">=5.5.0\"
  bsp_ues31s043h800v480c_u:
    path: ../../../bsp_ues31s043h800v480c_u
"""

CMAKE_PROJ = """cmake_minimum_required(VERSION 3.16)
include($ENV{{IDF_PATH}}/tools/cmake/project.cmake)
project({name})
"""

CMAKE_MAIN = """idf_component_register(SRCS "main.c" INCLUDE_DIRS ".")
"""

SDK_COMMON = """CONFIG_IDF_TARGET="esp32s31"
CONFIG_IDF_EXPERIMENTAL_FEATURES=y
CONFIG_ESP_DEFAULT_CPU_FREQ_MHZ_320=y
CONFIG_ESPTOOLPY_FLASHMODE_QIO=y
CONFIG_ESPTOOLPY_FLASHFREQ_80M=y
CONFIG_ESPTOOLPY_FLASHSIZE_16MB=y
CONFIG_SPIRAM=y
CONFIG_SPIRAM_MODE_OCT=y
CONFIG_SPIRAM_SPEED_200M=y
CONFIG_SPIRAM_XIP_FROM_PSRAM=y
CONFIG_FREERTOS_HZ=1000
CONFIG_ESP_TASK_WDT_EN=n
"""

SDK_S31 = """CONFIG_IDF_EXPERIMENTAL_FEATURES=y
CONFIG_ESP_DEFAULT_CPU_FREQ_MHZ_320=y
CONFIG_ESPTOOLPY_FLASHMODE_QIO=y
CONFIG_ESPTOOLPY_FLASHFREQ_80M=y
CONFIG_ESPTOOLPY_FLASHSIZE_16MB=y
CONFIG_SPIRAM=y
CONFIG_SPIRAM_MODE_OCT=y
CONFIG_SPIRAM_SPEED_200M=y
CONFIG_SPIRAM_XIP_FROM_PSRAM=y
CONFIG_FREERTOS_HZ=1000
"""


def write(path: str, content: str) -> None:
    os.makedirs(os.path.dirname(path), exist_ok=True)
    with open(path, "w", encoding="utf-8", newline="\n") as f:
        f.write(content)


def project(name: str, main_c: str, extra_yml: str = "", extra_sdk: str = "", extra_cmake: str = "") -> None:
    base = os.path.join(ROOT, name)
    write(os.path.join(base, "CMakeLists.txt"), CMAKE_PROJ.format(name=name.replace("-", "_")))
    write(os.path.join(base, "sdkconfig.defaults"), SDK_COMMON + extra_sdk)
    write(os.path.join(base, "sdkconfig.defaults.esp32s31"), SDK_S31)
    write(os.path.join(base, "main", "CMakeLists.txt"), extra_cmake or CMAKE_MAIN)
    write(os.path.join(base, "main", "idf_component.yml"), BSP_YML + extra_yml)
    write(os.path.join(base, "main", "main.c"), main_c)
    print("created", name)


DISPLAY = r'''#include <string.h>
#include "bsp/esp-bsp.h"
#include "esp_lcd_panel_ops.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

static const char *TAG = "bsp_display";

static void fill_rgb888(uint8_t *buf, size_t pixels, uint8_t r, uint8_t g, uint8_t b)
{
    for (size_t i = 0; i < pixels; i++) {
        buf[i * 3 + 0] = b;
        buf[i * 3 + 1] = g;
        buf[i * 3 + 2] = r;
    }
}

void app_main(void)
{
    esp_lcd_panel_handle_t panel = NULL;
    ESP_ERROR_CHECK(bsp_display_new(NULL, &panel, NULL));
    ESP_ERROR_CHECK(bsp_display_backlight_on());

    uint8_t line[BSP_LCD_H_RES * 3];
    fill_rgb888(line, BSP_LCD_H_RES, 16, 24, 40);
    for (int y = 0; y < BSP_LCD_V_RES; y++) {
        esp_lcd_panel_draw_bitmap(panel, 0, y, BSP_LCD_H_RES, y + 1, line);
    }

    esp_lcd_touch_handle_t tp = NULL;
    ESP_ERROR_CHECK(bsp_touch_new(NULL, &tp));
    ESP_LOGI(TAG, "touch ready, brightness sweep then draw dots");

    for (int p = 20; p <= 100; p += 20) {
        ESP_ERROR_CHECK(bsp_display_brightness_set(p));
        ESP_LOGI(TAG, "backlight %d%%", p);
        vTaskDelay(pdMS_TO_TICKS(400));
    }

    uint8_t dot[16 * 16 * 3];
    fill_rgb888(dot, 16 * 16, 0, 220, 220);

    while (1) {
        if (esp_lcd_touch_read_data(tp) == ESP_OK) {
            esp_lcd_touch_point_data_t pts[CONFIG_ESP_LCD_TOUCH_MAX_POINTS] = {0};
            uint8_t n = 0;
            if (esp_lcd_touch_get_data(tp, pts, &n, CONFIG_ESP_LCD_TOUCH_MAX_POINTS) == ESP_OK && n > 0) {
                int x0 = pts[0].x > 8 ? pts[0].x - 8 : 0;
                int y0 = pts[0].y > 8 ? pts[0].y - 8 : 0;
                int x1 = x0 + 16;
                int y1 = y0 + 16;
                if (x1 > BSP_LCD_H_RES) x1 = BSP_LCD_H_RES;
                if (y1 > BSP_LCD_V_RES) y1 = BSP_LCD_V_RES;
                esp_lcd_panel_draw_bitmap(panel, x0, y0, x1, y1, dot);
                ESP_LOGI(TAG, "touch %d,%d", pts[0].x, pts[0].y);
            }
        }
        vTaskDelay(pdMS_TO_TICKS(20));
    }
}
'''

SPEAKER = r'''#include "bsp/esp-bsp.h"
#include "esp_codec_dev.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include <math.h>
#include <stdint.h>

static const char *TAG = "bsp_spk";

void app_main(void)
{
    esp_codec_dev_handle_t spk = bsp_audio_codec_speaker_init();
    if (!spk) {
        ESP_LOGE(TAG, "speaker init failed");
        return;
    }
    esp_codec_dev_sample_info_t fs = {
        .bits_per_sample = 16,
        .channel = 2,
        .sample_rate = 48000,
    };
    ESP_ERROR_CHECK(esp_codec_dev_open(spk, &fs));
    ESP_ERROR_CHECK(esp_codec_dev_set_out_vol(spk, 60));
    ESP_ERROR_CHECK(bsp_audio_poweramp_enable(true));
    ESP_LOGI(TAG, "1 kHz tone on speakers");

    const int samples = 480;
    int16_t buf[480 * 2];
    float phase = 0;
    while (1) {
        for (int i = 0; i < samples; i++) {
            int16_t s = (int16_t)(sinf(phase) * 8000);
            buf[i * 2] = s;
            buf[i * 2 + 1] = s;
            phase += 2.0f * 3.1415926f * 1000.0f / 48000.0f;
            if (phase > 6.28f) phase -= 6.28f;
        }
        esp_codec_dev_write(spk, buf, sizeof(buf));
    }
}
'''

MIC = r'''#include <stdio.h>
#include "bsp/esp-bsp.h"
#include "esp_codec_dev.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

static const char *TAG = "bsp_mic";

void app_main(void)
{
    ESP_ERROR_CHECK(bsp_sdcard_mount());
    esp_codec_dev_handle_t mic = bsp_audio_codec_microphone_init();
    if (!mic) {
        ESP_LOGE(TAG, "mic init failed");
        return;
    }
    esp_codec_dev_sample_info_t fs = {
        .bits_per_sample = 16,
        .channel = 2,
        .sample_rate = 16000,
    };
    ESP_ERROR_CHECK(esp_codec_dev_open(mic, &fs));
    ESP_ERROR_CHECK(esp_codec_dev_set_in_gain(mic, 24.5f));

    FILE *f = fopen("/sdcard/bsp_mic.raw", "wb");
    if (!f) {
        ESP_LOGE(TAG, "open /sdcard/bsp_mic.raw failed");
        return;
    }
    int16_t buf[512];
    ESP_LOGI(TAG, "recording 5s to /sdcard/bsp_mic.raw");
    for (int i = 0; i < 16000 * 5 / 256; i++) {
        if (esp_codec_dev_read(mic, buf, sizeof(buf)) == ESP_OK) {
            fwrite(buf, 1, sizeof(buf), f);
        }
    }
    fclose(f);
    ESP_LOGI(TAG, "done");
}
'''

SD = r'''#include <dirent.h>
#include <stdint.h>
#include <sys/stat.h>
#include "bsp/esp-bsp.h"
#include "esp_log.h"

static const char *TAG = "bsp_sd";

void app_main(void)
{
    ESP_ERROR_CHECK(bsp_sdcard_mount());
    DIR *d = opendir(BSP_SDCARD_MOUNT_POINT);
    if (!d) {
        ESP_LOGE(TAG, "opendir failed");
        return;
    }
    struct dirent *e;
    while ((e = readdir(d)) != NULL) {
        ESP_LOGI(TAG, "  %s", e->d_name);
    }
    closedir(d);
    sdmmc_card_t *card = bsp_sdcard_get_handle();
    if (card) {
        ESP_LOGI(TAG, "capacity %llu MB",
                 ((uint64_t)card->csd.capacity * card->csd.sector_size) >> 20);
    }
}
'''

BUTTONS = r'''#include "bsp/esp-bsp.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

static const char *TAG = "bsp_btn";
static const char *NAMES[] = {"SW3", "SW4", "SW5", "SW6"};

static void cb(void *h, void *arg)
{
    int idx = (int)(intptr_t)arg;
    ESP_LOGI(TAG, "%s event=%s adc=%d mV", NAMES[idx],
             iot_button_get_event_str(iot_button_get_event(h)), bsp_adc_read_mv());
}

void app_main(void)
{
    button_handle_t btns[BSP_BUTTON_NUM];
    ESP_ERROR_CHECK(bsp_iot_button_create(btns, NULL, BSP_BUTTON_NUM));
    for (int i = 0; i < BSP_BUTTON_NUM; i++) {
        iot_button_register_cb(btns[i], BUTTON_SINGLE_CLICK, NULL, cb, (void *)(intptr_t)i);
        iot_button_register_cb(btns[i], BUTTON_PRESS_DOWN, NULL, cb, (void *)(intptr_t)i);
    }
    ESP_LOGI(TAG, "press SW3-SW6");
    while (1) {
        vTaskDelay(pdMS_TO_TICKS(500));
        ESP_LOGI(TAG, "adc %d mV", bsp_adc_read_mv());
    }
}
'''

UART1 = r'''#include <inttypes.h>
#include <stdio.h>
#include "bsp/esp-bsp.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

static const char *TAG = "bsp_uart1";

void app_main(void)
{
    ESP_ERROR_CHECK(bsp_uart1_init(115200));
    uint32_t n = 0;
    uint8_t buf[128];
    while (1) {
        char msg[48];
        int len = snprintf(msg, sizeof(msg), "S31 BSP UART1 tick %" PRIu32 "\r\n", ++n);
        uart_write_bytes(UART_NUM_1, msg, len);
        int r = uart_read_bytes(UART_NUM_1, buf, sizeof(buf) - 1, pdMS_TO_TICKS(200));
        if (r > 0) {
            buf[r] = 0;
            ESP_LOGI(TAG, "rx %d: %s", r, (char *)buf);
        }
        vTaskDelay(pdMS_TO_TICKS(1800));
    }
}
'''

RS485 = r'''#include <inttypes.h>
#include <stdio.h>
#include "bsp/esp-bsp.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

static const char *TAG = "bsp_rs485";

void app_main(void)
{
    ESP_ERROR_CHECK(bsp_rs485_init(115200));
    uint32_t n = 0;
    uint8_t buf[128];
    while (1) {
        char msg[48];
        int len = snprintf(msg, sizeof(msg), "S31 BSP RS485 tick %" PRIu32 "\r\n", ++n);
        bsp_rs485_write(msg, len);
        int r = uart_read_bytes(UART_NUM_2, buf, sizeof(buf) - 1, pdMS_TO_TICKS(200));
        if (r > 0) {
            buf[r] = 0;
            ESP_LOGI(TAG, "rx %d: %s", r, (char *)buf);
        }
        vTaskDelay(pdMS_TO_TICKS(1800));
    }
}
'''

CAN = r'''#include <inttypes.h>
#include "bsp/esp-bsp.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

static const char *TAG = "bsp_can";

void app_main(void)
{
    twai_node_handle_t node = NULL;
    ESP_ERROR_CHECK(bsp_can_init(500000, true, &node));
    uint32_t n = 0;
    while (1) {
        uint8_t data[4] = {
            (uint8_t)(n >> 24), (uint8_t)(n >> 16), (uint8_t)(n >> 8), (uint8_t)n
        };
        twai_frame_t frame = {
            .header.id = 0x123,
            .header.dlc = 4,
            .buffer = data,
            .buffer_len = 4,
        };
        if (twai_node_transmit(node, &frame, pdMS_TO_TICKS(100)) == ESP_OK) {
            ESP_LOGI(TAG, "tx id=0x123 n=%" PRIu32, n);
        }
        n++;
        vTaskDelay(pdMS_TO_TICKS(1000));
    }
}
'''

WIFI = r'''#include "bsp/esp-bsp.h"
#include "esp_event.h"
#include "esp_log.h"
#include "esp_netif.h"
#include "esp_wifi.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

static const char *TAG = "bsp_wifi";

#ifndef EXAMPLE_WIFI_SSID
#define EXAMPLE_WIFI_SSID       "viewe"
#define EXAMPLE_WIFI_PASSWORD   "yysj29432299"
#endif

static void on_got_ip(void *arg, esp_event_base_t base, int32_t id, void *data)
{
    ip_event_got_ip_t *e = data;
    ESP_LOGI(TAG, "got ip " IPSTR, IP2STR(&e->ip_info.ip));
}

void app_main(void)
{
    ESP_ERROR_CHECK(bsp_wifi_init_sta(EXAMPLE_WIFI_SSID, EXAMPLE_WIFI_PASSWORD));
    ESP_ERROR_CHECK(esp_event_handler_register(IP_EVENT, IP_EVENT_STA_GOT_IP, on_got_ip, NULL));
    while (1) {
        wifi_ap_record_t ap;
        if (esp_wifi_sta_get_ap_info(&ap) == ESP_OK) {
            ESP_LOGI(TAG, "SSID=%s RSSI=%d ch=%d", ap.ssid, ap.rssi, ap.primary);
        }
        vTaskDelay(pdMS_TO_TICKS(3000));
    }
}
'''

BLE = r'''#include "bsp/esp-bsp.h"
#include "esp_bt.h"
#include "esp_bt_main.h"
#include "esp_gap_ble_api.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

static const char *TAG = "bsp_ble";

static uint8_t adv_raw[] = {
    0x02, 0x01, 0x06,
    0x0B, 0x09, 'S', '3', '1', '-', 'B', 'S', 'P', '-', 'B', 'L', 'E',
};

static void gap_cb(esp_gap_ble_cb_event_t event, esp_ble_gap_cb_param_t *param)
{
    if (event == ESP_GAP_BLE_ADV_DATA_RAW_SET_COMPLETE_EVT) {
        esp_ble_adv_params_t p = {
            .adv_int_min = 0x20,
            .adv_int_max = 0x40,
            .adv_type = ADV_TYPE_IND,
            .own_addr_type = BLE_ADDR_TYPE_PUBLIC,
            .channel_map = ADV_CHNL_ALL,
            .adv_filter_policy = ADV_FILTER_ALLOW_SCAN_ANY_CON_ANY,
        };
        esp_ble_gap_start_advertising(&p);
    } else if (event == ESP_GAP_BLE_ADV_START_COMPLETE_EVT) {
        ESP_LOGI(TAG, "advertising S31-BSP-BLE");
    }
}

void app_main(void)
{
    ESP_ERROR_CHECK(bsp_bt_nvs_init());
    ESP_ERROR_CHECK(esp_bt_controller_mem_release(ESP_BT_MODE_CLASSIC_BT));
    esp_bt_controller_config_t cfg = BT_CONTROLLER_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_bt_controller_init(&cfg));
    ESP_ERROR_CHECK(esp_bt_controller_enable(ESP_BT_MODE_BLE));
    ESP_ERROR_CHECK(esp_bluedroid_init());
    ESP_ERROR_CHECK(esp_bluedroid_enable());
    ESP_ERROR_CHECK(esp_ble_gap_register_callback(gap_cb));
    ESP_ERROR_CHECK(esp_ble_gap_config_adv_data_raw(adv_raw, sizeof(adv_raw)));
    while (1) {
        vTaskDelay(pdMS_TO_TICKS(2000));
    }
}
'''

SPP = r'''#include "bsp/esp-bsp.h"
#include "esp_bt.h"
#include "esp_bt_device.h"
#include "esp_bt_main.h"
#include "esp_gap_bt_api.h"
#include "esp_log.h"
#include "esp_spp_api.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

static const char *TAG = "bsp_spp";

static void spp_cb(esp_spp_cb_event_t event, esp_spp_cb_param_t *param)
{
    if (event == ESP_SPP_INIT_EVT) {
        esp_bt_gap_set_device_name("S31-BSP-SPP");
        esp_bt_gap_set_scan_mode(ESP_BT_CONNECTABLE, ESP_BT_GENERAL_DISCOVERABLE);
        ESP_LOGI(TAG, "SPP ready, name S31-BSP-SPP");
    }
}

void app_main(void)
{
    ESP_ERROR_CHECK(bsp_bt_nvs_init());
    ESP_ERROR_CHECK(esp_bt_controller_mem_release(ESP_BT_MODE_BLE));
    esp_bt_controller_config_t cfg = BT_CONTROLLER_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_bt_controller_init(&cfg));
    ESP_ERROR_CHECK(esp_bt_controller_enable(ESP_BT_MODE_CLASSIC_BT));
    ESP_ERROR_CHECK(esp_bluedroid_init());
    ESP_ERROR_CHECK(esp_bluedroid_enable());
    ESP_ERROR_CHECK(esp_spp_register_callback(spp_cb));
    esp_spp_cfg_t spp_cfg = { .mode = ESP_SPP_MODE_CB };
    ESP_ERROR_CHECK(esp_spp_enhanced_init(&spp_cfg));
    while (1) {
        vTaskDelay(pdMS_TO_TICKS(2000));
    }
}
'''

USB = r'''#include "bsp/esp-bsp.h"
#include "class/hid/hid_device.h"
#include "esp_lcd_panel_ops.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "tinyusb.h"
#include "tinyusb_default_config.h"

static const char *TAG = "bsp_usb";
#define TUSB_DESC_TOTAL_LEN  (TUD_CONFIG_DESC_LEN + TUD_HID_DESC_LEN)

static const uint8_t hid_report_descriptor[] = { TUD_HID_REPORT_DESC_MOUSE() };
static const char *hid_string_descriptor[] = {
    (char[]){0x09, 0x04}, "VIEWE", "S31 HID Touch", "123456", "HID Mouse",
};
static const uint8_t hid_configuration_descriptor[] = {
    TUD_CONFIG_DESCRIPTOR(1, 1, 0, TUSB_DESC_TOTAL_LEN, TUSB_DESC_CONFIG_ATT_REMOTE_WAKEUP, 100),
    TUD_HID_DESCRIPTOR(0, 4, false, sizeof(hid_report_descriptor), 0x81, 16, 10),
};

uint8_t const *tud_hid_descriptor_report_cb(uint8_t instance)
{
    (void)instance;
    return hid_report_descriptor;
}
uint16_t tud_hid_get_report_cb(uint8_t i, uint8_t id, hid_report_type_t t, uint8_t *b, uint16_t n)
{
    (void)i; (void)id; (void)t; (void)b; (void)n;
    return 0;
}
void tud_hid_set_report_cb(uint8_t i, uint8_t id, hid_report_type_t t, uint8_t const *b, uint16_t n)
{
    (void)i; (void)id; (void)t; (void)b; (void)n;
}

void app_main(void)
{
    ESP_ERROR_CHECK(bsp_usb_init());
    esp_lcd_panel_handle_t panel = NULL;
    ESP_ERROR_CHECK(bsp_display_new(NULL, &panel, NULL));
    ESP_ERROR_CHECK(bsp_display_backlight_on());
    esp_lcd_touch_handle_t tp = NULL;
    ESP_ERROR_CHECK(bsp_touch_new(NULL, &tp));

    tinyusb_config_t tusb_cfg = TINYUSB_DEFAULT_CONFIG();
    tusb_cfg.descriptor.device = NULL;
    tusb_cfg.descriptor.full_speed_config = hid_configuration_descriptor;
    tusb_cfg.descriptor.string = hid_string_descriptor;
    tusb_cfg.descriptor.string_count = 5;
#if (TUD_OPT_HIGH_SPEED)
    tusb_cfg.descriptor.high_speed_config = hid_configuration_descriptor;
#endif
    ESP_ERROR_CHECK(tinyusb_driver_install(&tusb_cfg));
    ESP_LOGI(TAG, "USB HID + touch, plug Type-A to PC");

    bool down = false;
    uint16_t lx = 0, ly = 0;
    while (1) {
        if (esp_lcd_touch_read_data(tp) == ESP_OK) {
            esp_lcd_touch_point_data_t pts[CONFIG_ESP_LCD_TOUCH_MAX_POINTS] = {0};
            uint8_t n = 0;
            if (esp_lcd_touch_get_data(tp, pts, &n, CONFIG_ESP_LCD_TOUCH_MAX_POINTS) == ESP_OK && n > 0) {
                int8_t dx = 0, dy = 0;
                if (down) {
                    int32_t x = (int32_t)pts[0].x - lx;
                    int32_t y = (int32_t)pts[0].y - ly;
                    dx = x > 127 ? 127 : x < -127 ? -127 : (int8_t)x;
                    dy = y > 127 ? 127 : y < -127 ? -127 : (int8_t)y;
                }
                if (tud_mounted() && tud_hid_ready()) {
                    tud_hid_mouse_report(0, MOUSE_BUTTON_LEFT, dx, dy, 0, 0);
                }
                lx = pts[0].x;
                ly = pts[0].y;
                down = true;
            } else if (down) {
                if (tud_mounted() && tud_hid_ready()) {
                    tud_hid_mouse_report(0, 0, 0, 0, 0, 0);
                }
                down = false;
            }
        }
        vTaskDelay(pdMS_TO_TICKS(20));
    }
}
'''

LED = r'''#include "bsp/esp-bsp.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

static const char *TAG = "bsp_led";

void app_main(void)
{
    ESP_ERROR_CHECK(bsp_led_rgb_init());
    ESP_ERROR_CHECK(bsp_buzzer_init());
    ESP_LOGI(TAG, "WS2812 + buzzer");
    const uint8_t c[][3] = {{48,0,0},{0,48,0},{0,0,48},{48,48,48}};
    int i = 0;
    while (1) {
        ESP_ERROR_CHECK(bsp_led_rgb_set(c[i][0], c[i][1], c[i][2]));
        ESP_ERROR_CHECK(bsp_buzzer_beep(120, 0));
        i = (i + 1) % 4;
        vTaskDelay(pdMS_TO_TICKS(800));
    }
}
'''

README = r'''# BSP verification apps

These projects use `bsp_ues31s043h800v480c_u` only. Original examples under `02_s31_examples` are not modified.

| Folder | Original coverage |
|--------|-------------------|
| `01_display_touch` | LCD + GT911 + backlight PWM |
| `02_audio_speaker` | ES8389 speaker / PA |
| `03_audio_mic` | ES8389 mic + SD |
| `04_sdcard` | TF / SDMMC |
| `05_adc_buttons` | SW3–SW6 |
| `06_uart1` | UART1 GPIO33/34 |
| `07_rs485` | RS485 GPIO35/36 |
| `08_can` | CAN GPIO53/54 |
| `09_wifi_sta` | Wi-Fi 6 Station |
| `10_ble_gatt` | BLE advertise |
| `11_bt_spp` | Classic Bluetooth SPP |
| `12_usb_hid` | USB 2.0 HS HID + touch |
| `13_led_buzzer` | WS2812 + buzzer |

```powershell
. C:\Espressif\tools\Microsoft.master.PowerShell_profile.ps1
$env:PYTHONUTF8 = "1"
cd E:\idf-debuging\02_s31_examples\bsp_verify\01_display_touch
idf.py --preview set-target esp32s31
idf.py --preview -p COMx flash monitor
```

Change SSID in `09_wifi_sta` if needed. BLE and Classic BT must not be enabled in the same firmware.
'''


def main():
    os.makedirs(ROOT, exist_ok=True)
    write(os.path.join(ROOT, "README.md"), README)
    project("01_display_touch", DISPLAY)
    project("02_audio_speaker", SPEAKER)
    project("03_audio_mic", MIC)
    project("04_sdcard", SD)
    project("05_adc_buttons", BUTTONS)
    project("06_uart1", UART1)
    project("07_rs485", RS485)
    project("08_can", CAN)
    project("09_wifi_sta", WIFI, extra_sdk="CONFIG_ESP_WIFI_SOFTAP_SUPPORT=n\n")
    project(
        "10_ble_gatt",
        BLE,
        extra_sdk="CONFIG_PARTITION_TABLE_SINGLE_APP_LARGE=y\n"
        "CONFIG_BT_ENABLED=y\n"
        "CONFIG_BTDM_CTRL_MODE_BLE_ONLY=y\n"
        "CONFIG_BT_BLE_42_FEATURES_SUPPORTED=y\n",
    )
    project(
        "11_bt_spp",
        SPP,
        extra_sdk="CONFIG_PARTITION_TABLE_SINGLE_APP_LARGE=y\n"
        "CONFIG_BT_ENABLED=y\n"
        "CONFIG_BTDM_CTRL_MODE_BR_EDR_ONLY=y\n"
        "CONFIG_BT_CLASSIC_ENABLED=y\n"
        "CONFIG_BT_SPP_ENABLED=y\n"
        "CONFIG_BT_BLE_ENABLED=n\n",
    )
    project(
        "12_usb_hid",
        USB,
        extra_yml="  espressif/esp_tinyusb: \"^2.0.1~1\"\n",
    )
    project("13_led_buzzer", LED)


if __name__ == "__main__":
    main()
