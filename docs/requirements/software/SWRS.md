---
title: "Software Requirements Specification"
document_id: "SWRS-001"
project: "Sentinel Gateway"
version: "2.0.0"
date: 2026-03-18
status: "Approved"
asil_level: "ASIL-B (process rigor)"
authors: ["AI Requirements Agent"]
reviewers: ["Antonio Stepien"]
approvers: ["Antonio Stepien"]
aspice_process: "SWE.1 Software Requirements Analysis"
---

# Software Requirements Specification — Sentinel Gateway

## 1. Introduction

### 1.1 Purpose

This document specifies the software requirements for the Sentinel Gateway, derived from the System Requirements Specification (SRS-001). It serves as the basis for:
- Software architectural design (SWE.2)
- Detailed design and implementation (SWE.3)
- Unit and integration testing (SWE.4, SWE.5)

### 1.2 Scope

This SWRS covers all software components on both the Linux gateway (SE-01) and MCU firmware (SE-02), as defined in the System Architecture Document (SAD-001).

### 1.3 Reference Documents

| Document ID | Title | Version |
|-------------|-------|---------|
| SRS-001 | System Requirements Specification | v2.0.0 |
| SAD-001 | System Architecture Document | v2.0.0 |
| STKH-REQ-001 | Stakeholder Requirements | v2.0.0 |
| IFS-001 | Interface Specification | v2.0.0 |

### 1.4 Implementation Files Reference

| Component | Source Files |
|-----------|-------------|
| Wire Frame | `src/common/wire_frame.h`, `src/common/wire_frame.c` |
| MCU TCP Stack | `src/mcu/tcp_stack_qemu.c` (QEMU), `src/mcu/tcp_stack_lwip.c` (HW) |
| Linux Transport | `src/linux/tcp_transport.c` |
| Diagnostics | `src/linux/diagnostics.c` |
| Types/Errors | `src/common/sentinel_types.h`, `src/common/error_codes.h` |

### 1.5 Naming Convention

- **SWE-XXX-Y**: Software requirement derived from SYS-XXX, sub-requirement Y
- **Component prefix**: Indicates target — `[MCU]` or `[LINUX]` or `[SHARED]`

### 1.6 Wire Frame Format Quick Reference

All TCP messages use this exact framing (from `wire_frame.h`):

```
Offset  Size   Field         Notes
------  ----   -----         -----
0       4      length        Little-endian uint32, payload size only (NOT including header)
4       1      msg_type      Message type ID (see table below)
5       N      payload       Protobuf-encoded message body
```

**Message Type IDs** (from `sentinel_types.h`):
| ID | Name | Direction |
|----|------|-----------|
| 0x01 | MSG_SENSOR_DATA | MCU → Linux |
| 0x02 | MSG_ACTUATOR_CMD | Linux → MCU |
| 0x03 | MSG_ACTUATOR_RESP | MCU → Linux |
| 0x04 | MSG_HEALTH_STATUS | MCU → Linux |
| 0x05 | MSG_CONFIG_UPDATE | Linux → MCU |
| 0x06 | MSG_CONFIG_RESP | MCU → Linux |
| 0x07 | MSG_STATE_SYNC_REQ | Linux → MCU |
| 0x08 | MSG_STATE_SYNC_RESP | MCU → Linux |
| 0x09 | MSG_DIAG_REQ | Linux → MCU |
| 0x0A | MSG_DIAG_RESP | MCU → Linux |

### 1.7 Error Codes Quick Reference

From `error_codes.h`:
```c
typedef enum {
    SENTINEL_OK = 0,
    SENTINEL_ERR_INVALID_PARAM = -1,
    SENTINEL_ERR_OUT_OF_RANGE = -2,
    SENTINEL_ERR_TIMEOUT = -3,
    SENTINEL_ERR_COMM_FAILED = -4,
    SENTINEL_ERR_PROTO_ERROR = -5,
    SENTINEL_ERR_AUTH_FAILED = -6,
    SENTINEL_ERR_RATE_LIMITED = -7,
    SENTINEL_ERR_NOT_READY = -8,
    SENTINEL_ERR_FRAME_TOO_LARGE = -9,
    SENTINEL_ERR_UNKNOWN_MSG_TYPE = -10
} sentinel_err_t;
```

---

## 2. MCU Firmware — Sensor Acquisition

### 2.1 ADC Driver

**[SWE-001-1] ADC Peripheral Initialization**
- **Description**: The MCU ADC driver shall configure ADC1 for 4-channel scan mode (CH0-CH3) with DMA transfer, 12-bit resolution, and 3.3V reference voltage
- **Type**: Functional
- **Safety**: QM
- **Source**: [SYS-001], [SYS-006]
- **Component**: MCU / adc_driver
- **Acceptance Criteria**:
  - ADC scan completes for all 4 channels
  - Raw values range 0-4095
  - DMA transfer complete interrupt fires after each scan
- **Verified By**: [TC-SWE-001-1-1, TC-SWE-001-1-2]

**[SWE-001-2] ADC Channel Mapping**
- **Description**: The MCU ADC driver shall map ADC channels as follows: CH0=temperature, CH1=humidity, CH2=pressure, CH3=light
- **Type**: Functional
- **Safety**: QM
- **Source**: [SYS-002], [SYS-003], [SYS-004], [SYS-005]
- **Component**: MCU / adc_driver
- **Acceptance Criteria**:
  - Channel 0 reading corresponds to temperature sensor input
  - Channel 1 reading corresponds to humidity sensor input
  - Channel 2 reading corresponds to pressure sensor input
  - Channel 3 reading corresponds to light sensor input
- **Verified By**: [TC-SWE-001-2-1]

**[SWE-001-3] ADC Conversion Trigger**
- **Description**: The MCU ADC driver shall trigger ADC scan conversions from a hardware timer interrupt at the configured sample rate
- **Type**: Functional
- **Safety**: QM
- **Source**: [SYS-009], [SYS-010]
- **Component**: MCU / adc_driver
- **Acceptance Criteria**:
  - ADC conversions triggered at configured rate (1-100 Hz)
  - Timer-to-conversion jitter ≤ 500 µs
- **Verified By**: [TC-SWE-001-3-1, TC-SWE-001-3-2]

**[SWE-011-1] ADC Scan Duration**
- **Description**: The MCU ADC driver shall complete a full 4-channel scan within 100 µs
- **Type**: Performance
- **Safety**: QM
- **Source**: [SYS-011]
- **Component**: MCU / adc_driver
- **Acceptance Criteria**:
  - Time between scan start and DMA complete ≤ 100 µs
- **Verified By**: [TC-SWE-011-1-1]

### 2.2 Sensor Calibration

**[SWE-002-1] Temperature Calibration**
- **Description**: The sensor_acquisition module shall convert raw ADC CH0 values to temperature using linear mapping: T(°C) = (V / 3.3V) × 165.0 - 40.0, where V = (raw / 4095) × 3.3V
- **Type**: Functional
- **Safety**: QM
- **Source**: [SYS-002]
- **Component**: MCU / sensor_acquisition
- **Acceptance Criteria**:
  - Raw 0 → -40.0°C
  - Raw 2048 → 42.5°C ±0.5°C
  - Raw 4095 → 125.0°C ±0.5°C
