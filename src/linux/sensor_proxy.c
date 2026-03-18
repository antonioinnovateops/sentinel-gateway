/**
 * @file sensor_proxy.c
 * @brief SW-03: Sensor data reception and logging
 * @implements SWE-013, SWE-015
 */

#include "sensor_proxy.h"
#include "logger.h"
#include <string.h>
#include <stdio.h>

static sensor_reading_t g_latest[SENTINEL_MAX_CHANNELS];
static uint32_t g_msg_count = 0U;
static uint32_t g_err_count = 0U;

sentinel_err_t sensor_proxy_init(void)
{
    (void)memset(g_latest, 0, sizeof(g_latest));
    g_msg_count = 0U;
    g_err_count = 0U;
    return SENTINEL_OK;
}

sentinel_err_t sensor_proxy_process(const uint8_t *payload, size_t len)
{
    /* Decode protobuf SensorData payload
     * For now: simplified manual decode matching MCU encoder format
     * Full protobuf-c decode will replace this when generated files are integrated
     */
    (void)payload;
    (void)len;

    /* TODO: Decode SensorData protobuf into sensor_reading_t array
     * For each reading: validate channel, update g_latest, call logger_write_sensor
     */
    g_msg_count++;
    return SENTINEL_OK;
}

sentinel_err_t sensor_proxy_get_latest(uint8_t channel, sensor_reading_t *out)
{
    if ((channel >= SENTINEL_MAX_CHANNELS) || (out == NULL)) {
        return SENTINEL_ERR_INVALID_ARG;
    }
    *out = g_latest[channel];
    return SENTINEL_OK;
}

sentinel_err_t sensor_proxy_get_all(sensor_reading_t readings[4], uint32_t *count)
{
    if ((readings == NULL) || (count == NULL)) {
        return SENTINEL_ERR_INVALID_ARG;
    }
    (void)memcpy(readings, g_latest, sizeof(g_latest));
    *count = SENTINEL_MAX_CHANNELS;
    return SENTINEL_OK;
}
