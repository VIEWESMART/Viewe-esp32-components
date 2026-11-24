# ESP LCD Touch CST826 Controller

[![Component Registry](https://components.espressif.com/components/viewesmart/esp_lcd_touch_cst826/badge.svg)](https://components.espressif.com/components/viewesmart/esp_lcd_touch_cst826)

Implementation of the CST826 touch controller with the [esp_lcd_touch](https://components.espressif.com/components/espressif/esp_lcd_touch) component.

[中文](./README_CN.md)

| Touch controller | Communication interface |     Component name     | I2C address | Chip ID |
| :--------------: | :---------------------: | :--------------------: | :---------: | :-----: |
|      CST826      |           I2C           | esp_lcd_touch_cst826 |    0x15     | `0xAA` = `0x11` |

## CST826 vs CST816S

| Item | CST816S | CST826 |
| ---- | ------- | ------ |
| Chip ID register | `0xA7` (`0xB4`) | `0xAA` (`0x11`); `0xA7` returns `0` |
| FW version | `0xA9` | `0xA6` |
| Sleep command | write `0x03` to `0xE5` | write `0x03` to `0xA5` |
| Touch data | `0x02` count + XY | Same report layout |
| Mode switch (`0xFA`) | Supported | Fixed report mode (no register switch) |

> [!NOTE]
> Do **not** use `espressif/esp_lcd_touch_cst816s` against a CST826 panel if you rely on chip-ID probing: the official driver reads `0xA7` and will mis-identify or fail.
> If ID probe fails before the first touch (no RST wired), the driver logs a warning and continues.

## Add to project

Packages from this repository are uploaded to [Espressif's component service](https://components.espressif.com/).
You can add them to your project via `idf.py add-dependency`, e.g.

```
idf.py add-dependency "viewesmart/esp_lcd_touch_cst826^1.0.0"
```

Alternatively, create an `idf_component.yml`. More is in [Espressif's documentation](https://docs.espressif.com/projects/esp-idf/en/latest/esp32/api-guides/tools/idf-component-manager.html).

```yaml
dependencies:
  viewesmart/esp_lcd_touch_cst826: "^1.0.0"
```

## Example use

```c
    esp_lcd_panel_io_i2c_config_t io_config = ESP_LCD_TOUCH_IO_I2C_CST826_CONFIG();

    esp_lcd_touch_config_t tp_cfg = {
        .x_max = CONFIG_LCD_HRES,
        .y_max = CONFIG_LCD_VRES,
        .rst_gpio_num = CONFIG_LCD_TOUCH_RST,
        .int_gpio_num = CONFIG_LCD_TOUCH_INT,
        .levels = {
            .reset = 0,
            .interrupt = 0,
        },
        .flags = {
            .swap_xy = 0,
            .mirror_x = 0,
            .mirror_y = 0,
        },
    };

    esp_lcd_touch_handle_t tp;
    ESP_ERROR_CHECK(esp_lcd_touch_new_i2c_cst826(io_handle, &tp_cfg, &tp));
```
