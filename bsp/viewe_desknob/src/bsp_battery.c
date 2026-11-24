/*
 * SPDX-FileCopyrightText: 2026 VIEWE
 * SPDX-License-Identifier: Apache-2.0
 *
 * ESP32-S31 has no IDF ADC calibration scheme yet
 * (esp_adc/esp32s31/include/adc_cali_schemes.h leaves curve-fitting as TODO).
 * Voltage is a linear estimate from the differential ADC code span.
 * GPIO51 senses VCC_OUT (15k/10k), not a dedicated cell sense, and CHRG is
 * not wired, so this is not a fuel gauge.
 *
 * While USB lifts the rail, percent uses a slow time-based charge estimate
 * (not the elevated rail). After unplug, rail drop exits USB and voltage
 * re-maps SoC — no reboot required.
 */

#include "bsp/viewe_desknob.h"

#include "bsp_err_check.h"
#include "driver/gpio.h"
#include "esp_adc/adc_cali.h"
#include "esp_adc/adc_oneshot.h"
#include "esp_log.h"
#include "esp_timer.h"

static const char *TAG = "viewe_desknob.bat";

#define BSP_BATTERY_ADC_UNIT          ADC_UNIT_2
#define BSP_BATTERY_ADC_CHANNEL       ADC_CHANNEL_1
/* P-side single-ended codes from the ESP32-S31 ADC driver notes: -2 V .. +2 V. */
#define BSP_BATTERY_ADC_CODE_SPAN     4393
#define BSP_BATTERY_ADC_FULLSCALE_MV  2000
#define BSP_BATTERY_SAMPLE_COUNT      8

/*
 * USB lifts VCC_OUT; uncalibrated readings often stay below a full-cell absolute
 * threshold, so also treat a sudden rise vs the on-battery baseline as USB.
 * Exit on a drop from the USB rail peak (not a trap with AND-ed thresholds).
 */
#define BSP_BATTERY_USB_MV            4150
#define BSP_BATTERY_USB_STEP_MV       80
#define BSP_BATTERY_SETTLE_READS      3
/* Conservative time estimate without CHRG/capacity: 1% / 120 s. Cap at 99%. */
#define BSP_BATTERY_CHARGE_SEC_PER_PCT 120
#define BSP_BATTERY_FULL_MV           4200
#define BSP_BATTERY_EMPTY_MV          3300

static adc_oneshot_unit_handle_t s_adc;
static int s_last_pct = -1;
static int s_filt_mv;
static int s_batt_mv;
static int s_usb_rail_mv;
static int s_usb_entry_pct;
static int64_t s_usb_since_us;
static int s_settle;
static bool s_usb;
static bool s_cali_checked;
static bool s_cali_available;

/*
 * Approximate Li-ion points for a 3.7 V cell under light load.
 * Uncalibrated ADC + system-rail sense make this an estimate only.
 */
static const struct {
    int mv;
    int pct;
} s_soc_table[] = {
    {4200, 100},
    {4100, 91},
    {4000, 76},
    {3930, 62},
    {3870, 50},
    {3820, 40},
    {3780, 30},
    {3740, 22},
    {3680, 14},
    {3600, 8},
    {3500, 4},
    {3300, 0},
};

static int soc_from_mv(int mv)
{
    if (mv >= BSP_BATTERY_FULL_MV) {
        return 100;
    }
    if (mv <= BSP_BATTERY_EMPTY_MV) {
        return 0;
    }

    for (size_t i = 0; i + 1 < sizeof(s_soc_table) / sizeof(s_soc_table[0]); i++) {
        const int hi_mv = s_soc_table[i].mv;
        const int lo_mv = s_soc_table[i + 1].mv;
        if (mv <= hi_mv && mv >= lo_mv) {
            const int hi_pct = s_soc_table[i].pct;
            const int lo_pct = s_soc_table[i + 1].pct;
            const int span = hi_mv - lo_mv;
            if (span <= 0) {
                return hi_pct;
            }
            return lo_pct + (mv - lo_mv) * (hi_pct - lo_pct) / span;
        }
    }
    return 0;
}

static void check_adc_calibration_once(void)
{
    if (s_cali_checked) {
        return;
    }
    s_cali_checked = true;

    adc_cali_scheme_ver_t scheme = 0;
    const esp_err_t err = adc_cali_check_scheme(&scheme);
    s_cali_available = (err == ESP_OK && scheme != 0);
    if (!s_cali_available) {
        ESP_LOGW(TAG, "ESP32-S31 has no IDF ADC calibration scheme yet; battery mV is uncalibrated");
    }
}

bool bsp_battery_adc_calibrated(void)
{
    check_adc_calibration_once();
    return s_cali_available;
}

esp_err_t bsp_battery_init(void)
{
    if (s_adc) {
        return ESP_OK;
    }

    check_adc_calibration_once();

    const adc_oneshot_unit_init_cfg_t unit_cfg = {
        .unit_id = BSP_BATTERY_ADC_UNIT,
        .clk_src = ADC_DIGI_CLK_SRC_DEFAULT,
        .ulp_mode = ADC_ULP_MODE_DISABLE,
    };
    esp_err_t err = adc_oneshot_new_unit(&unit_cfg, &s_adc);
    if (err != ESP_OK) {
        return err;
    }

    const adc_oneshot_chan_cfg_t chan_cfg = {
        .bitwidth = ADC_BITWIDTH_DEFAULT,
        .atten = ADC_ATTEN_DB_0,
    };
    err = adc_oneshot_config_channel(s_adc, BSP_BATTERY_ADC_CHANNEL, &chan_cfg);
    if (err != ESP_OK) {
        adc_oneshot_del_unit(s_adc);
        s_adc = NULL;
        return err;
    }

    gpio_set_pull_mode(BSP_BATTERY_ADC_IO, GPIO_FLOATING);
    ESP_LOGI(TAG, "BAT_ADC GPIO%d, divider %d/%d ohm, cali=%s",
             BSP_BATTERY_ADC_IO, BSP_BATTERY_UPPER_OHM, BSP_BATTERY_LOWER_OHM,
             s_cali_available ? "yes" : "no");
    return ESP_OK;
}

