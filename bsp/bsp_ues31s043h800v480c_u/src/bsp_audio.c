/*
 * SPDX-FileCopyrightText: 2026 VIEWE
 * SPDX-License-Identifier: Apache-2.0
 */

#include "esp_check.h"
#include "esp_codec_dev_defaults.h"
#include "esp_log.h"
#include "driver/gpio.h"
#include "bsp/esp-bsp.h"
#include "bsp_err_check.h"

static const char *TAG = "BSP-AUDIO";

static i2s_chan_handle_t s_i2s_tx;
static i2s_chan_handle_t s_i2s_rx;
static bool s_i2s_inited;
static const audio_codec_data_if_t *s_data_if;

esp_err_t bsp_audio_init(const i2s_std_config_t *i2s_config)
{
    if (s_i2s_inited) {
        return ESP_OK;
    }

    i2s_chan_config_t chan_cfg = I2S_CHANNEL_DEFAULT_CONFIG(CONFIG_BSP_I2S_NUM, I2S_ROLE_MASTER);
    chan_cfg.auto_clear = true;
    chan_cfg.dma_desc_num = 8;
    chan_cfg.dma_frame_num = 1024;
    /* Full-duplex like the verified bt_audio board (A2DP TX + HFP RX). */
    BSP_ERROR_CHECK_RETURN_ERR(i2s_new_channel(&chan_cfg, &s_i2s_tx, &s_i2s_rx));

    i2s_std_config_t std_cfg = {
        .clk_cfg = I2S_STD_CLK_DEFAULT_CONFIG(48000),
        .slot_cfg = I2S_STD_PHILIPS_SLOT_DEFAULT_CONFIG(I2S_DATA_BIT_WIDTH_16BIT, I2S_SLOT_MODE_STEREO),
        .gpio_cfg = {
            .mclk = BSP_I2S_MCLK,
            .bclk = BSP_I2S_SCLK,
            .ws = BSP_I2S_LCLK,
            .dout = BSP_I2S_DOUT,
            .din = BSP_I2S_DSIN,
            .invert_flags = {
                .mclk_inv = false,
                .bclk_inv = false,
                .ws_inv = false,
            },
        },
    };
    std_cfg.clk_cfg.mclk_multiple = I2S_MCLK_MULTIPLE_256;
    std_cfg.clk_cfg.bclk_div = 8;
    std_cfg.slot_cfg.ws_width = 16;
    std_cfg.slot_cfg.bit_shift = true;
    std_cfg.slot_cfg.left_align = true;
    if (i2s_config) {
        std_cfg = *i2s_config;
    }

    BSP_ERROR_CHECK_RETURN_ERR(i2s_channel_init_std_mode(s_i2s_tx, &std_cfg));
    BSP_ERROR_CHECK_RETURN_ERR(i2s_channel_init_std_mode(s_i2s_rx, &std_cfg));
    BSP_ERROR_CHECK_RETURN_ERR(i2s_channel_enable(s_i2s_tx));
    BSP_ERROR_CHECK_RETURN_ERR(i2s_channel_enable(s_i2s_rx));

    audio_codec_i2s_cfg_t i2s_cfg = {
        .port = CONFIG_BSP_I2S_NUM,
        .rx_handle = s_i2s_rx,
        .tx_handle = s_i2s_tx,
    };
    s_data_if = audio_codec_new_i2s_data(&i2s_cfg);
    BSP_NULL_CHECK(s_data_if, ESP_FAIL);

    gpio_config_t pa = {
        .mode = GPIO_MODE_OUTPUT,
        .pin_bit_mask = 1ULL << BSP_POWER_AMP_IO,
    };
    BSP_ERROR_CHECK_RETURN_ERR(gpio_config(&pa));
    gpio_set_level(BSP_POWER_AMP_IO, 0);

    s_i2s_inited = true;
    ESP_LOGI(TAG, "I2S0 TX/RX MCLK=%d BCK=%d WS=%d DO=%d DI=%d",
             BSP_I2S_MCLK, BSP_I2S_SCLK, BSP_I2S_LCLK, BSP_I2S_DOUT, BSP_I2S_DSIN);
    return ESP_OK;
}

esp_err_t bsp_audio_poweramp_enable(bool enable)
{
    return gpio_set_level(BSP_POWER_AMP_IO, enable ? 1 : 0);
}

static esp_codec_dev_handle_t codec_create(esp_codec_dev_type_t type)
{
    if (bsp_audio_init(NULL) != ESP_OK) {
        return NULL;
    }
    if (bsp_i2c_init() != ESP_OK) {
        return NULL;
    }

    audio_codec_i2c_cfg_t i2c_cfg = {
        .port = CONFIG_BSP_I2C_NUM,
        .addr = ES8389_CODEC_DEFAULT_ADDR,
        .bus_handle = bsp_i2c_get_handle(),
    };
    const audio_codec_ctrl_if_t *ctrl_if = audio_codec_new_i2c_ctrl(&i2c_cfg);
    BSP_NULL_CHECK(ctrl_if, NULL);
    const audio_codec_gpio_if_t *gpio_if = audio_codec_new_gpio();
    BSP_NULL_CHECK(gpio_if, NULL);

    es8389_codec_cfg_t es_cfg = {
        .ctrl_if = ctrl_if,
        .gpio_if = gpio_if,
        .codec_mode = (type == ESP_CODEC_DEV_TYPE_IN) ? ESP_CODEC_DEV_WORK_MODE_ADC
                                                      : ESP_CODEC_DEV_WORK_MODE_DAC,
        .master_mode = false,
        /* MCLK is driven on GPIO48. use_mclk stays false: this esp_codec_dev
         * only loads the ES8389 divider table in that mode (clock from BCLK).
         * use_mclk true leaves the placeholder dividers and the DAC is silent. */
        .use_mclk = false,
        .pa_pin = (type == ESP_CODEC_DEV_TYPE_IN) ? GPIO_NUM_NC : BSP_POWER_AMP_IO,
        .pa_reverted = false,
        .hw_gain = {
            .pa_voltage = 5.0f,
            .codec_dac_voltage = 3.3f,
            /* NS4150B, Ri = 100 kΩ: 20*log10(2*150k/100k) ≈ 9.5 dB */
            .pa_gain = 9.5f,
        },
    };
    const audio_codec_if_t *codec_if = es8389_codec_new(&es_cfg);
    BSP_NULL_CHECK(codec_if, NULL);

    esp_codec_dev_cfg_t dev_cfg = {
        .dev_type = type,
        .codec_if = codec_if,
        .data_if = s_data_if,
    };
    return esp_codec_dev_new(&dev_cfg);
}

esp_codec_dev_handle_t bsp_audio_codec_speaker_init(void)
{
    return codec_create(ESP_CODEC_DEV_TYPE_OUT);
}

esp_codec_dev_handle_t bsp_audio_codec_microphone_init(void)
{
    return codec_create(ESP_CODEC_DEV_TYPE_IN);
}

esp_err_t bsp_i2s_get_handles(i2s_chan_handle_t *tx, i2s_chan_handle_t *rx)
{
    if (!s_i2s_inited) {
        BSP_ERROR_CHECK_RETURN_ERR(bsp_audio_init(NULL));
    }
    if (tx) {
        *tx = s_i2s_tx;
    }
    if (rx) {
        *rx = s_i2s_rx;
    }
    return ESP_OK;
}
