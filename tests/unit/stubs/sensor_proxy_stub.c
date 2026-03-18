/**
 * @file sensor_proxy_stub.c
 * @brief Stub for diagnostics tests
 */
#include "sensor_proxy.h"
#include <string.h>

static sensor_reading_t g_readings[SENTINEL_MAX_CHANNELS];

sentinel_err_t sensor_proxy_init(void) {
    memset(g_readings, 0, sizeof(g_readings));
    return SENTINEL_OK;
}

sentinel_err_t sensor_proxy_process(const uint8_t *p, size_t l) {
    (void)p; (void)l; return SENTINEL_OK;
}

sentinel_err_t sensor_proxy_get_latest(uint8_t ch, sensor_reading_t *out) {
    if (ch >= SENTINEL_MAX_CHANNELS || out == NULL) return SENTINEL_ERR_INVALID_ARG;
    *out = g_readings[ch];
    return SENTINEL_OK;
}

sentinel_err_t sensor_proxy_get_all(sensor_reading_t r[4], uint32_t *c) {
    if (r == NULL || c == NULL) return SENTINEL_ERR_INVALID_ARG;
    memcpy(r, g_readings, sizeof(g_readings));
    *c = SENTINEL_MAX_CHANNELS;
    return SENTINEL_OK;
}
