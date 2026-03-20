---
title: "System Requirements Specification"
document_id: "SRS-001"
project: "Sentinel Gateway"
version: "1.0.0"
date: 2026-03-17
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

### 1.2 Scope

**In Scope**: Dual-processor embedded gateway (Linux + MCU) with sensor acquisition, actuator control, inter-processor communication, health monitoring, diagnostics, and configuration management — all operating within a QEMU SIL environment.

**Out of Scope**: Cloud connectivity, OTA updates, GUI/HMI, real hardware deployment.

### 1.3 Definitions and Acronyms

| Term | Definition |
|------|------------|
| MCU | Microcontroller Unit (STM32U575, Cortex-M33) |
| SoC | System on Chip (emulated ARM Cortex-A53) |
| CDC-ECM | Communication Device Class — Ethernet Control Model (USB networking) |
| SIL | Software-in-the-Loop (fully emulated execution) |
| Protobuf | Protocol Buffers — Google's binary serialization format |
| nanopb | Lightweight protobuf library for embedded C |
| QEMU | Quick EMUlator — open-source machine emulator |
| ADC | Analog-to-Digital Converter |
| PWM | Pulse Width Modulation |
| MTBF | Mean Time Between Failures |

### 1.4 Reference Documents

| Document ID | Title | Version |
|-------------|-------|---------|
| PROD-SPEC-001 | Product Specification | v1.0.0 |
| STKH-REQ-001 | Stakeholder Requirements | v1.0.0 |

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
| SYS-030 to SYS-037 | [STKH-007] Structured Communication | Protobuf protocol definition |
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
- **Acceptance**: ADC peripheral configured for 4 channels, verified by reading all 4

**[SYS-002] Temperature Sensor Input**
- **Description**: ADC Channel 0 shall measure temperature sensor voltage in the range 0.0V to 3.3V, corresponding to -40°C to +125°C
- **Type**: Functional
- **Safety**: QM
- **Source**: [STKH-001]
- **Acceptance**: ADC reading of 1.65V maps to 42.5°C ±0.5°C

**[SYS-003] Humidity Sensor Input**
- **Description**: ADC Channel 1 shall measure humidity sensor voltage in the range 0.0V to 3.3V, corresponding to 0% to 100% relative humidity
- **Type**: Functional
- **Safety**: QM
- **Source**: [STKH-001]
- **Acceptance**: ADC reading of 1.65V maps to 50% RH ±1%

**[SYS-004] Pressure Sensor Input**
- **Description**: ADC Channel 2 shall measure pressure sensor voltage in the range 0.5V to 4.5V, corresponding to 300 hPa to 1100 hPa
- **Type**: Functional
- **Safety**: QM
- **Source**: [STKH-001]
- **Acceptance**: ADC reading of 2.5V maps to 700 hPa ±5 hPa

**[SYS-005] Light Sensor Input**
- **Description**: ADC Channel 3 shall measure ambient light sensor voltage in the range 0.0V to 3.3V, corresponding to 0 to 100,000 lux
- **Type**: Functional
- **Safety**: QM
- **Source**: [STKH-001]
- **Acceptance**: ADC reading of 1.65V maps to 50,000 lux ±1,000 lux

**[SYS-006] ADC Resolution**
- **Description**: The MCU ADC shall operate with a minimum resolution of 12 bits (4096 discrete levels)
- **Type**: Functional
- **Safety**: QM
- **Source**: [STKH-001]
- **Acceptance**: Raw ADC values range from 0 to 4095

**[SYS-007] Sensor Data Packaging**
- **Description**: The MCU shall encode sensor readings into protobuf `SensorData` messages containing: channel ID, raw ADC value, calibrated value (engineering units), timestamp (microseconds since boot), and sequence number
- **Type**: Functional
- **Safety**: QM
- **Source**: [STKH-001]
- **Acceptance**: Protobuf decode on Linux side yields all fields with correct values

**[SYS-008] Sensor Data Transmission**
- **Description**: The MCU shall transmit `SensorData` messages to the Linux gateway via TCP port 5001 at the configured sample rate
- **Type**: Functional
- **Safety**: QM
- **Source**: [STKH-001]
- **Acceptance**: Linux receives messages at configured rate ±5%

