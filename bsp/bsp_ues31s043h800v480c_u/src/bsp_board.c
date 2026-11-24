/*
 * SPDX-FileCopyrightText: 2026 VIEWE
 * SPDX-License-Identifier: Apache-2.0
 */

#include <inttypes.h>
#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_check.h"
#include "esp_log.h"
#include "esp_adc/adc_oneshot.h"
#include "esp_event.h"
#include "esp_netif.h"
#include "esp_vfs_fat.h"
#include "esp_wifi.h"
#include "nvs_flash.h"
#include "button_adc.h"
#include "driver/gpio.h"
#include "driver/ledc.h"
#include "driver/uart.h"
#include "bsp/esp-bsp.h"
#include "bsp_err_check.h"

static const char *TAG = "BSP";

static i2c_master_bus_handle_t s_i2c;
static bool s_i2c_inited;
static sdmmc_card_t *s_sd_card;
static adc_oneshot_unit_handle_t s_adc;
static adc_unit_t s_adc_unit;
static adc_channel_t s_adc_channel;
static led_strip_handle_t s_led;
static bool s_buzzer_inited;

typedef struct {
    uint16_t min_mv;
    uint16_t max_mv;
} bsp_adc_btn_win_t;

static const bsp_adc_btn_win_t s_btn_win[BSP_BUTTON_NUM] = {
    {100, 600},
    {600, 1080},
    {1080, 1605},
    {1605, 2200},
};

esp_err_t bsp_i2c_init(void)
{
    if (s_i2c_inited) {
        return ESP_OK;
    }
    const i2c_master_bus_config_t cfg = {
        .i2c_port = CONFIG_BSP_I2C_NUM,
        .sda_io_num = BSP_I2C_SDA,
        .scl_io_num = BSP_I2C_SCL,
        .clk_source = I2C_CLK_SRC_DEFAULT,
        .glitch_ignore_cnt = 7,
        .flags.enable_internal_pullup = true,
    };
    BSP_ERROR_CHECK_RETURN_ERR(i2c_new_master_bus(&cfg, &s_i2c));
    s_i2c_inited = true;
    return ESP_OK;
}

esp_err_t bsp_i2c_deinit(void)
{
    if (!s_i2c_inited) {
        return ESP_OK;
    }
    BSP_ERROR_CHECK_RETURN_ERR(i2c_del_master_bus(s_i2c));
    s_i2c = NULL;
    s_i2c_inited = false;
    return ESP_OK;
}

i2c_master_bus_handle_t bsp_i2c_get_handle(void)
{
    if (!s_i2c_inited) {
        bsp_i2c_init();
    }
    return s_i2c;
}

esp_err_t bsp_adc_initialize(void)
{
    if (s_adc) {
        return ESP_OK;
    }
    BSP_ERROR_CHECK_RETURN_ERR(adc_oneshot_io_to_channel(BSP_BUTTONS_ADC_GPIO, &s_adc_unit, &s_adc_channel));
    adc_oneshot_unit_init_cfg_t unit_cfg = {
        .unit_id = s_adc_unit,
    };
    BSP_ERROR_CHECK_RETURN_ERR(adc_oneshot_new_unit(&unit_cfg, &s_adc));
    adc_oneshot_chan_cfg_t chan_cfg = {
        .bitwidth = ADC_BITWIDTH_DEFAULT,
        .atten = ADC_ATTEN_DB_0,
    };
    return adc_oneshot_config_channel(s_adc, s_adc_channel, &chan_cfg);
}

int bsp_adc_read_raw(void)
{
    if (bsp_adc_initialize() != ESP_OK) {
        return -1;
    }
    int raw = 0;
    int sum = 0;
    for (int i = 0; i < 8; i++) {
        if (adc_oneshot_read(s_adc, s_adc_channel, &raw) != ESP_OK) {
            return -1;
        }
        sum += raw;
    }
    return sum / 8;
}

int bsp_adc_read_mv(void)
{
    int raw = bsp_adc_read_raw();
    if (raw < 0) {
        return -1;
    }
    /*
     * GPIO42 is ADC1 CH0 N-side. IDF: single-ended N-side is inverted,
     * -2 V..+2 V maps to raw 4393..0. S31 oneshot is 17-bit; if the
     * reading is still on that 4393 scale, do not shift.
     */
    if (raw > 8191) {
        raw >>= 5;
    }
    int voltage = (int32_t)raw * 4 * 1000 / 4393 - 2;
    return voltage < 0 ? 0 : voltage;
}

