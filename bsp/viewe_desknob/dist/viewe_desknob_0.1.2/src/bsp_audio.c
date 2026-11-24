/*
 * SPDX-FileCopyrightText: 2026 VIEWE
 * SPDX-License-Identifier: Apache-2.0
 */

#include "bsp/viewe_desknob.h"

#include "bsp_err_check.h"
#include "esp_codec_dev_defaults.h"
#include "esp_log.h"

static const char *TAG = "viewe_desknob.audio";

static i2s_chan_handle_t s_tx;
static i2s_chan_handle_t s_rx;
static const audio_codec_data_if_t *s_data_if;
static const audio_codec_if_t *s_codec_if;
static esp_codec_dev_handle_t s_speaker;
static esp_codec_dev_handle_t s_mic;

static i2s_std_config_t bsp_audio_default_config(void)
{
    i2s_std_config_t config = {
        .clk_cfg = I2S_STD_CLK_DEFAULT_CONFIG(BSP_AUDIO_DEFAULT_SAMPLE_RATE),
        .slot_cfg = I2S_STD_PHILIPS_SLOT_DEFAULT_CONFIG(BSP_AUDIO_DEFAULT_BITS_PER_SAMPLE, I2S_SLOT_MODE_STEREO),
        .gpio_cfg = {
            .mclk = BSP_AUDIO_I2S_MCLK,
            .bclk = BSP_AUDIO_I2S_SCLK,
            .ws = BSP_AUDIO_I2S_LCLK,
            .dout = BSP_AUDIO_I2S_DOUT,
            .din = BSP_AUDIO_I2S_DSIN,
            .invert_flags = {
                .mclk_inv = false,
                .bclk_inv = false,
                .ws_inv = false,
            },
        },
    };
    return config;
}

esp_err_t bsp_audio_init(const i2s_std_config_t *i2s_config)
{
    if (s_data_if) {
        return ESP_OK;
    }

    i2s_chan_config_t chan_cfg = I2S_CHANNEL_DEFAULT_CONFIG(I2S_NUM_0, I2S_ROLE_MASTER);
    chan_cfg.auto_clear = true;
    esp_err_t err = i2s_new_channel(&chan_cfg, &s_tx, &s_rx);
    if (err != ESP_OK) {
        return err;
    }

    i2s_std_config_t default_cfg = bsp_audio_default_config();
    const i2s_std_config_t *active = i2s_config ? i2s_config : &default_cfg;
    err = i2s_channel_init_std_mode(s_tx, active);
    if (err != ESP_OK) {
        goto fail;
    }
    err = i2s_channel_init_std_mode(s_rx, active);
    if (err != ESP_OK) {
        goto fail;
    }
    err = i2s_channel_enable(s_tx);
    if (err != ESP_OK) {
        goto fail;
    }
    err = i2s_channel_enable(s_rx);
    if (err != ESP_OK) {
        goto fail;
    }

    audio_codec_i2s_cfg_t i2s_cfg = {
        .port = I2S_NUM_0,
        .tx_handle = s_tx,
        .rx_handle = s_rx,
    };
    s_data_if = audio_codec_new_i2s_data(&i2s_cfg);
    if (!s_data_if) {
        err = ESP_FAIL;
        goto fail;
    }
    ESP_LOGI(TAG, "I2S duplex BCK=%d WS=%d DO=%d DI=%d PA=%d (MCLK not used)",
             BSP_AUDIO_I2S_SCLK, BSP_AUDIO_I2S_LCLK, BSP_AUDIO_I2S_DOUT,
             BSP_AUDIO_I2S_DSIN, BSP_POWER_AMP_IO);
    return ESP_OK;

fail:
    if (s_rx) {
        i2s_del_channel(s_rx);
        s_rx = NULL;
    }
    if (s_tx) {
        i2s_del_channel(s_tx);
        s_tx = NULL;
    }
    return err;
}

const audio_codec_data_if_t *bsp_audio_get_codec_itf(void)
{
    return s_data_if;
}

static const audio_codec_if_t *bsp_es8311_get(void)
{
    if (s_codec_if) {
        return s_codec_if;
    }
    if (bsp_i2c_init() != ESP_OK || bsp_audio_init(NULL) != ESP_OK) {
        return NULL;
    }

    const audio_codec_gpio_if_t *gpio_if = audio_codec_new_gpio();
    BSP_NULL_CHECK(gpio_if, NULL);

    audio_codec_i2c_cfg_t i2c_cfg = {
        .port = BSP_I2C_NUM,
        .addr = ES8311_CODEC_DEFAULT_ADDR,
        .bus_handle = bsp_i2c_get_handle(),
    };
    const audio_codec_ctrl_if_t *ctrl_if = audio_codec_new_i2c_ctrl(&i2c_cfg);
    BSP_NULL_CHECK(ctrl_if, NULL);

    es8311_codec_cfg_t es_cfg = {
        .ctrl_if = ctrl_if,
        .gpio_if = gpio_if,
        .codec_mode = ESP_CODEC_DEV_WORK_MODE_BOTH,
        .pa_pin = BSP_POWER_AMP_IO,
        .pa_reverted = false,
        .master_mode = false,
        .use_mclk = false,
        .digital_mic = false,
        .invert_mclk = false,
        .invert_sclk = false,
        .hw_gain = {
            .pa_voltage = 5.0f,
            .codec_dac_voltage = 3.3f,
        },
        .no_dac_ref = false,
    };
    s_codec_if = es8311_codec_new(&es_cfg);
    return s_codec_if;
}

esp_codec_dev_handle_t bsp_audio_codec_speaker_init(void)
{
    if (s_speaker) {
        return s_speaker;
    }
    const audio_codec_if_t *codec = bsp_es8311_get();
    BSP_NULL_CHECK(codec, NULL);

    esp_codec_dev_cfg_t dev_cfg = {
        .dev_type = ESP_CODEC_DEV_TYPE_OUT,
        .codec_if = codec,
        .data_if = s_data_if,
    };
    s_speaker = esp_codec_dev_new(&dev_cfg);
    return s_speaker;
}

esp_codec_dev_handle_t bsp_audio_codec_microphone_init(void)
{
    if (s_mic) {
        return s_mic;
    }
    const audio_codec_if_t *codec = bsp_es8311_get();
    BSP_NULL_CHECK(codec, NULL);

    esp_codec_dev_cfg_t dev_cfg = {
        .dev_type = ESP_CODEC_DEV_TYPE_IN,
        .codec_if = codec,
        .data_if = s_data_if,
    };
    s_mic = esp_codec_dev_new(&dev_cfg);
    return s_mic;
}
