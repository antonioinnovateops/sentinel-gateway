---
title: "Software Architecture Document"
document_id: "SWAD-001"
project: "Sentinel Gateway"
version: "2.0.0"
date: 2026-03-18
status: "Approved"
asil_level: "ASIL-B (process rigor)"
aspice_process: "SWE.2 Software Architectural Design"
authors: ["AI Architecture Agent"]
reviewers: ["Antonio Stepien"]
---

# Software Architecture Document — Sentinel Gateway

## 1. Introduction

### 1.1 Purpose

This document defines the software architecture for both the Linux gateway application and the MCU firmware, including component decomposition, interfaces, data flows, and requirement allocation.

### 1.2 Reference Documents

| Document ID | Title | Version |
|-------------|-------|---------|
| SRS-001 | System Requirements Specification | v1.0.0 |
| SAD-001 | System Architecture Document | v1.0.0 |
| SWRS-001 | Software Requirements Specification | v1.0.0 |

---

## 2. MCU Firmware Architecture

### 2.1 Component Diagram

![System Architecture Overview](../diagrams/system_overview.drawio.svg)

### 2.2 MCU Component Descriptions

#### 2.2.1 main_loop
- **Purpose**: System initialization and super-loop scheduling
- **Pattern**: Sequential initialization → infinite main loop with cooperative scheduling
- **Requirements**: SWE-067-1, SWE-027-1, SWE-070-1
- **Responsibilities**:
  - Execute init sequence (HAL → safe state → watchdog → config → USB → TCP → timers)
  - Main loop: poll TCP stack → process received messages → feed watchdog
  - Error handling: on any critical error, trigger fail-safe and continue loop

#### 2.2.2 sensor_acquisition
- **Purpose**: Sample ADC channels and produce calibrated sensor readings
- **Requirements**: SWE-001-1 to SWE-010-1
- **Dependencies**: adc_driver, protobuf_handler, config_store
- **Data Flow**: Timer ISR triggers ADC scan → DMA complete ISR sets flag → main loop reads data → calibrate → encode → transmit
- **Shared Data**: ADC DMA buffer (ISR writes, main loop reads) — protected by volatile + flag mechanism

#### 2.2.3 actuator_control
- **Purpose**: Process actuator commands with safety validation
- **Requirements**: SWE-016-1 to SWE-029-1 (all actuator + fail-safe requirements)
- **Safety**: ASIL-B — this is the most safety-critical module
- **Dependencies**: pwm_driver, protobuf_handler, config_store
- **Safety Mechanisms**:
  - Range validation (reject out-of-range)
  - Limit clamping (enforce configured limits)
  - Output readback verification (detect register corruption)
  - Communication timeout (fail-safe to 0%)
  - Rate limiting (prevent actuator abuse)

#### 2.2.4 protobuf_handler
- **Purpose**: Encode and decode all protobuf messages using nanopb
- **Requirements**: SWE-007-2, SWE-018-1, SWE-036-1, SWE-037-1
- **Dependencies**: nanopb library (generated code from sentinel.proto)
- **Constraints**: All buffers statically allocated (SWE-072-1), max message size 512 bytes

#### 2.2.5 tcp_stack
- **Purpose**: Manage TCP connections and wire frame encoding/decoding
- **Requirements**: SWE-030-1 to SWE-035-2, SWE-043-2
- **Implementation Note (v2.0)**:
  - **Bare-metal** (`tcp_stack.c`): lwIP-based, stubbed in v1.0
  - **QEMU user-mode** (`tcp_stack_qemu.c`): POSIX sockets, fully functional
- **Dependencies**: For QEMU: POSIX sockets; For bare-metal: usb_cdc + lwIP (TODO)
- **Pattern**: Non-blocking with select() (QEMU) or lwIP NO_SYS polling (bare-metal)

#### 2.2.6 config_store
- **Purpose**: Persist and manage runtime configuration in flash
- **Requirements**: SWE-058-1 to SWE-065-1
- **Dependencies**: flash_driver, protobuf_handler
- **Pattern**: Read-on-boot, write-on-change, CRC-32 validation

#### 2.2.7 health_reporter
- **Purpose**: Generate heartbeat messages and monitor gateway activity
- **Requirements**: SWE-038-1, SWE-040-1, SWE-041-1, SWE-029-1
- **Dependencies**: protobuf_handler, tcp_stack, watchdog_mgr

#### 2.2.8 watchdog_mgr
- **Purpose**: Initialize and manage hardware watchdog
- **Requirements**: SWE-069-1, SWE-070-1, SWE-071-1
- **Dependencies**: IWDG HAL, flash_driver (reset counter in backup registers)

### 2.3 MCU Data Flow