- **Verified By**: [TC-SWE-002-1-1, TC-SWE-002-1-2, TC-SWE-002-1-3]

**[SWE-003-1] Humidity Calibration**
- **Description**: The sensor_acquisition module shall convert raw ADC CH1 values to humidity using linear mapping: RH(%) = (raw / 4095) × 100.0
- **Type**: Functional
- **Safety**: QM
- **Source**: [SYS-003]
- **Component**: MCU / sensor_acquisition
- **Acceptance Criteria**:
  - Raw 0 → 0.0% RH
  - Raw 2048 → 50.0% RH ±1.0%
  - Raw 4095 → 100.0% RH
- **Verified By**: [TC-SWE-003-1-1, TC-SWE-003-1-2, TC-SWE-003-1-3]

**[SWE-004-1] Pressure Calibration**
- **Description**: The sensor_acquisition module shall convert raw ADC CH2 values to pressure using mapping: P(hPa) = 300.0 + (raw / 4095) × 800.0 (sensor outputs 0.5-4.5V for 300-1100 hPa, mapped through voltage divider to 0-3.3V ADC range)
- **Type**: Functional
- **Safety**: QM
- **Source**: [SYS-004]
- **Component**: MCU / sensor_acquisition
- **Acceptance Criteria**:
  - Raw 0 → 300 hPa
  - Raw 2048 → 700 hPa ±5 hPa
  - Raw 4095 → 1100 hPa
- **Verified By**: [TC-SWE-004-1-1, TC-SWE-004-1-2, TC-SWE-004-1-3]

**[SWE-005-1] Light Calibration**
- **Description**: The sensor_acquisition module shall convert raw ADC CH3 values to illuminance using linear mapping: Lux = (raw / 4095) × 100000.0
- **Type**: Functional
- **Safety**: QM
- **Source**: [SYS-005]
- **Component**: MCU / sensor_acquisition
- **Acceptance Criteria**:
  - Raw 0 → 0 lux
  - Raw 2048 → 50000 lux ±1000 lux
  - Raw 4095 → 100000 lux
- **Verified By**: [TC-SWE-005-1-1, TC-SWE-005-1-2, TC-SWE-005-1-3]

### 2.3 Sensor Data Packaging

**[SWE-007-1] SensorData Message Construction**
- **Description**: The sensor_acquisition module shall construct a protobuf `SensorData` message for each sample cycle containing: message header (sequence number, timestamp, protocol version), array of SensorReading (one per enabled channel with channel ID, raw value, calibrated value, unit string), and current sample rate
- **Type**: Functional
- **Safety**: QM
- **Source**: [SYS-007]
- **Component**: MCU / sensor_acquisition, MCU / protobuf_handler
- **Acceptance Criteria**:
  - All fields populated with valid values
  - Sequence number increments monotonically
  - Timestamp reflects actual sample time in microseconds
- **Verified By**: [TC-SWE-007-1-1, TC-SWE-007-1-2]

**[SWE-007-2] SensorData Encoding**
- **Description**: The MCU protobuf_handler shall encode SensorData messages using nanopb into a statically allocated buffer (max 256 bytes)
- **Type**: Functional
- **Safety**: QM
- **Source**: [SYS-007], [SYS-072]
- **Component**: MCU / protobuf_handler
- **Acceptance Criteria**:
  - Encoding succeeds for all 4 channels in a single message
  - Encoded size ≤ 256 bytes
  - No dynamic memory allocation
- **Verified By**: [TC-SWE-007-2-1, TC-SWE-007-2-2]

**[SWE-008-1] Telemetry Transmission**
- **Description**: The MCU tcp_stack module shall transmit encoded SensorData messages to the Linux gateway on TCP port 5001 with the wire framing format: [4-byte LE length][1-byte msg type 0x01][protobuf payload]
- **Type**: Functional
- **Safety**: QM
- **Source**: [SYS-008], [SYS-035]
- **Component**: MCU / tcp_stack
- **Implementation Note**: For QEMU SIL builds (`BUILD_QEMU_MCU=ON`), TCP is implemented via POSIX sockets in `tcp_stack_qemu.c`. For hardware builds, lwIP is used in `tcp_stack_lwip.c`.
- **Acceptance Criteria**:
  - Message received on Linux port 5001
  - Framing header correctly parsed
  - Message type byte is 0x01
- **Verified By**: [TC-SWE-008-1-1, TC-SWE-008-1-2]

### 2.4 Sample Rate Management

**[SWE-009-1] Per-Channel Timer Configuration**
- **Description**: The MCU sensor_acquisition module shall support independent sample rates per ADC channel, configurable from 1 Hz to 100 Hz in 1 Hz increments, using hardware timer compare values
- **Type**: Functional
- **Safety**: QM
- **Source**: [SYS-009], [SYS-012]
- **Component**: MCU / sensor_acquisition
- **Acceptance Criteria**:
  - Channel 0 at 100 Hz while Channel 3 at 1 Hz simultaneously
  - Sample rate accuracy ±5%
- **Verified By**: [TC-SWE-009-1-1, TC-SWE-009-1-2]

**[SWE-010-1] Sample Timing Jitter**
- **Description**: The MCU sensor_acquisition module shall ensure sample timing jitter does not exceed ±500 µs at any configured rate by using hardware timer interrupts (not software delays)
- **Type**: Performance
- **Safety**: QM
- **Source**: [SYS-010]
- **Component**: MCU / sensor_acquisition
- **Acceptance Criteria**:
  - Jitter ≤ 500 µs measured over 1000 consecutive samples at 100 Hz
- **Verified By**: [TC-SWE-010-1-1]

---

## 3. MCU Firmware — Actuator Control

### 3.1 PWM Output

**[SWE-016-1] Fan PWM Initialization**
- **Description**: The MCU pwm_driver shall configure TIM2 Channel 1 for 25 kHz PWM output with 16-bit resolution (65536 duty cycle steps), initially at 0% duty cycle (safe state)
- **Type**: Functional
- **Safety**: ASIL-B
- **Source**: [SYS-016], [SYS-027]
- **Component**: MCU / pwm_driver
- **Acceptance Criteria**:
  - PWM frequency = 25 kHz ±1%
  - Initial duty cycle = 0%
  - Resolution allows 0.0015% steps
- **Verified By**: [TC-SWE-016-1-1, TC-SWE-016-1-2]

**[SWE-017-1] Valve PWM Initialization**
- **Description**: The MCU pwm_driver shall configure TIM2 Channel 2 for 25 kHz PWM output with 16-bit resolution, initially at 0% duty cycle (safe state)
- **Type**: Functional
- **Safety**: ASIL-B
- **Source**: [SYS-017], [SYS-027]
- **Component**: MCU / pwm_driver
- **Acceptance Criteria**:
  - PWM frequency = 25 kHz ±1%
  - Initial duty cycle = 0%
