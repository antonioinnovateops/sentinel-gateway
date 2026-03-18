---
title: "System Requirements Specification"
document_id: "SRS-001"
project: "Sentinel Gateway"
version: "2.0.0"
date: 2026-03-18
status: "Approved"
asil_level: "ASIL-B (process rigor)"
authors: ["Antonio Stepien"]
reviewers: ["AI Requirements Agent"]
approvers: ["Antonio Stepien"]
aspice_process: "SYS.2 System Requirements Analysis"
---

# System Requirements Specification — Sentinel Gateway

## 1. Introduction

### 1.1 Purpose

This document specifies the system requirements for the Sentinel Gateway. It serves as the basis for:
- System architecture design (SYS.3)
- Software requirements derivation (SWE.1)
- System verification and validation (SYS.5)

**Version 2.0 Note**: This revision incorporates lessons learned from implementation, adding precise values, exact byte layouts, and verified implementation references.

### 1.2 Scope

**In Scope**: Dual-processor embedded gateway (Linux + MCU) with sensor acquisition, actuator control, inter-processor communication, health monitoring, diagnostics, and configuration management — all operating within a QEMU SIL environment.

**Out of Scope**: Cloud connectivity, OTA updates, GUI/HMI, real hardware deployment.

### 1.3 Definitions and Acronyms

| Term | Definition |
|------|------------|
| MCU | Microcontroller Unit (STM32U575, Cortex-M33 — emulated as MPS2-AN505 in QEMU) |
| SoC | System on Chip (emulated ARM Cortex-A53 in QEMU virt machine) |
| SIL | Software-in-the-Loop (QEMU user-mode emulation for SIL testing) |
| QEMU | Quick EMUlator — running in **user-mode** for MCU (not full system emulation) |
| ADC | Analog-to-Digital Converter (12-bit, 4 channels) |
| PWM | Pulse Width Modulation (25 kHz, 10-bit resolution) |
| Wire Frame | Message envelope: 4-byte LE length + 1-byte type + payload |

### 1.4 Reference Documents

| Document ID | Title | Version |
|-------------|-------|---------|
| PROD-SPEC-001 | Product Specification | v1.0.0 |
| STKH-REQ-001 | Stakeholder Requirements | v2.0.0 |

### 1.5 Implementation File Reference

Throughout this document, requirements reference implementation files using this pattern:
- `src/mcu/...` — MCU firmware (bare-metal or QEMU user-mode)
- `src/linux/...` — Linux gateway application
- `src/common/...` — Shared code (wire frame, types)

---

## 2. Stakeholder Requirements Traceability

| System Req ID | Stakeholder Requirement | Rationale |
|---------------|-------------------------|-----------|
| SYS-001 to SYS-008 | [STKH-001] Multi-Sensor Data Collection | Decompose into sensor-specific requirements |
| SYS-009 to SYS-012 | [STKH-002] Real-Time Sensor Sampling | Timing and jitter specifications |
| SYS-013 to SYS-015 | [STKH-003] Sensor Data Logging | Logging, timestamps, persistence |
| SYS-016 to SYS-021 | [STKH-004] Remote Actuator Command | Command flow and timing |
| SYS-022 to SYS-025 | [STKH-005] Actuator Safety Limits | Range validation and rejection |
| SYS-026 to SYS-029 | [STKH-006] Actuator Fail-Safe | Safe state transitions |
| SYS-030 to SYS-037 | [STKH-007] Structured Communication | Wire frame protocol definition |
| SYS-038 to SYS-041 | [STKH-008] Communication Loss Detection | Heartbeat and timeout |
| SYS-042 to SYS-045 | [STKH-009] Automatic Recovery | Recovery sequences |
| SYS-046 to SYS-051 | [STKH-010] Remote Diagnostics | Diagnostic interface |
| SYS-052 to SYS-055 | [STKH-011] Comprehensive Logging | Event logging |
| SYS-056 to SYS-057 | [STKH-012] Firmware Version Reporting | Version query |
| SYS-058 to SYS-062 | [STKH-013] Runtime Configuration | Config management |
| SYS-063 to SYS-065 | [STKH-014] Configuration Validation | Input validation |
| SYS-066 to SYS-068 | [STKH-015] Continuous Operation | Reliability |
| SYS-069 to SYS-071 | [STKH-016] Watchdog Protection | Watchdog timer |
| SYS-072 to SYS-073 | [STKH-017] No Dynamic Memory on MCU | Memory constraints |
| SYS-074 to SYS-076 | [STKH-018] ASPICE CL2 Compliance | Process requirements |
| SYS-077 to SYS-078 | [STKH-019] Full Traceability | Traceability |
| SYS-079 to SYS-080 | [STKH-020] MISRA C Compliance | Coding standards |

---

## 3. Functional Requirements

### 3.1 Sensor Data Acquisition

**[SYS-001] ADC Channel Count**
- **Description**: The MCU shall support a minimum of 4 simultaneous ADC input channels
- **Type**: Functional
- **Safety**: QM
- **Source**: [STKH-001]
- **Acceptance Criteria**:
  - ADC peripheral configured for 4 channels (CH0-CH3)
  - All 4 channels readable in a single scan via `adc_scan_all()`
  - Raw values verified in range 0-4095
- **Implementation**: `src/mcu/hal/adc_driver.c`, `ADC_MAX_CHANNELS = 4`
- **Verified By**: TC-SYS-001-1

**[SYS-002] Temperature Sensor Input**
- **Description**: ADC Channel 0 shall measure temperature sensor voltage in range 0.0V to 3.3V, corresponding to -40°C to +125°C
- **Type**: Functional
- **Safety**: QM
- **Source**: [STKH-001]
- **Acceptance Criteria**:
  - Calibration formula: `T = (raw / 4095.0) * 165.0 - 40.0`
  - ADC raw 0 → -40.0°C
  - ADC raw 2048 → 42.5°C ±0.5°C
  - ADC raw 4095 → 125.0°C
- **Implementation**: `src/mcu/sensor_acquisition.c:calibrate_temperature()`
- **Verified By**: TC-SYS-002-1

**[SYS-003] Humidity Sensor Input**
- **Description**: ADC Channel 1 shall measure humidity sensor voltage in range 0.0V to 3.3V, corresponding to 0% to 100% RH
- **Type**: Functional
- **Safety**: QM
- **Source**: [STKH-001]
- **Acceptance Criteria**:
  - Calibration formula: `RH = (raw / 4095.0) * 100.0`
  - ADC raw 2048 → 50.0% RH ±1.0%
- **Implementation**: `src/mcu/sensor_acquisition.c:calibrate_humidity()`
- **Verified By**: TC-SYS-003-1