![Sensor and Actuator Data Flow](../diagrams/data_flow.drawio.svg)

### 2.4 MCU Interrupt Priority Table

| Priority | Source | Handler | WCET |
|----------|--------|---------|------|
| 0 (highest) | IWDG | (hardware, no handler) | N/A |
| 1 | TIM3 (sample timer) | Start ADC conversion | < 1 µs |
| 2 | DMA1 (ADC complete) | Set sample_ready flag | < 1 µs |
| 3 | USB OTG FS | USB CDC-ECM events | < 50 µs |
| 4 | SysTick (1 kHz) | lwIP timer ticks | < 10 µs |
| Main loop | N/A | All application logic | < 500 ms |

---

## 3. Linux Gateway Architecture

### 3.1 Component Diagram

*The Linux gateway components are shown in the System Architecture Overview diagram above. See the left side (QEMU ARM64 Linux Gateway) for all components and their relationships.*

### 3.2 Linux Component Descriptions

#### 3.2.1 gateway_core
- **Purpose**: Main entry point, event loop, startup/shutdown orchestration
- **Requirements**: SWE-046-1, SWE-044-1, SWE-066-1, SWE-057-1
- **Pattern**: Single-process, epoll-based event loop
- **Thread Model**: 
  - Main thread: epoll event loop (all TCP I/O, message processing)
  - Logger thread: async log writer (dequeues from ring buffer, writes to disk)
- **Startup**: init logger → open server ports → wait USB → connect MCU → sync → loop

#### 3.2.2 sensor_proxy
- **Purpose**: Receive sensor data from MCU and forward to logger
- **Requirements**: SWE-013-1, SWE-013-2, SWE-036-2
- **Dependencies**: tcp_transport (receive), logger (write), protobuf-c (decode)
- **State**: Caches latest reading per channel for diagnostic queries

#### 3.2.3 actuator_proxy
- **Purpose**: Send actuator commands and process responses
- **Requirements**: SWE-018-2, SWE-019-2
- **Dependencies**: tcp_transport (send/receive), protobuf-c (encode/decode)
- **Pattern**: Request-response with 5-second timeout

#### 3.2.4 health_monitor
- **Purpose**: Monitor MCU heartbeat and manage recovery
- **Requirements**: SWE-039-1, SWE-042-1, SWE-043-1, SWE-045-1
- **Dependencies**: tcp_transport (receive), logger
- **State Machine**: MONITORING → COMM_LOSS → RECOVERING → FAULT

#### 3.2.5 diagnostics
- **Purpose**: TCP diagnostic CLI server
- **Requirements**: SWE-046-2 to SWE-054-1
- **Dependencies**: sensor_proxy (read cache), actuator_proxy (forward), health_monitor (status), logger (read log)
- **Pattern**: Line-oriented text protocol, max 3 concurrent clients

#### 3.2.6 config_manager
- **Purpose**: Send configuration updates to MCU
- **Requirements**: SWE-058-2, SWE-061-1
- **Dependencies**: tcp_transport (send), protobuf-c (encode)

#### 3.2.7 tcp_transport
- **Purpose**: Manage all TCP connections and wire framing
- **Requirements**: SWE-035-3, SWE-035-4, SWE-043-1
- **Connections**:
  - Client to MCU port 5000 (commands)
  - Server on port 5001 (telemetry, single MCU client)
  - Server on port 5002 (diagnostics, max 3 clients)
- **Framing**: 4-byte LE length + 1-byte type + payload

#### 3.2.8 logger
- **Purpose**: Structured event and sensor data logging
- **Requirements**: SWE-013-2, SWE-015-1, SWE-052-1, SWE-053-1, SWE-055-1
- **Pattern**: Lock-free ring buffer → writer thread → fsync every 5s
- **Files**: `/var/log/sentinel/sensor_data.jsonl`, `/var/log/sentinel/events.jsonl`

---

## 4. Interface Specifications

### 4.1 MCU Internal Interfaces

#### IF-MCU-01: adc_driver API
```c
/* src/mcu/hal/adc_driver.h */

#define ADC_MAX_CHANNELS  4U
#define ADC_RESOLUTION    4095U  /* 12-bit */

/** Initialize ADC peripheral */
sentinel_err_t adc_init(void);

/** Read single channel */
sentinel_err_t adc_read_channel(uint8_t channel, uint32_t *value);

/** Read all 4 channels at once */
sentinel_err_t adc_scan_all(uint32_t values[ADC_MAX_CHANNELS]);
```