### 3.2 Sensor Timing

**[SYS-009] Configurable Sample Rate**
- **Description**: The sensor sample rate shall be configurable from 1 Hz to 100 Hz in 1 Hz increments
- **Type**: Functional
- **Safety**: QM
- **Source**: [STKH-002]
- **Acceptance**: Sample rate verified at 1 Hz, 10 Hz, 50 Hz, and 100 Hz

**[SYS-010] Sample Timing Jitter**
- **Description**: The sensor sample timing jitter shall not exceed ±500 µs at any configured sample rate
- **Type**: Performance
- **Safety**: QM
- **Source**: [STKH-002]
- **Acceptance**: Statistical analysis of 1000 consecutive samples shows jitter ≤ 500 µs

**[SYS-011] ADC Conversion Time**
- **Description**: A single ADC conversion for all 4 channels shall complete within 100 µs
- **Type**: Performance
- **Safety**: QM
- **Source**: [STKH-002]
- **Acceptance**: GPIO toggle measurement around ADC scan shows ≤ 100 µs

**[SYS-012] Per-Channel Sample Rate**
- **Description**: Each ADC channel shall be independently configurable for sample rate
- **Type**: Functional
- **Safety**: QM
- **Source**: [STKH-002]
- **Acceptance**: Channel 0 at 100 Hz while Channel 3 at 1 Hz simultaneously

### 3.3 Data Logging

**[SYS-013] Linux-Side Data Logging**
- **Description**: The Linux gateway shall log all received `SensorData` messages to a structured log file (JSON lines format)
- **Type**: Functional
- **Safety**: QM
- **Source**: [STKH-003]
- **Acceptance**: Log file contains all received messages with no gaps in sequence numbers

**[SYS-014] Log Timestamp Resolution**
- **Description**: Log entries shall include timestamps with ≤ 1 ms resolution, synchronized to Linux system clock
- **Type**: Performance
- **Safety**: QM
- **Source**: [STKH-003]
- **Acceptance**: Log timestamps verified against system clock

**[SYS-015] Log Rotation**
- **Description**: The Linux gateway shall implement log rotation to prevent storage exhaustion (max 100 MB per log file, max 10 rotated files)
- **Type**: Functional
- **Safety**: QM
- **Source**: [STKH-003]
- **Acceptance**: Logs rotate at 100 MB boundary, oldest file deleted when 10 files exist

### 3.4 Actuator Control

**[SYS-016] Fan Speed Control**
- **Description**: The system shall control a fan via PWM output on MCU GPIO, with duty cycle range 0% (off) to 100% (full speed)
- **Type**: Functional
- **Safety**: ASIL-B
- **Source**: [STKH-004]
- **Acceptance**: PWM duty cycle matches commanded value within ±1%

**[SYS-017] Valve Position Control**
- **Description**: The system shall control a valve via PWM output on MCU GPIO, with duty cycle range 0% (fully closed) to 100% (fully open)
- **Type**: Functional
- **Safety**: ASIL-B
- **Source**: [STKH-004]
- **Acceptance**: PWM duty cycle matches commanded value within ±1%

**[SYS-018] Actuator Command Message**
- **Description**: The Linux gateway shall send `ActuatorCommand` protobuf messages to the MCU via TCP port 5000, containing: actuator ID, target value (percent), command timestamp, and sequence number
- **Type**: Functional
- **Safety**: ASIL-B
- **Source**: [STKH-004]
- **Acceptance**: MCU receives and decodes command correctly

**[SYS-019] Actuator Command Acknowledgment**
- **Description**: The MCU shall respond to each `ActuatorCommand` with an `ActuatorResponse` containing: actuator ID, applied value, status (OK/ERROR), and error code if applicable
- **Type**: Functional
- **Safety**: ASIL-B
- **Source**: [STKH-004]
- **Acceptance**: Linux receives acknowledgment for every command sent

**[SYS-020] Actuator Command Latency**
- **Description**: The time from Linux sending an `ActuatorCommand` to PWM output change on MCU shall not exceed 20 ms
- **Type**: Performance
- **Safety**: ASIL-B
- **Source**: [STKH-004]
- **Acceptance**: Measured latency ≤ 20 ms for 99th percentile over 1000 commands

