# ESP LCD GC9B72

[![Component Registry](https://components.espressif.com/components/viewesmart/esp_lcd_gc9b72/badge.svg)](https://components.espressif.com/components/viewesmart/esp_lcd_gc9b72)

Implementation of the GC9B72 LCD controller with [esp_lcd](https://docs.espressif.com/projects/esp-idf/en/latest/esp32s3/api-reference/peripherals/lcd.html) component.

| LCD controller | Communication interface | Component name | Notes |
| :------------: | :---------------------: | :------------: | :---- |
| GC9B72         | SPI/QSPI                | esp_lcd_gc9b72 | Default init targets 360×360 RGB565 panels |

## Add to project

Packages from this repository are uploaded to [Espressif's component service](https://components.espressif.com/).
You can add them to your project via `idf.py add-dependency`, e.g.

```
idf.py add-dependency "viewesmart/esp_lcd_gc9b72^1.0.0"
```

Alternatively, create an `idf_component.yml`:

```yaml
dependencies:
  viewesmart/esp_lcd_gc9b72: "^1.0.0"
```

More is in [Espressif's documentation](https://docs.espressif.com/projects/esp-idf/en/latest/esp32/api-guides/tools/idf-component-manager.html).

## Initialization Code

### SPI Interface

```c
    ESP_LOGI(TAG, "Initialize SPI bus");
    const spi_bus_config_t bus_config = GC9B72_PANEL_BUS_SPI_CONFIG(EXAMPLE_PIN_NUM_LCD_PCLK,
                                                                    EXAMPLE_PIN_NUM_LCD_DATA0,
                                                                    EXAMPLE_LCD_H_RES * 10 * sizeof(uint16_t));
    ESP_ERROR_CHECK(spi_bus_initialize(EXAMPLE_LCD_HOST, &bus_config, SPI_DMA_CH_AUTO));

    ESP_LOGI(TAG, "Install panel IO");
    esp_lcd_panel_io_handle_t io_handle = NULL;
    const esp_lcd_panel_io_spi_config_t io_config = GC9B72_PANEL_IO_SPI_CONFIG(EXAMPLE_PIN_NUM_LCD_CS, EXAMPLE_PIN_NUM_LCD_DC,
                                                                               callback, &callback_data);
    ESP_ERROR_CHECK(esp_lcd_new_panel_io_spi((esp_lcd_spi_bus_handle_t)EXAMPLE_LCD_HOST, &io_config, &io_handle));

/**
 * Uncomment these lines if use custom initialization commands.
 * The array should be declared as static const and positioned outside the function.
 */
// static const gc9b72_lcd_init_cmd_t lcd_init_cmds[] = {
// //  {cmd, { data }, data_size, delay_ms}
//    {0xfe, (uint8_t []){0x00}, 0, 0},
//    {0xef, (uint8_t []){0x00}, 0, 0},
//     ...
// };

    ESP_LOGI(TAG, "Install GC9B72 panel driver");
    esp_lcd_panel_handle_t panel_handle = NULL;
    const gc9b72_vendor_config_t vendor_config = {
        // .init_cmds = lcd_init_cmds,
        // .init_cmds_size = sizeof(lcd_init_cmds) / sizeof(gc9b72_lcd_init_cmd_t),
        .flags = {
            .use_qspi_interface = 0,
        },
    };
    const esp_lcd_panel_dev_config_t panel_config = {
        .reset_gpio_num = EXAMPLE_PIN_NUM_LCD_RST,
        .rgb_ele_order = LCD_RGB_ELEMENT_ORDER_RGB,     // Implemented by LCD command `36h`
        .bits_per_pixel = 16,                           // Implemented by LCD command `3Ah` (16/18/24)
        .vendor_config = (void *)&vendor_config,
    };
    ESP_ERROR_CHECK(esp_lcd_new_panel_gc9b72(io_handle, &panel_config, &panel_handle));
    esp_lcd_panel_reset(panel_handle);
    esp_lcd_panel_init(panel_handle);
    esp_lcd_panel_disp_on_off(panel_handle, true);
```

### QSPI Interface

```c
    ESP_LOGI(TAG, "Initialize QSPI bus");
    const spi_bus_config_t buscfg = GC9B72_PANEL_BUS_QSPI_CONFIG(EXAMPLE_PIN_NUM_LCD_PCLK,
                                                                    EXAMPLE_PIN_NUM_LCD_DATA0,
                                                                    EXAMPLE_PIN_NUM_LCD_DATA1,
                                                                    EXAMPLE_PIN_NUM_LCD_DATA2,
                                                                    EXAMPLE_PIN_NUM_LCD_DATA3,
                                                                    EXAMPLE_LCD_H_RES * 10 * sizeof(uint16_t));
    ESP_ERROR_CHECK(spi_bus_initialize(EXAMPLE_LCD_HOST, &buscfg, SPI_DMA_CH_AUTO));

    ESP_LOGI(TAG, "Install panel IO");
    esp_lcd_panel_io_handle_t io_handle = NULL;
    const esp_lcd_panel_io_spi_config_t io_config = GC9B72_PANEL_IO_QSPI_CONFIG(EXAMPLE_PIN_NUM_LCD_CS, callback, &callback_data);
    ESP_ERROR_CHECK(esp_lcd_new_panel_io_spi((esp_lcd_spi_bus_handle_t)EXAMPLE_LCD_HOST, &io_config, &io_handle));

    ESP_LOGI(TAG, "Install GC9B72 panel driver");
    esp_lcd_panel_handle_t panel_handle = NULL;
    const gc9b72_vendor_config_t vendor_config = {
        .flags = {
            .use_qspi_interface = 1,
        },
    };
    const esp_lcd_panel_dev_config_t panel_config = {
        .reset_gpio_num = EXAMPLE_PIN_NUM_LCD_RST,
        .rgb_ele_order = LCD_RGB_ELEMENT_ORDER_RGB,
        .bits_per_pixel = 16,
        .vendor_config = (void *)&vendor_config,
    };
    ESP_ERROR_CHECK(esp_lcd_new_panel_gc9b72(io_handle, &panel_config, &panel_handle));

    esp_lcd_panel_reset(panel_handle);
    esp_lcd_panel_init(panel_handle);
    esp_lcd_panel_disp_on_off(panel_handle, true);
```

## Notes

* Default vendor init sequence is for **360×360** GC9B72 panels. Override with `gc9b72_vendor_config_t.init_cmds` for other resolutions.
* Call `esp_lcd_panel_disp_on_off(panel, true)` after `esp_lcd_panel_init()`; display-on (`29h`) is not sent by the default init sequence.
* For low internal SRAM, prefer a small bounce buffer (e.g. `max_transfer_sz = H_RES * 10 * sizeof(uint16_t)`) and keep `trans_queue_depth` modest.
