/**
 * @file sensor_acquisition.c
 * @brief FW-01: ADC sampling, calibration, rate control
 * @implements SWE-001 through SWE-012
 */

#include "sensor_acquisition.h"
#include "hal/adc_driver.h"
#include "hal/systick.h"
#include <stddef.h>

/* Calibration constants (linear: calibrated = raw * scale + offset) */
static const float g_cal_scale[SENTINEL_MAX_CHANNELS] = {
    0.0806f,   /* CH0: Temperature °C (0-330°C over 0-4095) */
    0.0244f,   /* CH1: Humidity %RH (0-100 over 0-4095) */
    0.3663f,   /* CH2: Pressure hPa (300-1100 over 0-4095) */
    24.42f     /* CH3: Light lux (0-100000 over 0-4095) */
};

static const float g_cal_offset[SENTINEL_MAX_CHANNELS] = {
    -40.0f,    /* Temperature offset */
    0.0f,      /* Humidity offset */
    300.0f,    /* Pressure offset */
    0.0f       /* Light offset */
};

static uint32_t g_sample_rates_hz[SENTINEL_MAX_CHANNELS];
static uint32_t g_last_sample_ms[SENTINEL_MAX_CHANNELS];

sentinel_err_t sensor_acq_init(const system_config_t *config)
{
    if (config == NULL) {
        return SENTINEL_ERR_INVALID_ARG;
    }

    sentinel_err_t err = adc_init();
    if (err != SENTINEL_OK) {
        return err;
    }

    for (uint8_t i = 0U; i < SENTINEL_MAX_CHANNELS; i++) {
        g_sample_rates_hz[i] = config->sensor_rates_hz[i];
        g_last_sample_ms[i] = 0U;
    }
    return SENTINEL_OK;
}

bool sensor_acq_is_due(uint32_t now_ms)
{
    for (uint8_t ch = 0U; ch < SENTINEL_MAX_CHANNELS; ch++) {
        if (g_sample_rates_hz[ch] == 0U) {
            continue;
        }
        uint32_t interval = 1000U / g_sample_rates_hz[ch];
        if ((now_ms - g_last_sample_ms[ch]) >= interval) {
            return true;
        }
    }
    return false;
}

sentinel_err_t sensor_acq_sample(sensor_reading_t readings[SENTINEL_MAX_CHANNELS],
                                  uint32_t *count)
{
    if ((readings == NULL) || (count == NULL)) {
        return SENTINEL_ERR_INVALID_ARG;
    }

    uint32_t now_ms = systick_get_ms();
    uint32_t raw_values[SENTINEL_MAX_CHANNELS];
    *count = 0U;

    /* Scan all ADC channels */
    sentinel_err_t err = adc_scan_all(raw_values);
    if (err != SENTINEL_OK) {
        return err;
    }

    for (uint8_t ch = 0U; ch < SENTINEL_MAX_CHANNELS; ch++) {
        if (g_sample_rates_hz[ch] == 0U) {
            continue;
        }

        uint32_t interval = 1000U / g_sample_rates_hz[ch];
        if ((now_ms - g_last_sample_ms[ch]) >= interval) {
            readings[*count].channel = ch;
            readings[*count].raw_value = raw_values[ch];
            readings[*count].calibrated_value =
                ((float)raw_values[ch] * g_cal_scale[ch]) + g_cal_offset[ch];
            readings[*count].timestamp_ms = (uint64_t)now_ms;
            g_last_sample_ms[ch] = now_ms;
            (*count)++;
        }
    }
    return SENTINEL_OK;
}

void sensor_acq_set_rate(uint8_t channel, uint32_t rate_hz)
{
    if (channel < SENTINEL_MAX_CHANNELS) {
        g_sample_rates_hz[channel] = rate_hz;
    }
}
