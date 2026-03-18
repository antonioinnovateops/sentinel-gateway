/**
 * @file actuator_control.h
 * @brief FW-02: Actuator Control module
 */

#ifndef ACTUATOR_CONTROL_H
#define ACTUATOR_CONTROL_H

#include "../common/sentinel_types.h"

sentinel_err_t actuator_init(const system_config_t *config);
sentinel_err_t actuator_set(uint8_t id, float percent, uint32_t *status);
sentinel_err_t actuator_get(uint8_t id, float *percent);
sentinel_err_t actuator_set_limits(uint8_t id, float min_pct, float max_pct);
sentinel_err_t actuator_failsafe(void);
bool actuator_verify_readback(uint8_t id);

#endif /* ACTUATOR_CONTROL_H */