**[SYS-004] Pressure Sensor Input**
- **Description**: ADC Channel 2 shall measure pressure sensor voltage corresponding to 300 hPa to 1100 hPa
- **Type**: Functional
- **Safety**: QM
- **Source**: [STKH-001]
- **Acceptance Criteria**:
  - Calibration formula: `P = 300.0 + (raw / 4095.0) * 800.0`
  - ADC raw 2048 → 700 hPa ±5 hPa
- **Implementation**: `src/mcu/sensor_acquisition.c:calibrate_pressure()`
- **Verified By**: TC-SYS-004-1

**[SYS-005] Light Sensor Input**
- **Description**: ADC Channel 3 shall measure ambient light sensor voltage corresponding to 0 to 100,000 lux
- **Type**: Functional
- **Safety**: QM
- **Source**: [STKH-001]
- **Acceptance Criteria**:
  - Calibration formula: `Lux = (raw / 4095.0) * 100000.0`
  - ADC raw 2048 → 50,000 lux ±1,000 lux
- **Implementation**: `src/mcu/sensor_acquisition.c:calibrate_light()`
- **Verified By**: TC-SYS-005-1

**[SYS-006] ADC Resolution**
- **Description**: The MCU ADC shall operate with 12-bit resolution (4096 discrete levels)
- **Type**: Functional
- **Safety**: QM
- **Source**: [STKH-001]
- **Acceptance Criteria**:
  - Raw ADC values range from 0 to 4095 (`ADC_RESOLUTION = 4095`)
  - No truncation or rounding in raw value reporting
- **Implementation**: `src/mcu/hal/adc_driver.h:ADC_RESOLUTION`
- **Verified By**: TC-SYS-006-1

**[SYS-007] Sensor Data Packaging**
- **Description**: The MCU shall encode sensor readings into `sensor_reading_t` structures containing: channel ID (uint8_t), raw ADC value (uint32_t), calibrated value (float), timestamp (uint64_t ms since boot)
- **Type**: Functional
- **Safety**: QM
- **Source**: [STKH-001]
- **Acceptance Criteria**:
  - Structure defined in `src/common/sentinel_types.h`
  - All fields populated on each sample
  - Timestamp increments monotonically
- **Implementation**: `src/common/sentinel_types.h:sensor_reading_t`
- **Verified By**: TC-SYS-007-1

**[SYS-008] Sensor Data Transmission**
- **Description**: The MCU shall transmit sensor data to the Linux gateway via TCP port 5001 at the configured sample rate
- **Type**: Functional
- **Safety**: QM
- **Source**: [STKH-001]
- **Acceptance Criteria**:
  - Wire frame format: `[4B LE length][1B type=0x01][payload]`
  - Message type: `MSG_TYPE_SENSOR_DATA = 0x01`
  - Linux receives messages at configured rate ±5%
- **Implementation**: `src/mcu/tcp_stack.c` (bare-metal) or `src/mcu/tcp_stack_qemu.c` (QEMU)
- **Verified By**: TC-SYS-008-1

### 3.2 Sensor Timing

**[SYS-009] Configurable Sample Rate**
- **Description**: The sensor sample rate shall be configurable from 1 Hz to 100 Hz in 1 Hz increments
- **Type**: Functional
- **Safety**: QM
- **Source**: [STKH-002]
- **Acceptance Criteria**:
  - Sample rate stored in `system_config_t.sensor_rates_hz[]` (per-channel)
  - Valid range: 1-100 Hz (reject values outside)
  - Default: 10 Hz per channel
- **Implementation**: `src/mcu/config_store.c`
- **Verified By**: TC-SYS-009-1

**[SYS-010] Sample Timing Jitter**
- **Description**: The sensor sample timing jitter shall not exceed ±500 µs at any configured sample rate
- **Type**: Performance
- **Safety**: QM
- **Source**: [STKH-002]
- **Acceptance Criteria**:
  - Jitter measured via consecutive timestamp differences
  - Standard deviation of sample intervals ≤ 500 µs
  - **Known Limitation**: QEMU user-mode timing is approximate; use real hardware for precise jitter measurement
- **Implementation**: `src/mcu/hal/systick.c`
- **Verified By**: TC-SYS-010-1

**[SYS-011] ADC Conversion Time**
- **Description**: A single ADC conversion for all 4 channels shall complete within 100 µs
- **Type**: Performance
- **Safety**: QM
- **Source**: [STKH-002]
- **Acceptance Criteria**:
  - `adc_scan_all()` returns within 100 µs
  - **Note**: QEMU simulates instant ADC reads
- **Implementation**: `src/mcu/hal/adc_driver.c:adc_scan_all()`
- **Verified By**: TC-SYS-011-1

**[SYS-012] Per-Channel Sample Rate**
- **Description**: Each ADC channel shall be independently configurable for sample rate
- **Type**: Functional
- **Safety**: QM
- **Source**: [STKH-002]
- **Acceptance Criteria**:
  - `sensor_rates_hz[4]` array in config (one rate per channel)
  - Channel 0 at 100 Hz while Channel 3 at 1 Hz simultaneously
- **Implementation**: `src/common/sentinel_types.h:system_config_t`
- **Verified By**: TC-SYS-012-1

### 3.3 Data Logging

**[SYS-013] Linux-Side Data Logging**
- **Description**: The Linux gateway shall log all received sensor data to a structured log file (JSON Lines format)
- **Type**: Functional
- **Safety**: QM
- **Source**: [STKH-003]
- **Acceptance Criteria**:
  - Log path: `/var/log/sentinel/sensor_data.jsonl`
  - Each line: `{"ts":"...", "ch":0, "raw":2048, "cal":42.5, "unit":"°C"}`
  - No gaps in sequence numbers (logged as warnings if gaps detected)
- **Implementation**: `src/linux/logger.c:logger_log_sensor()`
- **Verified By**: TC-SYS-013-1

**[SYS-014] Log Timestamp Resolution**
- **Description**: Log entries shall include timestamps with ≤ 1 ms resolution
- **Type**: Performance
- **Safety**: QM
- **Source**: [STKH-003]
- **Acceptance Criteria**:
  - Use `clock_gettime(CLOCK_REALTIME)` for timestamps
  - Format: ISO-8601 with milliseconds: `2026-03-17T10:00:00.123Z`
- **Implementation**: `src/linux/logger.c`
- **Verified By**: TC-SYS-014-1

**[SYS-015] Log Rotation**
- **Description**: The Linux gateway shall implement log rotation (max 100 MB per file, max 10 files)
- **Type**: Functional
- **Safety**: QM
- **Source**: [STKH-003]
- **Acceptance Criteria**:
  - Rotate when file exceeds 100 MB
  - Filename pattern: `sensor_data.jsonl`, `sensor_data.1.jsonl`, ..., `sensor_data.9.jsonl`
  - Delete oldest when 10 files exist
