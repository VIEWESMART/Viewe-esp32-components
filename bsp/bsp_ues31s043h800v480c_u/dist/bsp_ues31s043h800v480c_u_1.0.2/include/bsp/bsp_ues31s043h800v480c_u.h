/*
 * SPDX-FileCopyrightText: 2026 VIEWE
 * SPDX-License-Identifier: Apache-2.0
 *
 * @file
 * @brief ESP BSP: UES31S043H800V480C-U (ESP32-S31 4.3" 800x480)
 */

#pragma once

#include "sdkconfig.h"
#include "driver/gpio.h"
#include "driver/i2c_master.h"
#include "driver/i2s_std.h"
#include "driver/sdmmc_host.h"
#include "driver/uart.h"
#include "esp_codec_dev.h"
#include "esp_err.h"
#include "esp_vfs_fat.h"
#include "iot_button.h"
#include "led_strip.h"
#include "bsp/config.h"
#include "bsp/display.h"
#include "bsp/touch.h"

#if (BSP_CONFIG_NO_GRAPHIC_LIB == 0)
#include "lvgl.h"
#include "esp_lvgl_port.h"
#endif

#if defined(CONFIG_SOC_TWAI_SUPPORTED)
#include "esp_twai.h"
#include "esp_twai_onchip.h"
#endif

/**************************************************************************************************
 *  BSP Board Name
 **************************************************************************************************/
#define BSP_BOARD_UES31S043H800V480C_U

/**************************************************************************************************
 *  BSP Capabilities
 **************************************************************************************************/
#define BSP_CAPS_DISPLAY          1
#define BSP_CAPS_TOUCH            1
#define BSP_CAPS_BUTTONS          1
#define BSP_CAPS_AUDIO            1
#define BSP_CAPS_AUDIO_SPEAKER    1
#define BSP_CAPS_AUDIO_MIC        1
#define BSP_CAPS_SDCARD           1
#define BSP_CAPS_LED              1
#define BSP_CAPS_BUZZER           1
#define BSP_CAPS_USB              1
#define BSP_CAPS_UART1            1
#define BSP_CAPS_RS485            1
#define BSP_CAPS_CAN              1
#define BSP_CAPS_WIFI             1
#define BSP_CAPS_BLE              1
#define BSP_CAPS_BT_CLASSIC       1
#define BSP_CAPS_IMU              0

/**************************************************************************************************
 *  Board pinout
 **************************************************************************************************/

/** @defgroup g01_i2c I2C */
#define BSP_I2C_SCL               (GPIO_NUM_1)
#define BSP_I2C_SDA               (GPIO_NUM_0)

/** @defgroup g03_audio Audio */
#define BSP_I2S_MCLK              (GPIO_NUM_48) /* CODEC_I2S_MCLK */
#define BSP_I2S_SCLK              (GPIO_NUM_49)
#define BSP_I2S_LCLK              (GPIO_NUM_50)
#define BSP_I2S_DOUT              (GPIO_NUM_51)
#define BSP_I2S_DSIN              (GPIO_NUM_52)
#define BSP_POWER_AMP_IO          (GPIO_NUM_47)

/** @defgroup g04_display Display and Touch */
#define BSP_LCD_PCLK              (GPIO_NUM_40)
#define BSP_LCD_HSYNC             (GPIO_NUM_44)
#define BSP_LCD_VSYNC             (GPIO_NUM_45)
#define BSP_LCD_DE                (GPIO_NUM_43)
#define BSP_LCD_DISP              (GPIO_NUM_NC)
#define BSP_LCD_BACKLIGHT         (GPIO_NUM_39)
#define BSP_LCD_TOUCH_RST         (GPIO_NUM_NC)
#define BSP_LCD_TOUCH_INT         (GPIO_NUM_NC)

/* RGB888 bus, RGB666 panel: unused LSBs are GPIO_NUM_NC.
 * S31 RGB data_width must be 8/16/24, so drive 24-bit and wire R2-R7/G2-G7/B2-B7. */
