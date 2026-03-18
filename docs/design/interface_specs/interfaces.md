---
title: "Interface Specifications"
document_id: "IFS-001"
project: "Sentinel Gateway"
version: "2.0.0"
date: 2026-03-18
status: "Approved"
aspice_process: "SWE.2 BP3 — Define interfaces"
---

# Interface Specifications — Sentinel Gateway

## 1. Overview

This document specifies all interfaces in the Sentinel Gateway system:
1. Wire frame format (TCP message envelope)
2. TCP port assignments and connection behavior
3. Diagnostic command protocol
4. HAL driver interfaces
5. Data type definitions

**Version 2.0 Note**: Updated with exact byte layouts, implementation references, and lessons learned from implementation.

---

## 2. Wire Frame Format

### 2.1 Frame Structure

Every message sent over TCP is wrapped in a wire frame. There is **no CRC in the wire frame** — TCP provides integrity via checksums.

```
+--------+--------+--------+--------+--------+--------+--------+
| Byte 0 | Byte 1 | Byte 2 | Byte 3 | Byte 4 | Byte 5 | ... N  |
+--------+--------+--------+--------+--------+--------+--------+
|     payload_length (LE uint32)    |msg_type| payload[0..N]  |
+--------+--------+--------+--------+--------+--------+--------+
```

| Offset | Size | Field | Description |
|--------|------|-------|-------------|
| 0 | 4 | `payload_length` | Little-endian uint32. Number of payload bytes (not including header). |
| 4 | 1 | `msg_type` | Message type identifier (0x01-0x41). |
| 5 | N | `payload` | Application data. Max 507 bytes. |

**Header Size**: `WIRE_FRAME_HEADER_SIZE = 5`
**Max Payload**: `WIRE_FRAME_MAX_PAYLOAD = 507`
**Max Frame**: `WIRE_FRAME_MAX_SIZE = 512`

### 2.2 Implementation

```c
/* src/common/sentinel_types.h */
#define WIRE_FRAME_HEADER_SIZE  5U
#define WIRE_FRAME_MAX_PAYLOAD  507U
#define WIRE_FRAME_MAX_SIZE     (WIRE_FRAME_HEADER_SIZE + WIRE_FRAME_MAX_PAYLOAD)

/* src/common/wire_frame.h */
sentinel_err_t wire_frame_encode(uint8_t msg_type,
                                  const uint8_t *payload, size_t payload_len,
                                  uint8_t *out_buf, size_t *out_len);

sentinel_err_t wire_frame_decode_header(const uint8_t *buf, size_t buf_len,
                                         uint8_t *msg_type, uint32_t *payload_len);
```

### 2.3 Message Types

| ID | Constant | Direction | Description |
|----|----------|-----------|-------------|
| 0x01 | `MSG_TYPE_SENSOR_DATA` | MCU → Linux | Sensor readings (telemetry) |
| 0x02 | `MSG_TYPE_HEALTH_STATUS` | MCU → Linux | Health heartbeat |
| 0x10 | `MSG_TYPE_ACTUATOR_CMD` | Linux → MCU | Actuator command |
| 0x11 | `MSG_TYPE_ACTUATOR_RSP` | MCU → Linux | Actuator response |
| 0x20 | `MSG_TYPE_CONFIG_UPDATE` | Linux → MCU | Configuration change |
| 0x21 | `MSG_TYPE_CONFIG_RSP` | MCU → Linux | Configuration response |
| 0x30 | `MSG_TYPE_DIAG_REQ` | Linux → MCU | Diagnostic request |
| 0x31 | `MSG_TYPE_DIAG_RSP` | MCU → Linux | Diagnostic response |
| 0x40 | `MSG_TYPE_STATE_SYNC_REQ` | Linux → MCU | State sync request |
| 0x41 | `MSG_TYPE_STATE_SYNC_RSP` | MCU → Linux | State sync response |

### 2.4 Encoding Example

