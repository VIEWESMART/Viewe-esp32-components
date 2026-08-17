if("${IDF_VERSION_MAJOR}.${IDF_VERSION_MINOR}" VERSION_GREATER_EQUAL "5.3")
    set(REQ esp_driver_i2c esp_driver_gpio esp_driver_ledc esp_driver_spi)
    set(PRIV_REQ "")
else()
    set(REQ driver)
    set(PRIV_REQ "")
endif()

idf_component_register(
    SRCS "src/bsp_uedx48480040e_wb_a.c" "src/bsp_sdcard.c" "src/bsp_wifi.c"
    INCLUDE_DIRS "include"
    PRIV_INCLUDE_DIRS "priv_include"
    REQUIRES ${REQ} esp_lcd esp_lcd_gc9503 esp_lcd_touch_ft5x06 fatfs sdmmc
             esp_lvgl_adapter esp_wifi esp_netif esp_event nvs_flash
    PRIV_REQUIRES ${PRIV_REQ} esp_lcd_panel_io_additions esp_timer
)