#define BSP_LCD_DATA0             (GPIO_NUM_NC) /* B0 */
#define BSP_LCD_DATA1             (GPIO_NUM_NC) /* B1 */
#define BSP_LCD_DATA2             (GPIO_NUM_14) /* B2 */
#define BSP_LCD_DATA3             (GPIO_NUM_15) /* B3 */
#define BSP_LCD_DATA4             (GPIO_NUM_16) /* B4 */
#define BSP_LCD_DATA5             (GPIO_NUM_17) /* B5 */
#define BSP_LCD_DATA6             (GPIO_NUM_18) /* B6 */
#define BSP_LCD_DATA7             (GPIO_NUM_19) /* B7 */
#define BSP_LCD_DATA8             (GPIO_NUM_NC) /* G0 */
#define BSP_LCD_DATA9             (GPIO_NUM_NC) /* G1 */
#define BSP_LCD_DATA10            (GPIO_NUM_8)  /* G2 */
#define BSP_LCD_DATA11            (GPIO_NUM_9)  /* G3 */
#define BSP_LCD_DATA12            (GPIO_NUM_10) /* G4 */
#define BSP_LCD_DATA13            (GPIO_NUM_11) /* G5 */
#define BSP_LCD_DATA14            (GPIO_NUM_12) /* G6 */
#define BSP_LCD_DATA15            (GPIO_NUM_13) /* G7 */
#define BSP_LCD_DATA16            (GPIO_NUM_NC) /* R0 */
#define BSP_LCD_DATA17            (GPIO_NUM_NC) /* R1 */
#define BSP_LCD_DATA18            (GPIO_NUM_2)  /* R2 */
#define BSP_LCD_DATA19            (GPIO_NUM_3)  /* R3 */
#define BSP_LCD_DATA20            (GPIO_NUM_4)  /* R4 */
#define BSP_LCD_DATA21            (GPIO_NUM_5)  /* R5 */
#define BSP_LCD_DATA22            (GPIO_NUM_6)  /* R6 */
#define BSP_LCD_DATA23            (GPIO_NUM_7)  /* R7 */

/** @defgroup g02_storage SD Card */
#define BSP_SD_D0                 (GPIO_NUM_20)
#define BSP_SD_D1                 (GPIO_NUM_21)
#define BSP_SD_D2                 (GPIO_NUM_22)
#define BSP_SD_D3                 (GPIO_NUM_23)
#define BSP_SD_CMD                (GPIO_NUM_25)
#define BSP_SD_CLK                (GPIO_NUM_24)
#define BSP_SD_DET                (GPIO_NUM_NC)
/* Initial boards leave the card powered; neither GPIO60 nor GPIO57 gates it.
 * Schematic V1.1 wires SD_CTRL to GPIO57 (AO3401A, gate pulldown, active low).
 * Driving it low keeps a V1.1 card on; driving it high turns that card off. */
#define BSP_SD_POWER              (GPIO_NUM_57)

/** @defgroup g05_buttons Buttons */
#define BSP_BUTTONS_ADC_GPIO      (GPIO_NUM_42)
#define BSP_BUTTON_BOOT_IO        (GPIO_NUM_61)

/** @defgroup g06_led Leds / buzzer */
#define BSP_RGB_LED_IO            (GPIO_NUM_37)
#define BSP_BUZZER_IO             (GPIO_NUM_46)

/** @defgroup g07_usb USB 2.0 HS PHY (dedicated, not GPIOs) */
#define BSP_USB_DP_PIN            (44) /* chip package pin, not GPIO44 */
#define BSP_USB_DM_PIN            (45) /* chip package pin, not GPIO45 */

/** @defgroup g08_uart UART1 */
#define BSP_UART1_TX              (GPIO_NUM_33)
#define BSP_UART1_RX              (GPIO_NUM_34)

/** @defgroup g09_rs485 RS485 (UART2, hardware auto DE) */
#define BSP_RS485_TX              (GPIO_NUM_35)
#define BSP_RS485_RX              (GPIO_NUM_36)

/** @defgroup g10_can CAN / TWAI */
#define BSP_CAN_TX                (GPIO_NUM_53)
#define BSP_CAN_RX                (GPIO_NUM_54)

#define BSP_SDCARD_MOUNT_POINT    CONFIG_BSP_SD_MOUNT_POINT

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
    BSP_BUTTON_SW3 = 0,
    BSP_BUTTON_SW4,
    BSP_BUTTON_SW5,
    BSP_BUTTON_SW6,
    BSP_BUTTON_NUM
} bsp_button_t;

/**************************************************************************************************
 * I2C
 **************************************************************************************************/
esp_err_t bsp_i2c_init(void);
esp_err_t bsp_i2c_deinit(void);
i2c_master_bus_handle_t bsp_i2c_get_handle(void);