- **Implementation**: `src/linux/logger.c:logger_rotate_if_needed()`
- **Verified By**: TC-SYS-015-1

### 3.4 Actuator Control

**[SYS-016] Fan Speed Control**
- **Description**: The system shall control a fan via PWM output (actuator ID 0), duty cycle 0-100%
- **Type**: Functional
- **Safety**: ASIL-B
- **Source**: [STKH-004]
- **Acceptance Criteria**:
  - PWM channel 0 maps to fan
  - Duty cycle accuracy ±1% (0.1% resolution via `PWM_ARR_VALUE = 999`)
  - PWM frequency: 25 kHz (not adjustable)
- **Implementation**: `src/mcu/hal/pwm_driver.c:pwm_set_duty(0, percent)`
- **Verified By**: TC-SYS-016-1

**[SYS-017] Valve Position Control**
- **Description**: The system shall control a valve via PWM output (actuator ID 1), duty cycle 0-100%
- **Type**: Functional
- **Safety**: ASIL-B
- **Source**: [STKH-004]
- **Acceptance Criteria**:
  - PWM channel 1 maps to valve
  - Duty cycle accuracy ±1%
- **Implementation**: `src/mcu/hal/pwm_driver.c:pwm_set_duty(1, percent)`
- **Verified By**: TC-SYS-017-1

**[SYS-018] Actuator Command Message**
- **Description**: The Linux gateway shall send actuator commands to MCU via TCP port 5000
- **Type**: Functional
- **Safety**: ASIL-B
- **Source**: [STKH-004]
- **Acceptance Criteria**:
  - Wire frame: `[4B LE length][1B type=0x10][payload]`
  - Message type: `MSG_TYPE_ACTUATOR_CMD = 0x10`
  - Payload contains: actuator_id (uint8_t), commanded_percent (float)
- **Implementation**: `src/linux/actuator_proxy.c:actuator_proxy_send_command()`
- **Verified By**: TC-SYS-018-1

**[SYS-019] Actuator Command Acknowledgment**
- **Description**: The MCU shall respond to each actuator command with an acknowledgment
- **Type**: Functional
- **Safety**: ASIL-B
- **Source**: [STKH-004]
- **Acceptance Criteria**:
  - Message type: `MSG_TYPE_ACTUATOR_RSP = 0x11`
  - Response contains: actuator_id, applied_percent (after clamping), status code
  - Status codes: `SENTINEL_OK`, `SENTINEL_ERR_OUT_OF_RANGE`, `SENTINEL_ERR_FULL` (rate limited)
- **Implementation**: `src/mcu/actuator_control.c:actuator_send_response()`
- **Verified By**: TC-SYS-019-1

**[SYS-020] Actuator Command Latency**
- **Description**: Time from Linux sending command to PWM output change shall not exceed 20 ms
- **Type**: Performance
- **Safety**: ASIL-B
- **Source**: [STKH-004]
- **Acceptance Criteria**:
  - Measured: `response_timestamp_ms - command_timestamp_ms ≤ 20`
  - 99th percentile over 1000 commands ≤ 20 ms
- **Implementation**: End-to-end TCP + processing
- **Verified By**: TC-SYS-020-1

**[SYS-021] Actuator Command Rate Limiting**
- **Description**: MCU shall accept max 50 commands/second per actuator; excess rejected
- **Type**: Functional
- **Safety**: ASIL-B
- **Source**: [STKH-004]
- **Acceptance Criteria**:
  - 50 commands in 1 second: all accepted
  - 51st command: rejected with `SENTINEL_ERR_FULL`
  - Counter resets after 1 second
- **Implementation**: `src/mcu/actuator_control.c` (sliding window counter)
- **Verified By**: TC-SYS-021-1

### 3.5 Actuator Safety

**[SYS-022] Actuator Range Validation**
- **Description**: MCU shall validate command values are within configured min/max range
- **Type**: Safety
- **Safety**: ASIL-B
- **Source**: [STKH-005]
- **Acceptance Criteria**:
  - Command value < min: rejected with `SENTINEL_ERR_OUT_OF_RANGE`
  - Command value > max: rejected with `SENTINEL_ERR_OUT_OF_RANGE`
  - Values at boundaries: accepted
- **Implementation**: `src/mcu/actuator_control.c:actuator_validate_command()`
- **Verified By**: TC-SYS-022-1

**[SYS-023] Fan Safety Limits**
- **Description**: Fan duty cycle limited to configurable max (default 95%) and min (default 0%)
- **Type**: Safety
- **Safety**: ASIL-B
- **Source**: [STKH-005]
- **Acceptance Criteria**:
  - Default config: `actuator_min_percent[0] = 0.0`, `actuator_max_percent[0] = 95.0`
  - Command 100% → applied as 95% (clamped, not rejected)
  - Applied value reported in response
- **Implementation**: `src/mcu/actuator_control.c:clamp_to_limits()`
- **Verified By**: TC-SYS-023-1

**[SYS-024] Valve Safety Limits**
- **Description**: Valve duty cycle limited to configurable max (default 100%) and min (default 0%)
- **Type**: Safety
- **Safety**: ASIL-B
- **Source**: [STKH-005]
- **Acceptance Criteria**:
  - Default config: `actuator_min_percent[1] = 0.0`, `actuator_max_percent[1] = 100.0`
- **Implementation**: `src/mcu/config_store.c:config_load_defaults()`
- **Verified By**: TC-SYS-024-1

**[SYS-025] Safety Limit Configuration Protection**
- **Description**: Safety limit changes require authentication (future: token validation)
- **Type**: Safety
- **Safety**: ASIL-B
- **Source**: [STKH-005]
- **Acceptance Criteria**:
  - ConfigUpdate with safety limit changes: logged with WARNING
  - **Note**: Token validation not implemented in v1.0; limits can be changed
- **Implementation**: `src/mcu/config_store.c`
- **Verified By**: TC-SYS-025-1

### 3.6 Actuator Fail-Safe

**[SYS-026] Communication Loss Fail-Safe**
- **Description**: If MCU detects no messages from Linux for 3 seconds, set all actuators to 0%
- **Type**: Safety
- **Safety**: ASIL-B
- **Source**: [STKH-006]
- **Acceptance Criteria**:
  - Timeout: `SENTINEL_COMM_TIMEOUT_MS = 3000`
  - All PWM outputs → 0% via `pwm_set_all_zero()`
  - State changes to `STATE_FAILSAFE`
- **Implementation**: `src/mcu/health_reporter.c:check_comm_timeout()`
- **Verified By**: TC-SYS-026-1