- **Verified By**: [TC-SWE-017-1-1]

**[SWE-016-2] PWM Duty Cycle Update**
- **Description**: The MCU actuator_control module shall update PWM duty cycle by writing to the TIM2 compare register, applying the change within 1 timer period (40 µs at 25 kHz)
- **Type**: Performance
- **Safety**: ASIL-B
- **Source**: [SYS-020]
- **Component**: MCU / actuator_control, MCU / pwm_driver
- **Acceptance Criteria**:
  - PWM output changes within 40 µs of register write
- **Verified By**: [TC-SWE-016-2-1]

### 3.2 Command Processing

**[SWE-018-1] ActuatorCommand Decoding**
- **Description**: The MCU protobuf_handler shall decode incoming ActuatorCommand messages from TCP port 5000 wire format, extracting: actuator_id, target_percent, auth_token, and message header fields
- **Type**: Functional
- **Safety**: ASIL-B
- **Source**: [SYS-018]
- **Component**: MCU / protobuf_handler
- **Acceptance Criteria**:
  - All fields decoded correctly
  - Invalid protobuf data returns error (not crash)
- **Verified By**: [TC-SWE-018-1-1, TC-SWE-018-1-2]

**[SWE-022-1] Actuator Range Validation**
- **Description**: The MCU actuator_control module shall validate that target_percent is within the configured min/max limits for the specified actuator before applying; values outside range shall be rejected with STATUS_ERROR_OUT_OF_RANGE
- **Type**: Safety
- **Safety**: ASIL-B
- **Source**: [SYS-022]
- **Component**: MCU / actuator_control
- **Acceptance Criteria**:
  - Value 101.0% rejected for any actuator
  - Value -1.0% rejected for any actuator
  - Value at max limit accepted
  - Value at min limit accepted
- **Verified By**: [TC-SWE-022-1-1, TC-SWE-022-1-2, TC-SWE-022-1-3, TC-SWE-022-1-4]

**[SWE-022-2] Actuator Output Readback Verification**
- **Description**: After writing a PWM duty cycle, the MCU actuator_control module shall read back the TIM2 compare register and verify it matches the intended value within ±1 step; mismatch shall trigger fail-safe
- **Type**: Safety
- **Safety**: ASIL-B
- **Source**: [SYS-022]
- **Component**: MCU / actuator_control
- **Acceptance Criteria**:
  - Readback matches written value
  - Deliberately corrupted register triggers fail-safe
- **Verified By**: [TC-SWE-022-2-1, TC-SWE-022-2-2]

**[SWE-023-1] Fan Safety Limit Enforcement**
- **Description**: The MCU actuator_control module shall clamp fan duty cycle commands to the configured maximum (default 95%) and minimum (default 0%) before applying
- **Type**: Safety
- **Safety**: ASIL-B
- **Source**: [SYS-023]
- **Component**: MCU / actuator_control
- **Acceptance Criteria**:
  - Command for 100% clamped to 95% (default max)
  - Clamped value reported in ActuatorResponse.applied_percent
- **Verified By**: [TC-SWE-023-1-1, TC-SWE-023-1-2]

**[SWE-024-1] Valve Safety Limit Enforcement**
- **Description**: The MCU actuator_control module shall clamp valve duty cycle commands to configured max (default 100%) and min (default 0%)
- **Type**: Safety
- **Safety**: ASIL-B
- **Source**: [SYS-024]
- **Component**: MCU / actuator_control
- **Acceptance Criteria**:
  - Limits enforced at boundary values
- **Verified By**: [TC-SWE-024-1-1]

**[SWE-021-1] Actuator Rate Limiting**
- **Description**: The MCU actuator_control module shall track command count per actuator per second using a sliding window; commands exceeding 50/second shall be rejected with STATUS_ERROR_RATE_LIMITED
- **Type**: Functional
- **Safety**: ASIL-B
- **Source**: [SYS-021]
- **Component**: MCU / actuator_control
- **Acceptance Criteria**:
  - 50 commands within 1 second accepted
  - 51st command rejected
  - Counter resets after 1 second window
- **Verified By**: [TC-SWE-021-1-1, TC-SWE-021-1-2]

**[SWE-019-1] ActuatorResponse Construction**
- **Description**: The MCU actuator_control module shall construct an ActuatorResponse message for every received ActuatorCommand containing: actuator_id, applied_percent (actual value after clamping), status code, and error message (if applicable)
- **Type**: Functional
- **Safety**: ASIL-B
- **Source**: [SYS-019]
- **Component**: MCU / actuator_control, MCU / protobuf_handler
- **Acceptance Criteria**:
  - Response sent for every command
  - applied_percent reflects actual PWM value
  - status reflects validation result
- **Verified By**: [TC-SWE-019-1-1, TC-SWE-019-1-2]

### 3.3 Fail-Safe

**[SWE-026-1] Communication Loss Timer**
- **Description**: The MCU actuator_control module shall maintain a communication timeout timer that resets on each received message from Linux; if timer reaches 3 seconds with no messages, the module shall set all actuator outputs to 0% duty cycle
- **Type**: Safety
- **Safety**: ASIL-B
- **Source**: [SYS-026]
- **Component**: MCU / actuator_control
- **Acceptance Criteria**:
  - Actuators at 0% within 3.5 seconds of last Linux message
  - Timer resets on any valid received message
- **Verified By**: [TC-SWE-026-1-1, TC-SWE-026-1-2]

**[SWE-027-1] Boot Safe State**
- **Description**: The MCU main_loop shall initialize all PWM outputs to 0% duty cycle during startup, before enabling any communication or processing
- **Type**: Safety
- **Safety**: ASIL-B
- **Source**: [SYS-027]
- **Component**: MCU / main_loop, MCU / pwm_driver
- **Acceptance Criteria**:
  - PWM outputs at 0% before TCP connection established
- **Verified By**: [TC-SWE-027-1-1]

**[SWE-028-1] Protobuf Error Fail-Safe**
- **Description**: On any protobuf decode error for ActuatorCommand or ConfigUpdate messages, the MCU shall transition all actuators to safe state (0%) and set MCU state to FAILSAFE
- **Type**: Safety
- **Safety**: ASIL-B
- **Source**: [SYS-028]
- **Component**: MCU / protobuf_handler, MCU / actuator_control
- **Acceptance Criteria**:
  - Corrupted protobuf data triggers fail-safe within 100 ms
  - MCU state changes to FAILSAFE
- **Verified By**: [TC-SWE-028-1-1, TC-SWE-028-1-2]

**[SWE-029-1] Fail-Safe Health Report**
- **Description**: When entering fail-safe state, the MCU health_reporter shall immediately send a HealthStatus message with state=FAILSAFE and the appropriate failsafe_cause code
- **Type**: Functional
- **Safety**: ASIL-B
- **Source**: [SYS-029]
- **Component**: MCU / health_reporter
- **Acceptance Criteria**:
  - HealthStatus with FAILSAFE state sent within 100 ms of fail-safe trigger
  - Correct cause code (COMM_LOSS, WATCHDOG, PROTO_ERROR)