Encoding a 3-byte payload with message type 0x01:
```
Input:  msg_type=0x01, payload=[0xAA, 0xBB, 0xCC], payload_len=3
Output: [0x03, 0x00, 0x00, 0x00, 0x01, 0xAA, 0xBB, 0xCC]
         ^--- LE length=3 ----^  ^type^  ^-- payload --^
```

---

## 3. TCP Port Assignments

### 3.1 Port Configuration

| Port | Constant | Listener | Connector | Purpose | Max Clients |
|------|----------|----------|-----------|---------|-------------|
| 5000 | `SENTINEL_PORT_COMMAND` | MCU | Linux | Commands/Responses | 1 |
| 5001 | `SENTINEL_PORT_TELEMETRY` | Linux | MCU | Sensor/Health telemetry | 1 |
| 5002 | `SENTINEL_PORT_DIAG` | Linux | External | Diagnostic interface | 1 |

**Environment Variable Overrides**:
- `SENTINEL_MCU_HOST`: MCU IP address (default: `192.168.7.2`, use `127.0.0.1` for QEMU SIL)
- `SENTINEL_LINUX_HOST`: Linux IP address (default: `192.168.7.1`)

### 3.2 Connection Behavior

**Port 5000 (Command)**:
- MCU listens, Linux connects
- Single connection; new connection replaces existing
- Messages: ActuatorCommand, ConfigUpdate, DiagnosticRequest (Linux → MCU)
- Messages: ActuatorResponse, ConfigResponse, DiagnosticResponse (MCU → Linux)

**Port 5001 (Telemetry)**:
- Linux listens, MCU connects
- Single connection; reconnects on failure with exponential backoff
- Messages: SensorData, HealthStatus (MCU → Linux only)

**Port 5002 (Diagnostics)**:
- Linux listens, external clients connect
- Single connection; new connection drops existing (**not multi-client**)
- Plain-text protocol (not wire frame)

### 3.3 Reconnection Behavior

On TCP connection loss:
1. Initial delay: 100 ms
2. Double delay each attempt: 100, 200, 400, 800, 1600, 3200, 5000 ms
3. Maximum delay: 5000 ms (cap)
4. Infinite retries (no limit on MCU; Linux has 3 recovery attempts)

```c
/* Exponential backoff pseudocode */
delay_ms = 100;
while (!connected) {
    attempt_connect();
    if (!connected) {
        sleep_ms(delay_ms);
        delay_ms = min(delay_ms * 2, 5000);
    }
}
```

---

## 4. Shared Data Types

### 4.1 Header File

All types defined in `src/common/sentinel_types.h`:

```c
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

/* Default IP addresses */
#define SENTINEL_LINUX_IP  "192.168.7.1"
#define SENTINEL_MCU_IP    "192.168.7.2"

/* Timing constants */
#define SENTINEL_HEALTH_INTERVAL_MS   1000U
#define SENTINEL_COMM_TIMEOUT_MS      3000U
#define SENTINEL_WATCHDOG_TIMEOUT_MS  2000U
#define SENTINEL_ACT_RESPONSE_TIMEOUT_MS 500U

/* Protocol version */
#define SENTINEL_PROTO_VERSION_MAJOR  1U
#define SENTINEL_PROTO_VERSION_MINOR  0U
```

### 4.2 Sensor Reading Structure

```c
typedef struct {
    uint8_t  channel;           /* 0-3 */
    uint32_t raw_value;         /* 0-4095 (12-bit ADC) */
    float    calibrated_value;  /* Engineering units */
    uint64_t timestamp_ms;      /* Milliseconds since boot */
} sensor_reading_t;
```

### 4.3 Actuator State Structure

```c
typedef struct {
    uint8_t  actuator_id;           /* 0=Fan, 1=Valve */
    float    commanded_percent;     /* What was requested */
    float    applied_percent;       /* What was actually applied (after clamping) */
    uint32_t status;                /* SENTINEL_OK or error code */
    uint64_t command_timestamp_ms;
    uint64_t response_timestamp_ms;
} actuator_state_t;
```

### 4.4 System Configuration Structure