**[SYS-021] Actuator Command Rate Limiting**
- **Description**: The MCU shall accept actuator commands at a maximum rate of 50 commands per second per actuator; excess commands shall be rejected with rate-limit error
- **Type**: Functional
- **Safety**: ASIL-B
- **Source**: [STKH-004]
- **Acceptance**: 51st command within 1 second is rejected

### 3.5 Actuator Safety

**[SYS-022] Actuator Range Validation**
- **Description**: The MCU shall validate that actuator command values are within configured min/max range before applying
- **Type**: Safety
- **Safety**: ASIL-B
- **Source**: [STKH-005]
- **Acceptance**: Command with value 101% is rejected, value -1% is rejected

**[SYS-023] Fan Safety Limits**
- **Description**: Fan PWM duty cycle shall be limited to a configurable maximum (default: 95%) and minimum (default: 0%)
- **Type**: Safety
- **Safety**: ASIL-B
- **Source**: [STKH-005]
- **Acceptance**: Command for 100% duty cycle when max=95% is clamped to 95%

**[SYS-024] Valve Safety Limits**
- **Description**: Valve PWM duty cycle shall be limited to a configurable maximum (default: 100%) and minimum (default: 0%)
- **Type**: Safety
- **Safety**: ASIL-B
- **Source**: [STKH-005]
- **Acceptance**: Limits enforced at boundary values

**[SYS-025] Safety Limit Configuration Protection**
- **Description**: Safety limit configuration changes shall require authentication and be logged
- **Type**: Safety
- **Safety**: ASIL-B
- **Source**: [STKH-005]
- **Acceptance**: Unauthenticated limit change is rejected

### 3.6 Actuator Fail-Safe

**[SYS-026] Communication Loss Fail-Safe**
- **Description**: If the MCU detects communication loss with the Linux gateway for ≥ 3 seconds, all actuator outputs shall transition to safe state (0% duty cycle)
- **Type**: Safety
- **Safety**: ASIL-B
- **Source**: [STKH-006]
- **Acceptance**: Actuators at 0% within 3.5 seconds of communication loss

**[SYS-027] Watchdog Fail-Safe**
- **Description**: On watchdog reset, the MCU shall initialize all actuator outputs to safe state (0% duty cycle) before resuming normal operation
- **Type**: Safety
- **Safety**: ASIL-B
- **Source**: [STKH-006]
- **Acceptance**: PWM outputs at 0% after watchdog reset, verified by oscilloscope (emulated)

**[SYS-028] Error State Fail-Safe**
- **Description**: On any error condition that prevents safe actuator control (protobuf decode failure, invalid sensor data affecting closed-loop control), actuators shall transition to safe state
- **Type**: Safety
- **Safety**: ASIL-B
- **Source**: [STKH-006]
- **Acceptance**: Injected protobuf corruption triggers safe state within 100 ms

**[SYS-029] Fail-Safe State Reporting**
- **Description**: When actuators enter fail-safe state, the MCU shall report the event and cause via `HealthStatus` message with `FAILSAFE` status
- **Type**: Functional
- **Safety**: ASIL-B
- **Source**: [STKH-006]
- **Acceptance**: Health status message received on Linux with correct cause code

### 3.7 Inter-Processor Communication

**[SYS-030] USB CDC-ECM Link**
- **Description**: The MCU shall implement USB CDC-ECM device class to provide Ethernet-over-USB connectivity to the Linux host
- **Type**: Functional
- **Safety**: QM
- **Source**: [STKH-007]
- **Acceptance**: Linux enumerates `usb0` network interface on MCU connection

**[SYS-031] Static IP Configuration**
- **Description**: IP addresses shall be statically assigned: Linux host = 192.168.7.1/24, MCU device = 192.168.7.2/24
- **Type**: Functional
- **Safety**: QM
- **Source**: [STKH-007]
- **Acceptance**: `ping 192.168.7.2` from Linux succeeds

**[SYS-032] TCP Command Port**
- **Description**: The MCU shall listen on TCP port 5000 for command/response messages (ActuatorCommand, ConfigUpdate, DiagnosticRequest)
- **Type**: Functional
- **Safety**: QM
- **Source**: [STKH-007]
- **Acceptance**: TCP connection established on port 5000