esp_err_t bsp_sdcard_mount(void)
{
    if (s_sd_card) {
        return ESP_OK;
    }

    gpio_reset_pin(BSP_SD_POWER);
    gpio_config_t pwr = {
        .mode = GPIO_MODE_OUTPUT,
        .pin_bit_mask = 1ULL << BSP_SD_POWER,
        .pull_up_en = GPIO_PULLUP_DISABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
    };
    BSP_ERROR_CHECK_RETURN_ERR(gpio_config(&pwr));
    gpio_set_level(BSP_SD_POWER, 0); /* V1.1 SD_CTRL active low; initial boards ignore this pin */
    vTaskDelay(pdMS_TO_TICKS(200));

    sdmmc_host_t host = SDMMC_HOST_DEFAULT();
    host.slot = SDMMC_HOST_SLOT_0;
    host.max_freq_khz = SDMMC_FREQ_DEFAULT;

    sdmmc_slot_config_t slot = SDMMC_SLOT_CONFIG_DEFAULT();
    slot.width = 4;
    slot.clk = BSP_SD_CLK;
    slot.cmd = BSP_SD_CMD;
    slot.d0 = BSP_SD_D0;
    slot.d1 = BSP_SD_D1;
    slot.d2 = BSP_SD_D2;
    slot.d3 = BSP_SD_D3;
    slot.cd = SDMMC_SLOT_NO_CD;
    slot.wp = SDMMC_SLOT_NO_WP;
    slot.flags = SDMMC_SLOT_FLAG_INTERNAL_PULLUP;

    esp_vfs_fat_sdmmc_mount_config_t mount = {
#ifdef CONFIG_BSP_SD_FORMAT_ON_MOUNT_FAIL
        .format_if_mount_failed = true,
#else
        .format_if_mount_failed = false,
#endif
        .max_files = 8,
        .allocation_unit_size = 16 * 1024,
    };
    esp_err_t err = ESP_FAIL;
    for (int i = 0; i < 3; i++) {
        err = esp_vfs_fat_sdmmc_mount(BSP_SDCARD_MOUNT_POINT, &host, &slot, &mount, &s_sd_card);
        if (err == ESP_OK) {
            ESP_LOGI(TAG, "SD mounted at %s", BSP_SDCARD_MOUNT_POINT);
            return ESP_OK;
        }
        ESP_LOGW(TAG, "SD mount try %d failed: %s", i + 1, esp_err_to_name(err));
        vTaskDelay(pdMS_TO_TICKS(200));
    }
    return err;
}

esp_err_t bsp_sdcard_unmount(void)
{
    if (s_sd_card == NULL) {
        return ESP_OK;
    }
    esp_err_t err = esp_vfs_fat_sdcard_unmount(BSP_SDCARD_MOUNT_POINT, s_sd_card);
    if (err != ESP_OK) {
        return err;
    }
    s_sd_card = NULL;
    gpio_set_level(BSP_SD_POWER, 1); /* V1.1 AO3401A off; initial boards stay powered */
    return ESP_OK;
}

sdmmc_card_t *bsp_sdcard_get_handle(void)
{
    return s_sd_card;
}

esp_err_t bsp_iot_button_create(button_handle_t btn_array[], int *btn_cnt, int btn_array_size)
{
    BSP_NULL_CHECK(btn_array, ESP_ERR_INVALID_ARG);
    if (btn_array_size < BSP_BUTTON_NUM) {
        return ESP_ERR_INVALID_ARG;
    }
    BSP_ERROR_CHECK_RETURN_ERR(bsp_adc_initialize());

    const button_config_t btn_cfg = {0};
    for (int i = 0; i < BSP_BUTTON_NUM; i++) {
        button_adc_config_t adc_cfg = {
            .adc_handle = &s_adc,
            .unit_id = s_adc_unit,
            .adc_channel = s_adc_channel,
            .button_index = i,
            .min = s_btn_win[i].min_mv,
            .max = s_btn_win[i].max_mv,
        };
        BSP_ERROR_CHECK_RETURN_ERR(iot_button_new_adc_device(&btn_cfg, &adc_cfg, &btn_array[i]));
    }
    if (btn_cnt) {
        *btn_cnt = BSP_BUTTON_NUM;
    }
    return ESP_OK;
}

esp_err_t bsp_led_rgb_init(void)
{
    if (s_led) {
        return ESP_OK;
    }
    led_strip_config_t strip_cfg = {
        .strip_gpio_num = BSP_RGB_LED_IO,
        .max_leds = 1,
        .led_model = LED_MODEL_WS2812,
        .color_component_format = LED_STRIP_COLOR_COMPONENT_FMT_GRB,
    };
    led_strip_rmt_config_t rmt_cfg = {
        .clk_src = RMT_CLK_SRC_DEFAULT,
        .resolution_hz = 10 * 1000 * 1000,
    };
    BSP_ERROR_CHECK_RETURN_ERR(led_strip_new_rmt_device(&strip_cfg, &rmt_cfg, &s_led));
    return led_strip_clear(s_led);
}

