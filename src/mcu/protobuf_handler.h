/**
 * @file protobuf_handler.h
 * @brief FW-03: nanopb encode/decode for MCU
 */

#ifndef MCU_PROTOBUF_HANDLER_H
#define MCU_PROTOBUF_HANDLER_H

#include "../common/sentinel_types.h"
#include "health_reporter.h"
#include <stddef.h>

/* Encode buffer size */
#define PB_ENCODE_BUF_SIZE  512U

/** Encode SensorData into wire frame */
sentinel_err_t pb_encode_sensor_data(const sensor_reading_t *readings,
                                      uint32_t count, uint32_t rate_hz,
                                      uint8_t *out_buf, size_t *out_len);

/** Encode HealthStatus into wire frame */
sentinel_err_t pb_encode_health_status(const health_data_t *health,
                                        uint8_t *out_buf, size_t *out_len);

/** Encode ActuatorResponse into wire frame */
sentinel_err_t pb_encode_actuator_response(uint8_t actuator_id,
                                            float applied_percent,
                                            uint32_t status,
                                            uint8_t *out_buf, size_t *out_len);

/** Encode ConfigResponse into wire frame */
sentinel_err_t pb_encode_config_response(uint32_t status,
                                          const char *error_msg,
                                          uint8_t *out_buf, size_t *out_len);

/** Decode an incoming wire frame and dispatch */
sentinel_err_t pb_decode_and_dispatch(const uint8_t *frame, size_t frame_len);

/** Get next sequence number */
uint32_t pb_get_sequence(void);

#endif /* MCU_PROTOBUF_HANDLER_H */
