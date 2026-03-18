---
title: "Stakeholder Requirements"
document_id: "STKH-REQ-001"
version: "2.0.0"
date: 2026-03-18
status: "Approved"
author: "Antonio Stepien (Product Owner)"
aspice_process: "SYS.1 Stakeholder Requirements Elicitation"
---

# Stakeholder Requirements — Sentinel Gateway

## 1. Introduction

This document captures stakeholder needs and expectations for the Sentinel Gateway product. These requirements are the highest-level inputs to the development process and drive all downstream system and software requirements.

**Important Implementation Notes**: This document has been updated to reflect lessons learned from implementation. Each requirement includes concrete acceptance criteria that can be verified in the QEMU SIL environment.

## 2. Stakeholders

| ID | Stakeholder | Role | Concerns |
|----|-------------|------|----------|
| SH-01 | System Integrator | Deploys gateway in field | Reliability, ease of integration, diagnostics |
| SH-02 | Application Developer | Develops custom logic on Linux side | Clean APIs, documentation, extensibility |
| SH-03 | Maintenance Engineer | Services deployed units | Remote diagnostics, firmware updates, logging |
| SH-04 | Safety Assessor | Validates process compliance | Traceability, documentation, safety analysis |
| SH-05 | Sensor/Actuator Vendor | Provides peripheral devices | Interface compatibility, timing guarantees |

## 3. Stakeholder Requirements

### 3.1 Data Acquisition

**[STKH-001] Multi-Sensor Data Collection**
- **Description**: The system shall collect data from multiple environmental sensors simultaneously
- **Rationale**: Industrial monitoring requires concurrent measurement of temperature, humidity, pressure, and ambient light
- **Priority**: Must Have
- **Acceptance Criteria**:
  - ≥4 ADC channels (CH0-CH3) operational simultaneously
  - Configurable sample rates per channel (1-100 Hz)
  - All channels readable via `sensor read <ch>` diagnostic command
- **Implementation Notes**:
  - ADC driver: `src/mcu/hal/adc_driver.c` (`adc_scan_all()`)
  - Channel mapping defined in `src/common/sentinel_types.h` (`SENTINEL_MAX_CHANNELS = 4`)
  - Verification: `tests/unit/test_sensor_acquisition.c`

**[STKH-002] Real-Time Sensor Sampling**
- **Description**: Sensor data shall be sampled with deterministic timing (jitter ≤ 500 µs)
- **Rationale**: Process control applications require predictable measurement intervals
- **Priority**: Must Have
- **Acceptance Criteria**:
  - Demonstrated jitter ≤ 500 µs at 100 Hz sample rate
  - SysTick-based timer triggering (not software delays)
  - Timing verified via timestamp analysis in QEMU SIL
- **Implementation Notes**:
  - Timer driver: `src/mcu/hal/systick.c`
  - QEMU user-mode uses `usleep()` for timing approximation
  - Known limitation: QEMU timing is less precise than real hardware

**[STKH-003] Sensor Data Logging**
- **Description**: All sensor data shall be logged on the gateway with timestamps for later retrieval
- **Rationale**: Historical data analysis, trend monitoring, and regulatory compliance
- **Priority**: Must Have
- **Acceptance Criteria**:
  - Data logged with ≤1ms timestamp resolution
  - JSON Lines format at `/var/log/sentinel/sensor_data.jsonl`
  - Log rotation at 100 MB (max 10 files)
- **Implementation Notes**:
  - Logger: `src/linux/logger.c`
  - Timestamp uses `clock_gettime(CLOCK_REALTIME)`

### 3.2 Actuator Control

**[STKH-004] Remote Actuator Command**
- **Description**: The gateway shall accept commands to control physical actuators (fans, valves) connected to the MCU
- **Rationale**: Closed-loop control based on sensor readings
- **Priority**: Must Have
- **Acceptance Criteria**:
  - Commands applied within 20ms (measured via response timestamp)
  - 2 PWM channels: Fan (ID=0), Valve (ID=1)
  - Testable via `actuator set <id> <pct>` diagnostic command
- **Implementation Notes**:
  - PWM driver: `src/mcu/hal/pwm_driver.c` (`pwm_set_duty()`)
  - Actuator control: `src/mcu/actuator_control.c`
  - PWM resolution: 0-999 (0.1% steps) via `PWM_ARR_VALUE`