#### IF-MCU-02: pwm_driver API
```c
/* src/mcu/hal/pwm_driver.h */

#define PWM_MAX_CHANNELS 2U
#define PWM_ARR_VALUE    999U  /* 0-999 for 0.0-100.0% (0.1% resolution) */

/** Initialize PWM peripheral */
sentinel_err_t pwm_init(void);

/** Set duty cycle (0.0-100.0%) */
sentinel_err_t pwm_set_duty(uint8_t channel, float percent);

/** Read back duty cycle for verification */
sentinel_err_t pwm_get_duty(uint8_t channel, float *percent);

/** Set all channels to 0% (fail-safe) */
sentinel_err_t pwm_set_all_zero(void);
```

#### IF-MCU-03: sensor_acquisition API
```c
/** Initialize sensor acquisition (timer + ADC) */
int32_t sensor_acq_init(const sensor_config_t *config);

/** Process completed ADC scan (call from main loop when ready) */
int32_t sensor_acq_process(void);

/** Get latest reading for a channel */
int32_t sensor_acq_get_reading(sensor_channel_t channel, sensor_reading_t *reading);

/** Update sample rate for a channel */
int32_t sensor_acq_set_rate(sensor_channel_t channel, uint32_t rate_hz);
```

#### IF-MCU-04: actuator_control API
```c
/** Initialize actuator control with safety limits */
int32_t actuator_ctrl_init(const actuator_limits_t *limits);

/** Process incoming actuator command (decode + validate + apply) */
int32_t actuator_ctrl_process_command(const uint8_t *data, size_t len, 
                                       uint8_t *response, size_t *resp_len);

/** Force all actuators to safe state (0%) */
void actuator_ctrl_enter_safe_state(failsafe_cause_t cause);

/** Check if fail-safe is active */
bool actuator_ctrl_is_failsafe(void);

/** Update safety limits (requires auth) */
int32_t actuator_ctrl_update_limits(const actuator_limits_t *limits, uint32_t auth_token);
```

#### IF-MCU-05: config_store API
```c
/** Load configuration from flash (or defaults if CRC invalid) */
int32_t config_store_load(system_config_t *config);

/** Save configuration to flash with CRC */
int32_t config_store_save(const system_config_t *config);

/** Validate configuration values */
int32_t config_store_validate(const system_config_t *config);

/** Get factory default configuration */
void config_store_get_defaults(system_config_t *config);
```

#### IF-MCU-06: watchdog_mgr API
```c
/** Initialize IWDG with 2-second timeout */
int32_t watchdog_mgr_init(void);

/** Feed the watchdog (call every ≤500ms) */
void watchdog_mgr_feed(void);

/** Get watchdog reset count (from backup register) */
uint32_t watchdog_mgr_get_reset_count(void);

/** Check if last reset was from watchdog */
bool watchdog_mgr_was_watchdog_reset(void);
```

#### IF-MCU-07: health_reporter API
```c
/** Initialize health reporter (1Hz timer) */
int32_t health_reporter_init(void);

/** Send heartbeat (call from timer or main loop) */
int32_t health_reporter_send_heartbeat(void);

/** Update MCU state */
void health_reporter_set_state(mcu_state_t state);

/** Notify of gateway activity (reset comm timeout) */
void health_reporter_gateway_activity(void);
```

### 4.2 Linux Internal Interfaces

#### IF-LINUX-01: tcp_transport API
```c
/** Initialize transport layer (create sockets, start listening) */
int32_t transport_init(transport_config_t *config);

/** Get epoll fd for integration with main event loop */
int transport_get_epoll_fd(void);

/** Process epoll events (call from main loop) */
int32_t transport_process_events(void);

/** Send a message to MCU command port */
int32_t transport_send_command(uint8_t msg_type, const uint8_t *data, size_t len);

/** Register callback for received message type */
typedef void (*msg_handler_t)(uint8_t msg_type, const uint8_t *data, size_t len, void *ctx);
int32_t transport_register_handler(uint8_t msg_type, msg_handler_t handler, void *ctx);
```

#### IF-LINUX-02: logger API
```c
/** Initialize logger (open files, start writer thread) */
int32_t logger_init(const char *log_dir, log_level_t min_level);

/** Log a system event */
void logger_event(log_level_t level, const char *module, const char *fmt, ...);

/** Log sensor data */
void logger_sensor_data(const sensor_data_t *data);

/** Get recent log entries (for diagnostic GET_LOG command) */
int32_t logger_get_recent(log_entry_t *entries, size_t max_entries, size_t *count);

/** Flush and shutdown logger */
void logger_shutdown(void);
```

#### IF-LINUX-03: diagnostics API
```c
/** Initialize diagnostic server on port 5002 */
int32_t diag_init(diag_context_t *ctx);

/** Get epoll fd for main event loop integration */
int diag_get_epoll_fd(void);

/** Process diagnostic client events */
int32_t diag_process_events(void);
```

