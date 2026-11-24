/*
 * SPDX-FileCopyrightText: 2026 VIEWE
 * SPDX-License-Identifier: Apache-2.0
 */

/**
 * @file
 * @brief Board Support Package for the VIEWE DeskNob (ESP32-S31, 2.1" round knob display).
 *
 * Pins below are the ones used by the working classic-Bluetooth speaker example
 * on this board, plus the dock RGB data pin and the battery sense pin from the
 * Core_S31 / Dock_2 schematics.
 */

#pragma once

#include <stdbool.h>
#include <stdint.h>

#include "sdkconfig.h"
#include "driver/gpio.h"
#include "driver/i2c_master.h"
#include "driver/i2s_std.h"
#include "esp_codec_dev.h"
#include "esp_lv_adapter.h"
#include "iot_button.h"
#include "lvgl.h"

#include "bsp/display.h"
#include "bsp/touch.h"

#ifdef __cplusplus
extern "C" {
#endif

/**************************************************************************************************
 * BSP Capabilities
 **************************************************************************************************/

#define BSP_CAPS_DISPLAY        1
#define BSP_CAPS_TOUCH          1
#define BSP_CAPS_BUTTONS        1
#define BSP_CAPS_KNOB           0
#define BSP_CAPS_AUDIO          1
#define BSP_CAPS_AUDIO_SPEAKER  1
#define BSP_CAPS_AUDIO_MIC      1
#define BSP_CAPS_SDCARD         1
#define BSP_CAPS_IMU            0
#define BSP_CAPS_LED            1
#define BSP_CAPS_BATTERY        1
#define BSP_CAPS_WIFI           1
#define BSP_CAPS_BT_CLASSIC     1
#define BSP_CAPS_BT_BLE         1
#if CONFIG_BSP_USB_MSC
#define BSP_CAPS_USB_MSC        1
#else
#define BSP_CAPS_USB_MSC        0
#endif

/**************************************************************************************************
 * Pinout
 *
 * Core display / touch (FPC1)
 *   LCD QSPI: CS=17 SCL=18 D0=16 D1=15 D2=14 D3=13 RST=12 TE=19 BL=7
 *   Touch I2C: SDA=10 SCL=9 RST=8 INT=11
 *
 * Core SDMMC 4-bit
 *   D0=20 D1=21 D2=22 D3=23 CLK=24 CMD=25
 *   Card-detect pin is not on the known IO map.
 *
 * Dock ES8311
 *   I2C SCL=35 SDA=40
 *   I2S BCK=43 WS=45 DO=46 DI=44
 *   PA_CTRL=47
 *   Schematic MCLK is GPIO42. The working firmware does not drive MCLK.
 *
 * Dual detection switches (replace the mechanical encoder)
 *   CCW / volume-down side = GPIO3 (former EC1_A), active low, idle high
 *   CW  / volume-up side   = GPIO2 (former EC1_B), active low, idle high
 *
 * Dock RGB strip
 *   RGB_DATA = GPIO61, 12x XL-1010RGBC-2812B, series 100 ohm.
 *   GPIO61 is also the BOOT strap. The strip driver parks the pin high after each frame.
 *
 * Dock power
 *   GPIO51 = BAT_ADC = ADC2 channel 1.
 *   VCC_OUT -- R1 15k -- BAT_ADC -- R21 10k -- GND.
 *   TP4055 CHRG is not wired to the MCU.
 *
 * USB 2.0 device
 *   Internal UTMI PHY, USB OTG High Speed. Pads are USB_DP and USB_DM.
 *   Not GPIO-mapped. VBUS is on the connector and is not wired to a GPIO.
 *
 * Wi-Fi, Classic Bluetooth and BLE use the on-chip radio. No board GPIO.
 **************************************************************************************************/

/* Audio codec I2C */
#define BSP_I2C_SCL                 (GPIO_NUM_35)
#define BSP_I2C_SDA                 (GPIO_NUM_40)
#define BSP_I2C_NUM                 ((i2c_port_t)CONFIG_BSP_I2C_NUM)

/* Touch I2C (separate bus from the codec; use the other port from CONFIG_BSP_I2C_NUM) */
#define BSP_TOUCH_I2C_SCL           (GPIO_NUM_9)
#define BSP_TOUCH_I2C_SDA           (GPIO_NUM_10)
#if CONFIG_BSP_I2C_NUM == 0
#define BSP_TOUCH_I2C_NUM           (I2C_NUM_1)
#else
#define BSP_TOUCH_I2C_NUM           (I2C_NUM_0)
#endif

/* QSPI LCD GC9B72 */
#define BSP_LCD_SPI_NUM             (SPI2_HOST)
#define BSP_LCD_CS                  (GPIO_NUM_17)
#define BSP_LCD_PCLK                (GPIO_NUM_18)
#define BSP_LCD_DATA0               (GPIO_NUM_16)
#define BSP_LCD_DATA1               (GPIO_NUM_15)
#define BSP_LCD_DATA2               (GPIO_NUM_14)
#define BSP_LCD_DATA3               (GPIO_NUM_13)
#define BSP_LCD_RST                 (GPIO_NUM_12)
#define BSP_LCD_TE                  (GPIO_NUM_19)
#define BSP_LCD_BACKLIGHT           (GPIO_NUM_7)
#define BSP_LCD_BK_LIGHT_ON_LEVEL   (1)

#define BSP_LCD_TOUCH_SDA           BSP_TOUCH_I2C_SDA
#define BSP_LCD_TOUCH_SCL           BSP_TOUCH_I2C_SCL
#define BSP_LCD_TOUCH_RST           (GPIO_NUM_8)
#define BSP_LCD_TOUCH_INT           (GPIO_NUM_11)

/* SD card. No card-detect GPIO is known. */
#define BSP_SD_CLK                  (GPIO_NUM_24)
#define BSP_SD_CMD                  (GPIO_NUM_25)
#define BSP_SD_D0                   (GPIO_NUM_20)
#define BSP_SD_D1                   (GPIO_NUM_21)
#define BSP_SD_D2                   (GPIO_NUM_22)
#define BSP_SD_D3                   (GPIO_NUM_23)
#define BSP_SD_DET                  (GPIO_NUM_NC)

/* ES8311 + speaker amplifier */
#define BSP_AUDIO_I2S_MCLK          (GPIO_NUM_NC)
#define BSP_AUDIO_I2S_SCLK          (GPIO_NUM_43)
#define BSP_AUDIO_I2S_LCLK          (GPIO_NUM_45)
#define BSP_AUDIO_I2S_DOUT          (GPIO_NUM_46)
#define BSP_AUDIO_I2S_DSIN          (GPIO_NUM_44)
#define BSP_POWER_AMP_IO            (GPIO_NUM_47)

#define BSP_AUDIO_DEFAULT_SAMPLE_RATE       (48000)
#define BSP_AUDIO_DEFAULT_BITS_PER_SAMPLE   (16)
#define BSP_AUDIO_DEFAULT_CHANNELS          (2)

/* Dual-side detection switches */
#define BSP_BUTTON_CCW_IO           (GPIO_NUM_3)
#define BSP_BUTTON_CW_IO            (GPIO_NUM_2)
#define BSP_BUTTON_ACTIVE_LEVEL     (0)

/* RGB LED strip. Shared with the BOOT strap. */
#define BSP_LED_STRIP_IO            (GPIO_NUM_61)
#define BSP_LED_STRIP_COUNT         (CONFIG_BSP_LED_STRIP_COUNT)

/* Battery / system-rail sense. VCC_OUT -- 15k -- GPIO51 -- 10k -- GND. */
#define BSP_BATTERY_ADC_IO          (GPIO_NUM_51)
#define BSP_BATTERY_UPPER_OHM       (15000)
#define BSP_BATTERY_LOWER_OHM       (10000)

/** @defgroup g01_i2c I2C
 *  @{
 */

/**
 * @brief Initialize the audio-codec I2C master.
 *
 * Safe to call more than once. The touch bus is owned by bsp_touch_new().
 */
esp_err_t bsp_i2c_init(void);

/**
 * @brief Deinitialize the audio-codec I2C master.
 */
esp_err_t bsp_i2c_deinit(void);

/**
 * @brief Audio-codec I2C bus handle, or NULL if bsp_i2c_init() has not been called.
 */
i2c_master_bus_handle_t bsp_i2c_get_handle(void);

/** @} */

/** @defgroup g02_audio Audio
 *  @{
 */

/**
 * @brief Initialize duplex I2S for the ES8311.
 *
 * @param[in] i2s_config Philips slot configuration. NULL selects 48 kHz stereo.
 */
esp_err_t bsp_audio_init(const i2s_std_config_t *i2s_config);

/**
 * @brief I2S data interface used by the codec devices. NULL before bsp_audio_init().
 */
const audio_codec_data_if_t *bsp_audio_get_codec_itf(void);

/**
 * @brief ES8311 speaker (DAC) codec device.
 *
 * Playback path: I2S DOUT GPIO46, PA enable GPIO47.
 */
esp_codec_dev_handle_t bsp_audio_codec_speaker_init(void);

/**
 * @brief ES8311 microphone (ADC) codec device.
 *
 * Capture path: I2S DIN GPIO44. The codec is the same ES8311 as the speaker.
 */
esp_codec_dev_handle_t bsp_audio_codec_microphone_init(void);

/** @} */

/** @defgroup g03_sd SD Card
 *  @{
 */

#define BSP_SD_MOUNT_POINT          "/sdcard"
#define BSP_SDCARD_MOUNT_POINT      BSP_SD_MOUNT_POINT

/**
 * @brief Mount the SD card (SDMMC 4-bit, FAT).
 *
 * There is no card-detect pin. A missing card returns an error from the driver.
 */
esp_err_t bsp_sdcard_mount(void);

/**
 * @brief Unmount the SD card.
 */
esp_err_t bsp_sdcard_unmount(void);

/** @} */

/** @defgroup g06_buttons Buttons
 *  @brief Two active-low detection switches used instead of a quadrature encoder.
 *  @{
 */

typedef enum {
    BSP_BUTTON_CCW = 0, /*!< GPIO3. Volume-down side of the working example. */
    BSP_BUTTON_CW,      /*!< GPIO2. Volume-up side of the working example. */
    BSP_BUTTON_NUM,
} bsp_button_t;

/**
 * @brief Create both detection switches as espressif/button handles.
 *
 * @param[out] btn_array      Output array.
 * @param[out] btn_cnt        Number of created handles. Can be NULL.
 * @param[in]  btn_array_size Must be at least BSP_BUTTON_NUM.
 */
esp_err_t bsp_iot_button_create(button_handle_t btn_array[], int *btn_cnt, int btn_array_size);

/** @} */

/** @defgroup g07_led LED strip
 *  @brief 12x WS2812 on GPIO61. The pin is shared with BOOT and is parked high when idle.
 *  @{
 */

/**
 * @brief Initialize the RGB strip and turn every LED off.
 */
esp_err_t bsp_led_strip_init(void);

/**
 * @brief Set one LED and keep the change until bsp_led_refresh().
 */
esp_err_t bsp_led_set_rgb(int index, uint8_t red, uint8_t green, uint8_t blue);

/**
 * @brief Push the LED buffer to the strip.
 */
esp_err_t bsp_led_refresh(void);

/**
 * @brief Turn every LED off and refresh.
 */
esp_err_t bsp_led_clear(void);

/** @} */

/** @defgroup g07b_battery Battery
 *  @brief GPIO51 senses VCC_OUT, the rail after the dock power switch.
 *
 *  Limitations (current hardware + IDF):
 *  - ESP32-S31 has no official ADC calibration scheme yet
 *    (`adc_cali_schemes.h` leaves curve-fitting as TODO), so mV is uncalibrated.
 *  - Sense node is VCC_OUT via 15k/10k, not the cell terminal.
 *  - TP4055 CHRG is not wired; there is no fuel gauge.
 *  Percent is only a Li-ion voltage estimate (4.20 V ≈ 100%, 3.30 V ≈ 0%).
 *  @{
 */

/**
 * @brief Initialize the battery-sense ADC.
 *
 * Safe to call more than once. bsp_battery_get_voltage_mv() calls this if needed.
 */
esp_err_t bsp_battery_init(void);

/**
 * @brief Whether IDF reports an ADC calibration scheme for this chip.
 *
 * On ESP32-S31 this currently returns false.
 */
bool bsp_battery_adc_calibrated(void);

/**
 * @brief Read VCC_OUT in millivolts (uncalibrated linear estimate).
 *
 * Divider: R1 15 kohm over R21 10 kohm. Mapping uses P-side codes
 * (0 = -2 V, 4393 = +2 V) because no calibration scheme is available.
 *
 * @param[out] voltage_mv System rail voltage. NULL returns ESP_ERR_INVALID_ARG.
 */
esp_err_t bsp_battery_get_voltage_mv(int *voltage_mv);

/**
 * @brief Estimated battery state of charge (not a fuel gauge).
 *
 * On battery: maps filtered VCC_OUT with a Li-ion curve.
 * While USB/charge is inferred: returns a slow time-based estimate from the
 * last on-battery percent (1% / 2 min, capped at 99). The lifted rail is not
 * used for SoC. After unplug, SoC is remeasured from the rail (no reboot needed).
 *
 * @param[out] percent      0..100, or -1 if unknown. May be NULL.
 * @param[out] usb_powered  true when VCC_OUT looks like USB/charger. May be NULL.
 */
esp_err_t bsp_battery_get_level(int *percent, bool *usb_powered);

/** @} */

/** @defgroup g08_spiffs SPIFFS
 *  @{
 */

esp_err_t bsp_spiffs_mount(void);
esp_err_t bsp_spiffs_unmount(void);

/** @} */

/** @defgroup g09_wifi Wi-Fi
 *  @brief On-chip radio. No antenna GPIO is exposed by this board.
 *  @{
 */

/**
 * @brief Initialize NVS, netif and the Wi-Fi STA driver, then start Wi-Fi.
 *
 * Does not connect to an access point.
 */
esp_err_t bsp_wifi_init(void);

/**
 * @brief Blocking scan. bsp_wifi_init() is called if needed.
 *
 * @param[out] ap_count Number of APs found. Can be NULL.
 */
esp_err_t bsp_wifi_scan(uint16_t *ap_count);

/** @} */

/** @defgroup g10_bt Bluetooth
 *  @brief Classic Bluetooth and BLE on the on-chip radio.
 *  @{
 */

typedef enum {
    BSP_BT_MODE_BLE = 1,
    BSP_BT_MODE_CLASSIC = 2,
    BSP_BT_MODE_BTDM = 3,
} bsp_bt_mode_t;

/**
 * @brief Initialize the BT controller and Bluedroid.
 *
 * The mode must also be enabled in menuconfig (BTDM for classic + BLE together).
 * This does not start A2DP or a GATT service. Those stay in the application.
 */
esp_err_t bsp_bt_init(bsp_bt_mode_t mode, const char *device_name);

/**
 * @brief Make Classic Bluetooth connectable and generally discoverable.
 */
esp_err_t bsp_bt_classic_set_discoverable(bool enable);

/**
 * @brief Start BLE advertising with the device name from bsp_bt_init().
 */
esp_err_t bsp_bt_ble_start_advertising(void);

/** @} */

/** @defgroup g12_usb USB mass storage
 *  @brief USB 2.0 High Speed device on the internal UTMI PHY (USB_DP / USB_DM).
 *
 *  Enable CONFIG_BSP_USB_MSC. bsp_usb_msc_init() uses a FAT partition whose
 *  label matches CONFIG_BSP_USB_MSC_PARTITION_LABEL (default "udisk").
 *  bsp_usb_msc_sdcard_init() exposes the SD card instead, and does not format it.
 *  @{
 */

/**
 * @brief Turn the FAT partition into a USB disk and expose it to the host.
 *
 * Creates README.TXT on first use, then gives the volume to the USB host.
 * Requires CONFIG_BSP_USB_MSC (otherwise returns ESP_ERR_NOT_SUPPORTED).
 */
esp_err_t bsp_usb_msc_init(void);

/**
 * @brief Expose the whole SD card as a USB disk.
 *
 * The card is not formatted and its existing filesystem is left as-is.
 * Do not call bsp_sdcard_mount() in the same boot: the PC owns the card.
 * Requires CONFIG_BSP_USB_MSC (otherwise returns ESP_ERR_NOT_SUPPORTED).
 *
 * @param[out] capacity_bytes Card size in bytes. Can be NULL.
 */
esp_err_t bsp_usb_msc_sdcard_init(uint64_t *capacity_bytes);

/** @} */

/** @defgroup g11_display_lvgl Display and LVGL
 *  @{
 */

/**
 * @brief LVGL display configuration.
 *
 * lvgl_port_cfg is the esp_lvgl_adapter configuration used on ESP-IDF 6.
 */
typedef struct {
    esp_lv_adapter_config_t lvgl_port_cfg;
    uint32_t buffer_size; /*!< Draw buffer size in pixels. 0 selects BSP_LCD_DRAW_BUFF_SIZE. */
    bool double_buffer;
    struct {
        unsigned int buff_dma: 1;
        unsigned int buff_spiram: 1;
    } flags;
} bsp_display_cfg_t;

/**
 * @brief Initialize the panel, touch and LVGL.
 *
 * Backlight stays off until bsp_display_backlight_on().
 */
lv_display_t *bsp_display_start(void);

/**
 * @brief Same as bsp_display_start() with an explicit configuration.
 *
 * @param[in] cfg Configuration. NULL selects the board default.
 */
lv_display_t *bsp_display_start_with_config(const bsp_display_cfg_t *cfg);

/**
 * @brief Touch input device registered with LVGL. NULL if touch failed.
 */
lv_indev_t *bsp_display_get_input_dev(void);

/**
 * @brief Take the LVGL mutex.
 *
 * @param timeout_ms 0 waits forever.
 */
bool bsp_display_lock(uint32_t timeout_ms);

/**
 * @brief Give the LVGL mutex.
 */
void bsp_display_unlock(void);

/**
 * @brief Rotate the LVGL display.
 */
void bsp_display_rotate(lv_display_t *disp, lv_display_rotation_t rotation);

/** @} */

#ifdef __cplusplus
}
#endif
