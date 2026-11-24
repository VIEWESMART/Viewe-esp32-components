# ESP LCD Touch CHSC6540 Controller

[![Component Registry](https://components.espressif.com/components/viewesmart/esp_lcd_touch_chsc6540/badge.svg)](https://components.espressif.com/components/viewesmart/esp_lcd_touch_chsc6540)

Implementation of the CHSC6540 touch controller with the [esp_lcd_touch](https://components.espressif.com/components/espressif/esp_lcd_touch) component.

[中文](./README_CN.md)

| Touch controller | Communication interface |     Component name     | I2C address |
| :--------------: | :---------------------: | :--------------------: | :---------: |
|     CHSC6540     |           I2C           | esp_lcd_touch_chsc6540 |    0x2E     |

> [!NOTE]
> Some modules fail the chip-ID read on register `0xA7` before the first touch event. Set `CONFIG_ESP_LCD_TOUCH_CHSC6540_DISABLE_READ_ID=y` in menuconfig to skip that read.

## Add to project

Packages from this repository are uploaded to [Espressif's component service](https://components.espressif.com/).
You can add them to your project via `idf.py add-dependency`, e.g.

```
idf.py add-dependency "viewesmart/esp_lcd_touch_chsc6540^1.0.0"
```

Alternatively, create an `idf_component.yml`. More is in [Espressif's documentation](https://docs.espressif.com/projects/esp-idf/en/latest/esp32/api-guides/tools/idf-component-manager.html).

```yaml
dependencies:
  viewesmart/esp_lcd_touch_chsc6540: "^1.0.0"
```

## Example use

Define a semaphore and the interrupt callback before initializing the touch controller:

```c
static SemaphoreHandle_t touch_mux;

touch_mux = xSemaphoreCreateBinary();

static void touch_callback(esp_lcd_touch_handle_t tp)
{
    BaseType_t xHigherPriorityTaskWoken = pdFALSE;
    xSemaphoreGiveFromISR(touch_mux, &xHigherPriorityTaskWoken);

    if (xHigherPriorityTaskWoken) {
        portYIELD_FROM_ISR();
    }
}
```

Create the I2C master bus and the panel IO, then create the touch handle:

```c
#include "driver/i2c_master.h"
#include "esp_lcd_touch_chsc6540.h"

i2c_master_bus_handle_t i2c_bus = NULL;
const i2c_master_bus_config_t i2c_bus_conf = {
    .clk_source = I2C_CLK_SRC_DEFAULT,
    .i2c_port = I2C_NUM_0,
    .sda_io_num = CONFIG_LCD_TOUCH_SDA,
    .scl_io_num = CONFIG_LCD_TOUCH_SCL,
    .glitch_ignore_cnt = 7,
    .flags.enable_internal_pullup = true,
};
ESP_ERROR_CHECK(i2c_new_master_bus(&i2c_bus_conf, &i2c_bus));

esp_lcd_panel_io_handle_t tp_io_handle = NULL;
esp_lcd_panel_io_i2c_config_t io_config = ESP_LCD_TOUCH_IO_I2C_CHSC6540_CONFIG();
ESP_ERROR_CHECK(esp_lcd_new_panel_io_i2c(i2c_bus, &io_config, &tp_io_handle));

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
    .interrupt_callback = touch_callback,
};

esp_lcd_touch_handle_t tp;
ESP_ERROR_CHECK(esp_lcd_touch_new_i2c_chsc6540(tp_io_handle, &tp_cfg, &tp));
```

Read data from the touch controller and store it in RAM. Call it from a poll loop, or only after the interrupt fires:

```c
if (xSemaphoreTake(touch_mux, 0) == pdTRUE) {
    ESP_ERROR_CHECK(esp_lcd_touch_read_data(tp));
}
```

Get one touch point. `swap_xy`, `mirror_x`, and `mirror_y` are applied by `esp_lcd_touch_get_data()`.

```c
esp_lcd_touch_point_data_t points[1] = {0};
uint8_t touch_cnt = 0;

ESP_ERROR_CHECK(esp_lcd_touch_get_data(tp, points, &touch_cnt, 1));
if (touch_cnt > 0) {
    ESP_LOGI(TAG, "Touch position: [%d, %d]", points[0].x, points[0].y);
}
```

## Notes

* Touch report starts at register `0x00`.
* Point count is byte `[2]`. X/Y are packed in bytes `[3..6]`, using the same nibble layout as the CST-series controllers.
* Chip ID is read from register `0xA7` unless `CONFIG_ESP_LCD_TOUCH_CHSC6540_DISABLE_READ_ID` is enabled.
