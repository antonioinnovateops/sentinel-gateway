/**
 * @file sensor_acquisition.h
 * @brief FW-01: Sensor Acquisition module
 */

#ifndef SENSOR_ACQUISITION_H
#define SENSOR_ACQUISITION_H

#include "../common/sentinel_types.h"

sentinel_err_t sensor_acq_init(const system_config_t *config);
sentinel_err_t sensor_acq_sample(sensor_reading_t readings[SENTINEL_MAX_CHANNELS],
                                  uint32_t *count);
void sensor_acq_set_rate(uint8_t channel, uint32_t rate_hz);
bool sensor_acq_is_due(uint32_t now_ms);

#endif /* SENSOR_ACQUISITION_H */