**[STKH-005] Actuator Safety Limits**
- **Description**: Actuator outputs shall be constrained to configurable safe operating ranges
- **Rationale**: Prevent equipment damage from erroneous commands
- **Priority**: Must Have
- **Acceptance Criteria**:
  - Out-of-range commands rejected with `SENTINEL_ERR_OUT_OF_RANGE`
  - Default limits: Fan 0-95%, Valve 0-100%
  - Limits configurable via ConfigUpdate message
- **Implementation Notes**:
  - Limit enforcement: `src/mcu/actuator_control.c` (`actuator_validate_range()`)
  - Config storage: `src/mcu/config_store.c`
  - Limits stored in flash at `FLASH_CONFIG_ADDR` (0x08030000)

**[STKH-006] Actuator Fail-Safe**
- **Description**: On any system fault, actuators shall transition to a defined safe state
- **Rationale**: Equipment protection in fault conditions
- **Priority**: Must Have
- **Acceptance Criteria**:
  - Safe state (0% duty cycle) reached within 2 seconds of fault detection
  - Triggers: communication loss, watchdog reset, protobuf decode error
  - Safe state verified via `pwm_set_all_zero()` call
- **Implementation Notes**:
  - Fail-safe handler: `src/mcu/actuator_control.c` (`actuator_enter_failsafe()`)
  - Communication timeout: 3 seconds (configurable via `SENTINEL_COMM_TIMEOUT_MS`)

### 3.3 Communication

**[STKH-007] Structured Inter-Processor Communication**
- **Description**: Data exchange between Linux and MCU shall use a versioned, type-safe protocol
- **Rationale**: Prevent data corruption, enable independent firmware updates
- **Priority**: Must Have
- **Acceptance Criteria**:
  - Wire frame format: `[4-byte LE length][1-byte msg_type][payload]`
  - Protocol version embedded in all messages (major=1, minor=0)
  - Backward-compatible message handling
- **Implementation Notes**:
  - Wire frame: `src/common/wire_frame.c` (shared between Linux and MCU)
  - Header size: 5 bytes (`WIRE_FRAME_HEADER_SIZE`)
  - Max payload: 507 bytes (`WIRE_FRAME_MAX_PAYLOAD`)
  - Message types defined in `src/common/sentinel_types.h` (`MSG_TYPE_*`)
  - **IMPORTANT**: Frame format does NOT include CRC - TCP provides integrity

**[STKH-008] Communication Loss Detection**
- **Description**: The system shall detect communication loss between Linux and MCU within 3 seconds
- **Rationale**: Enable automatic recovery before process variables drift
- **Priority**: Must Have
- **Acceptance Criteria**:
  - Loss detected via missed heartbeats (3 consecutive = 3 seconds)
  - Health status reports `comm_ok = false`
  - Testable by stopping MCU in QEMU
- **Implementation Notes**:
  - Linux heartbeat monitor: `src/linux/health_monitor.c`
  - MCU gateway monitor: `src/mcu/health_reporter.c`
  - Heartbeat interval: 1000ms (`SENTINEL_HEALTH_INTERVAL_MS`)

**[STKH-009] Automatic Recovery**
- **Description**: The system shall automatically attempt recovery from communication failures
- **Rationale**: Minimize downtime in unattended deployments
- **Priority**: Must Have
- **Acceptance Criteria**:
  - TCP reconnection with exponential backoff (100ms initial, 5s max)
  - State resynchronization after reconnection
  - Max 3 recovery attempts before entering FAULT state
- **Implementation Notes**:
  - Reconnection: `src/linux/tcp_transport.c`
  - State sync: `MSG_TYPE_STATE_SYNC_REQ/RSP` (0x40/0x41)

### 3.4 Diagnostics

**[STKH-010] Remote Diagnostic Access**
- **Description**: A maintenance engineer shall be able to query system status, read sensors, and control actuators remotely
- **Rationale**: Field troubleshooting without physical access
- **Priority**: Must Have
- **Acceptance Criteria**:
  - TCP port 5002 accepts text commands
  - Commands: `status`, `sensor read <ch>`, `actuator set <id> <pct>`, `version`, `help`
  - **IMPORTANT**: Commands are case-sensitive (lowercase only)