```c
typedef struct {
    uint32_t sensor_rates_hz[SENTINEL_MAX_CHANNELS];    /* 1-100 Hz per channel */
    float    actuator_min_percent[SENTINEL_MAX_ACTUATORS];
    float    actuator_max_percent[SENTINEL_MAX_ACTUATORS];
    uint32_t heartbeat_interval_ms;
    uint32_t comm_timeout_ms;
} system_config_t;
```

### 4.5 System State Enumeration

```c
typedef enum {
    STATE_INIT     = 0,  /* Starting up */
    STATE_NORMAL   = 1,  /* Normal operation */
    STATE_DEGRADED = 2,  /* Communication issues detected */
    STATE_FAILSAFE = 3,  /* Safe state (actuators at 0%) */
    STATE_ERROR    = 4   /* Fatal error, requires restart */
} system_state_t;
```

### 4.6 Fault IDs

```c
typedef enum {
    FAULT_NONE             = 0,
    FAULT_COMM_LOSS        = 1,  /* No messages from peer for timeout period */
    FAULT_ACTUATOR_READBACK = 2, /* PWM register readback mismatch */
    FAULT_ADC_TIMEOUT      = 3,  /* ADC conversion timeout */
    FAULT_FLASH_CRC        = 4,  /* Config CRC mismatch */
    FAULT_STACK_OVERFLOW   = 5   /* Stack canary triggered */
} fault_id_t;
```

---

## 5. Error Codes

Defined in `src/common/error_codes.h`:

```c
typedef enum {
    SENTINEL_OK              =   0,  /* Success */
    SENTINEL_ERR_INVALID_ARG =  -1,  /* NULL pointer, invalid ID */
    SENTINEL_ERR_OUT_OF_RANGE = -2,  /* Value exceeds limits */
    SENTINEL_ERR_TIMEOUT     =  -3,  /* Operation timed out */
    SENTINEL_ERR_COMM        =  -4,  /* Communication failure */
    SENTINEL_ERR_DECODE      =  -5,  /* Wire frame/protobuf decode error */
    SENTINEL_ERR_ENCODE      =  -6,  /* Encode error */
    SENTINEL_ERR_FULL        =  -7,  /* Buffer full, rate limited */
    SENTINEL_ERR_NOT_READY   =  -8,  /* Module not initialized */
    SENTINEL_ERR_AUTH        =  -9,  /* Authentication failed (reserved) */
    SENTINEL_ERR_INTERNAL    = -10   /* Internal error */
} sentinel_err_t;
```

---

## 6. Diagnostic Command Protocol

### 6.1 Protocol Overview

- **Transport**: TCP port 5002, plain text
- **Command format**: `command [arg1] [arg2]\n` (newline-terminated)
- **Response format**: Depends on command (see below)
- **Case sensitivity**: Commands are **lowercase only** (case-sensitive)

### 6.2 Command Reference

| Command | Arguments | Response | Example |
|---------|-----------|----------|---------|
| `status` | (none) | `state=<STATE> uptime=<N>s wd_resets=<N> comm=<OK\|LOST>` | `status` → `state=NORMAL uptime=123s wd_resets=0 comm=OK` |
| `sensor read` | `<ch:0-3>` | `ch=<N> raw=<N> cal=<F>` | `sensor read 0` → `ch=0 raw=2048 cal=42.500` |
| `actuator set` | `<id:0-1> <pct:0-100>` | `OK` or `ERROR` | `actuator set 0 50.0` → `OK` |
| `version` | (none) | `linux=<VER> mcu=<VER>` | `version` → `linux=1.0.0 mcu=pending` |
| `help` | (none) | Command list | `help` → `Commands: status, sensor read <ch>, ...` |

### 6.3 Error Responses

| Error | Response |
|-------|----------|
| Unknown command | `ERROR: unknown command` |
| Invalid channel | `ERROR: invalid channel` |
| Invalid syntax | `ERROR: usage: <correct_syntax>` |
| MCU error | `ERROR` (generic for actuator failures) |

### 6.4 Implementation

```c
/* src/linux/diagnostics.c */
sentinel_err_t diagnostics_process_command(const char *cmd,
                                            char *response,
                                            size_t response_size);
```