- **Verified By**: [TC-SWE-029-1-1, TC-SWE-029-1-2]

---

## 4. MCU Firmware — Communication

### 4.1 USB CDC-ECM

**[SWE-030-1] USB CDC-ECM Device Initialization**
- **Description**: The MCU usb_cdc module shall initialize the USB OTG FS peripheral in device mode with CDC-ECM class, presenting as an Ethernet adapter to the Linux host
- **Type**: Functional
- **Safety**: QM
- **Source**: [SYS-030]
- **Component**: MCU / usb_cdc
- **Acceptance Criteria**:
  - Linux host enumerates `usb0` interface on MCU connection
  - CDC-ECM descriptors correct per USB spec
- **Verified By**: [TC-SWE-030-1-1]

**[SWE-031-1] MCU IP Configuration**
- **Description**: The MCU tcp_stack module shall configure the CDC-ECM network interface with static IP 192.168.7.2, subnet mask 255.255.255.0, no gateway
- **Type**: Functional
- **Safety**: QM
- **Source**: [SYS-031]
- **Component**: MCU / tcp_stack
- **Implementation Note**: For QEMU SIL builds, the MCU connects to `127.0.0.1` (localhost) via POSIX sockets. The IP address can be overridden via the `SENTINEL_MCU_HOST` environment variable on the Linux side.
- **Acceptance Criteria**:
  - MCU responds to ICMP ping from 192.168.7.1 (hardware)
  - MCU connects to localhost (QEMU SIL)
- **Verified By**: [TC-SWE-031-1-1]

### 4.2 TCP Server/Client

**[SWE-032-1] TCP Command Server**
- **Description**: The MCU tcp_stack module shall listen on TCP port 5000 and accept a single connection from the Linux gateway for command/response messages
- **Type**: Functional
- **Safety**: QM
- **Source**: [SYS-032]
- **Component**: MCU / tcp_stack
- **Acceptance Criteria**:
  - TCP SYN on port 5000 accepted
  - Connection established (3-way handshake complete)
- **Verified By**: [TC-SWE-032-1-1]

**[SWE-033-1] TCP Telemetry Client**
- **Description**: The MCU tcp_stack module shall connect to Linux gateway TCP port 5001 for streaming telemetry (SensorData, HealthStatus)
- **Type**: Functional
- **Safety**: QM
- **Source**: [SYS-033]
- **Component**: MCU / tcp_stack
- **Acceptance Criteria**:
  - TCP connection to 192.168.7.1:5001 established
  - Reconnection attempted on connection loss
- **Verified By**: [TC-SWE-033-1-1]

**[SWE-035-1] Wire Frame Encoding (MCU)**
- **Description**: The MCU tcp_stack module shall encode outgoing messages with wire framing: 4-byte little-endian payload length, followed by 1-byte message type ID, followed by protobuf payload bytes
- **Type**: Functional
- **Safety**: QM
- **Source**: [SYS-035]
- **Component**: MCU / tcp_stack
- **Acceptance Criteria**:
  - Length field matches actual payload size
  - Message type byte matches message content
- **Verified By**: [TC-SWE-035-1-1]

**[SWE-035-2] Wire Frame Decoding (MCU)**
- **Description**: The MCU tcp_stack module shall decode incoming messages by reading 4-byte length, 1-byte type, then reading exactly `length` bytes of protobuf payload; incomplete frames shall be buffered
- **Type**: Functional
- **Safety**: QM
- **Source**: [SYS-035]
- **Component**: MCU / tcp_stack
- **Acceptance Criteria**:
  - Multi-message TCP stream correctly demultiplexed
  - Partial frame buffered until complete
- **Verified By**: [TC-SWE-035-2-1, TC-SWE-035-2-2]

**[SWE-036-1] Sequence Number Generation (MCU)**
- **Description**: The MCU shall maintain a global 32-bit sequence counter, incrementing by 1 for each outgoing message, wrapping from UINT32_MAX to 0
- **Type**: Functional
- **Safety**: QM
- **Source**: [SYS-036]
- **Component**: MCU / protobuf_handler
- **Acceptance Criteria**:
  - Consecutive messages have consecutive sequence numbers
  - Counter wraps correctly at UINT32_MAX
- **Verified By**: [TC-SWE-036-1-1, TC-SWE-036-1-2]

**[SWE-037-1] Protocol Version Embedding (MCU)**
- **Description**: The MCU protobuf_handler shall include protocol version {major=1, minor=0} in every outgoing message header
- **Type**: Functional
- **Safety**: QM
- **Source**: [SYS-037]
- **Component**: MCU / protobuf_handler
- **Acceptance Criteria**:
  - Version field present in all messages
  - major=1, minor=0
- **Verified By**: [TC-SWE-037-1-1]

### 4.3 Health Monitoring

**[SWE-038-1] Heartbeat Timer**
- **Description**: The MCU health_reporter shall send a HealthStatus message every 1000 ms (±50 ms) via the telemetry TCP connection, containing: MCU state, comm status, uptime, watchdog reset count, stack usage, actuator states
- **Type**: Functional
- **Safety**: ASIL-B
- **Source**: [SYS-038]
- **Component**: MCU / health_reporter
- **Acceptance Criteria**:
  - HealthStatus received at 1 Hz ±5%
  - All fields populated with current values
- **Verified By**: [TC-SWE-038-1-1, TC-SWE-038-1-2]

**[SWE-040-1] MCU Gateway Monitor**
- **Description**: The MCU shall maintain a gateway activity timer that resets on each message received from Linux; if 5 seconds elapse with no activity, MCU shall declare COMM_DISCONNECTED and trigger fail-safe
- **Type**: Safety
- **Safety**: ASIL-B
- **Source**: [SYS-040]
- **Component**: MCU / health_reporter
- **Acceptance Criteria**:
  - Fail-safe triggered within 5 seconds of Linux silence
  - CommStatus transitions to DISCONNECTED
- **Verified By**: [TC-SWE-040-1-1]

**[SWE-041-1] Communication Status Tracking (MCU)**
- **Description**: The MCU health_reporter shall track communication status as: CONNECTED (normal), DEGRADED (sequence gap detected in last 20 messages), DISCONNECTED (timeout reached)
- **Type**: Functional
- **Safety**: QM
- **Source**: [SYS-041]
- **Component**: MCU / health_reporter
- **Acceptance Criteria**:
  - Status transitions correctly between states
  - DEGRADED detected on >5% sequence gaps
- **Verified By**: [TC-SWE-041-1-1]

---

## 5. MCU Firmware — Watchdog & Configuration

### 5.1 Watchdog

**[SWE-069-1] IWDG Initialization**
- **Description**: The MCU watchdog_mgr shall configure the Independent Watchdog (IWDG) with a 2-second timeout using the LSI oscillator (independent of main clock)
- **Type**: Safety
- **Safety**: ASIL-B
- **Source**: [SYS-069]
- **Component**: MCU / watchdog_mgr
- **Acceptance Criteria**:
  - IWDG starts with 2-second timeout
  - IWDG cannot be disabled after initialization
