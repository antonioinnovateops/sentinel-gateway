---
title: "Interface Specifications"
document_id: "IFS-001"
project: "Sentinel Gateway"
version: "1.0.0"
date: 2026-03-17
status: "Approved"
aspice_process: "SWE.2 BP3 — Define interfaces"
---

# Interface Specifications — Sentinel Gateway

## 1. Shared Data Types

### 1.1 Common Types (shared between Linux and MCU)

```c
/* sentinel_types.h — shared type definitions */

#include <stdint.h>
#include <stdbool.h>

/** Sensor channel enumeration */
typedef enum {
    SENSOR_CH_TEMPERATURE = 0U,
    SENSOR_CH_HUMIDITY    = 1U,
    SENSOR_CH_PRESSURE    = 2U,
    SENSOR_CH_LIGHT       = 3U,
    SENSOR_CH_COUNT       = 4U
} sensor_channel_t;

/** Actuator identification */
typedef enum {
    ACTUATOR_FAN   = 0U,
    ACTUATOR_VALVE = 1U,
    ACTUATOR_COUNT = 2U
} actuator_id_t;

/** MCU operational state */
typedef enum {
    MCU_STATE_INIT     = 0U,
    MCU_STATE_NORMAL   = 1U,
    MCU_STATE_DEGRADED = 2U,
    MCU_STATE_FAILSAFE = 3U,
    MCU_STATE_ERROR    = 4U
} mcu_state_t;

/** Communication link status */
typedef enum {
    COMM_CONNECTED    = 0U,
    COMM_DEGRADED     = 1U,
    COMM_DISCONNECTED = 2U
} comm_status_t;

/** Fail-safe cause codes */
typedef enum {
    FAILSAFE_NONE           = 0U,
    FAILSAFE_COMM_LOSS      = 1U,
    FAILSAFE_WATCHDOG       = 2U,
    FAILSAFE_PROTO_ERROR    = 3U,
    FAILSAFE_INTERNAL_ERROR = 4U
} failsafe_cause_t;

/** Log severity levels */
typedef enum {
    LOG_DEBUG    = 0U,
    LOG_INFO     = 1U,
    LOG_WARNING  = 2U,
    LOG_ERROR    = 3U,
    LOG_CRITICAL = 4U
} log_level_t;

/** Single sensor reading */
typedef struct {
    sensor_channel_t channel;
    uint16_t raw_value;           /* 0-4095 (12-bit ADC) */
    float calibrated_value;       /* Engineering units */
    char unit[8];                 /* Unit string */
    uint64_t timestamp_us;        /* Microseconds since boot */
    uint32_t sequence_number;
} sensor_reading_t;

/** Actuator state */
typedef struct {
    actuator_id_t id;
    float current_duty_percent;   /* 0.0-100.0% */
    float target_duty_percent;    /* Last commanded value */
    bool is_active;               /* False if fail-safe */
} actuator_state_t;

/** Per-channel sensor configuration */
typedef struct {
    sensor_channel_t channel;
    uint32_t sample_rate_hz;      /* 1-100 Hz */
    bool enabled;
} sensor_config_t;

/** Per-actuator safety limits */
typedef struct {
    actuator_id_t id;
    float min_percent;            /* Minimum duty cycle */
    float max_percent;            /* Maximum duty cycle */
} actuator_limits_t;

/** System configuration (persisted in flash) */
typedef struct {
    sensor_config_t sensors[SENSOR_CH_COUNT];
    actuator_limits_t actuators[ACTUATOR_COUNT];
    uint32_t auth_token;          /* Authentication token for safety changes */
    uint32_t crc32;               /* CRC-32 of all fields above */
} system_config_t;

/** Wire frame message types */
typedef enum {
    MSG_TYPE_SENSOR_DATA       = 0x01U,
    MSG_TYPE_HEALTH_STATUS     = 0x02U,
    MSG_TYPE_ACTUATOR_COMMAND  = 0x10U,
    MSG_TYPE_ACTUATOR_RESPONSE = 0x11U,
    MSG_TYPE_CONFIG_UPDATE     = 0x20U,
    MSG_TYPE_CONFIG_RESPONSE   = 0x21U,
    MSG_TYPE_DIAG_REQUEST      = 0x30U,
    MSG_TYPE_DIAG_RESPONSE     = 0x31U,
    MSG_TYPE_SYNC_REQUEST      = 0x40U,
    MSG_TYPE_SYNC_RESPONSE     = 0x41U
} msg_type_t;

/** Response status codes */
typedef enum {
    RESP_OK                    = 0U,
    RESP_ERR_INVALID_ACTUATOR  = 1U,
    RESP_ERR_OUT_OF_RANGE      = 2U,
    RESP_ERR_RATE_LIMITED      = 3U,
    RESP_ERR_AUTH_FAILED       = 4U,
    RESP_ERR_FAILSAFE_ACTIVE   = 5U,
    RESP_ERR_INTERNAL          = 6U
} response_status_t;

/** Error codes (function return values) */
#define SENTINEL_OK              ( 0)
#define SENTINEL_ERR_INVALID_ARG (-1)
#define SENTINEL_ERR_TIMEOUT     (-2)
#define SENTINEL_ERR_COMM        (-3)
#define SENTINEL_ERR_DECODE      (-4)
#define SENTINEL_ERR_AUTH        (-5)
#define SENTINEL_ERR_RANGE       (-6)
#define SENTINEL_ERR_RATE_LIMIT  (-7)
#define SENTINEL_ERR_FLASH       (-8)
#define SENTINEL_ERR_FAILSAFE    (-9)
#define SENTINEL_ERR_INTERNAL    (-10)
```