esp_err_t bsp_led_rgb_set(uint8_t r, uint8_t g, uint8_t b)
{
    BSP_ERROR_CHECK_RETURN_ERR(bsp_led_rgb_init());
    BSP_ERROR_CHECK_RETURN_ERR(led_strip_set_pixel(s_led, 0, r, g, b));
    return led_strip_refresh(s_led);
}

esp_err_t bsp_buzzer_init(void)
{
    if (s_buzzer_inited) {
        return ESP_OK;
    }
    ledc_timer_config_t timer = {
        .speed_mode = LEDC_LOW_SPEED_MODE,
        .duty_resolution = LEDC_TIMER_13_BIT,
        .timer_num = CONFIG_BSP_BUZZER_LEDC_TIMER,
        .freq_hz = 3000,
        .clk_cfg = LEDC_AUTO_CLK,
    };
    BSP_ERROR_CHECK_RETURN_ERR(ledc_timer_config(&timer));
    ledc_channel_config_t ch = {
        .speed_mode = LEDC_LOW_SPEED_MODE,
        .channel = CONFIG_BSP_BUZZER_LEDC_CH,
        .timer_sel = CONFIG_BSP_BUZZER_LEDC_TIMER,
        .gpio_num = BSP_BUZZER_IO,
        .duty = 0,
        .hpoint = 0,
    };
    BSP_ERROR_CHECK_RETURN_ERR(ledc_channel_config(&ch));
    s_buzzer_inited = true;
    return ESP_OK;
}

esp_err_t bsp_buzzer_set(bool on)
{
    BSP_ERROR_CHECK_RETURN_ERR(bsp_buzzer_init());
    uint32_t duty = on ? ((1 << 13) / 2) : 0;
    BSP_ERROR_CHECK_RETURN_ERR(ledc_set_duty(LEDC_LOW_SPEED_MODE, CONFIG_BSP_BUZZER_LEDC_CH, duty));
    return ledc_update_duty(LEDC_LOW_SPEED_MODE, CONFIG_BSP_BUZZER_LEDC_CH);
}

esp_err_t bsp_buzzer_beep(uint32_t on_ms, uint32_t off_ms)
{
    BSP_ERROR_CHECK_RETURN_ERR(bsp_buzzer_set(true));
    vTaskDelay(pdMS_TO_TICKS(on_ms));
    BSP_ERROR_CHECK_RETURN_ERR(bsp_buzzer_set(false));
    if (off_ms) {
        vTaskDelay(pdMS_TO_TICKS(off_ms));
    }
    return ESP_OK;
}

static esp_err_t uart_setup(uart_port_t port, int tx, int rx, uint32_t baud)
{
    uart_config_t cfg = {
        .baud_rate = (int)baud,
        .data_bits = UART_DATA_8_BITS,
        .parity = UART_PARITY_DISABLE,
        .stop_bits = UART_STOP_BITS_1,
        .flow_ctrl = UART_HW_FLOWCTRL_DISABLE,
        .source_clk = UART_SCLK_DEFAULT,
    };
    BSP_ERROR_CHECK_RETURN_ERR(uart_driver_install(port, 2048, 1024, 0, NULL, 0));
    BSP_ERROR_CHECK_RETURN_ERR(uart_param_config(port, &cfg));
    BSP_ERROR_CHECK_RETURN_ERR(uart_set_pin(port, tx, rx, UART_PIN_NO_CHANGE, UART_PIN_NO_CHANGE));
    return uart_flush(port);
}

esp_err_t bsp_uart1_init(uint32_t baud_rate)
{
    ESP_LOGI(TAG, "UART1 TX=%d RX=%d baud=%" PRIu32, BSP_UART1_TX, BSP_UART1_RX, baud_rate);
    return uart_setup(UART_NUM_1, BSP_UART1_TX, BSP_UART1_RX, baud_rate);
}

esp_err_t bsp_rs485_init(uint32_t baud_rate)
{
    ESP_LOGI(TAG, "RS485 UART2 TX=%d RX=%d (auto DE) baud=%" PRIu32,
             BSP_RS485_TX, BSP_RS485_RX, baud_rate);
    BSP_ERROR_CHECK_RETURN_ERR(uart_setup(UART_NUM_2, BSP_RS485_TX, BSP_RS485_RX, baud_rate));
    return uart_set_mode(UART_NUM_2, UART_MODE_UART);
}