- **Verified By**: [TC-SWE-069-1-1]

**[SWE-070-1] Watchdog Feed**
- **Description**: The MCU main_loop shall call watchdog_mgr_feed() at least every 500 ms from the main super-loop iteration
- **Type**: Safety
- **Safety**: ASIL-B
- **Source**: [SYS-070]
- **Component**: MCU / main_loop, MCU / watchdog_mgr
- **Acceptance Criteria**:
  - Watchdog fed every loop iteration
  - Feed interval ≤ 500 ms under worst-case load
- **Verified By**: [TC-SWE-070-1-1]

**[SWE-071-1] Watchdog Reset Counter**
- **Description**: The MCU watchdog_mgr shall read the reset reason register on startup; if watchdog reset detected, increment a persistent counter stored in flash backup registers
- **Type**: Functional
- **Safety**: QM
- **Source**: [SYS-071]
- **Component**: MCU / watchdog_mgr
- **Acceptance Criteria**:
  - Counter increments on watchdog reset
  - Counter reported in HealthStatus messages
- **Verified By**: [TC-SWE-071-1-1]

### 5.2 Configuration Store

**[SWE-058-1] Configuration Update Processing**
- **Description**: The MCU config_store shall process ConfigUpdate messages by: (1) validating all fields, (2) applying sensor rate changes to timer configuration, (3) storing updated config in flash with CRC-32, (4) returning ConfigResponse with status
- **Type**: Functional
- **Safety**: QM
- **Source**: [SYS-058]
- **Component**: MCU / config_store
- **Acceptance Criteria**:
  - Valid config applied within 1 second
  - ConfigResponse.status = OK for valid config
- **Verified By**: [TC-SWE-058-1-1]

**[SWE-059-1] Safety Limit Update with Authentication**
- **Description**: The MCU config_store shall require a valid auth_token (non-zero, matching stored token) for actuator limit changes; invalid token returns STATUS_ERROR_AUTH_FAILED
- **Type**: Safety
- **Safety**: ASIL-B
- **Source**: [SYS-059], [SYS-025]
- **Component**: MCU / config_store
- **Acceptance Criteria**:
  - Valid token: limits updated
  - Invalid token: rejected with AUTH_FAILED
  - Zero token: rejected
- **Verified By**: [TC-SWE-059-1-1, TC-SWE-059-1-2]

**[SWE-060-1] Flash Configuration Persistence**
- **Description**: The MCU config_store shall write configuration to a dedicated flash sector with CRC-32 checksum appended; configuration survives power cycle and watchdog reset
- **Type**: Functional
- **Safety**: QM
- **Source**: [SYS-060], [SYS-065]
- **Component**: MCU / config_store
- **Acceptance Criteria**:
  - Config readable after simulated power cycle
  - CRC validates after reload
- **Verified By**: [TC-SWE-060-1-1, TC-SWE-060-1-2]

**[SWE-062-1] Factory Default Configuration**
- **Description**: The MCU config_store shall provide factory defaults: all channels 10 Hz, fan max 95%, fan min 0%, valve max 100%, valve min 0%; defaults loaded on first boot or CRC failure
- **Type**: Functional
- **Safety**: QM
- **Source**: [SYS-062]
- **Component**: MCU / config_store
- **Acceptance Criteria**:
  - Defaults loaded when flash is blank
  - Defaults loaded when flash CRC is invalid
- **Verified By**: [TC-SWE-062-1-1, TC-SWE-062-1-2]

**[SWE-063-1] Sample Rate Validation**
- **Description**: The MCU config_store shall reject sample rate values < 1 Hz or > 100 Hz with STATUS_ERROR_OUT_OF_RANGE
- **Type**: Functional
- **Safety**: QM
- **Source**: [SYS-063]
- **Component**: MCU / config_store
- **Acceptance Criteria**:
  - 0 Hz rejected
  - 101 Hz rejected
  - 1 Hz accepted
  - 100 Hz accepted
- **Verified By**: [TC-SWE-063-1-1, TC-SWE-063-1-2]

**[SWE-064-1] Actuator Limit Validation**
- **Description**: The MCU config_store shall reject actuator limit values < 0.0% or > 100.0% with STATUS_ERROR_OUT_OF_RANGE; additionally, min must be ≤ max
- **Type**: Safety
- **Safety**: ASIL-B
- **Source**: [SYS-064]
- **Component**: MCU / config_store
- **Acceptance Criteria**:
  - -1% rejected
  - 101% rejected
  - min > max rejected
- **Verified By**: [TC-SWE-064-1-1, TC-SWE-064-1-2, TC-SWE-064-1-3]

**[SWE-065-1] Configuration CRC Validation**
- **Description**: The MCU config_store shall compute CRC-32 (polynomial 0x04C11DB7) over configuration data on read; if CRC mismatch, log error and load factory defaults
- **Type**: Functional
- **Safety**: QM
- **Source**: [SYS-065]
- **Component**: MCU / config_store
- **Acceptance Criteria**:
  - Valid CRC: config loaded normally
  - Invalid CRC: factory defaults loaded, error logged
- **Verified By**: [TC-SWE-065-1-1, TC-SWE-065-1-2]

### 5.3 Memory Constraints

**[SWE-072-1] Static Allocation Enforcement**
- **Description**: The MCU firmware shall not use malloc, calloc, realloc, or free functions; all memory shall be statically allocated at compile time or on the stack
- **Type**: Constraint
- **Safety**: ASIL-B
- **Source**: [SYS-072]
- **Component**: MCU / all modules
- **Acceptance Criteria**:
  - Static analysis confirms zero heap function references in project code
  - Linker map shows no heap section usage
- **Verified By**: [TC-SWE-072-1-1]

**[SWE-073-1] Stack Usage Limit**
- **Description**: The MCU firmware worst-case stack usage shall not exceed 32 KB, verified by static stack analysis
- **Type**: Constraint
- **Safety**: QM
- **Source**: [SYS-073]
- **Component**: MCU / all modules
- **Acceptance Criteria**:
  - Stack analysis tool reports ≤ 32 KB worst-case
  - Stack canary check in debug builds
- **Verified By**: [TC-SWE-073-1-1]

---

## 6. Linux Gateway — Core Application

### 6.1 Event Loop

**[SWE-046-1] Main Event Loop**
- **Description**: The Linux gateway_core shall implement an epoll-based event loop multiplexing: TCP command socket (port 5000 client), TCP telemetry socket (port 5001 server), TCP diagnostic socket (port 5002 server), and signal handlers (SIGTERM, SIGINT for graceful shutdown)
- **Type**: Functional
- **Safety**: QM
- **Source**: [SYS-032], [SYS-033], [SYS-046]
- **Component**: LINUX / gateway_core
- **Acceptance Criteria**:
  - All three TCP connections handled concurrently
  - No blocking calls in event loop
  - Graceful shutdown on SIGTERM
- **Verified By**: [TC-SWE-046-1-1]

### 6.2 Sensor Proxy