**[SYS-033] TCP Telemetry Port**
- **Description**: The MCU shall connect to Linux TCP port 5001 to stream SensorData and HealthStatus messages
- **Type**: Functional
- **Safety**: QM
- **Source**: [STKH-007]
- **Acceptance**: Telemetry data received on port 5001

**[SYS-034] Protobuf Message Format**
- **Description**: All inter-processor messages shall use Protocol Buffers v3 encoding as defined in `docs/design/interface_specs/proto/sentinel.proto`
- **Type**: Functional
- **Safety**: QM
- **Source**: [STKH-007]
- **Acceptance**: Messages decoded successfully by both protobuf-c (Linux) and nanopb (MCU)

**[SYS-035] Message Framing**
- **Description**: Each protobuf message on TCP shall be prefixed with a 4-byte little-endian length field followed by a 1-byte message type identifier
- **Type**: Functional
- **Safety**: QM
- **Source**: [STKH-007]
- **Acceptance**: Multi-message stream correctly demultiplexed

**[SYS-036] Message Sequence Numbers**
- **Description**: All messages shall include a monotonically increasing 32-bit sequence number, wrapping at UINT32_MAX
- **Type**: Functional
- **Safety**: QM
- **Source**: [STKH-007]
- **Acceptance**: Sequence gaps detected and logged by receiver

**[SYS-037] Protocol Version Field**
- **Description**: All messages shall include a protocol version field (major.minor) for backward compatibility detection
- **Type**: Functional
- **Safety**: QM
- **Source**: [STKH-007]
- **Acceptance**: Version mismatch logged as warning, compatible versions accepted

### 3.8 Communication Health Monitoring

**[SYS-038] MCU Heartbeat**
- **Description**: The MCU shall send a `HealthStatus` heartbeat message every 1000 ms (±50 ms) via the telemetry port
- **Type**: Functional
- **Safety**: ASIL-B
- **Source**: [STKH-008]
- **Acceptance**: Heartbeat received at 1 Hz ±5%

**[SYS-039] Linux Heartbeat Monitoring**
- **Description**: The Linux gateway shall monitor MCU heartbeat messages and declare communication loss if no heartbeat is received for 3 consecutive intervals (3 seconds)
- **Type**: Functional
- **Safety**: ASIL-B
- **Source**: [STKH-008]
- **Acceptance**: Communication loss event raised within 3.5 seconds of MCU silence

**[SYS-040] MCU Gateway Monitoring**
- **Description**: The MCU shall monitor for Linux command/response activity and declare communication loss if no message is received for 5 seconds
- **Type**: Functional
- **Safety**: ASIL-B
- **Source**: [STKH-008]
- **Acceptance**: MCU enters fail-safe within 5 seconds of Linux silence

**[SYS-041] Communication Status Reporting**
- **Description**: Both Linux and MCU shall maintain and report communication link status: CONNECTED, DEGRADED (packet loss > 5%), DISCONNECTED
- **Type**: Functional
- **Safety**: QM
- **Source**: [STKH-008]
- **Acceptance**: Status transitions logged with timestamps

### 3.9 Automatic Recovery

**[SYS-042] USB Power Cycle Recovery**
- **Description**: On MCU communication loss, the Linux gateway shall attempt recovery by toggling USB power (simulated via QEMU monitor) after 5 seconds
- **Type**: Functional
- **Safety**: QM
- **Source**: [STKH-009]
- **Acceptance**: MCU re-enumerates and communication resumes

**[SYS-043] TCP Reconnection**
- **Description**: On TCP connection drop, both sides shall attempt reconnection with exponential backoff (initial: 100 ms, max: 5 seconds)
- **Type**: Functional
- **Safety**: QM
- **Source**: [STKH-009]
- **Acceptance**: Connection re-established within 10 seconds after transient failure

**[SYS-044] State Resynchronization**
- **Description**: After communication recovery, the Linux gateway shall request full state from MCU (current sensor values, actuator states, configuration) before resuming normal operation
- **Type**: Functional
- **Safety**: QM
- **Source**: [STKH-009]
- **Acceptance**: State sync message exchange completes within 2 seconds