**[SYS-027] Watchdog Fail-Safe**
- **Description**: On watchdog reset, MCU initializes all PWM to 0% before any communication
- **Type**: Safety
- **Safety**: ASIL-B
- **Source**: [STKH-006]
- **Acceptance Criteria**:
  - `pwm_init()` sets all channels to 0% before `tcp_stack_init()`
  - Boot sequence in `main.c` enforces order
- **Implementation**: `src/mcu/main.c:main()`, `src/mcu/hal/pwm_driver.c:pwm_init()`
- **Verified By**: TC-SYS-027-1

**[SYS-028] Error State Fail-Safe**
- **Description**: On protobuf decode failure or internal error, actuators enter safe state
- **Type**: Safety
- **Safety**: ASIL-B
- **Source**: [STKH-006]
- **Acceptance Criteria**:
  - `wire_frame_decode_header()` returns error → fail-safe within 100 ms
  - `actuator_enter_failsafe()` called
- **Implementation**: `src/mcu/protobuf_handler.c`
- **Verified By**: TC-SYS-028-1

**[SYS-029] Fail-Safe State Reporting**
- **Description**: When entering fail-safe, MCU reports event via HealthStatus message
- **Type**: Functional
- **Safety**: ASIL-B
- **Source**: [STKH-006]
- **Acceptance Criteria**:
  - Message type: `MSG_TYPE_HEALTH_STATUS = 0x02`
  - State field: `STATE_FAILSAFE = 3`
  - Fault ID indicates cause (e.g., `FAULT_COMM_LOSS = 1`)
- **Implementation**: `src/mcu/health_reporter.c:send_health_status()`
- **Verified By**: TC-SYS-029-1

### 3.7 Inter-Processor Communication

**[SYS-030] USB CDC-ECM Link (Real Hardware)**
- **Description**: MCU provides Ethernet-over-USB to Linux host
- **Type**: Functional
- **Safety**: QM
- **Source**: [STKH-007]
- **Acceptance Criteria**:
  - Linux enumerates `usb0` network interface
  - **QEMU Note**: In SIL, TCP runs over host loopback (no USB emulation)
- **Implementation**: N/A for QEMU SIL (stubbed)
- **Verified By**: TC-SYS-030-1 (hardware only)

**[SYS-031] Static IP Configuration**
- **Description**: IP addresses statically assigned: Linux 192.168.7.1, MCU 192.168.7.2
- **Type**: Functional
- **Safety**: QM
- **Source**: [STKH-007]
- **Acceptance Criteria**:
  - Defined in `src/common/sentinel_types.h`:
    - `SENTINEL_LINUX_IP = "192.168.7.1"`
    - `SENTINEL_MCU_IP = "192.168.7.2"`
  - For QEMU SIL: use `127.0.0.1` (both processes on same host)
  - Environment override: `SENTINEL_MCU_HOST` environment variable
- **Implementation**: `src/linux/main.c` reads `SENTINEL_MCU_HOST` or defaults
- **Verified By**: TC-SYS-031-1

**[SYS-032] TCP Command Port**
- **Description**: MCU listens on TCP port 5000 for commands
- **Type**: Functional
- **Safety**: QM
- **Source**: [STKH-007]
- **Acceptance Criteria**:
  - Port: `SENTINEL_PORT_COMMAND = 5000`
  - Single connection accepted (subsequent connections replace previous)
  - Messages: ActuatorCommand (0x10), ConfigUpdate (0x20), DiagnosticRequest (0x30)
- **Implementation**: `src/mcu/tcp_stack_qemu.c:setup_command_listener()`
- **Verified By**: TC-SYS-032-1

**[SYS-033] TCP Telemetry Port**
- **Description**: Linux listens on TCP port 5001; MCU connects to stream telemetry
- **Type**: Functional
- **Safety**: QM
- **Source**: [STKH-007]
- **Acceptance Criteria**:
  - Port: `SENTINEL_PORT_TELEMETRY = 5001`
  - Messages: SensorData (0x01), HealthStatus (0x02)
- **Implementation**: `src/linux/tcp_transport.c:transport_listen_telemetry()`
- **Verified By**: TC-SYS-033-1

**[SYS-034] Wire Frame Message Format**
- **Description**: All TCP messages use wire frame format
- **Type**: Functional
- **Safety**: QM
- **Source**: [STKH-007]
- **Acceptance Criteria**:
  - Format: See exact byte layout below
  - No CRC in wire frame (TCP provides integrity)
- **Implementation**: `src/common/wire_frame.c`
- **Verified By**: TC-SYS-034-1

**Wire Frame Byte Layout**:
```
Offset  Size  Field           Description
------  ----  -------------   -----------------------------------------
0       4     payload_length  Little-endian uint32, bytes of payload only
4       1     msg_type        Message type ID (0x01-0x41)
5       N     payload         Application data (max 507 bytes)
------  ----  -------------   -----------------------------------------
Total: 5 + N bytes (WIRE_FRAME_HEADER_SIZE = 5, WIRE_FRAME_MAX_PAYLOAD = 507)
```

**Message Type IDs**:
| ID | Name | Direction | Description |
|----|------|-----------|-------------|
| 0x01 | MSG_TYPE_SENSOR_DATA | MCU → Linux | Sensor readings |
| 0x02 | MSG_TYPE_HEALTH_STATUS | MCU → Linux | Health/heartbeat |
| 0x10 | MSG_TYPE_ACTUATOR_CMD | Linux → MCU | Actuator command |
| 0x11 | MSG_TYPE_ACTUATOR_RSP | MCU → Linux | Actuator response |
| 0x20 | MSG_TYPE_CONFIG_UPDATE | Linux → MCU | Config change |
| 0x21 | MSG_TYPE_CONFIG_RSP | MCU → Linux | Config response |
| 0x30 | MSG_TYPE_DIAG_REQ | Linux → MCU | Diagnostic request |
| 0x31 | MSG_TYPE_DIAG_RSP | MCU → Linux | Diagnostic response |
| 0x40 | MSG_TYPE_STATE_SYNC_REQ | Linux → MCU | State sync request |
| 0x41 | MSG_TYPE_STATE_SYNC_RSP | MCU → Linux | State sync response |

**[SYS-035] Message Framing Encoding/Decoding**
- **Description**: Each message prefixed with 4-byte LE length + 1-byte type
- **Type**: Functional
- **Safety**: QM
- **Source**: [STKH-007]
- **Acceptance Criteria**:
  - `wire_frame_encode()`: Creates header + payload
  - `wire_frame_decode_header()`: Extracts type and payload length
  - Partial frames buffered until complete
- **Implementation**: `src/common/wire_frame.c`
- **Verified By**: TC-SYS-035-1, `tests/unit/test_wire_frame.c`

