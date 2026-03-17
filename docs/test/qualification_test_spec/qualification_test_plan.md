---
title: "System Qualification Test Plan"
document_id: "QTP-001"
project: "Sentinel Gateway"
version: "1.0.0"
date: 2026-03-17
status: "Approved"
aspice_process: "SWE.6 Software Qualification Test"
---

# System Qualification Test Plan — Sentinel Gateway

## 1. Purpose

Verify that the complete Sentinel Gateway system meets all 80 system requirements (SRS-001) when running in the QEMU SIL environment.

## 2. Test Environment

- QEMU ARM64 (Linux gateway) + QEMU ARM (MCU firmware)
- Virtual network bridge (simulating USB CDC-ECM)
- Python test harness connecting via TCP
- Test automation script: `tests/system/run_qualification.py`

## 3. Qualification Tests

### QT-SYS-001: ADC Channel Count
- **Requirement**: [SYS-001] ≥4 ADC channels
- **Method**: Read all 4 channels via READ_SENSOR, verify distinct values
- **Pass**: 4 channels return valid, distinct readings

### QT-SYS-002 to QT-SYS-005: Sensor Calibration
- **Requirement**: [SYS-002-005] Correct calibration per channel
- **Method**: Inject known ADC values (QEMU), verify calibrated output
- **Pass**: Values within specified tolerance

### QT-SYS-006: ADC Resolution
- **Requirement**: [SYS-006] 12-bit (0-4095)
- **Method**: Check raw values in SensorData messages
- **Pass**: Raw values use full 0-4095 range

### QT-SYS-007: Sensor Data Packaging
- **Requirement**: [SYS-007] Complete SensorData protobuf
- **Method**: Decode telemetry, verify all fields present
- **Pass**: channel_id, raw, calibrated, unit, timestamp, sequence all populated

### QT-SYS-008: Sensor Transmission
- **Requirement**: [SYS-008] Transmit at configured rate on TCP:5001
- **Method**: Count messages per second at default 10 Hz
- **Pass**: Rate = 10 Hz ±5%

### QT-SYS-009: Configurable Sample Rate
- **Requirement**: [SYS-009] 1-100 Hz configurable
- **Method**: Set rate to 1, 10, 50, 100 Hz, measure actual rate
- **Pass**: All rates within ±5%

### QT-SYS-010: Sample Jitter
- **Requirement**: [SYS-010] Jitter ≤ 500µs
- **Method**: Analyze timestamps of 1000 consecutive samples at 100 Hz
- **Pass**: Max jitter ≤ 500µs

### QT-SYS-011: ADC Conversion Time
- **Requirement**: [SYS-011] ≤ 100µs for 4 channels
- **Method**: MCU firmware timing measurement (debug output)
- **Pass**: ≤ 100µs

### QT-SYS-012: Per-Channel Rate
- **Requirement**: [SYS-012] Independent channel rates
- **Method**: Set CH0=100Hz, CH3=1Hz, verify both
- **Pass**: Both rates independently correct

### QT-SYS-013 to QT-SYS-015: Data Logging
- **Requirements**: [SYS-013-015] JSON log, timestamp, rotation
- **Method**: Check log file format, verify timestamps, fill to rotation
- **Pass**: Valid JSON lines, ms timestamps, rotation at 100MB

### QT-SYS-016 to QT-SYS-017: Actuator Control
- **Requirements**: [SYS-016-017] Fan and valve PWM
- **Method**: SET_ACTUATOR commands, verify via GET_STATUS
- **Pass**: Duty cycle matches command ±1%

### QT-SYS-018 to QT-SYS-019: Command/Response
- **Requirements**: [SYS-018-019] ActuatorCommand and Response
- **Method**: Send command, verify response with correct fields
- **Pass**: Response received for every command

### QT-SYS-020: Actuator Latency
- **Requirement**: [SYS-020] ≤ 20ms end-to-end
- **Method**: Timestamp before send, check MCU response timestamp
- **Pass**: 99th percentile ≤ 20ms over 1000 commands

### QT-SYS-021: Rate Limiting
- **Requirement**: [SYS-021] Max 50 cmd/sec/actuator
- **Method**: Send 51 commands in 1 second
- **Pass**: 51st rejected with RATE_LIMITED