**[SYS-045] Recovery Attempt Limit**
- **Description**: The Linux gateway shall attempt a maximum of 3 recovery cycles before entering permanent fault state requiring human intervention
- **Type**: Functional
- **Safety**: QM
- **Source**: [STKH-009]
- **Acceptance**: After 3 failed recoveries, system logs permanent fault and ceases recovery attempts

### 3.10 Diagnostics

**[SYS-046] Diagnostic TCP Interface**
- **Description**: The Linux gateway shall provide a diagnostic command interface on TCP port 5002, accepting plain-text commands and returning plain-text responses
- **Type**: Functional
- **Safety**: QM
- **Source**: [STKH-010]
- **Acceptance**: `telnet 192.168.7.1 5002` connects and accepts commands

**[SYS-047] Read Sensor Command**
- **Description**: The diagnostic interface shall support `sensor read <channel_id>` command, returning the latest calibrated value and timestamp
- **Type**: Functional
- **Safety**: QM
- **Source**: [STKH-010]
- **Acceptance**: `sensor read 0` returns temperature value
- **Note**: Lowercase command syntax provides better interactive UX (telnet-friendly)

**[SYS-048] Set Actuator Command**
- **Description**: The diagnostic interface shall support `actuator set <actuator_id> <value>` command, applying the value and returning confirmation
- **Type**: Functional
- **Safety**: QM
- **Source**: [STKH-010]
- **Acceptance**: `actuator set 0 50` sets fan to 50% and returns OK

**[SYS-049] Get Status Command**
- **Description**: The diagnostic interface shall support `status` command, returning system status including: communication state, MCU health, all sensor values, all actuator states
- **Type**: Functional
- **Safety**: QM
- **Source**: [STKH-010]
- **Acceptance**: Complete status dump returned as structured text

**[SYS-050] Get Version Command**
- **Description**: The diagnostic interface shall support `version` command, returning Linux and MCU firmware versions
- **Type**: Functional
- **Safety**: QM
- **Source**: [STKH-010], [STKH-012]
- **Acceptance**: Both version strings returned

**[SYS-051] Reset MCU Command**
- **Description**: The diagnostic interface shall support `RESET_MCU` command, triggering a USB power cycle to reset the MCU
- **Type**: Functional
- **Safety**: QM
- **Source**: [STKH-010]
- **Acceptance**: MCU resets and re-establishes communication

### 3.11 Event Logging

**[SYS-052] System Event Log**
- **Description**: The Linux gateway shall maintain a system event log recording: sensor data, actuator commands, communication events, errors, state changes, diagnostic commands
- **Type**: Functional
- **Safety**: QM
- **Source**: [STKH-011]
- **Acceptance**: All event types present in log after exercising all features

**[SYS-053] Log Persistence**
- **Description**: System logs shall persist across Linux gateway reboots (stored on persistent filesystem)
- **Type**: Functional
- **Safety**: QM
- **Source**: [STKH-011]
- **Acceptance**: Logs from previous boot session available after reboot

**[SYS-054] Log Retrieval**
- **Description**: The diagnostic interface shall support `GET_LOG [count]` command, returning the most recent `count` log entries (default: 50)
- **Type**: Functional
- **Safety**: QM
- **Source**: [STKH-011]
- **Acceptance**: `GET_LOG 10` returns 10 most recent entries

**[SYS-055] Error Event Priority**
- **Description**: Error events shall be logged with severity levels: DEBUG, INFO, WARNING, ERROR, CRITICAL
- **Type**: Functional
- **Safety**: QM
- **Source**: [STKH-011]
- **Acceptance**: Each severity level represented in log

### 3.12 Firmware Version

**[SYS-056] Linux Firmware Version**
- **Description**: The Linux gateway shall report its firmware version in format `<major>.<minor>.<patch>-<git_hash>` (e.g., `1.0.0-a3b4c5d`)
- **Type**: Functional
- **Safety**: QM
- **Source**: [STKH-012]
- **Acceptance**: Version string matches build metadata