**[SYS-036] Message Sequence Numbers**
- **Description**: All messages include 32-bit sequence number (in payload, not header)
- **Type**: Functional
- **Safety**: QM
- **Source**: [STKH-007]
- **Acceptance Criteria**:
  - Monotonically increasing per sender
  - Wraps from UINT32_MAX to 0
  - Gaps logged as warnings by receiver
- **Implementation**: Application-level in protobuf payload
- **Verified By**: TC-SYS-036-1

**[SYS-037] Protocol Version Field**
- **Description**: Messages include protocol version (major.minor) in payload
- **Type**: Functional
- **Safety**: QM
- **Source**: [STKH-007]
- **Acceptance Criteria**:
  - Current version: major=1, minor=0
  - Defined: `SENTINEL_PROTO_VERSION_MAJOR = 1`, `SENTINEL_PROTO_VERSION_MINOR = 0`
- **Implementation**: `src/common/sentinel_types.h`
- **Verified By**: TC-SYS-037-1

### 3.8 Communication Health Monitoring

**[SYS-038] MCU Heartbeat**
- **Description**: MCU sends HealthStatus every 1000 ms (±50 ms)
- **Type**: Functional
- **Safety**: ASIL-B
- **Source**: [STKH-008]
- **Acceptance Criteria**:
  - Interval: `SENTINEL_HEALTH_INTERVAL_MS = 1000`
  - Jitter: ±50 ms (±5%)
  - Contains: state, uptime_s, watchdog_resets, comm status
- **Implementation**: `src/mcu/health_reporter.c:health_reporter_tick()`
- **Verified By**: TC-SYS-038-1

**[SYS-039] Linux Heartbeat Monitoring**
- **Description**: Linux declares comm loss if 3 consecutive heartbeats missed (3 seconds)
- **Type**: Functional
- **Safety**: ASIL-B
- **Source**: [STKH-008]
- **Acceptance Criteria**:
  - Miss 3 heartbeats → `health.comm_ok = false`
  - Event logged with WARNING level
- **Implementation**: `src/linux/health_monitor.c:health_monitor_check_heartbeat()`
- **Verified By**: TC-SYS-039-1

**[SYS-040] MCU Gateway Monitoring**
- **Description**: MCU declares comm loss if no Linux message for 5 seconds
- **Type**: Functional
- **Safety**: ASIL-B
- **Source**: [STKH-008]
- **Acceptance Criteria**:
  - Timeout: 5 seconds (separate from 3s actuator fail-safe)
  - Triggers fail-safe state
- **Implementation**: `src/mcu/health_reporter.c`
- **Verified By**: TC-SYS-040-1

**[SYS-041] Communication Status Reporting**
- **Description**: Both sides report comm status: CONNECTED, DEGRADED, DISCONNECTED
- **Type**: Functional
- **Safety**: QM
- **Source**: [STKH-008]
- **Acceptance Criteria**:
  - DEGRADED: > 5% packet loss detected via sequence gaps
  - Transitions logged with timestamps
- **Implementation**: `src/common/sentinel_types.h:system_state_t`
- **Verified By**: TC-SYS-041-1

### 3.9 Automatic Recovery

**[SYS-042] USB Power Cycle Recovery**
- **Description**: On MCU comm loss, Linux toggles USB power after 5 seconds
- **Type**: Functional
- **Safety**: QM
- **Source**: [STKH-009]
- **Acceptance Criteria**:
  - **Real hardware**: USB power toggle via GPIO
  - **QEMU**: Not applicable (user-mode has no USB)
  - Recovery logged
- **Implementation**: `src/linux/health_monitor.c:trigger_recovery()`
- **Verified By**: TC-SYS-042-1 (hardware only)

**[SYS-043] TCP Reconnection**
- **Description**: On TCP drop, both sides attempt reconnection with exponential backoff
- **Type**: Functional
- **Safety**: QM
- **Source**: [STKH-009]
- **Acceptance Criteria**:
  - Initial delay: 100 ms
  - Max delay: 5 seconds
  - Doubling each attempt: 100, 200, 400, 800, 1600, 3200, 5000, 5000...
- **Implementation**: `src/linux/tcp_transport.c`, `src/mcu/tcp_stack_qemu.c`
- **Verified By**: TC-SYS-043-1

**[SYS-044] State Resynchronization**
- **Description**: After reconnection, Linux requests full state from MCU
- **Type**: Functional
- **Safety**: QM
- **Source**: [STKH-009]
- **Acceptance Criteria**:
  - Send `MSG_TYPE_STATE_SYNC_REQ` (0x40)
  - Receive `MSG_TYPE_STATE_SYNC_RSP` (0x41) within 2 seconds
  - Update local sensor/actuator caches
- **Implementation**: `src/linux/gateway_core.c:request_state_sync()`
- **Verified By**: TC-SYS-044-1

**[SYS-045] Recovery Attempt Limit**
- **Description**: Max 3 recovery cycles before permanent FAULT state
- **Type**: Functional
- **Safety**: QM
- **Source**: [STKH-009]
- **Acceptance Criteria**:
  - Counter increments on each failed recovery
  - After 3 failures: `state = STATE_ERROR`, no more attempts
  - Requires human intervention (restart gateway)
- **Implementation**: `src/linux/health_monitor.c:recovery_attempt_count`
- **Verified By**: TC-SYS-045-1

### 3.10 Diagnostics

**[SYS-046] Diagnostic TCP Interface**
- **Description**: Linux provides diagnostic interface on TCP port 5002
- **Type**: Functional
- **Safety**: QM
- **Source**: [STKH-010]
- **Acceptance Criteria**:
  - Port: `SENTINEL_PORT_DIAG = 5002`
  - Plain-text protocol, newline-terminated commands
  - Single client at a time (new connection replaces old)
- **Implementation**: `src/linux/tcp_transport.c:transport_listen_diagnostics()`
- **Verified By**: TC-SYS-046-1

**[SYS-047] Read Sensor Command**
- **Description**: `sensor read <ch>` returns latest calibrated value
- **Type**: Functional
- **Safety**: QM
- **Source**: [STKH-010]
- **Acceptance Criteria**:
  - **Command**: `sensor read 0` (lowercase, space-separated)
  - **Response**: `ch=0 raw=2048 cal=42.500\n`
  - Invalid channel: `ERROR: invalid channel\n`
- **Implementation**: `src/linux/diagnostics.c:diagnostics_process_command()`
- **Verified By**: TC-SYS-047-1

**[SYS-048] Set Actuator Command**
- **Description**: `actuator set <id> <pct>` applies value and returns status
- **Type**: Functional
- **Safety**: QM
- **Source**: [STKH-010]
- **Acceptance Criteria**:
  - **Command**: `actuator set 0 50.0`
  - **Response**: `OK\n` or `ERROR\n`
  - Invalid syntax: `ERROR: usage: actuator set <id> <pct>\n`
