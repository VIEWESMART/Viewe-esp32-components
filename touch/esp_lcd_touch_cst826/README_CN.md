# ESP LCD Touch CST826 控制器

[![Component Registry](https://components.espressif.com/components/viewesmart/esp_lcd_touch_cst826/badge.svg)](https://components.espressif.com/components/viewesmart/esp_lcd_touch_cst826)

基于 [esp_lcd_touch](https://components.espressif.com/components/espressif/esp_lcd_touch) 的 CST826 触摸控制器驱动。

[English](./README.md)

| 触摸控制器 | 通信接口 |       组件名        | I2C 地址 | 芯片 ID |
| :--------: | :------: | :-----------------: | :------: | :-----: |
|   CST826   |   I2C    | esp_lcd_touch_cst826 |  0x15   | `0xAA` = `0x11` |

## CST826 与 CST816S 差异

| 项目 | CST816S | CST826 |
| ---- | ------- | ------ |
| 芯片 ID 寄存器 | `0xA7`（`0xB4`） | `0xAA`（`0x11`）；`0xA7` 常为 0 |
| 固件版本 | `0xA9` | `0xA6` |
| 睡眠指令 | 向 `0xE5` 写 `0x03` | 向 `0xA5` 写 `0x03` |
| 报点数据 | `0x02` + XY | 布局兼容 |
| 模式切换（`0xFA`） | 支持 | 固定报点模式 |

> [!NOTE]
> 不要对 CST826 面板使用 `espressif/esp_lcd_touch_cst816s` 做芯片 ID 探测：官方驱动读的是 `0xA7`，会误判或失败。
> 若未接 RST、首次触摸前 ID 探测失败，本驱动会打警告并继续初始化。

## 添加到工程

组件发布在 [乐鑫组件注册表](https://components.espressif.com/)。可用 `idf.py add-dependency` 添加：

```
idf.py add-dependency "viewesmart/esp_lcd_touch_cst826^1.0.0"
```

也可以写在工程的 `idf_component.yml` 里：

```yaml
dependencies:
  viewesmart/esp_lcd_touch_cst826: "^1.0.0"
```

## 使用示例

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
