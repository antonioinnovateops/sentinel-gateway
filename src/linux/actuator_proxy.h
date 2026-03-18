/**
 * @file actuator_proxy.h
 * @brief SW-04: Sends actuator commands to MCU
 */

#ifndef ACTUATOR_PROXY_H
#define ACTUATOR_PROXY_H

#include "../common/sentinel_types.h"
#include <stddef.h>

typedef void (*actuator_response_cb_t)(uint8_t actuator_id, float applied,
                                        uint32_t status, void *ctx);

sentinel_err_t actuator_proxy_init(void);
sentinel_err_t actuator_proxy_send_command(uint8_t actuator_id, float value_percent,
                                            actuator_response_cb_t cb, void *ctx);
sentinel_err_t actuator_proxy_process_response(const uint8_t *payload, size_t len);
sentinel_err_t actuator_proxy_get_state(uint8_t actuator_id, actuator_state_t *out);

#endif /* ACTUATOR_PROXY_H */
