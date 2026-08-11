# ESP LCD Touch CST820 控制器

[![Component Registry](https://components.espressif.com/components/viewesmart/esp_lcd_touch_cst820/badge.svg)](https://components.espressif.com/components/viewesmart/esp_lcd_touch_cst820)

基于 [esp_lcd_touch](https://components.espressif.com/components/espressif/esp_lcd_touch) 的 CST820 触摸控制器实现。

[English](./README.md)

| 触摸控制器 | 通信接口 |       组件名       |                                  数据手册                                  |
| :--------: | :------: | :----------------: | :------------------------------------------------------------------------: |
|   CST820   |   I2C    | esp_lcd_touch_cst820 | [datasheet](https://github.com/VIEWESMART/Viewe-esp32-components/blob/main/touch/esp_lcd_touch_cst820/CST820_Datasheet_V1.2.pdf) |

## 添加到工程

组件可上传至 [乐鑫组件服务](https://components.espressif.com/)。
可通过 `idf.py add-dependency` 添加，例如：

```
idf.py add-dependency "viewesmart/esp_lcd_touch_cst820^1.0.3"
```

也可编写 `idf_component.yml`。详见 [ESP-IDF 文档](https://docs.espressif.com/projects/esp-idf/zh_CN/latest/esp32/api-guides/tools/idf-component-manager.html)。

## 使用示例

先创建互斥量：

```c
static SemaphoreHandle_t touch_mux;

touch_mux = xSemaphoreCreateBinary();
```

中断回调：

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

初始化 I2C master 总线与触摸 Panel IO：

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

轮询读取触摸数据（或在 IRQ 后读取）：

```c
if (xSemaphoreTake(touch_mux, 0) == pdTRUE) {
    esp_lcd_touch_read_data(tp); // 仅在中断触发后读取
}
```

获取单点坐标：

```c
uint16_t touch_x[1];
uint16_t touch_y[1];
uint16_t touch_strength[1];
uint8_t touch_cnt = 0;

bool pressed = esp_lcd_touch_get_coordinates(tp, touch_x, touch_y, touch_strength, &touch_cnt, 1);
```
