set(srcs
    "src/viewe_desknob.c"
    "src/bsp_display.c"
    "src/bsp_audio.c"
    "src/bsp_sdcard.c"
    "src/bsp_led.c"
    "src/bsp_battery.c"
    "src/bsp_radio.c"
)

set(reqs
    driver
    esp_driver_gpio
    esp_adc
    esp_driver_i2c
    esp_driver_i2s
    esp_driver_ledc
    esp_driver_spi
    esp_lcd
    esp_timer
    esp_lvgl_adapter
    button
    led_strip
    esp_lcd_gc9b72
    esp_lcd_touch_cst826
    esp_codec_dev
    esp_driver_sdmmc
    fatfs
    sdmmc
    spiffs
    esp_wifi
    esp_netif
    nvs_flash
    bt
)

if(CONFIG_BSP_USB_MSC)
    list(APPEND srcs "src/bsp_usb.c")
    list(APPEND reqs esp_tinyusb wear_levelling esp_partition)
else()
    list(APPEND srcs "src/bsp_usb_stub.c")
endif()

idf_component_register(
    SRCS ${srcs}
    INCLUDE_DIRS "include"
    PRIV_INCLUDE_DIRS "priv_include"
    REQUIRES ${reqs}
)