- **Implementation Notes**:
  - Diagnostics server: `src/linux/diagnostics.c` (`diagnostics_process_command()`)
  - Single client at a time (epoll design limitation)
  - Test: `echo "status" | nc localhost 5002`

**[STKH-011] Comprehensive Logging**
- **Description**: All system events (sensor readings, commands, errors, state changes) shall be logged with timestamps
- **Rationale**: Post-incident analysis and compliance auditing
- **Priority**: Must Have
- **Acceptance Criteria**:
  - Events logged to `/var/log/sentinel/events.jsonl`
  - Severity levels: DEBUG, INFO, WARNING, ERROR, CRITICAL
  - Logs persist across gateway reboots
- **Implementation Notes**:
  - Logger: `src/linux/logger.c`
  - Flush interval: 5 seconds or on shutdown (fsync)

**[STKH-012] Firmware Version Reporting**
- **Description**: Both Linux and MCU firmware versions shall be queryable remotely
- **Rationale**: Fleet management, update tracking
- **Priority**: Should Have
- **Acceptance Criteria**:
  - Version format: `<major>.<minor>.<patch>` (e.g., "1.0.0")
  - Queryable via `version` diagnostic command
- **Implementation Notes**:
  - Version defined at build time via CMake `project(VERSION x.y.z)`
  - Header: `config/version.h.in` → `build/generated/sentinel_version.h`
  - Runtime: `SENTINEL_VERSION` macro

### 3.5 Configuration

**[STKH-013] Runtime Configuration**
- **Description**: Sensor sample rates, actuator limits, and communication parameters shall be configurable without firmware rebuild
- **Rationale**: Adapt to different deployment scenarios
- **Priority**: Must Have
- **Acceptance Criteria**:
  - Configuration applied within 1 second
  - Persists across MCU resets (stored in flash)
  - Configurable via MSG_TYPE_CONFIG_UPDATE (0x20)
- **Implementation Notes**:
  - Config store: `src/mcu/config_store.c`
  - Flash sector: 4KB at 0x08030000 (`FLASH_CONFIG_ADDR`)
  - CRC-32 validation on load

**[STKH-014] Configuration Validation**
- **Description**: Invalid configuration values shall be rejected with clear error messages
- **Rationale**: Prevent misconfiguration in field
- **Priority**: Must Have
- **Acceptance Criteria**:
  - Sample rate: 1-100 Hz (reject outside range)
  - Actuator limits: 0-100% (reject outside range)
  - Error code returned: `SENTINEL_ERR_OUT_OF_RANGE`
- **Implementation Notes**:
  - Validation: `src/mcu/config_store.c` (`config_validate()`)

### 3.6 Reliability & Safety

**[STKH-015] Continuous Operation**
- **Description**: The system shall operate continuously for ≥1 year without human intervention
- **Rationale**: Remote/unattended deployment sites
- **Priority**: Must Have
- **Acceptance Criteria**:
  - MTBF ≥ 8,760 hours (demonstrated via FMEA analysis)
  - No memory leaks (static allocation only on MCU)
  - Log rotation prevents storage exhaustion
- **Implementation Notes**:
  - Static allocation enforced via `__HEAP_SIZE=0` (MCU)
  - Log rotation: 100MB × 10 files max

**[STKH-016] Watchdog Protection**
- **Description**: MCU firmware shall be protected against hangs by hardware watchdog
- **Rationale**: Recovery from firmware faults
- **Priority**: Must Have
- **Acceptance Criteria**:
  - Watchdog timeout: 2 seconds
  - Feed interval: ≤500ms (in main super-loop)
  - Reset counter persisted and reported in HealthStatus
- **Implementation Notes**:
  - Watchdog driver: `src/mcu/hal/watchdog_driver.c`
  - Manager: `src/mcu/watchdog_mgr.c` (`iwdg_feed()`)
  - QEMU: watchdog simulated, no-op in user-mode

**[STKH-017] No Dynamic Memory on MCU**
- **Description**: MCU firmware shall not use heap allocation (malloc/free)
- **Rationale**: Eliminate memory fragmentation and leak risks in long-running systems
- **Priority**: Must Have
- **Acceptance Criteria**:
  - Static analysis confirms zero heap function calls
  - Linker script has no `.heap` section
  - Build defines: `__HEAP_SIZE=0`, `_REENT_SMALL=1`
