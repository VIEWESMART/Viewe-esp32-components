# ESP LCD Touch CST820 Controller

[![Component Registry](https://components.espressif.com/components/viewesmart/esp_lcd_touch_cst820/badge.svg)](https://components.espressif.com/components/viewesmart/esp_lcd_touch_cst820)

Implementation of the CST820 touch controller with [esp_lcd_touch](https://components.espressif.com/components/espressif/esp_lcd_touch) component.

[中文](./README_CN.md)

| Touch controller | Communication interface |    Component name     |                             Link to datasheet                              |
| :--------------: | :---------------------: | :-------------------: | :------------------------------------------------------------------------: |
|     CST820       |           I2C           | esp_lcd_touch_cst820  | [datasheet](https://github.com/kodediy/esp_lcd_touch_cst820/blob/main/CST820_Datasheet_V1.2.pdf) |

## Add to project

Packages can be uploaded to [Espressif's component service](https://components.espressif.com/).
You can add this component to your project via `idf.py add-dependency`, e.g.

```
idf.py add-dependency "viewesmart/esp_lcd_touch_cst820^1.0.2"
```

Alternatively, create an `idf_component.yml`. More is in [Espressif's documentation](https://docs.espressif.com/projects/esp-idf/en/latest/esp32/api-guides/tools/idf-component-manager.html).

## Example use

Define a mutex for the touch and create it before initialize the touch:

```c
static SemaphoreHandle_t touch_mux;

touch_mux = xSemaphoreCreateBinary();
```

Define a callback function used by ISR:

```c
static void touch_callback(esp_lcd_touch_handle_t tp)
{
    BaseType_t xHigherPriorityTaskWoken = pdFALSE;
    xSemaphoreGiveFromISR(touch_mux, &xHigherPriorityTaskWoken);

    if (xHigherPriorityTaskWoken) {
        portYIELD_FROM_ISR();
    }
}
```

Initialize I2C master bus and touch panel IO:

```c
#include "driver/i2c_master.h"
#include "esp_lcd_touch_cst820.h"

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
esp_lcd_panel_io_i2c_config_t tp_io_config = ESP_LCD_TOUCH_IO_I2C_CST820_CONFIG();
ESP_ERROR_CHECK(esp_lcd_new_panel_io_i2c(i2c_bus, &tp_io_config, &tp_io_handle));

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
ESP_ERROR_CHECK(esp_lcd_touch_new_i2c_cst820(tp_io_handle, &tp_cfg, &tp));
```

Read data from the touch controller and store it in RAM. It should be called regularly in a poll loop (or after IRQ):

```c
if (xSemaphoreTake(touch_mux, 0) == pdTRUE) {
    esp_lcd_touch_read_data(tp); // read only when ISR was triggered
}
```

Get attributes of a single touch point:

```c
uint16_t touch_x[1];
uint16_t touch_y[1];
uint16_t touch_strength[1];
uint8_t touch_cnt = 0;

bool pressed = esp_lcd_touch_get_coordinates(tp, touch_x, touch_y, touch_strength, &touch_cnt, 1);
```