**Important**: Commands use `strncmp()` for prefix matching:
- `"sensor read "` (with trailing space) matches commands starting with `sensor read `
- Channel is parsed as `cmd[12] - '0'` (single digit)

---

## 7. HAL Driver Interfaces

### 7.1 ADC Driver

```c
/* src/mcu/hal/adc_driver.h */
#define ADC_MAX_CHANNELS  4U
#define ADC_RESOLUTION    4095U  /* 12-bit: 0-4095 */

sentinel_err_t adc_init(void);
sentinel_err_t adc_read_channel(uint8_t channel, uint32_t *value);
sentinel_err_t adc_scan_all(uint32_t values[ADC_MAX_CHANNELS]);
```

**QEMU Implementation**: `src/mcu/hal/qemu/hal_shim_posix.c`
- Returns simulated values with slight drift
- Base values: `{2048, 1024, 3000, 500}` for channels 0-3

### 7.2 PWM Driver

```c
/* src/mcu/hal/pwm_driver.h */
#define PWM_MAX_CHANNELS 2U
#define PWM_ARR_VALUE    999U  /* 0-999 for 0.0%-100.0% (0.1% resolution) */

sentinel_err_t pwm_init(void);
sentinel_err_t pwm_set_duty(uint8_t channel, float percent);
sentinel_err_t pwm_get_duty(uint8_t channel, float *percent);
sentinel_err_t pwm_set_all_zero(void);
```

**Duty Cycle Calculation**:
```c
uint32_t compare = (uint32_t)((percent / 100.0f) * PWM_ARR_VALUE);
/* 0% → compare=0, 100% → compare=999 */
```

### 7.3 Watchdog Driver

```c
/* src/mcu/hal/watchdog_driver.h */
sentinel_err_t iwdg_init(uint32_t timeout_ms);
void iwdg_feed(void);
bool iwdg_was_reset_cause(void);
```

**QEMU Implementation**: No-op (QEMU user-mode has no watchdog)

### 7.4 Flash Driver

```c
/* src/mcu/hal/flash_driver.h */
#define FLASH_CONFIG_ADDR   0x08030000UL
#define FLASH_CONFIG_SIZE   4096U  /* 4 KB sector */
#define FLASH_PAGE_SIZE     4096U

sentinel_err_t flash_init(void);
sentinel_err_t flash_erase_page(uint32_t addr);
sentinel_err_t flash_write(uint32_t addr, const uint8_t *data, size_t len);
sentinel_err_t flash_read(uint32_t addr, uint8_t *data, size_t len);
```

**QEMU Implementation**: RAM-backed array (not persistent across runs)

### 7.5 GPIO Driver

```c
/* src/mcu/hal/gpio_driver.h */
sentinel_err_t gpio_init(void);
void gpio_led_set(bool on);
void gpio_led_toggle(void);
```

---

## 8. Calibration Formulae

### 8.1 ADC to Engineering Units

All channels use 12-bit ADC (0-4095) with 3.3V reference.

| Channel | Sensor | Formula | Unit | Range |
|---------|--------|---------|------|-------|
| 0 | Temperature | `T = (raw / 4095.0) * 165.0 - 40.0` | °C | -40 to +125 |
| 1 | Humidity | `RH = (raw / 4095.0) * 100.0` | % | 0 to 100 |
| 2 | Pressure | `P = 300.0 + (raw / 4095.0) * 800.0` | hPa | 300 to 1100 |
| 3 | Light | `L = (raw / 4095.0) * 100000.0` | lux | 0 to 100000 |

### 8.2 Reference Values

| Channel | Raw 0 | Raw 2048 | Raw 4095 |
|---------|-------|----------|----------|
| Temperature | -40.0°C | 42.5°C | 125.0°C |
| Humidity | 0% | 50% | 100% |
| Pressure | 300 hPa | 700 hPa | 1100 hPa |
| Light | 0 lux | 50000 lux | 100000 lux |

### 8.3 Implementation