/**************************************************************************************************
 * ADC
 **************************************************************************************************/
esp_err_t bsp_adc_initialize(void);
int bsp_adc_read_raw(void);
int bsp_adc_read_mv(void);

/**************************************************************************************************
 * SD card
 **************************************************************************************************/
esp_err_t bsp_sdcard_mount(void);
esp_err_t bsp_sdcard_unmount(void);
sdmmc_card_t *bsp_sdcard_get_handle(void);

/**************************************************************************************************
 * Audio
 **************************************************************************************************/
esp_err_t bsp_audio_init(const i2s_std_config_t *i2s_config);
esp_codec_dev_handle_t bsp_audio_codec_speaker_init(void);
esp_codec_dev_handle_t bsp_audio_codec_microphone_init(void);
esp_err_t bsp_audio_poweramp_enable(bool enable);
esp_err_t bsp_i2s_get_handles(i2s_chan_handle_t *tx, i2s_chan_handle_t *rx);

/**************************************************************************************************
 * Buttons / LED / buzzer
 **************************************************************************************************/
esp_err_t bsp_iot_button_create(button_handle_t btn_array[], int *btn_cnt, int btn_array_size);
esp_err_t bsp_led_rgb_init(void);
esp_err_t bsp_led_rgb_set(uint8_t r, uint8_t g, uint8_t b);
esp_err_t bsp_buzzer_init(void);
esp_err_t bsp_buzzer_set(bool on);
esp_err_t bsp_buzzer_beep(uint32_t on_ms, uint32_t off_ms);

/**************************************************************************************************
 * UART1 / RS485 / CAN
 **************************************************************************************************/
esp_err_t bsp_uart1_init(uint32_t baud_rate);
esp_err_t bsp_rs485_init(uint32_t baud_rate);
esp_err_t bsp_rs485_write(const void *data, size_t len);

#if defined(CONFIG_SOC_TWAI_SUPPORTED)
/** self_test: no ACK required, and the controller receives its own TX (loopback). */
esp_err_t bsp_can_init(uint32_t bitrate, bool self_test, twai_node_handle_t *ret_node);
#endif

/**************************************************************************************************
 * USB 2.0 High-Speed
 *
 * The HS PHY is on dedicated USB_DP / USB_DM pads. Applications install TinyUSB
 * themselves (HID, UAC, vendor). This helper only documents that no GPIO mux is needed.
 **************************************************************************************************/
esp_err_t bsp_usb_init(void);

/**************************************************************************************************
 * Wi-Fi / Bluetooth
 *
 * Radio is on the module PCB antenna. These helpers wrap IDF init used by the
 * verified examples. BLE and Classic Bluetooth are mutually exclusive in one firmware.
 **************************************************************************************************/
esp_err_t bsp_wifi_init_sta(const char *ssid, const char *password);
esp_err_t bsp_bt_nvs_init(void);

/**************************************************************************************************
 * LVGL (esp_lvgl_port)
 *
 * Locked path: 2 PSRAM framebuffers, direct mode, swap on VSYNC.
 * LVGL is not thread-safe. Take bsp_display_lock() before any lv_* call.
 **************************************************************************************************/
#if (BSP_CONFIG_NO_GRAPHIC_LIB == 0)
typedef struct {
    lvgl_port_cfg_t lvgl_port_cfg; /*!< LVGL port task / tick configuration */
    uint32_t buffer_size;          /*!< Draw buffer size in pixels */
    bool double_buffer;            /*!< Allocate two LVGL draw buffers */
    struct {
        unsigned int buff_dma: 1;    /*!< DMA-capable draw buffer */
        unsigned int buff_spiram: 1; /*!< Place draw buffers in PSRAM */
    } flags;
} bsp_display_cfg_t;

lv_display_t *bsp_display_start(void);
lv_display_t *bsp_display_start_with_config(const bsp_display_cfg_t *cfg);
lv_indev_t *bsp_display_get_input_dev(void);
bool bsp_display_lock(uint32_t timeout_ms);
void bsp_display_unlock(void);
esp_err_t bsp_display_enter_sleep(void);
esp_err_t bsp_display_exit_sleep(void);
void bsp_display_rotate(lv_display_t *disp, lv_display_rotation_t rotation);
#endif /* BSP_CONFIG_NO_GRAPHIC_LIB == 0 */

#ifdef __cplusplus
}
#endif