- **Implementation**: `src/linux/diagnostics.c`
- **Verified By**: TC-SYS-048-1

**[SYS-049] Get Status Command**
- **Description**: `status` returns system overview
- **Type**: Functional
- **Safety**: QM
- **Source**: [STKH-010]
- **Acceptance Criteria**:
  - **Command**: `status`
  - **Response**: `state=NORMAL uptime=123s wd_resets=0 comm=OK\n`
- **Implementation**: `src/linux/diagnostics.c`
- **Verified By**: TC-SYS-049-1

**[SYS-050] Get Version Command**
- **Description**: `version` returns firmware versions
- **Type**: Functional
- **Safety**: QM
- **Source**: [STKH-010], [STKH-012]
- **Acceptance Criteria**:
  - **Command**: `version`
  - **Response**: `linux=1.0.0 mcu=pending\n`
  - MCU version populated after first health message
- **Implementation**: `src/linux/diagnostics.c`
- **Verified By**: TC-SYS-050-1

**[SYS-051] Help Command**
- **Description**: `help` lists available commands
- **Type**: Functional
- **Safety**: QM
- **Source**: [STKH-010]
- **Acceptance Criteria**:
  - **Command**: `help`
  - **Response**: `Commands: status, sensor read <ch>, actuator set <id> <pct>, version, help\n`
- **Implementation**: `src/linux/diagnostics.c`
- **Verified By**: TC-SYS-051-1

### 3.11 Event Logging

**[SYS-052] System Event Log**
- **Description**: Linux maintains event log with all event types
- **Type**: Functional
- **Safety**: QM
- **Source**: [STKH-011]
- **Acceptance Criteria**:
  - Log path: `/var/log/sentinel/events.jsonl`
  - Event types: sensor, actuator, comm, error, config, diag
  - JSON Lines format
- **Implementation**: `src/linux/logger.c`
- **Verified By**: TC-SYS-052-1

**[SYS-053] Log Persistence**
- **Description**: Logs persist across gateway reboots
- **Type**: Functional
- **Safety**: QM
- **Source**: [STKH-011]
- **Acceptance Criteria**:
  - `fsync()` called every 5 seconds
  - `fsync()` called on SIGTERM
  - Logs readable after gateway restart
- **Implementation**: `src/linux/logger.c:logger_flush()`
- **Verified By**: TC-SYS-053-1

**[SYS-054] Log Retrieval**
- **Description**: Diagnostic command to retrieve recent logs (not implemented in v1.0)
- **Type**: Functional
- **Safety**: QM
- **Source**: [STKH-011]
- **Acceptance Criteria**:
  - **Note**: Deferred to v1.1; logs readable via file access
- **Implementation**: N/A
- **Verified By**: N/A

**[SYS-055] Error Event Priority**
- **Description**: Severity levels: DEBUG, INFO, WARNING, ERROR, CRITICAL
- **Type**: Functional
- **Safety**: QM
- **Source**: [STKH-011]
- **Acceptance Criteria**:
  - Enum: `LOG_DEBUG=0, LOG_INFO=1, LOG_WARNING=2, LOG_ERROR=3, LOG_CRITICAL=4`
  - Runtime filter via `LOG_LEVEL` environment variable
- **Implementation**: `src/linux/logger.h`
- **Verified By**: TC-SYS-055-1

### 3.12 Firmware Version

**[SYS-056] Linux Firmware Version**
- **Description**: Linux reports version `<major>.<minor>.<patch>`
- **Type**: Functional
- **Safety**: QM
- **Source**: [STKH-012]
- **Acceptance Criteria**:
  - Format: "1.0.0" (no git hash in v1.0)
  - Defined: `SENTINEL_VERSION` macro from CMake
- **Implementation**: `config/version.h.in` → `build/generated/sentinel_version.h`
- **Verified By**: TC-SYS-056-1

**[SYS-057] MCU Firmware Version**
- **Description**: MCU reports version in same format via HealthStatus
- **Type**: Functional
- **Safety**: QM
- **Source**: [STKH-012]
- **Acceptance Criteria**:
  - Embedded in HealthStatus message payload
  - Same `SENTINEL_VERSION` macro used
- **Implementation**: `src/mcu/health_reporter.c`
- **Verified By**: TC-SYS-057-1

### 3.13 Configuration Management

**[SYS-058] Sensor Rate Configuration**
- **Description**: Sensor sample rates configurable via ConfigUpdate message
- **Type**: Functional
- **Safety**: QM
- **Source**: [STKH-013]
- **Acceptance Criteria**:
  - Message type: `MSG_TYPE_CONFIG_UPDATE = 0x20`
  - Applied within 1 second
  - Response: `MSG_TYPE_CONFIG_RSP = 0x21`
- **Implementation**: `src/mcu/config_store.c:config_apply_update()`
- **Verified By**: TC-SYS-058-1

**[SYS-059] Actuator Limit Configuration**
- **Description**: Actuator min/max configurable via ConfigUpdate
- **Type**: Safety
- **Safety**: ASIL-B
- **Source**: [STKH-013]
- **Acceptance Criteria**:
  - `actuator_min_percent[2]`, `actuator_max_percent[2]` in config
  - New limits enforced on next command
- **Implementation**: `src/mcu/config_store.c`
- **Verified By**: TC-SYS-059-1

**[SYS-060] Configuration Persistence**
- **Description**: Config stored in flash, survives power cycles
- **Type**: Functional
- **Safety**: QM
- **Source**: [STKH-013]
- **Acceptance Criteria**:
  - Flash address: `FLASH_CONFIG_ADDR = 0x08030000`
  - Size: 4 KB sector (`FLASH_CONFIG_SIZE = 4096`)
  - CRC-32 appended
- **Implementation**: `src/mcu/config_store.c`, `src/mcu/hal/flash_driver.c`
- **Verified By**: TC-SYS-060-1

**[SYS-061] Configuration Read-Back**
- **Description**: Current config queryable via DiagnosticRequest
- **Type**: Functional
- **Safety**: QM
- **Source**: [STKH-013]
- **Acceptance Criteria**:
  - Request type: `MSG_TYPE_DIAG_REQ = 0x30`
  - Response includes current config values
- **Implementation**: `src/mcu/protobuf_handler.c`
- **Verified By**: TC-SYS-061-1

**[SYS-062] Default Configuration**
- **Description**: Factory defaults on first boot or CRC failure
- **Type**: Functional
- **Safety**: QM
- **Source**: [STKH-013]
- **Acceptance Criteria**:
  - All channels: 10 Hz
  - Fan: min=0%, max=95%
  - Valve: min=0%, max=100%
  - Heartbeat interval: 1000 ms
  - Comm timeout: 3000 ms