**[SWE-013-1] Sensor Data Reception**
- **Description**: The Linux sensor_proxy shall receive SensorData messages from TCP port 5001, decode using protobuf-c, validate sequence numbers (log gaps), and forward to the logger
- **Type**: Functional
- **Safety**: QM
- **Source**: [SYS-013]
- **Component**: LINUX / sensor_proxy
- **Acceptance Criteria**:
  - All SensorData messages received and decoded
  - Sequence number gaps logged as warnings
- **Verified By**: [TC-SWE-013-1-1, TC-SWE-013-1-2]

**[SWE-013-2] Sensor Data Logging**
- **Description**: The Linux logger shall write received sensor data to a JSON Lines file at `/var/log/sentinel/sensor_data.jsonl` with fields: timestamp, channel, raw_value, calibrated_value, unit, sequence_number
- **Type**: Functional
- **Safety**: QM
- **Source**: [SYS-013], [SYS-014]
- **Component**: LINUX / logger
- **Acceptance Criteria**:
  - Each line is valid JSON
  - Timestamp resolution ≤ 1 ms
  - File written without gaps
- **Verified By**: [TC-SWE-013-2-1, TC-SWE-013-2-2]

**[SWE-015-1] Log Rotation**
- **Description**: The Linux logger shall rotate log files when size exceeds 100 MB, keeping a maximum of 10 rotated files; oldest file deleted when limit reached
- **Type**: Functional
- **Safety**: QM
- **Source**: [SYS-015]
- **Component**: LINUX / logger
- **Acceptance Criteria**:
  - File rotated at 100 MB boundary
  - No more than 10 rotated files exist
- **Verified By**: [TC-SWE-015-1-1]

### 6.3 Actuator Proxy

**[SWE-018-2] ActuatorCommand Encoding (Linux)**
- **Description**: The Linux actuator_proxy shall encode ActuatorCommand protobuf messages using protobuf-c and transmit via TCP port 5000 with wire framing [4-byte LE length][type 0x10][payload]
- **Type**: Functional
- **Safety**: ASIL-B
- **Source**: [SYS-018]
- **Component**: LINUX / actuator_proxy
- **Acceptance Criteria**:
  - Message encoded correctly
  - Wire framing with type 0x10
- **Verified By**: [TC-SWE-018-2-1]

**[SWE-019-2] ActuatorResponse Processing (Linux)**
- **Description**: The Linux actuator_proxy shall receive and decode ActuatorResponse messages, log the result, and report errors to the diagnostic interface
- **Type**: Functional
- **Safety**: ASIL-B
- **Source**: [SYS-019]
- **Component**: LINUX / actuator_proxy
- **Acceptance Criteria**:
  - Response received and decoded for each sent command
  - Error responses logged as warnings
- **Verified By**: [TC-SWE-019-2-1]

### 6.4 Health Monitor

**[SWE-039-1] Heartbeat Timeout Detection**
- **Description**: The Linux health_monitor shall track MCU heartbeat messages and declare communication loss if 3 consecutive heartbeats are missed (3 seconds at 1 Hz rate)
- **Type**: Functional
- **Safety**: ASIL-B
- **Source**: [SYS-039]
- **Component**: LINUX / health_monitor
- **Acceptance Criteria**:
  - COMM_LOSS event raised at t=3 seconds after last heartbeat
  - Event raised within 3.5 seconds
- **Verified By**: [TC-SWE-039-1-1, TC-SWE-039-1-2]

**[SWE-042-1] USB Power Cycle Recovery**
- **Description**: On MCU communication loss, the Linux health_monitor shall wait 5 seconds, then execute MCU reset by sending `device_del` and `device_add` commands to QEMU monitor socket (simulating USB power cycle)
- **Type**: Functional
- **Safety**: QM
- **Source**: [SYS-042]
- **Component**: LINUX / health_monitor
- **Acceptance Criteria**:
  - QEMU monitor commands sent
  - MCU re-enumerates on USB bus
- **Verified By**: [TC-SWE-042-1-1]

**[SWE-043-1] TCP Reconnection with Backoff**
- **Description**: The Linux gateway_core shall attempt TCP reconnection on connection loss with exponential backoff: initial delay 100 ms, doubling each attempt, maximum delay 5 seconds, maximum 10 attempts before giving up
- **Type**: Functional
- **Safety**: QM
- **Source**: [SYS-043]
- **Component**: LINUX / gateway_core
- **Acceptance Criteria**:
  - Reconnection attempted with increasing delays
  - Connection re-established within 10 seconds for transient failure
- **Verified By**: [TC-SWE-043-1-1]

**[SWE-044-1] State Resynchronization**
- **Description**: After TCP reconnection, the Linux gateway_core shall send a StateSyncRequest message and wait up to 2 seconds for StateSyncResponse; received state shall update local caches for sensor values, actuator states, and configuration
- **Type**: Functional
- **Safety**: QM
- **Source**: [SYS-044]
- **Component**: LINUX / gateway_core
- **Acceptance Criteria**:
  - StateSyncRequest sent immediately after reconnection
  - Local state updated from response
  - Timeout handled (retry or fail)
- **Verified By**: [TC-SWE-044-1-1]

**[SWE-045-1] Recovery Attempt Limit**
- **Description**: The Linux health_monitor shall track recovery attempts; after 3 failed recovery cycles (USB power cycle + reconnection), system shall enter FAULT state and cease recovery attempts
- **Type**: Functional
- **Safety**: QM
- **Source**: [SYS-045]
- **Component**: LINUX / health_monitor
- **Acceptance Criteria**:
  - Counter increments on each failed recovery
  - FAULT state entered after 3 failures
  - No further recovery attempts in FAULT state
- **Verified By**: [TC-SWE-045-1-1]

### 6.5 Diagnostics Server

**[SWE-046-2] Diagnostic TCP Server**
- **Description**: The Linux diagnostics module shall listen on TCP port 5002, accepting a single client connection at a time, with line-delimited plain-text command/response protocol (newline terminated)
- **Type**: Functional
- **Safety**: QM
- **Source**: [SYS-046]
- **Component**: LINUX / diagnostics
- **Implementation Note**: The current epoll implementation uses a single-slot design for simplicity. Only one diagnostic client can connect at a time; subsequent connections are refused until the current client disconnects.
- **Acceptance Criteria**:
  - Connection accepted on port 5002
  - Commands parsed line-by-line
  - Second connection attempt refused while first client connected
- **Verified By**: [TC-SWE-046-2-1, TC-SWE-046-2-2]

**[SWE-047-1] sensor read Command**
- **Description**: The diagnostics module shall handle `sensor read <channel_id>` by returning the latest cached sensor reading for the specified channel in format: `OK SENSOR <channel_id> <calibrated_value> <unit> <timestamp>\n`
- **Type**: Functional
- **Safety**: QM
- **Source**: [SYS-047]
- **Component**: LINUX / diagnostics
- **Implementation Note**: Diagnostic commands are **lowercase only** and **case-sensitive**. The command `SENSOR READ` or `Sensor Read` will be rejected as unknown.
- **Acceptance Criteria**:
  - `sensor read 0` returns latest reading for channel 0
  - `sensor read 5` returns `ERROR INVALID_CHANNEL\n`
  - `SENSOR READ 0` returns `ERROR UNKNOWN_COMMAND\n` (case-sensitive!)