**[SYS-057] MCU Firmware Version**
- **Description**: The MCU shall report its firmware version in the same format, queryable via protobuf `DiagnosticRequest` message
- **Type**: Functional
- **Safety**: QM
- **Source**: [STKH-012]
- **Acceptance**: Version string matches MCU build metadata

### 3.13 Configuration Management

**[SYS-058] Sensor Rate Configuration**
- **Description**: Sensor sample rates (per channel) shall be configurable via protobuf `ConfigUpdate` message from Linux to MCU
- **Type**: Functional
- **Safety**: QM
- **Source**: [STKH-013]
- **Acceptance**: Sample rate changes within 1 second of ConfigUpdate receipt

**[SYS-059] Actuator Limit Configuration**
- **Description**: Actuator safety limits (min/max duty cycle per actuator) shall be configurable via `ConfigUpdate` message
- **Type**: Safety
- **Safety**: ASIL-B
- **Source**: [STKH-013]
- **Acceptance**: New limits enforced on next actuator command

**[SYS-060] Configuration Persistence**
- **Description**: MCU configuration shall be stored in non-volatile memory (flash) and survive power cycles and watchdog resets
- **Type**: Functional
- **Safety**: QM
- **Source**: [STKH-013]
- **Acceptance**: Configuration preserved after MCU reset

**[SYS-061] Configuration Read-Back**
- **Description**: The current MCU configuration shall be queryable via `DiagnosticRequest` message with `GET_CONFIG` command
- **Type**: Functional
- **Safety**: QM
- **Source**: [STKH-013]
- **Acceptance**: Returned config matches previously set values

**[SYS-062] Default Configuration**
- **Description**: On first boot or configuration corruption, the MCU shall use factory default configuration: all channels at 10 Hz, fan max 95%, valve max 100%
- **Type**: Functional
- **Safety**: QM
- **Source**: [STKH-013]
- **Acceptance**: Default values applied on factory-fresh firmware

### 3.14 Configuration Validation

**[SYS-063] Sample Rate Validation**
- **Description**: The MCU shall reject sample rate values outside 1-100 Hz range with error response
- **Type**: Functional
- **Safety**: QM
- **Source**: [STKH-014]
- **Acceptance**: Rate of 0 Hz or 101 Hz rejected

**[SYS-064] Actuator Limit Validation**
- **Description**: The MCU shall reject actuator limit values outside 0-100% range with error response
- **Type**: Safety
- **Safety**: ASIL-B
- **Source**: [STKH-014]
- **Acceptance**: Limit of -1% or 101% rejected

**[SYS-065] Configuration CRC**
- **Description**: Stored configuration shall include a CRC-32 checksum; configuration with invalid CRC shall be replaced with defaults
- **Type**: Functional
- **Safety**: QM
- **Source**: [STKH-014]
- **Acceptance**: Corrupted flash data triggers default configuration load

---

## 4. Non-Functional Requirements

### 4.1 Performance

**[SYS-066] System Boot Time**
- **Description**: The Linux gateway shall be ready to accept MCU connections within 30 seconds of power-on (QEMU start)
- **Type**: Performance
- **Safety**: QM
- **Source**: [STKH-015]
- **Acceptance**: TCP port 5001 listening within 30 seconds

**[SYS-067] MCU Boot Time**
- **Description**: The MCU shall complete initialization and establish USB CDC-ECM link within 5 seconds of power-on
- **Type**: Performance
- **Safety**: QM
- **Source**: [STKH-015]
- **Acceptance**: USB enumeration complete within 5 seconds

**[SYS-068] TCP Throughput**
- **Description**: The TCP link between Linux and MCU shall sustain ≥ 1 Mbps throughput
- **Type**: Performance
- **Safety**: QM
- **Source**: [STKH-015]
- **Acceptance**: iperf3 measurement ≥ 1 Mbps

### 4.2 Reliability

**[SYS-069] Hardware Watchdog**
- **Description**: The MCU shall configure and feed the hardware watchdog timer with a 2-second timeout
- **Type**: Safety
- **Safety**: ASIL-B
- **Source**: [STKH-016]
- **Acceptance**: Deliberate watchdog starvation causes MCU reset within 2 seconds