- **Implementation**: `src/mcu/config_store.c:config_load_defaults()`
- **Verified By**: TC-SYS-062-1

### 3.14 Configuration Validation

**[SYS-063] Sample Rate Validation**
- **Description**: Reject sample rate outside 1-100 Hz
- **Type**: Functional
- **Safety**: QM
- **Source**: [STKH-014]
- **Acceptance Criteria**:
  - 0 Hz → rejected with `SENTINEL_ERR_OUT_OF_RANGE`
  - 101 Hz → rejected with `SENTINEL_ERR_OUT_OF_RANGE`
  - 1-100 Hz → accepted
- **Implementation**: `src/mcu/config_store.c:config_validate()`
- **Verified By**: TC-SYS-063-1

**[SYS-064] Actuator Limit Validation**
- **Description**: Reject limits outside 0-100%
- **Type**: Safety
- **Safety**: ASIL-B
- **Source**: [STKH-014]
- **Acceptance Criteria**:
  - -1% → rejected
  - 101% → rejected
  - min > max → rejected
- **Implementation**: `src/mcu/config_store.c:config_validate()`
- **Verified By**: TC-SYS-064-1

**[SYS-065] Configuration CRC**
- **Description**: CRC-32 checksum validates stored config
- **Type**: Functional
- **Safety**: QM
- **Source**: [STKH-014]
- **Acceptance Criteria**:
  - CRC polynomial: 0x04C11DB7 (standard CRC-32)
  - CRC mismatch → load defaults, log error
- **Implementation**: `src/mcu/config_store.c:config_compute_crc()`
- **Verified By**: TC-SYS-065-1

---

## 4. Non-Functional Requirements

### 4.1 Performance

**[SYS-066] System Boot Time**
- **Description**: Linux gateway ready within 30 seconds of QEMU start
- **Type**: Performance
- **Safety**: QM
- **Source**: [STKH-015]
- **Acceptance Criteria**:
  - TCP port 5001 listening within 30 seconds
  - Log message: "Gateway started"
- **Implementation**: `src/linux/main.c`
- **Verified By**: TC-SYS-066-1

**[SYS-067] MCU Boot Time**
- **Description**: MCU ready within 5 seconds
- **Type**: Performance
- **Safety**: QM
- **Source**: [STKH-015]
- **Acceptance Criteria**:
  - TCP port 5000 listening within 5 seconds
  - First HealthStatus sent within 6 seconds
- **Implementation**: `src/mcu/main.c` (QEMU: `src/mcu/hal/qemu/main_qemu.c`)
- **Verified By**: TC-SYS-067-1

**[SYS-068] TCP Throughput**
- **Description**: TCP link sustains ≥ 1 Mbps
- **Type**: Performance
- **Safety**: QM
- **Source**: [STKH-015]
- **Acceptance Criteria**:
  - Measured with 100 Hz × 4 channels × 50 bytes ≈ 20 KB/s (well under 1 Mbps)
  - **Note**: QEMU loopback easily exceeds this
- **Implementation**: N/A (network stack)
- **Verified By**: TC-SYS-068-1

### 4.2 Reliability

**[SYS-069] Hardware Watchdog**
- **Description**: MCU configures 2-second watchdog timeout
- **Type**: Safety
- **Safety**: ASIL-B
- **Source**: [STKH-016]
- **Acceptance Criteria**:
  - `iwdg_init(2000)` called at startup
  - Watchdog cannot be disabled after init
- **Implementation**: `src/mcu/hal/watchdog_driver.c:iwdg_init()`
- **Verified By**: TC-SYS-069-1

**[SYS-070] Watchdog Feed Frequency**
- **Description**: Watchdog fed at least every 500 ms
- **Type**: Safety
- **Safety**: ASIL-B
- **Source**: [STKH-016]
- **Acceptance Criteria**:
  - `iwdg_feed()` called in main super-loop
  - Loop iteration time ≤ 500 ms worst-case
- **Implementation**: `src/mcu/main.c:main()` loop
- **Verified By**: TC-SYS-070-1

**[SYS-071] Watchdog Reset Counter**
- **Description**: Persistent counter of watchdog resets
- **Type**: Functional
- **Safety**: QM
- **Source**: [STKH-016]
- **Acceptance Criteria**:
  - `iwdg_was_reset_cause()` returns true after WDT reset
  - Counter stored in backup register (survives reset)
  - Reported in HealthStatus
- **Implementation**: `src/mcu/watchdog_mgr.c`
- **Verified By**: TC-SYS-071-1

### 4.3 Memory Constraints

**[SYS-072] Static Memory Allocation (MCU)**
- **Description**: No heap allocation in MCU firmware
- **Type**: Constraint
- **Safety**: ASIL-B
- **Source**: [STKH-017]
- **Acceptance Criteria**:
  - `grep -r "malloc\|calloc\|realloc\|free" src/mcu/` returns nothing
  - Build defines: `__HEAP_SIZE=0`
  - nanopb: `PB_BUFFER_ONLY=1`, `PB_NO_ERRMSG=1`
- **Implementation**: Verified by static analysis
- **Verified By**: TC-SYS-072-1

**[SYS-073] Stack Usage Limit (MCU)**
- **Description**: Stack usage ≤ 32 KB
- **Type**: Constraint
- **Safety**: QM
- **Source**: [STKH-017]
- **Acceptance Criteria**:
  - Linker script: `_stack_size = 32K`
  - Stack analysis tool reports worst-case ≤ 32 KB
- **Implementation**: `src/mcu/stm32u575.ld`
- **Verified By**: TC-SYS-073-1

---

## 5. Process Requirements

**[SYS-074] Work Product Completeness**
- **Description**: All ASPICE CL2 work products generated
- **Type**: Process
- **Safety**: N/A
- **Source**: [STKH-018]
- **Acceptance Criteria**:
  - Documents in `docs/` for: SYS.2, SYS.3, SWE.1-SWE.6, SUP.2, SUP.8
- **Verified By**: Document checklist

**[SYS-075] Review Records**
- **Description**: Every work product has review record
- **Type**: Process
- **Safety**: N/A
- **Source**: [STKH-018]
- **Acceptance Criteria**:
  - Reviewer name, date, findings documented
  - Located in `docs/reviews/`
- **Verified By**: Review checklist

**[SYS-076] Configuration Management**
- **Description**: All artifacts in Git with meaningful commits
- **Type**: Process
- **Safety**: N/A
- **Source**: [STKH-018]
- **Acceptance Criteria**:
  - Commit messages reference requirement/task IDs
  - Branch strategy documented
- **Verified By**: Git log review