### QT-SYS-022 to QT-SYS-025: Actuator Safety
- **Requirements**: [SYS-022-025] Validation, limits, auth
- **Method**: Send out-of-range, over-limit, unauthenticated commands
- **Pass**: All rejected with correct error codes

### QT-SYS-026 to QT-SYS-029: Fail-Safe
- **Requirements**: [SYS-026-029] Comm loss, watchdog, error, reporting
- **Method**: Simulate each failure, verify safe state + HealthStatus
- **Pass**: Actuators at 0%, correct cause code reported

### QT-SYS-030 to QT-SYS-037: Communication
- **Requirements**: [SYS-030-037] USB, IP, TCP, protobuf, framing, seq, version
- **Method**: Network capture, message decode, field verification
- **Pass**: All protocol elements correct

### QT-SYS-038 to QT-SYS-041: Health Monitoring
- **Requirements**: [SYS-038-041] Heartbeat, timeout, monitoring, status
- **Method**: Monitor heartbeat rate, inject comm loss, check status transitions
- **Pass**: 1Hz heartbeat, 3s detection, correct status transitions

### QT-SYS-042 to QT-SYS-045: Recovery
- **Requirements**: [SYS-042-045] USB cycle, reconnect, sync, limit
- **Method**: Cause comm loss, observe recovery sequence
- **Pass**: Recovery within 10s, state sync complete, 3-failure limit

### QT-SYS-046 to QT-SYS-051: Diagnostics
- **Requirements**: [SYS-046-051] All diagnostic commands
- **Method**: Exercise each command via TCP:5002
- **Pass**: All commands return expected format

### QT-SYS-052 to QT-SYS-055: Logging
- **Requirements**: [SYS-052-055] Event log, persistence, retrieval, severity
- **Method**: Generate events, verify in log, test severity filtering
- **Pass**: All event types present, correct severity

### QT-SYS-056 to QT-SYS-057: Version
- **Requirements**: [SYS-056-057] Linux and MCU version
- **Method**: GET_VERSION, verify format includes git hash
- **Pass**: Both versions in correct format

### QT-SYS-058 to QT-SYS-065: Configuration
- **Requirements**: [SYS-058-065] Update, persist, validate, CRC, defaults
- **Method**: Config update cycle, persistence test, injection of bad values
- **Pass**: All config operations correct

### QT-SYS-066 to QT-SYS-068: Performance
- **Requirements**: [SYS-066-068] Boot time, throughput
- **Method**: Measure boot times, iperf throughput
- **Pass**: Linux ≤30s, MCU ≤5s, throughput ≥1Mbps

### QT-SYS-069 to QT-SYS-071: Watchdog
- **Requirements**: [SYS-069-071] IWDG, feed, counter
- **Method**: Normal operation + simulated hang
- **Pass**: WDG active, counter tracked

### QT-SYS-072 to QT-SYS-073: Memory
- **Requirements**: [SYS-072-073] Static alloc, stack limit
- **Method**: Static analysis of binary
- **Pass**: Zero malloc, stack ≤32KB

### QT-SYS-074 to QT-SYS-080: Process
- **Requirements**: [SYS-074-080] Work products, reviews, traceability, MISRA
- **Method**: Document audit, traceability script, MISRA report
- **Pass**: All work products exist, zero orphans, zero Required violations

## 4. Qualification Summary

| Category | Test Count | SYS Requirements Covered |
|----------|-----------|-------------------------|
| Sensor | 12 | SYS-001 to SYS-012 |
| Logging | 3 | SYS-013 to SYS-015 |
| Actuator | 10 | SYS-016 to SYS-025 |
| Fail-Safe | 4 | SYS-026 to SYS-029 |
| Communication | 8 | SYS-030 to SYS-037 |
| Health | 4 | SYS-038 to SYS-041 |
| Recovery | 4 | SYS-042 to SYS-045 |
| Diagnostics | 6 | SYS-046 to SYS-051 |
| Event Logging | 4 | SYS-052 to SYS-055 |
| Version | 2 | SYS-056 to SYS-057 |
| Configuration | 8 | SYS-058 to SYS-065 |
| Performance | 3 | SYS-066 to SYS-068 |
| Reliability | 3 | SYS-069 to SYS-071 |
| Memory | 2 | SYS-072 to SYS-073 |
| Process | 7 | SYS-074 to SYS-080 |
| **TOTAL** | **80** | **All 80 SYS requirements** |