- **Verified By**: [TC-SWE-047-1-1, TC-SWE-047-1-2, TC-SWE-047-1-3]

**[SWE-048-1] actuator set Command**
- **Description**: The diagnostics module shall handle `actuator set <actuator_id> <value>` by sending an ActuatorCommand to MCU and returning the response status
- **Type**: Functional
- **Safety**: QM
- **Source**: [SYS-048]
- **Component**: LINUX / diagnostics
- **Implementation Note**: Command is lowercase only. Example: `actuator set 0 50`
- **Acceptance Criteria**:
  - `actuator set 0 50` returns `OK ACTUATOR 0 SET 50.0%\n`
  - `actuator set 0 200` returns `ERROR OUT_OF_RANGE\n`
  - `SET_ACTUATOR 0 50` returns `ERROR UNKNOWN_COMMAND\n`
- **Verified By**: [TC-SWE-048-1-1, TC-SWE-048-1-2]

**[SWE-049-1] status Command**
- **Description**: The diagnostics module shall handle `status` (lowercase) by returning aggregated system status: comm state, MCU state, all sensor readings, all actuator states, uptime, watchdog reset count
- **Type**: Functional
- **Safety**: QM
- **Source**: [SYS-049]
- **Component**: LINUX / diagnostics
- **Implementation Note**: Command is lowercase only: `status`. The command `STATUS` or `get_status` will fail.
- **Acceptance Criteria**:
  - All status fields present in response
  - Response formatted as multi-line text
- **Verified By**: [TC-SWE-049-1-1]

**[SWE-050-1] version Command**
- **Description**: The diagnostics module shall handle `version` (lowercase) by returning Linux version (from build metadata) and MCU version (from last DiagnosticResponse)
- **Type**: Functional
- **Safety**: QM
- **Source**: [SYS-050], [SYS-056], [SYS-057]
- **Component**: LINUX / diagnostics
- **Acceptance Criteria**:
  - Both versions returned in format: `Linux: <version>\nMCU: <version>\n`
- **Verified By**: [TC-SWE-050-1-1]

**[SWE-051-1] reset mcu Command**
- **Description**: The diagnostics module shall handle `reset mcu` (lowercase) by triggering USB power cycle recovery sequence (same as automatic recovery)
- **Type**: Functional
- **Safety**: QM
- **Source**: [SYS-051]
- **Component**: LINUX / diagnostics, LINUX / health_monitor
- **Acceptance Criteria**:
  - MCU reset initiated
  - Response: `OK MCU_RESET_INITIATED\n`
- **Verified By**: [TC-SWE-051-1-1]

**[SWE-054-1] log Command**
- **Description**: The diagnostics module shall handle `log [count]` (lowercase) by returning the most recent `count` (default 50, max 500) log entries from the event log
- **Type**: Functional
- **Safety**: QM
- **Source**: [SYS-054]
- **Component**: LINUX / diagnostics
- **Acceptance Criteria**:
  - `log 10` returns last 10 entries
  - Entries in chronological order (newest last)
- **Verified By**: [TC-SWE-054-1-1]

**[SWE-054-2] help Command**
- **Description**: The diagnostics module shall handle `help` (lowercase) by returning a list of available commands with brief descriptions
- **Type**: Functional
- **Safety**: QM
- **Source**: [SYS-046]
- **Component**: LINUX / diagnostics
- **Acceptance Criteria**:
  - Lists all available commands
  - Shows usage syntax for each command
- **Verified By**: [TC-SWE-054-2-1]

### 6.6 Event Logging

**[SWE-052-1] System Event Logger**
- **Description**: The Linux logger shall maintain a system event log at `/var/log/sentinel/events.jsonl` recording all events with: ISO-8601 timestamp, severity level, event type, source module, message, and associated data
- **Type**: Functional
- **Safety**: QM
- **Source**: [SYS-052]
- **Component**: LINUX / logger
- **Acceptance Criteria**:
  - All event types logged (sensor, actuator, comm, error, config, diag)
  - Each line is valid JSON
- **Verified By**: [TC-SWE-052-1-1]

**[SWE-053-1] Log Persistence**
- **Description**: The Linux logger shall flush log files to disk using fsync() at least every 5 seconds and on graceful shutdown, ensuring persistence across reboots
- **Type**: Functional
- **Safety**: QM
- **Source**: [SYS-053]
- **Component**: LINUX / logger
- **Acceptance Criteria**:
  - Logs present after simulated reboot
  - No data loss for events older than 5 seconds before power loss
- **Verified By**: [TC-SWE-053-1-1]

**[SWE-055-1] Log Severity Levels**
- **Description**: The Linux logger shall support severity levels: DEBUG (0), INFO (1), WARNING (2), ERROR (3), CRITICAL (4); runtime log level filtering configurable
- **Type**: Functional
- **Safety**: QM
- **Source**: [SYS-055]
- **Component**: LINUX / logger
- **Acceptance Criteria**:
  - All severity levels available
  - Setting level to WARNING suppresses DEBUG and INFO
- **Verified By**: [TC-SWE-055-1-1]

### 6.7 Wire Protocol (Linux Side)

**[SWE-035-3] Wire Frame Encoding (Linux)**
- **Description**: The Linux tcp_transport module shall encode outgoing messages with wire framing: 4-byte LE length + 1-byte message type + protobuf payload
- **Type**: Functional
- **Safety**: QM
- **Source**: [SYS-035]
- **Component**: LINUX / tcp_transport
- **Acceptance Criteria**:
  - Framing matches MCU expectations
- **Verified By**: [TC-SWE-035-3-1]

**[SWE-035-4] Wire Frame Decoding (Linux)**
- **Description**: The Linux tcp_transport module shall decode incoming wire frames, buffering partial frames and dispatching complete messages to the appropriate handler based on message type byte
- **Type**: Functional
- **Safety**: QM
- **Source**: [SYS-035]
- **Component**: LINUX / tcp_transport
- **Acceptance Criteria**:
  - Correct message dispatch for all type IDs
  - Partial frames handled without data loss
- **Verified By**: [TC-SWE-035-4-1]

**[SWE-036-2] Sequence Number Tracking (Linux)**
- **Description**: The Linux sensor_proxy and health_monitor shall track incoming sequence numbers and log warnings for any gaps (missing numbers) or duplicates
- **Type**: Functional
- **Safety**: QM
- **Source**: [SYS-036]
- **Component**: LINUX / sensor_proxy, LINUX / health_monitor
- **Acceptance Criteria**:
  - Gap of 1+ numbers logged as warning
  - Duplicate number logged as warning
- **Verified By**: [TC-SWE-036-2-1]

### 6.8 Configuration (Linux Side)

