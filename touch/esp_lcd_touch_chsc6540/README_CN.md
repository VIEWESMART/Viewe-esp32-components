# ESP LCD Touch CHSC6540 控制器

[![Component Registry](https://components.espressif.com/components/viewesmart/esp_lcd_touch_chsc6540/badge.svg)](https://components.espressif.com/components/viewesmart/esp_lcd_touch_chsc6540)

基于 [esp_lcd_touch](https://components.espressif.com/components/espressif/esp_lcd_touch) 的 CHSC6540 触摸控制器驱动。

[English](./README.md)

| 触摸控制器 | 通信接口 |       组件名        | I2C 地址 |
| :--------: | :------: | :-----------------: | :------: |
|  CHSC6540  |   I2C    | esp_lcd_touch_chsc6540 |  0x2E   |

> [!NOTE]
> 部分模组在第一次触摸事件之前读取寄存器 `0xA7` 会失败。在 menuconfig 中打开 `CONFIG_ESP_LCD_TOUCH_CHSC6540_DISABLE_READ_ID` 可跳过芯片 ID 读取。

## 添加到工程

组件发布在 [乐鑫组件注册表](https://components.espressif.com/)。可用 `idf.py add-dependency` 添加：

```
idf.py add-dependency "viewesmart/esp_lcd_touch_chsc6540^1.0.0"
```

也可以写在工程的 `idf_component.yml` 里。详见 [ESP-IDF 文档](https://docs.espressif.com/projects/esp-idf/zh_CN/latest/esp32/api-guides/tools/idf-component-manager.html)。

```yaml
dependencies:
  viewesmart/esp_lcd_touch_chsc6540: "^1.0.0"
```

## 使用示例

初始化前创建信号量，并提供中断回调：

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

创建 I2C 主机、面板 IO，再创建触摸句柄：

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

在轮询任务中读取触摸数据；若使用中断，则只在中断触发后读取：

```c
if (xSemaphoreTake(touch_mux, 0) == pdTRUE) {
    ESP_ERROR_CHECK(esp_lcd_touch_read_data(tp));
}
```

读取一个触摸点。`swap_xy`、`mirror_x`、`mirror_y` 由 `esp_lcd_touch_get_data()` 统一处理。

```c
esp_lcd_touch_point_data_t points[1] = {0};
uint8_t touch_cnt = 0;

ESP_ERROR_CHECK(esp_lcd_touch_get_data(tp, points, &touch_cnt, 1));
if (touch_cnt > 0) {
    ESP_LOGI(TAG, "Touch position: [%d, %d]", points[0].x, points[0].y);
}
```

## 说明

* 触摸报文从寄存器 `0x00` 开始。
* 点数在字节 `[2]`，X/Y 打包在字节 `[3..6]`，半字节布局与 CST 系列一致。
* 芯片 ID 从寄存器 `0xA7` 读取。打开 `CONFIG_ESP_LCD_TOUCH_CHSC6540_DISABLE_READ_ID` 后跳过该读取。
