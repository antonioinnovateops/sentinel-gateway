/**
 * @file sensor_proxy.h
 * @brief SW-03: Receives sensor data from MCU, caches and logs
 */

#ifndef SENSOR_PROXY_H
#define SENSOR_PROXY_H

#include "../common/sentinel_types.h"
#include <stddef.h>

sentinel_err_t sensor_proxy_init(void);
sentinel_err_t sensor_proxy_process(const uint8_t *payload, size_t len);
sentinel_err_t sensor_proxy_get_latest(uint8_t channel, sensor_reading_t *out);
sentinel_err_t sensor_proxy_get_all(sensor_reading_t readings[4], uint32_t *count);

#endif /* SENSOR_PROXY_H */
