set(srcs
    "src/bsp_board.c"
    "src/bsp_display.c"
    "src/bsp_audio.c"
)
set(reqs
    esp_driver_gpio
    esp_driver_i2c
    esp_driver_i2s
    esp_driver_sdmmc
    esp_driver_uart
    esp_driver_ledc
    esp_driver_twai
    fatfs
    esp_adc
    esp_lcd
    esp_wifi
    nvs_flash
    esp_event
    esp_netif
)

# lvgl / esp_lvgl_port are injected by the component manager from idf_component.yml
# when BSP_NO_GRAPHIC_LIB is off. Do not list them here: early expansion runs
# before Kconfig, so a CONFIG_ gate on REQUIRES would always pull them in.
if(NOT CONFIG_BSP_NO_GRAPHIC_LIB)
    list(APPEND srcs "src/bsp_lvgl.c")
endif()

idf_component_register(
    SRCS ${srcs}
    INCLUDE_DIRS
        "include"
    PRIV_INCLUDE_DIRS
        "priv_include"
    REQUIRES ${reqs}
    PRIV_REQUIRES
        esp_timer
        esp_psram
)