---

## 5. Requirement-to-Component Allocation

| Component | SWE Requirements | Count |
|-----------|-----------------|-------|
| MCU/adc_driver | SWE-001-1, SWE-001-2, SWE-001-3, SWE-011-1 | 4 |
| MCU/sensor_acquisition | SWE-002-1, SWE-003-1, SWE-004-1, SWE-005-1, SWE-007-1, SWE-009-1, SWE-010-1 | 7 |
| MCU/protobuf_handler | SWE-007-2, SWE-018-1, SWE-036-1, SWE-037-1 | 4 |
| MCU/tcp_stack | SWE-008-1, SWE-030-1, SWE-031-1, SWE-032-1, SWE-033-1, SWE-035-1, SWE-035-2, SWE-043-2 | 8 |
| MCU/pwm_driver | SWE-016-1, SWE-016-2, SWE-017-1 | 3 |
| MCU/actuator_control | SWE-021-1, SWE-022-1, SWE-022-2, SWE-023-1, SWE-024-1, SWE-026-1, SWE-027-1, SWE-028-1, SWE-019-1 | 9 |
| MCU/config_store | SWE-058-1, SWE-059-1, SWE-060-1, SWE-062-1, SWE-063-1, SWE-064-1, SWE-065-1 | 7 |
| MCU/watchdog_mgr | SWE-069-1, SWE-070-1, SWE-071-1 | 3 |
| MCU/health_reporter | SWE-038-1, SWE-040-1, SWE-041-1, SWE-029-1 | 4 |
| MCU/main_loop | SWE-067-1, SWE-027-1, SWE-070-1 | 3 |
| LINUX/gateway_core | SWE-046-1, SWE-044-1, SWE-066-1, SWE-057-1 | 4 |
| LINUX/sensor_proxy | SWE-013-1, SWE-036-2 | 2 |
| LINUX/actuator_proxy | SWE-018-2, SWE-019-2 | 2 |
| LINUX/health_monitor | SWE-039-1, SWE-042-1, SWE-043-1, SWE-045-1 | 4 |
| LINUX/diagnostics | SWE-046-2, SWE-047-1, SWE-048-1, SWE-049-1, SWE-050-1, SWE-051-1, SWE-054-1 | 7 |
| LINUX/config_manager | SWE-058-2, SWE-061-1 | 2 |
| LINUX/tcp_transport | SWE-035-3, SWE-035-4, SWE-043-1 | 3 |
| LINUX/logger | SWE-013-2, SWE-015-1, SWE-052-1, SWE-053-1, SWE-055-1 | 5 |

---

## 6. Error Handling Strategy

### 6.1 MCU Error Handling
- **Principle**: Fail-safe on any unrecoverable error
- **Protobuf decode error**: Log error, send NACK, continue (unless actuator command → fail-safe)
- **TCP connection loss**: Set comm_disconnected flag, trigger timeout if persistent
- **ADC failure**: Skip channel, report in HealthStatus
- **Flash write failure**: Retry once, log error, continue with RAM config
- **Stack overflow**: Watchdog reset (detected by canary pattern)

### 6.2 Linux Error Handling
- **Principle**: Log, recover if possible, escalate if not
- **TCP connection loss**: Reconnect with exponential backoff
- **MCU communication loss**: USB power cycle recovery
- **Protobuf decode error**: Log warning, skip message
- **Disk full**: Stop logging, alert via diagnostic interface
- **Fatal error**: Graceful shutdown, log reason

### 6.3 Error Code Convention
```c
/* src/common/error_codes.h */
typedef enum {
    SENTINEL_OK              =   0,
    SENTINEL_ERR_INVALID_ARG =  -1,
    SENTINEL_ERR_OUT_OF_RANGE = -2,
    SENTINEL_ERR_TIMEOUT     =  -3,
    SENTINEL_ERR_COMM        =  -4,
    SENTINEL_ERR_DECODE      =  -5,
    SENTINEL_ERR_ENCODE      =  -6,
    SENTINEL_ERR_FULL        =  -7,  /* Rate limited */
    SENTINEL_ERR_NOT_READY   =  -8,
    SENTINEL_ERR_AUTH        =  -9,
    SENTINEL_ERR_INTERNAL    = -10
} sentinel_err_t;
```

---

## 7. Memory Architecture

### 7.1 MCU Memory Map

![MCU Memory Map](../diagrams/mcu_memory_map.drawio.svg)

---

## 8. Traceability

This document traces forward to detailed design (`docs/design/detailed/`) and backward to SWRS-001 (Software Requirements Specification) via the allocation table in Section 5.