**[SYS-070] Watchdog Feed Frequency**
- **Description**: The MCU main loop shall feed the watchdog at least every 500 ms under normal operation
- **Type**: Safety
- **Safety**: ASIL-B
- **Source**: [STKH-016]
- **Acceptance**: Watchdog feed interval measured ≤ 500 ms

**[SYS-071] Watchdog Reset Counter**
- **Description**: The MCU shall maintain a persistent counter of watchdog resets, reportable via HealthStatus
- **Type**: Functional
- **Safety**: QM
- **Source**: [STKH-016]
- **Acceptance**: Counter increments on each watchdog reset

### 4.3 Memory Constraints

**[SYS-072] Static Memory Allocation (MCU)**
- **Description**: The MCU firmware shall use only static memory allocation (no malloc, calloc, realloc, or free)
- **Type**: Constraint
- **Safety**: ASIL-B
- **Source**: [STKH-017]
- **Acceptance**: Static analysis confirms zero heap function calls

**[SYS-073] Stack Usage Limit (MCU)**
- **Description**: The MCU firmware total stack usage shall not exceed 32 KB
- **Type**: Constraint
- **Safety**: QM
- **Source**: [STKH-017]
- **Acceptance**: Stack analysis tool reports ≤ 32 KB worst-case usage

---

## 5. Process Requirements

**[SYS-074] Work Product Completeness**
- **Description**: All ASPICE CL2 work products for processes SYS.2, SYS.3, SWE.1-SWE.6, SUP.2, SUP.8 shall be generated
- **Type**: Process
- **Safety**: N/A
- **Source**: [STKH-018]
- **Acceptance**: Checklist of all work products with document references

**[SYS-075] Review Records**
- **Description**: Every work product shall have a review record documenting reviewer, date, findings, and disposition
- **Type**: Process
- **Safety**: N/A
- **Source**: [STKH-018]
- **Acceptance**: Review records exist for all documents

**[SYS-076] Configuration Management**
- **Description**: All artifacts shall be version-controlled in Git with meaningful commit messages referencing requirement or task IDs
- **Type**: Process
- **Safety**: N/A
- **Source**: [STKH-018]
- **Acceptance**: Git log shows structured commit messages

**[SYS-077] Bidirectional Traceability**
- **Description**: Every system requirement shall trace forward to software requirements, architecture, code, and tests; every test shall trace backward to requirements
- **Type**: Process
- **Safety**: N/A
- **Source**: [STKH-019]
- **Acceptance**: Traceability matrix with zero orphans in either direction

**[SYS-078] Traceability Matrix Automation**
- **Description**: The traceability matrix shall be automatically generated from tagged artifacts (requirements IDs, @implements, @verified_by)
- **Type**: Process
- **Safety**: N/A
- **Source**: [STKH-019]
- **Acceptance**: Script generates matrix from source files

**[SYS-079] MISRA C:2012 Required Rules**
- **Description**: All C source code shall have zero MISRA C:2012 Required rule violations
- **Type**: Process
- **Safety**: N/A
- **Source**: [STKH-020]
- **Acceptance**: Static analysis report shows 0 Required violations

**[SYS-080] MISRA C:2012 Advisory Rules**
- **Description**: All MISRA C:2012 Advisory rule deviations shall be documented with rationale
- **Type**: Process
- **Safety**: N/A
- **Source**: [STKH-020]
- **Acceptance**: Deviation records exist for all Advisory violations

**[SYS-081] SIL Developer Accessibility**
- **Description**: The project shall provide a single-command SIL test environment that enables any developer to execute the full integration test plan (ITP-001) against their implementation without manual setup, QEMU configuration, or hardware. The SIL environment shall map each automated test to its corresponding integration test scenario (IT-01 through IT-12) and system requirement, and shall produce a JUnit XML report for CI integration.
- **Type**: Process
- **Safety**: N/A
- **Source**: [STKH-016], [STKH-019]
- **Acceptance**: `docker-compose run --rm sil` executes all integration test scenarios, outputs per-test pass/fail with requirement traceability, and writes JUnit XML to `results/sil/`
- **Verified By**: Developer guide at `docs/guides/sil_developer_guide.md`

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
| Process | SYS-074 to SYS-081 | 8 | 0 |
| **TOTAL** | SYS-001 to SYS-081 | **81** | **22** |
