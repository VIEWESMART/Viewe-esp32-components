/*
 * SPDX-FileCopyrightText: 2026 VIEWE
 * SPDX-License-Identifier: Apache-2.0
 */

#include "bsp/viewe_desknob.h"

#include "bsp_err_check.h"
#include "button_gpio.h"
#include "esp_log.h"
#include "esp_spiffs.h"

static const char *TAG = "viewe_desknob";

static i2c_master_bus_handle_t s_i2c_bus;
static int s_i2c_users;

esp_err_t bsp_i2c_init(void)
{
    if (s_i2c_bus) {
        s_i2c_users++;
        return ESP_OK;
    }

    const i2c_master_bus_config_t cfg = {
        .clk_source = I2C_CLK_SRC_DEFAULT,
        .i2c_port = BSP_I2C_NUM,
        .scl_io_num = BSP_I2C_SCL,
        .sda_io_num = BSP_I2C_SDA,
        .glitch_ignore_cnt = 7,
        .flags.enable_internal_pullup = true,
    };
    BSP_ERROR_CHECK_RETURN_ERR(i2c_new_master_bus(&cfg, &s_i2c_bus));
    s_i2c_users = 1;
    ESP_LOGI(TAG, "Codec I2C ready, SCL=%d SDA=%d", BSP_I2C_SCL, BSP_I2C_SDA);
    return ESP_OK;
}

esp_err_t bsp_i2c_deinit(void)
{
    if (!s_i2c_bus) {
        return ESP_OK;
    }
    if (--s_i2c_users > 0) {
        return ESP_OK;
    }
    BSP_ERROR_CHECK_RETURN_ERR(i2c_del_master_bus(s_i2c_bus));
    s_i2c_bus = NULL;
    s_i2c_users = 0;
    return ESP_OK;
}

i2c_master_bus_handle_t bsp_i2c_get_handle(void)
{
    return s_i2c_bus;
}

esp_err_t bsp_iot_button_create(button_handle_t btn_array[], int *btn_cnt, int btn_array_size)
{
    BSP_ERROR_CHECK_RETURN_ERR(btn_array && btn_array_size >= BSP_BUTTON_NUM ? ESP_OK : ESP_ERR_INVALID_ARG);

    const gpio_num_t pins[BSP_BUTTON_NUM] = {
        BSP_BUTTON_CCW_IO,
        BSP_BUTTON_CW_IO,
    };

    for (int i = 0; i < BSP_BUTTON_NUM; i++) {
        btn_array[i] = NULL;
        const button_config_t btn_cfg = {
            .long_press_time = 0,
            .short_press_time = 0,
        };
        const button_gpio_config_t gpio_cfg = {
            .gpio_num = pins[i],
            .active_level = BSP_BUTTON_ACTIVE_LEVEL,
            .enable_power_save = false,
            .disable_pull = false,
        };
        esp_err_t err = iot_button_new_gpio_device(&btn_cfg, &gpio_cfg, &btn_array[i]);
        if (err != ESP_OK) {
            for (int j = 0; j < i; j++) {
                if (btn_array[j]) {
                    iot_button_delete(btn_array[j]);
                    btn_array[j] = NULL;
                }
            }
            return err;
        }
    }

    if (btn_cnt) {
        *btn_cnt = BSP_BUTTON_NUM;
    }
    ESP_LOGI(TAG, "Switches: CCW GPIO%d, CW GPIO%d, active level %d",
             BSP_BUTTON_CCW_IO, BSP_BUTTON_CW_IO, BSP_BUTTON_ACTIVE_LEVEL);
    return ESP_OK;
}

esp_err_t bsp_spiffs_mount(void)
{
    const esp_vfs_spiffs_conf_t conf = {
        .base_path = CONFIG_BSP_SPIFFS_MOUNT_POINT,
        .partition_label = CONFIG_BSP_SPIFFS_PARTITION_LABEL,
        .max_files = CONFIG_BSP_SPIFFS_MAX_FILES,
        .format_if_mount_failed = false,
    };
    return esp_vfs_spiffs_register(&conf);
}

esp_err_t bsp_spiffs_unmount(void)
{
    return esp_vfs_spiffs_unregister(CONFIG_BSP_SPIFFS_PARTITION_LABEL);
}
