/**
 * @file sentinel_types.h
 * @brief Shared data types for Sentinel Gateway
 */

#ifndef SENTINEL_TYPES_H
#define SENTINEL_TYPES_H

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include "error_codes.h"

/* Maximum channels/actuators */
#define SENTINEL_MAX_CHANNELS   4U
#define SENTINEL_MAX_ACTUATORS  2U

/* TCP ports */
#define SENTINEL_PORT_COMMAND    5000U
#define SENTINEL_PORT_TELEMETRY  5001U
#define SENTINEL_PORT_DIAG       5002U

/* Network addresses */
#define SENTINEL_LINUX_IP  "192.168.7.1"
#define SENTINEL_MCU_IP    "192.168.7.2"

/* Timing */
#define SENTINEL_HEALTH_INTERVAL_MS   1000U
#define SENTINEL_COMM_TIMEOUT_MS      3000U
#define SENTINEL_WATCHDOG_TIMEOUT_MS  2000U
#define SENTINEL_ACT_RESPONSE_TIMEOUT_MS 500U

/* Wire frame constants */
#define WIRE_FRAME_HEADER_SIZE  5U   /* 4-byte length + 1-byte type */
#define WIRE_FRAME_MAX_PAYLOAD  507U /* Max protobuf payload */
#define WIRE_FRAME_MAX_SIZE     (WIRE_FRAME_HEADER_SIZE + WIRE_FRAME_MAX_PAYLOAD)

/* Message type IDs (wire format) */
#define MSG_TYPE_SENSOR_DATA       0x01U
#define MSG_TYPE_HEALTH_STATUS     0x02U
#define MSG_TYPE_ACTUATOR_CMD      0x10U
#define MSG_TYPE_ACTUATOR_RSP      0x11U
#define MSG_TYPE_CONFIG_UPDATE     0x20U
#define MSG_TYPE_CONFIG_RSP        0x21U
#define MSG_TYPE_DIAG_REQ          0x30U
#define MSG_TYPE_DIAG_RSP          0x31U
#define MSG_TYPE_STATE_SYNC_REQ    0x40U
#define MSG_TYPE_STATE_SYNC_RSP    0x41U

/* Sensor reading */
typedef struct {
    uint8_t  channel;
    uint32_t raw_value;
    float    calibrated_value;
    uint64_t timestamp_ms;
} sensor_reading_t;

/* Actuator state */
typedef struct {
    uint8_t  actuator_id;
    float    commanded_percent;
    float    applied_percent;
    uint32_t status;
    uint64_t command_timestamp_ms;
    uint64_t response_timestamp_ms;
} actuator_state_t;

/* System configuration */
typedef struct {
    uint32_t sensor_rates_hz[SENTINEL_MAX_CHANNELS];
    float    actuator_min_percent[SENTINEL_MAX_ACTUATORS];
    float    actuator_max_percent[SENTINEL_MAX_ACTUATORS];
    uint32_t heartbeat_interval_ms;
    uint32_t comm_timeout_ms;
} system_config_t;

/* System state enum */
typedef enum {
    STATE_INIT     = 0,
    STATE_NORMAL   = 1,
    STATE_DEGRADED = 2,
    STATE_FAILSAFE = 3,
    STATE_ERROR    = 4
} system_state_t;

/* Fault IDs */
typedef enum {
    FAULT_NONE             = 0,
    FAULT_COMM_LOSS        = 1,
    FAULT_ACTUATOR_READBACK = 2,
    FAULT_ADC_TIMEOUT      = 3,
    FAULT_FLASH_CRC        = 4,
    FAULT_STACK_OVERFLOW   = 5
} fault_id_t;

/* Protocol version */
#define SENTINEL_PROTO_VERSION_MAJOR  1U
#define SENTINEL_PROTO_VERSION_MINOR  0U

/* Firmware version */
#ifndef SENTINEL_VERSION
#define SENTINEL_VERSION "1.0.0"
#endif

#endif /* SENTINEL_TYPES_H */