**[SYS-077] Bidirectional Traceability**
- **Description**: Requirements trace to code and tests; tests trace to requirements
- **Type**: Process
- **Safety**: N/A
- **Source**: [STKH-019]
- **Acceptance Criteria**:
  - Zero orphaned requirements
  - Code files: `@implements SYS-XXX`
  - Test files: `@verified_by TC-SYS-XXX-Y`
- **Verified By**: Traceability matrix

**[SYS-078] Traceability Matrix Automation**
- **Description**: Matrix auto-generated from tagged artifacts
- **Type**: Process
- **Safety**: N/A
- **Source**: [STKH-019]
- **Acceptance Criteria**:
  - `tools/trace_extractor.py` generates matrix
  - CI validates no orphans
- **Verified By**: CI pipeline

**[SYS-079] MISRA C:2012 Required Rules**
- **Description**: Zero Required rule violations
- **Type**: Process
- **Safety**: N/A
- **Source**: [STKH-020]
- **Acceptance Criteria**:
  - cppcheck with MISRA addon: 0 Required violations
  - CI enforced
- **Implementation**: `docker/Dockerfile.analysis`
- **Verified By**: Static analysis report

**[SYS-080] MISRA C:2012 Advisory Rules**
- **Description**: Advisory deviations documented
- **Type**: Process
- **Safety**: N/A
- **Source**: [STKH-020]
- **Acceptance Criteria**:
  - Deviation records in `docs/reviews/misra_compliance_plan.md`
  - Each deviation has rationale
- **Verified By**: Deviation review

---

## 6. Requirement Summary

| Category | ID Range | Count | Safety-Relevant |
|----------|----------|-------|-----------------|
| Sensor Acquisition | SYS-001 to SYS-008 | 8 | 0 |
| Sensor Timing | SYS-009 to SYS-012 | 4 | 0 |
| Data Logging | SYS-013 to SYS-015 | 3 | 0 |
| Actuator Control | SYS-016 to SYS-021 | 6 | 6 |
| Actuator Safety | SYS-022 to SYS-025 | 4 | 4 |
| Actuator Fail-Safe | SYS-026 to SYS-029 | 4 | 4 |
| Communication | SYS-030 to SYS-037 | 8 | 0 |
| Health Monitoring | SYS-038 to SYS-041 | 4 | 3 |
| Recovery | SYS-042 to SYS-045 | 4 | 0 |
| Diagnostics | SYS-046 to SYS-051 | 6 | 0 |
| Event Logging | SYS-052 to SYS-055 | 4 | 0 |
| Firmware Version | SYS-056 to SYS-057 | 2 | 0 |
| Configuration | SYS-058 to SYS-062 | 5 | 1 |
| Config Validation | SYS-063 to SYS-065 | 3 | 1 |
| Performance | SYS-066 to SYS-068 | 3 | 0 |
| Reliability | SYS-069 to SYS-071 | 3 | 2 |
| Memory | SYS-072 to SYS-073 | 2 | 1 |
| Process | SYS-074 to SYS-080 | 7 | 0 |
| **TOTAL** | SYS-001 to SYS-080 | **80** | **22** |

---

## 7. Implementation Quick Reference

### 7.1 Key Constants

| Constant | Value | Defined In |
|----------|-------|------------|
| `SENTINEL_MAX_CHANNELS` | 4 | `sentinel_types.h` |
| `SENTINEL_MAX_ACTUATORS` | 2 | `sentinel_types.h` |
| `SENTINEL_PORT_COMMAND` | 5000 | `sentinel_types.h` |
| `SENTINEL_PORT_TELEMETRY` | 5001 | `sentinel_types.h` |
| `SENTINEL_PORT_DIAG` | 5002 | `sentinel_types.h` |
| `SENTINEL_HEALTH_INTERVAL_MS` | 1000 | `sentinel_types.h` |
| `SENTINEL_COMM_TIMEOUT_MS` | 3000 | `sentinel_types.h` |
| `SENTINEL_WATCHDOG_TIMEOUT_MS` | 2000 | `sentinel_types.h` |
| `WIRE_FRAME_HEADER_SIZE` | 5 | `sentinel_types.h` |
| `WIRE_FRAME_MAX_PAYLOAD` | 507 | `sentinel_types.h` |
| `ADC_MAX_CHANNELS` | 4 | `adc_driver.h` |
| `ADC_RESOLUTION` | 4095 | `adc_driver.h` |
| `PWM_MAX_CHANNELS` | 2 | `pwm_driver.h` |
| `PWM_ARR_VALUE` | 999 | `pwm_driver.h` |
| `FLASH_CONFIG_ADDR` | 0x08030000 | `flash_driver.h` |
| `FLASH_CONFIG_SIZE` | 4096 | `flash_driver.h` |

### 7.2 Error Codes

| Code | Value | Description |
|------|-------|-------------|
| `SENTINEL_OK` | 0 | Success |
| `SENTINEL_ERR_INVALID_ARG` | -1 | Invalid argument |
| `SENTINEL_ERR_OUT_OF_RANGE` | -2 | Value out of range |
| `SENTINEL_ERR_TIMEOUT` | -3 | Operation timeout |
| `SENTINEL_ERR_COMM` | -4 | Communication error |
| `SENTINEL_ERR_DECODE` | -5 | Decode failure |
| `SENTINEL_ERR_ENCODE` | -6 | Encode failure |
| `SENTINEL_ERR_FULL` | -7 | Buffer/queue full (rate limited) |
| `SENTINEL_ERR_NOT_READY` | -8 | Not initialized |
| `SENTINEL_ERR_AUTH` | -9 | Authentication failed |
| `SENTINEL_ERR_INTERNAL` | -10 | Internal error |

### 7.3 Build Commands

```bash
# Linux gateway (native or cross-compiled)
cmake -B build -DBUILD_LINUX=ON -DBUILD_MCU=OFF -DBUILD_TESTS=OFF
cmake --build build

# MCU for QEMU user-mode
cmake -B build-qemu -DBUILD_LINUX=OFF -DBUILD_MCU=OFF -DBUILD_QEMU_MCU=ON
cmake --build build-qemu

# Unit tests (host)
cmake -B build-tests -DBUILD_LINUX=OFF -DBUILD_MCU=OFF -DBUILD_TESTS=ON
cmake --build build-tests
ctest --test-dir build-tests
```

### 7.4 Diagnostic Commands

| Command | Example | Response Format |
|---------|---------|-----------------|
| `status` | `status` | `state=NORMAL uptime=123s wd_resets=0 comm=OK` |
| `sensor read <ch>` | `sensor read 0` | `ch=0 raw=2048 cal=42.500` |
| `actuator set <id> <pct>` | `actuator set 0 50.0` | `OK` or `ERROR` |
| `version` | `version` | `linux=1.0.0 mcu=pending` |
| `help` | `help` | Command list |

**Note**: Commands are case-sensitive (lowercase only).