## 2. Wire Frame Format

### 2.1 Frame Structure
![Wire Frame Format](../../architecture/diagrams/wire_frame_format.drawio.svg)

### 2.2 Maximum Frame Size
- Maximum payload: 512 bytes
- Maximum frame: 517 bytes (4 + 1 + 512)
- MCU static buffers sized for this maximum

## 3. TCP Port Assignments

| Port | Listener | Connector | Purpose | Max Clients |
|------|----------|-----------|---------|-------------|
| 5000 | MCU | Linux | Commands | 1 |
| 5001 | Linux | MCU | Telemetry | 1 |
| 5002 | Linux | External | Diagnostics | 3 |

## 4. Diagnostic Command Protocol

### 4.1 Format
- Client sends: `COMMAND [arg1] [arg2]\r\n`
- Server responds: `OK [data]\r\n` or `ERROR [code] [message]\r\n`
- Multi-line responses end with `END\r\n`

### 4.2 Command Reference

| Command | Arguments | Response | Example |
|---------|-----------|----------|---------|
| `READ_SENSOR` | `<channel_id:0-3>` | `OK <name> <value> <unit> <timestamp>` | `READ_SENSOR 0` → `OK TEMPERATURE 23.5 °C 1710672000000` |
| `SET_ACTUATOR` | `<actuator_id:0-1> <percent:0-100>` | `OK ACTUATOR <id> SET <applied>%` | `SET_ACTUATOR 0 50` → `OK ACTUATOR 0 SET 50.0%` |
| `GET_STATUS` | (none) | Multi-line status dump | `GET_STATUS` → `COMM: CONNECTED\nMCU: NORMAL\n...END` |
| `GET_VERSION` | (none) | `LINUX <ver>\nMCU <ver>` | `GET_VERSION` → `LINUX 1.0.0-abc123\nMCU 1.0.0-def456\nEND` |
| `GET_LOG` | `[count:1-500]` | Multi-line log entries | `GET_LOG 10` → `2026-03-17T10:00:00 INFO ...\n...END` |
| `GET_CONFIG` | (none) | Multi-line config dump | `GET_CONFIG` → `CH0: 10Hz enabled\n...END` |
| `RESET_MCU` | (none) | `OK MCU_RESET_INITIATED` | `RESET_MCU` → `OK MCU_RESET_INITIATED` |
| `HELP` | (none) | Command list | `HELP` → `Available commands:\n...END` |

## 5. Calibration Formulae

| Channel | Raw → Engineering | Inverse |
|---------|-------------------|---------|
| Temperature | T = (raw/4095 × 3.3/3.3) × 165.0 - 40.0 °C | raw = (T + 40.0) / 165.0 × 4095 |
| Humidity | RH = raw/4095 × 100.0 % | raw = RH / 100.0 × 4095 |
| Pressure | P = 300.0 + raw/4095 × 800.0 hPa | raw = (P - 300.0) / 800.0 × 4095 |
| Light | L = raw/4095 × 100000.0 lux | raw = L / 100000.0 × 4095 |
