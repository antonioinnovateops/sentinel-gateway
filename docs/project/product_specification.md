---
title: "Product Specification — Sentinel Gateway"
document_id: "PROD-SPEC-001"
version: "1.0.0"
date: 2026-03-17
status: "Approved"
author: "Antonio Stepien"
classification: "Internal"
---

# Product Specification — Sentinel Gateway

## 1. Product Vision

Sentinel Gateway is a dual-processor embedded system designed for industrial monitoring and control applications. It combines a Linux-based application processor for high-level data processing, connectivity, and diagnostics with a real-time microcontroller for deterministic sensor acquisition and actuator control.

The system demonstrates a modern embedded architecture pattern: **Linux SoC + MCU peripheral** connected via USB Ethernet, using Protocol Buffers for structured inter-processor communication.

## 2. Target Market

- Industrial IoT gateways
- Building automation controllers
- Agricultural monitoring systems
- Environmental sensor aggregation platforms
- Any application requiring high-level processing + real-time I/O

## 3. System Overview

### 3.1 Hardware Architecture (Emulated)

| Component | Specification | Emulation |
|-----------|--------------|-----------|
| Main CPU | ARM Cortex-A53 (64-bit) | QEMU `virt` machine, aarch64 |
| MCU | STM32U575 (Cortex-M33) | QEMU `netduinoplus2` or custom |
| RAM (CPU) | 512 MB DDR4 | QEMU memory |
| Flash (MCU) | 2 MB internal | QEMU pflash |
| RAM (MCU) | 786 KB SRAM | QEMU memory |
| Interconnect | USB 2.0 Full Speed | QEMU virtual USB + TAP networking |
| USB Mode | CDC-ECM (Ethernet over USB) | Virtual network bridge |

### 3.2 Software Architecture

**Linux Side (Main CPU)**:
- Yocto-based minimal Linux (Poky, `core-image-minimal` + custom layers)
- Custom application layer: Gateway Core daemon
- Protobuf-based messaging over TCP/IP
- Health monitoring with automatic MCU recovery
- Diagnostic shell (UDS-inspired command interface)

**MCU Side (STM32U575)**:
- Bare-metal C firmware (no RTOS for simplicity; FreeRTOS as stretch goal)
- Sensor acquisition: 4x ADC channels (temperature, humidity, pressure, light)
- Actuator control: 2x PWM outputs (fan speed, valve position)
- Protobuf message encoding/decoding (nanopb — lightweight protobuf for C)
- USB CDC-ECM device mode for TCP/IP connectivity
- Hardware watchdog with configurable timeout

### 3.3 Communication Protocol

**Transport**: TCP/IP over USB CDC-ECM
- MCU acts as USB device (CDC-ECM gadget)
- Linux host enumerates USB device, creates `usb0` network interface
- Static IP assignment: Linux = 192.168.7.1, MCU = 192.168.7.2
- TCP port 5000 for command/response, TCP port 5001 for streaming telemetry

**Application Protocol**: Protocol Buffers v3
- Defined in `docs/design/interface_specs/proto/sentinel.proto`
- Message types: SensorData, ActuatorCommand, HealthStatus, DiagnosticRequest/Response
- Versioned schema with backward compatibility

## 4. Functional Requirements (High-Level)

### 4.1 Sensor Data Acquisition
- MCU shall sample 4 ADC channels at configurable rates (1 Hz to 100 Hz)
- MCU shall package sensor readings as protobuf `SensorData` messages
- MCU shall transmit sensor data to Linux gateway via TCP port 5001
- Linux gateway shall receive, validate, and log sensor data

### 4.2 Actuator Control
- Linux gateway shall send `ActuatorCommand` messages to MCU via TCP port 5000
- MCU shall validate commands (range check, rate limiting)
- MCU shall apply PWM output within 10ms of command receipt
- MCU shall acknowledge command execution with status response

### 4.3 Health Monitoring
- MCU shall send `HealthStatus` messages every 1 second
- Linux gateway shall detect MCU communication loss within 3 seconds
- Linux gateway shall attempt MCU reset via USB power cycle on communication loss
- MCU hardware watchdog shall reset MCU if firmware hangs (timeout: 2 seconds)

### 4.4 Diagnostics
- Linux gateway shall provide a diagnostic command interface (TCP port 5002)
- Supported commands: read sensor, set actuator, get status, get firmware version, reset MCU
- All diagnostic commands shall be logged with timestamps

### 4.5 Configuration
- Sensor sample rates shall be configurable via protobuf `ConfigUpdate` messages
- Actuator limits shall be configurable (min/max PWM duty cycle)
- Configuration shall persist across MCU resets (stored in flash)

## 5. Non-Functional Requirements

### 5.1 Performance
- Sensor-to-log latency: ≤ 50ms (from ADC sample to Linux log entry)
- Actuator command latency: ≤ 20ms (from Linux send to PWM output change)
- Protobuf encode/decode: ≤ 1ms on MCU
- TCP throughput: ≥ 1 Mbps sustained

### 5.2 Reliability
- MCU firmware shall recover from any single fault via watchdog reset
- Linux gateway shall handle MCU disconnect/reconnect without restart
- No dynamic memory allocation on MCU (all static/stack allocation)
- Mean time between failures: ≥ 8,760 hours (1 year continuous operation)

### 5.3 Safety
- ASIL-B process rigor (for demonstration purposes)
- All safety-relevant functions shall have redundant checks
- Actuator outputs shall have configurable safety limits
- Watchdog failure shall force all actuators to safe state (0% output)

### 5.4 Security
- Protobuf messages shall include sequence numbers for replay detection
- Configuration changes shall require authentication token
- Firmware update capability (stretch goal)

### 5.5 Maintainability
- MISRA C:2012 compliance (Required + Advisory rules)
- Doxygen documentation for all public APIs
- ≥95% unit test statement coverage
- Modular architecture with clearly defined interfaces

## 6. Constraints

### 6.1 Technical Constraints
- **Language**: C11 (MCU and Linux application)
- **Build System**: CMake 3.20+
- **Protobuf**: nanopb for MCU, protobuf-c for Linux
- **Coding Standard**: MISRA C:2012
- **QEMU Version**: 8.0+ (for ARM virt machine and USB passthrough)
- **Yocto Release**: Scarthgap (4.0) or later

### 6.2 Process Constraints
- All work products follow ASPICE CL2 structure
- Full traceability: stakeholder → system → software → architecture → code → tests
- Every design decision documented in ADRs
- Human review gates between phases (simulated as self-review for AI execution)

## 7. Acceptance Criteria

The product is accepted when:

1. Both QEMU instances boot and establish USB Ethernet connectivity
2. Sensor data flows from MCU to Linux at configured sample rate
3. Actuator commands flow from Linux to MCU with ≤20ms latency
4. Health monitoring detects and recovers from simulated MCU hang
5. Diagnostic interface responds to all defined commands
6. All unit tests pass with ≥95% coverage
7. All integration tests pass in SIL environment
8. Complete traceability matrix with zero orphans
9. MISRA C:2012 compliance report with zero Required violations

## 8. Out of Scope

- Real hardware deployment (QEMU SIL only)
- Cloud connectivity (future enhancement)
- OTA firmware updates (stretch goal)
- RTOS on MCU (stretch goal — bare-metal first)
- GUI/HMI (command-line diagnostics only)