**[SWE-058-2] Configuration Command Encoding (Linux)**
- **Description**: The Linux config_manager shall encode ConfigUpdate protobuf messages and send via TCP port 5000 when configuration changes are requested (via diagnostic interface or programmatic API)
- **Type**: Functional
- **Safety**: QM
- **Source**: [SYS-058]
- **Component**: LINUX / config_manager
- **Acceptance Criteria**:
  - ConfigUpdate sent with correct field values
  - ConfigResponse received and status checked
- **Verified By**: [TC-SWE-058-2-1]

**[SWE-061-1] Configuration Read-Back (Linux)**
- **Description**: The Linux config_manager shall support querying current MCU configuration by sending DiagnosticRequest with DIAG_GET_CONFIG command and caching the response
- **Type**: Functional
- **Safety**: QM
- **Source**: [SYS-061]
- **Component**: LINUX / config_manager
- **Acceptance Criteria**:
  - Config request sent, response received
  - Cached config matches MCU state
- **Verified By**: [TC-SWE-061-1-1]

### 6.9 Linux Boot & Initialization

**[SWE-066-1] Gateway Startup Sequence**
- **Description**: The Linux gateway_core shall execute startup in order: (1) initialize logger, (2) open TCP server ports (5001, 5002), (3) wait for MCU USB enumeration, (4) connect to MCU TCP port 5000, (5) request state sync, (6) enter main event loop
- **Type**: Functional
- **Safety**: QM
- **Source**: [SYS-066]
- **Component**: LINUX / gateway_core
- **Acceptance Criteria**:
  - All ports listening within 30 seconds of boot
  - State sync complete before processing commands
- **Verified By**: [TC-SWE-066-1-1]

### 6.10 MCU Diagnostic Pass-Through

**[SWE-057-1] MCU Version Query**
- **Description**: The Linux gateway_core shall query MCU firmware version on startup by sending DiagnosticRequest with DIAG_GET_VERSION command and caching the response
- **Type**: Functional
- **Safety**: QM
- **Source**: [SYS-057]
- **Component**: LINUX / gateway_core
- **Acceptance Criteria**:
  - MCU version string cached after startup
  - Version available via GET_VERSION diagnostic command
- **Verified By**: [TC-SWE-057-1-1]

---

## 7. Shared — TCP Reconnection

**[SWE-043-2] MCU TCP Reconnection**
- **Description**: The MCU tcp_stack shall attempt reconnection to Linux telemetry port (5001) on connection loss with exponential backoff: initial 100 ms, max 5 seconds
- **Type**: Functional
- **Safety**: QM
- **Source**: [SYS-043]
- **Component**: MCU / tcp_stack
- **Acceptance Criteria**:
  - Reconnection attempted after TCP RST or timeout
  - Backoff timing correct
- **Verified By**: [TC-SWE-043-2-1]

---

## 8. MCU Boot Sequence

**[SWE-067-1] MCU Initialization Sequence**
- **Description**: The MCU main_loop shall execute startup in order: (1) HAL init (clocks, GPIO), (2) set all PWM to safe state (0%), (3) init watchdog (2s timeout), (4) load config from flash, (5) init USB CDC-ECM, (6) init TCP stack, (7) start sensor timers, (8) enter super-loop
- **Type**: Functional
- **Safety**: ASIL-B
- **Source**: [SYS-067], [SYS-027]
- **Component**: MCU / main_loop
- **Acceptance Criteria**:
  - Safe state established before comms enabled
  - USB enumeration within 5 seconds
  - Watchdog active before entering main loop
- **Verified By**: [TC-SWE-067-1-1, TC-SWE-067-1-2]

---

## 9. Requirement Summary

| Category | IDs | Count | Safety (ASIL-B) |
|----------|-----|-------|:---:|
| MCU Sensor Acquisition | SWE-001-1/2/3, SWE-002-1, SWE-003-1, SWE-004-1, SWE-005-1, SWE-007-1/2, SWE-008-1, SWE-009-1, SWE-010-1, SWE-011-1 | 13 | 0 |
| MCU PWM / Actuator | SWE-016-1/2, SWE-017-1, SWE-018-1, SWE-019-1, SWE-021-1, SWE-022-1/2, SWE-023-1, SWE-024-1 | 10 | 10 |
| MCU Fail-Safe | SWE-026-1, SWE-027-1, SWE-028-1, SWE-029-1 | 4 | 4 |
| MCU Protocol / TCP | SWE-030-1, SWE-031-1, SWE-032-1, SWE-033-1, SWE-035-1/2, SWE-036-1, SWE-037-1 | 8 | 0 |
| MCU Health Reporter | SWE-038-1, SWE-039-1, SWE-040-1, SWE-041-1 | 4 | 3 |
| MCU Config Store | SWE-058-1, SWE-059-1, SWE-060-1, SWE-062-1, SWE-063-1, SWE-064-1, SWE-065-1 | 7 | 2 |
| MCU Watchdog / Memory | SWE-069-1, SWE-070-1, SWE-071-1, SWE-072-1, SWE-073-1 | 5 | 3 |
| MCU Boot | SWE-067-1 | 1 | 1 |
| Linux Gateway Core | SWE-046-1, SWE-066-1 | 2 | 0 |
| Linux Sensor Proxy | SWE-013-1/2, SWE-015-1 | 3 | 0 |
| Linux Actuator Proxy | SWE-018-2, SWE-019-2 | 2 | 2 |
| Linux Health / Recovery | SWE-042-1, SWE-043-1/2, SWE-044-1, SWE-045-1 | 5 | 0 |
| Linux Diagnostics | SWE-046-2, SWE-047-1, SWE-048-1, SWE-049-1, SWE-050-1, SWE-051-1, SWE-057-1 | 7 | 0 |
| Linux Logging | SWE-052-1, SWE-053-1, SWE-054-1, SWE-055-1 | 4 | 0 |
| Linux Wire Protocol | SWE-035-3/4, SWE-036-2 | 3 | 0 |
| Linux Config | SWE-058-2, SWE-061-1 | 2 | 0 |
| **TOTAL** | | **80** | **25** |

---

## 10. Diagnostic Command Quick Reference

**All commands are lowercase only and case-sensitive.**

| Command | Description | Example |
|---------|-------------|---------|
| `status` | Show system state | `status` |
| `sensor read <ch>` | Read sensor channel 0-3 | `sensor read 0` |
| `actuator set <id> <val>` | Set actuator 0-1 to value % | `actuator set 0 50` |
| `version` | Show Linux and MCU versions | `version` |
| `log [count]` | Show last N log entries | `log 10` |
| `config get` | Show current config | `config get` |
| `config set <key> <val>` | Set config value | `config set channel0_rate 50` |
| `reset mcu` | Trigger MCU reset | `reset mcu` |
| `help` | Show command list | `help` |

**Invalid Examples** (these will fail):
- `STATUS` — uppercase not recognized
- `GET_STATUS` — underscore format not recognized
- `Sensor Read 0` — mixed case not recognized

---

## 11. Traceability

Forward traceability to architecture components is maintained in `docs/traceability/swe_to_arch_trace.md`.
Backward traceability to system requirements is maintained in `docs/traceability/sys_to_swe_trace.md`.