esp_err_t bsp_rs485_write(const void *data, size_t len)
{
    if (!data || !len) {
        return ESP_ERR_INVALID_ARG;
    }
    int n = uart_write_bytes(UART_NUM_2, data, len);
    if (n < 0) {
        return ESP_FAIL;
    }
    return uart_wait_tx_done(UART_NUM_2, pdMS_TO_TICKS(100));
}

#if defined(CONFIG_SOC_TWAI_SUPPORTED)
esp_err_t bsp_can_init(uint32_t bitrate, bool self_test, twai_node_handle_t *ret_node)
{
    BSP_NULL_CHECK(ret_node, ESP_ERR_INVALID_ARG);
    twai_onchip_node_config_t cfg = {
        .io_cfg = {
            .tx = BSP_CAN_TX,
            .rx = BSP_CAN_RX,
            .quanta_clk_out = GPIO_NUM_NC,
            .bus_off_indicator = GPIO_NUM_NC,
        },
        .bit_timing = {
            .bitrate = bitrate,
        },
        .fail_retry_cnt = 3,
        .tx_queue_depth = 8,
        .flags = {
            .enable_self_test = self_test,
            .enable_loopback = self_test,
        },
    };
    BSP_ERROR_CHECK_RETURN_ERR(twai_new_node_onchip(&cfg, ret_node));
    BSP_ERROR_CHECK_RETURN_ERR(twai_node_enable(*ret_node));
    ESP_LOGI(TAG, "CAN TX=%d RX=%d bitrate=%" PRIu32 " self_test=%d loopback=%d",
             BSP_CAN_TX, BSP_CAN_RX, bitrate, self_test, self_test);
    return ESP_OK;
}
#endif

esp_err_t bsp_usb_init(void)
{
    ESP_LOGI(TAG, "USB 2.0 HS PHY on dedicated USB_DP/USB_DM (pkg %d/%d), not GPIO",
             BSP_USB_DP_PIN, BSP_USB_DM_PIN);
    return ESP_OK;
}

esp_err_t bsp_bt_nvs_init(void)
{
    esp_err_t err = nvs_flash_init();
    if (err == ESP_ERR_NVS_NO_FREE_PAGES || err == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        err = nvs_flash_init();
    }
    return err;
}

static void bsp_wifi_event_handler(void *arg, esp_event_base_t base, int32_t id, void *data)
{
    (void)arg;
    (void)data;
    if (base == WIFI_EVENT && id == WIFI_EVENT_STA_START) {
        esp_wifi_connect();
    } else if (base == WIFI_EVENT && id == WIFI_EVENT_STA_DISCONNECTED) {
        ESP_LOGW(TAG, "Wi-Fi disconnected, reconnect");
        esp_wifi_connect();
    }
}

esp_err_t bsp_wifi_init_sta(const char *ssid, const char *password)
{
    BSP_NULL_CHECK(ssid, ESP_ERR_INVALID_ARG);
    BSP_ERROR_CHECK_RETURN_ERR(bsp_bt_nvs_init());
    BSP_ERROR_CHECK_RETURN_ERR(esp_netif_init());
    esp_err_t err = esp_event_loop_create_default();
    if (err != ESP_OK && err != ESP_ERR_INVALID_STATE) {
        return err;
    }
    if (esp_netif_get_handle_from_ifkey("WIFI_STA_DEF") == NULL) {
        esp_netif_create_default_wifi_sta();
    }

    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    BSP_ERROR_CHECK_RETURN_ERR(esp_wifi_init(&cfg));
    BSP_ERROR_CHECK_RETURN_ERR(esp_event_handler_register(WIFI_EVENT, WIFI_EVENT_STA_START,
                                                          bsp_wifi_event_handler, NULL));
    BSP_ERROR_CHECK_RETURN_ERR(esp_event_handler_register(WIFI_EVENT, WIFI_EVENT_STA_DISCONNECTED,
                                                          bsp_wifi_event_handler, NULL));

    wifi_config_t wifi_config = {0};
    strncpy((char *)wifi_config.sta.ssid, ssid, sizeof(wifi_config.sta.ssid) - 1);
    if (password) {
        strncpy((char *)wifi_config.sta.password, password, sizeof(wifi_config.sta.password) - 1);
    }
    wifi_config.sta.threshold.authmode = WIFI_AUTH_WPA2_PSK;

    BSP_ERROR_CHECK_RETURN_ERR(esp_wifi_set_mode(WIFI_MODE_STA));
    BSP_ERROR_CHECK_RETURN_ERR(esp_wifi_set_config(WIFI_IF_STA, &wifi_config));
    BSP_ERROR_CHECK_RETURN_ERR(esp_wifi_start());
    ESP_LOGI(TAG, "Wi-Fi STA start SSID=%s", ssid);
    return ESP_OK;
}
