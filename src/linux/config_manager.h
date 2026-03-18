/**
 * @file config_manager.h
 * @brief SW-07: System configuration management
 */

#ifndef CONFIG_MANAGER_H
#define CONFIG_MANAGER_H

#include "../common/sentinel_types.h"
#include <stddef.h>

sentinel_err_t config_manager_init(const char *config_file_path);
sentinel_err_t config_manager_update_sensor_rate(uint8_t channel, uint32_t rate_hz);
sentinel_err_t config_manager_update_actuator_limits(uint8_t actuator, float min_pct, float max_pct);
sentinel_err_t config_manager_get_current(system_config_t *out);
sentinel_err_t config_manager_process_response(const uint8_t *payload, size_t len);

#endif /* CONFIG_MANAGER_H */