esp_err_t bsp_battery_get_voltage_mv(int *voltage_mv)
{
    BSP_NULL_CHECK(voltage_mv, ESP_ERR_INVALID_ARG);
    BSP_ERROR_CHECK_RETURN_ERR(bsp_battery_init());

    int raw_sum = 0;
    for (int i = 0; i < BSP_BATTERY_SAMPLE_COUNT; i++) {
        int raw = 0;
        BSP_ERROR_CHECK_RETURN_ERR(adc_oneshot_read(s_adc, BSP_BATTERY_ADC_CHANNEL, &raw));
        if (raw < 0) {
            raw = 0;
        } else if (raw > BSP_BATTERY_ADC_CODE_SPAN) {
            raw = BSP_BATTERY_ADC_CODE_SPAN;
        }
        raw_sum += raw;
    }

    const int raw = raw_sum / BSP_BATTERY_SAMPLE_COUNT;
    int pin_mv = (int)(((int64_t)raw * (BSP_BATTERY_ADC_FULLSCALE_MV * 2)) / BSP_BATTERY_ADC_CODE_SPAN)
                 - BSP_BATTERY_ADC_FULLSCALE_MV;
    if (pin_mv < 0) {
        pin_mv = 0;
    } else if (pin_mv > BSP_BATTERY_ADC_FULLSCALE_MV) {
        pin_mv = BSP_BATTERY_ADC_FULLSCALE_MV;
    }

    const int rail_mv = pin_mv * (BSP_BATTERY_UPPER_OHM + BSP_BATTERY_LOWER_OHM) / BSP_BATTERY_LOWER_OHM;
    *voltage_mv = rail_mv;
    ESP_LOGD(TAG, "raw %d pin %d mV rail %d mV", raw, pin_mv, rail_mv);
    return ESP_OK;
}

static int charge_est_pct(void)
{
    if (s_usb_entry_pct < 0) {
        return -1;
    }
    const int64_t elapsed_us = esp_timer_get_time() - s_usb_since_us;
    if (elapsed_us < 0) {
        return s_usb_entry_pct;
    }
    const int gained = (int)(elapsed_us / (1000000LL * BSP_BATTERY_CHARGE_SEC_PER_PCT));
    int pct = s_usb_entry_pct + gained;
    if (pct > 99) {
        pct = 99;
    }
    return pct;
}

esp_err_t bsp_battery_get_level(int *percent, bool *usb_powered)
{
    int mv = 0;
    BSP_ERROR_CHECK_RETURN_ERR(bsp_battery_get_voltage_mv(&mv));

    if (!s_usb) {
        const bool abs_usb = (mv >= BSP_BATTERY_USB_MV);
        const bool step_usb = (s_batt_mv > 0 && mv >= s_batt_mv + BSP_BATTERY_USB_STEP_MV);
        if (abs_usb || step_usb) {
            s_usb = true;
            s_usb_rail_mv = mv;
            s_usb_since_us = esp_timer_get_time();
            s_usb_entry_pct = (s_last_pct >= 0) ? s_last_pct
                              : ((s_batt_mv > 0) ? soc_from_mv(s_batt_mv) : -1);
            ESP_LOGI(TAG, "USB/charge inferred (rail %d mV), hold/estimate from %d%%",
                     mv, s_usb_entry_pct);
        }
    } else {
        if (mv > s_usb_rail_mv) {
            s_usb_rail_mv = mv;
        }
        /* Unplug: rail falls from the USB peak. */
        if (mv + BSP_BATTERY_USB_STEP_MV <= s_usb_rail_mv) {
            s_usb = false;
            s_settle = BSP_BATTERY_SETTLE_READS;
            s_filt_mv = 0;
            ESP_LOGI(TAG, "USB/charge ended (rail %d mV, peak was %d), remeasure SoC",
                     mv, s_usb_rail_mv);
        }
    }

    if (usb_powered) {
        *usb_powered = s_usb;
    }

    if (s_usb) {
        const int est = charge_est_pct();
        if (est >= 0) {
            s_last_pct = est;
        }
        if (percent) {
            *percent = s_last_pct;
        }
        return ESP_OK;
    }

    if (s_settle > 0) {
        s_settle--;
        s_filt_mv = (s_filt_mv == 0) ? mv : (s_filt_mv + mv) / 2;
        if (s_settle > 0) {
            if (percent) {
                *percent = s_last_pct;
            }
            return ESP_OK;
        }
    } else if (s_filt_mv == 0) {
        s_filt_mv = mv;
    } else {
        s_filt_mv = (s_filt_mv * 3 + mv) / 4;
    }

    s_batt_mv = s_filt_mv;
    s_last_pct = soc_from_mv(s_filt_mv);
    if (percent) {
        *percent = s_last_pct;
    }
    return ESP_OK;
}