```c
/* src/mcu/sensor_acquisition.c */
static float calibrate_temperature(uint32_t raw) {
    return (((float)raw / 4095.0f) * 165.0f) - 40.0f;
}

static float calibrate_humidity(uint32_t raw) {
    return ((float)raw / 4095.0f) * 100.0f;
}

static float calibrate_pressure(uint32_t raw) {
    return 300.0f + (((float)raw / 4095.0f) * 800.0f);
}

static float calibrate_light(uint32_t raw) {
    return ((float)raw / 4095.0f) * 100000.0f;
}
```

---

## 9. Message Payloads

### 9.1 Sensor Data Payload

Sent via `MSG_TYPE_SENSOR_DATA` (0x01):

```c
typedef struct {
    uint8_t  channel;
    uint32_t raw_value;
    float    calibrated_value;
    uint64_t timestamp_ms;
} sensor_data_payload_t;
```

**Byte Layout** (packed, little-endian):
```
Offset  Size  Field
------  ----  -----
0       1     channel
1       4     raw_value (LE)
5       4     calibrated_value (IEEE 754 float, LE)
9       8     timestamp_ms (LE)
------  ----  -----
Total: 17 bytes per sensor reading
```

### 9.2 Health Status Payload

Sent via `MSG_TYPE_HEALTH_STATUS` (0x02):

```c
typedef struct {
    uint8_t  state;            /* system_state_t */
    uint32_t uptime_s;
    uint32_t watchdog_resets;
    uint8_t  comm_ok;          /* bool */
} health_status_payload_t;
```

### 9.3 Actuator Command Payload

Sent via `MSG_TYPE_ACTUATOR_CMD` (0x10):

```c
typedef struct {
    uint8_t actuator_id;
    float   commanded_percent;
} actuator_cmd_payload_t;
```

### 9.4 Actuator Response Payload

Sent via `MSG_TYPE_ACTUATOR_RSP` (0x11):

```c
typedef struct {
    uint8_t  actuator_id;
    float    applied_percent;  /* After clamping */
    int32_t  status;           /* sentinel_err_t */
} actuator_rsp_payload_t;
```

---

## 10. Unit Test Stubs

For unit testing on host (x86), HAL drivers are stubbed:

### 10.1 PWM Stub

```c
/* tests/unit/stubs/pwm_driver_stub.c */
static float g_pwm_duty[PWM_MAX_CHANNELS] = {0};

sentinel_err_t pwm_set_duty(uint8_t channel, float percent) {
    if (channel >= PWM_MAX_CHANNELS) return SENTINEL_ERR_INVALID_ARG;
    if (percent < 0.0f || percent > 100.0f) return SENTINEL_ERR_OUT_OF_RANGE;
    g_pwm_duty[channel] = percent;
    return SENTINEL_OK;
}
```

### 10.2 ADC Stub

```c
/* tests/unit/stubs/adc_stub.c */
static uint32_t g_adc_values[ADC_MAX_CHANNELS] = {2048, 2048, 2048, 2048};

sentinel_err_t adc_read_channel(uint8_t channel, uint32_t *value) {
    if (channel >= ADC_MAX_CHANNELS || value == NULL) {
        return SENTINEL_ERR_INVALID_ARG;
    }
    *value = g_adc_values[channel];
    return SENTINEL_OK;
}

/* Test helper to inject values */
void stub_adc_set_value(uint8_t channel, uint32_t value) {
    g_adc_values[channel] = value;
}
```

---

## 11. Traceability

| Requirement | Interface Element |
|-------------|-------------------|
| SYS-034, SYS-035 | Wire frame format (Section 2) |
| SYS-032, SYS-033 | TCP port assignments (Section 3) |
| SYS-046-SYS-051 | Diagnostic protocol (Section 6) |
| SYS-001-SYS-006 | ADC driver (Section 7.1) |
| SYS-016, SYS-017 | PWM driver (Section 7.2) |
| SYS-069, SYS-070 | Watchdog driver (Section 7.3) |
| SYS-060, SYS-065 | Flash driver (Section 7.4) |
| SYS-002-SYS-005 | Calibration formulae (Section 8) |
