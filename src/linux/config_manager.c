/**
 * @file config_manager.c
 * @brief SW-07: Configuration management
 * @implements SWE-058, SWE-061
 */

#include "config_manager.h"
#include <string.h>
#include <stdio.h>

static system_config_t g_config;

static void load_defaults(void)
{
    for (uint8_t i = 0U; i < SENTINEL_MAX_CHANNELS; i++) {
        g_config.sensor_rates_hz[i] = 10U;
    }
    g_config.actuator_min_percent[0] = 0.0f;
    g_config.actuator_min_percent[1] = 0.0f;
    g_config.actuator_max_percent[0] = 95.0f;
    g_config.actuator_max_percent[1] = 95.0f;
    g_config.heartbeat_interval_ms = SENTINEL_HEALTH_INTERVAL_MS;
    g_config.comm_timeout_ms = SENTINEL_COMM_TIMEOUT_MS;
}

sentinel_err_t config_manager_init(const char *config_file_path)
{
    load_defaults();

    if (config_file_path != NULL) {
        /* TODO: Parse JSON config file and override defaults */
        (void)config_file_path;
    }
    return SENTINEL_OK;
}

sentinel_err_t config_manager_update_sensor_rate(uint8_t channel, uint32_t rate_hz)
{
    if (channel >= SENTINEL_MAX_CHANNELS) {
        return SENTINEL_ERR_INVALID_ARG;
    }
    if ((rate_hz < 1U) || (rate_hz > 100U)) {
        return SENTINEL_ERR_OUT_OF_RANGE;
    }
    g_config.sensor_rates_hz[channel] = rate_hz;
    /* TODO: Send ConfigUpdate protobuf to MCU */
    return SENTINEL_OK;
}

sentinel_err_t config_manager_update_actuator_limits(uint8_t actuator,
                                                      float min_pct, float max_pct)
{
    if (actuator >= SENTINEL_MAX_ACTUATORS) {
        return SENTINEL_ERR_INVALID_ARG;
    }
    if (min_pct >= max_pct) {
        return SENTINEL_ERR_INVALID_ARG;
    }
    g_config.actuator_min_percent[actuator] = min_pct;
    g_config.actuator_max_percent[actuator] = max_pct;
    /* TODO: Send ConfigUpdate protobuf to MCU */
    return SENTINEL_OK;
}

sentinel_err_t config_manager_get_current(system_config_t *out)
{
    if (out == NULL) {
        return SENTINEL_ERR_INVALID_ARG;
    }
    *out = g_config;
    return SENTINEL_OK;
}

sentinel_err_t config_manager_process_response(const uint8_t *payload, size_t len)
{
    /* TODO: Decode ConfigResponse protobuf */
    (void)payload;
    (void)len;
    return SENTINEL_OK;
}