- **Implementation Notes**:
  - Enforced in `cmake/arm-none-eabi.cmake`
  - nanopb configured for buffer-only mode (`PB_BUFFER_ONLY=1`)

### 3.7 Development Process

**[STKH-018] ASPICE CL2 Compliance**
- **Description**: The development process shall follow ASPICE Capability Level 2 practices
- **Rationale**: Industry compliance, process maturity demonstration
- **Priority**: Must Have
- **Acceptance Criteria**:
  - All SYS, SWE, and SUP process work products complete
  - Review records for all documents
  - Configuration management via Git
- **Implementation Notes**:
  - Work products in `docs/` directory
  - Traceability matrices in `docs/traceability/`

**[STKH-019] Full Traceability**
- **Description**: Every requirement shall be traceable to architecture, code, and tests
- **Rationale**: ASPICE assessment readiness, impact analysis capability
- **Priority**: Must Have
- **Acceptance Criteria**:
  - Zero orphaned requirements in traceability matrix
  - Code files include `@implements` tags
  - Test files include `@verified_by` tags
- **Implementation Notes**:
  - Traceability extraction via `tools/trace_extractor.py`

**[STKH-020] MISRA C Compliance**
- **Description**: All C code shall comply with MISRA C:2012
- **Rationale**: Code quality, safety standard alignment
- **Priority**: Must Have
- **Acceptance Criteria**:
  - Zero Required rule violations
  - Documented Advisory rule deviations
  - cppcheck with MISRA addon in CI
- **Implementation Notes**:
  - Analysis: `docker/Dockerfile.analysis`
  - Deviation records: `docs/reviews/misra_compliance_plan.md`

## 4. Priority Summary

| Priority | Count | IDs |
|----------|-------|-----|
| Must Have | 19 | STKH-001 through STKH-020 (excl. STKH-012) |
| Should Have | 1 | STKH-012 |

## 5. Build Environment Critical Notes

Based on implementation lessons learned:

### 5.1 ARM Toolchain Installation

**DO NOT** download ARM GCC from `developer.arm.com` - the URL structure changes frequently.

**CORRECT** approach (Ubuntu 24.04):
```bash
apt-get install gcc-arm-none-eabi libnewlib-arm-none-eabi
```

This provides `arm-none-eabi-gcc` 13.x which is compatible with all project requirements.

### 5.2 Docker UID Collision

Ubuntu 24.04 ships with a user at UID 1000. If your Dockerfile does:
```dockerfile
RUN useradd -m builder
```

It may fail if UID 1000 is taken. Use this pattern:
```dockerfile
RUN useradd -m builder || true
```

### 5.3 Required C11 Headers

MCU bare-metal code requires explicit includes:
```c
#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include <string.h>
```

These are NOT automatically available in freestanding environments.

### 5.4 CMake Build Options

```bash
cmake .. \
  -DBUILD_LINUX=ON \   # Linux gateway
  -DBUILD_MCU=OFF \    # Bare-metal MCU (STM32)
  -DBUILD_QEMU_MCU=ON \ # MCU for QEMU user-mode
  -DBUILD_TESTS=ON \   # Unit tests (host)
  -DENABLE_COVERAGE=OFF
```

## 6. Network Configuration

| Property | Value | Environment Variable |
|----------|-------|---------------------|
| Linux IP | 192.168.7.1 | `SENTINEL_LINUX_HOST` |
| MCU IP | 192.168.7.2 | `SENTINEL_MCU_HOST` |
| Command Port | 5000 | `SENTINEL_PORT_COMMAND` |
| Telemetry Port | 5001 | `SENTINEL_PORT_TELEMETRY` |
| Diagnostics Port | 5002 | `SENTINEL_PORT_DIAG` |

For QEMU SIL testing, use `127.0.0.1` for both hosts (user-mode networking).

## 7. Traceability

These stakeholder requirements flow into the System Requirements Specification (SRS, document SRS-001) via the SYS.2 process. Traceability is maintained in `docs/traceability/stkh_to_sys_trace.md`.
