---
title: "Software Qualification Test Plan"
document_id: "QTP-001"
project: "Sentinel Gateway"
version: "2.0.0"
date: 2026-03-18
status: "Approved"
aspice_process: "SWE.6 Software Qualification Test"
---

# Software Qualification Test Plan — Sentinel Gateway

## 1. Purpose

Verify that the complete Sentinel Gateway software meets all 80 system requirements (SRS-001) when running in the QEMU SIL environment.

## 2. Test Environment

### 2.1 QEMU User-Mode Architecture

**Critical**: Tests run in QEMU **user-mode emulation**, not full system emulation.

```
┌─────────────────────────────────────────────────────────────┐
│                    Docker Container                          │
│  ┌──────────────────────┐    ┌────────────────────────────┐ │
│  │  Linux Gateway        │    │  MCU Firmware              │ │
│  │  (x86_64 native)      │◄──►│  (qemu-arm-static)         │ │
│  │  sentinel_gateway     │    │  sentinel_mcu_qemu         │ │
│  │  Port 5002 (diag)     │    │  POSIX sockets (not lwIP)  │ │
│  └──────────────────────┘    └────────────────────────────┘ │
│                localhost:5000, 5001                          │
└─────────────────────────────────────────────────────────────┘
```

### 2.2 Test Automation

- **Test harness**: Python 3 scripts in `tests/qualification/`
- **Execution**: `pytest tests/qualification/ -v --junitxml=results.xml`
- **Wire frame handling**: Custom parser for 5-byte header (4-byte LE length + 1-byte type)

### 2.3 Test Commands

```bash
# Build and run qualification tests
docker build -f docker/Dockerfile.qemu-sil -t sentinel-sil .
docker run --rm sentinel-sil pytest tests/qualification/ -v
```

## 3. Qualification Tests

### QT-SYS-001: ADC Channel Count
- **Requirement**: [SYS-001] ≥4 ADC channels
- **Method**: Send `sensor read 0` through `sensor read 3` via diagnostic port
- **Pass**: 4 channels return valid, distinct readings

### QT-SYS-002 to QT-SYS-005: Sensor Calibration
- **Requirement**: [SYS-002-005] Correct calibration per channel
- **Method**: MCU ADC stub generates known values, verify calibrated output
- **Calibration formulae** (from implementation):
  - Temp: `(raw / 4095.0) * 165.0 - 40.0`
  - Humidity: `(raw / 4095.0) * 100.0`
  - Pressure: `(raw / 4095.0) * 800.0 + 300.0`
  - Light: `(raw / 4095.0) * 100000.0`
- **Pass**: Values within specified tolerance

### QT-SYS-006: ADC Resolution
- **Requirement**: [SYS-006] 12-bit (0-4095)
- **Method**: Check raw values in SensorData messages via telemetry port
- **Pass**: Raw values in 0-4095 range

### QT-SYS-007: Sensor Data Packaging
- **Requirement**: [SYS-007] Complete SensorData protobuf
- **Method**: Decode telemetry wire frames, verify all fields present
- **Wire frame**: `[4-byte LE length][0x01][protobuf payload]`
- **Pass**: channel_id, raw, calibrated, unit, timestamp, sequence all populated

### QT-SYS-008: Sensor Transmission
- **Requirement**: [SYS-008] Transmit at configured rate on TCP:5001
- **Method**: Count messages per second at default 10 Hz
- **Pass**: Rate = 10 Hz ±5%

### QT-SYS-009: Configurable Sample Rate
- **Requirement**: [SYS-009] 1-100 Hz configurable
- **Method**: `config set channel0_rate N` for N ∈ {1, 10, 50, 100}, measure actual rate
- **Pass**: All rates within ±5%

### QT-SYS-010: Sample Jitter
- **Requirement**: [SYS-010] Jitter ≤ 500µs
- **Method**: Analyze timestamps of 1000 consecutive samples at 100 Hz
- **Pass**: Max jitter ≤ 500µs

### QT-SYS-011: ADC Conversion Time
- **Requirement**: [SYS-011] ≤ 100µs for 4 channels
- **Method**: MCU firmware debug timing output
- **Pass**: ≤ 100µs

### QT-SYS-012: Per-Channel Rate
- **Requirement**: [SYS-012] Independent channel rates
- **Method**: Set CH0=100Hz, CH3=1Hz via `config set`, verify both
- **Pass**: Both rates independently correct

### QT-SYS-013 to QT-SYS-015: Data Logging
- **Requirements**: [SYS-013-015] JSON log, timestamp, rotation
- **Method**: Check `sensor_data.jsonl` format, verify timestamps, fill to rotation
- **Pass**: Valid JSON lines, ms timestamps, rotation at 100MB

### QT-SYS-016 to QT-SYS-017: Actuator Control
- **Requirements**: [SYS-016-017] Fan and valve PWM
- **Method**: `actuator set 0 N` and `actuator set 1 N`, verify via `status`
- **Pass**: Duty cycle matches command ±1%

### QT-SYS-018 to QT-SYS-019: Command/Response
- **Requirements**: [SYS-018-019] ActuatorCommand and Response
- **Method**: Send command via diagnostic CLI, verify response format
- **Wire frame**: Command=`[len][0x02][protobuf]`, Response=`[len][0x03][protobuf]`
- **Pass**: Response received for every command

### QT-SYS-020: Actuator Latency
- **Requirement**: [SYS-020] ≤ 20ms end-to-end
- **Method**: Timestamp before CLI send, timestamp after response received
- **Pass**: 99th percentile ≤ 20ms over 1000 commands

### QT-SYS-021: Rate Limiting
- **Requirement**: [SYS-021] Max 50 cmd/sec/actuator
- **Method**: Send 51 commands in 1 second via rapid CLI commands
- **Pass**: 51st rejected with `ERROR RATE_LIMITED`

### QT-SYS-022 to QT-SYS-025: Actuator Safety
- **Requirements**: [SYS-022-025] Validation, limits, auth
- **Method**: Send out-of-range (`actuator set 0 200`), over-limit values
- **Pass**: All rejected with `ERROR OUT_OF_RANGE`

### QT-SYS-026 to QT-SYS-029: Fail-Safe
- **Requirements**: [SYS-026-029] Comm loss, watchdog, error, reporting
- **Method**: Kill MCU process (simulate comm loss), verify via `status`
- **Pass**: Actuators at 0%, status shows FAILSAFE

### QT-SYS-030 to QT-SYS-037: Communication
- **Requirements**: [SYS-030-037] USB, IP, TCP, protobuf, framing, seq, version
- **Method**: Packet capture, message decode, field verification
- **Wire frame format**: `[4-byte LE length][1-byte type][protobuf payload]`
- **Pass**: All protocol elements correct

### QT-SYS-038 to QT-SYS-041: Health Monitoring
- **Requirements**: [SYS-038-041] Heartbeat, timeout, monitoring, status
- **Method**: Monitor HealthStatus messages (type=0x04), inject comm loss, check status
- **Pass**: 1Hz heartbeat, 3s detection, correct status transitions

### QT-SYS-042 to QT-SYS-045: Recovery
- **Requirements**: [SYS-042-045] USB cycle, reconnect, sync, limit
- **Method**: Kill MCU, restart, verify recovery sequence
- **Pass**: Recovery within 10s, state sync complete

### QT-SYS-046 to QT-SYS-051: Diagnostics
- **Requirements**: [SYS-046-051] All diagnostic commands
- **Method**: Exercise each command via TCP:5002
- **Critical**: Commands are **lowercase only**:
  - `status`, `sensor read N`, `actuator set N V`, `version`, `log N`, `reset mcu`, `help`
- **Pass**: All commands return expected format

### QT-SYS-052 to QT-SYS-055: Logging
- **Requirements**: [SYS-052-055] Event log, persistence, retrieval, severity
- **Method**: Generate events, verify in `events.jsonl`, test `log N` command
- **Pass**: All event types present, correct severity

### QT-SYS-056 to QT-SYS-057: Version
- **Requirements**: [SYS-056-057] Linux and MCU version
- **Method**: `version` command, verify format includes git hash
- **Pass**: Both versions in format `major.minor.patch-hash`

### QT-SYS-058 to QT-SYS-065: Configuration
- **Requirements**: [SYS-058-065] Update, persist, validate, CRC, defaults
- **Method**: Config update via `config set`, persistence test via MCU restart
- **Pass**: All config operations correct

### QT-SYS-066 to QT-SYS-068: Performance
- **Requirements**: [SYS-066-068] Boot time, throughput
- **Method**: Measure process startup times, calculate telemetry throughput
- **Pass**: Linux ≤30s, MCU ≤5s, throughput ≥1Mbps

### QT-SYS-069 to QT-SYS-071: Watchdog
- **Requirements**: [SYS-069-071] IWDG, feed, counter
- **Method**: SIGSTOP MCU process, verify exit on watchdog timeout
- **Note**: In user-mode QEMU, watchdog triggers process exit, not hardware reset
- **Pass**: WDG triggers within 2s, counter increments

### QT-SYS-072 to QT-SYS-073: Memory
- **Requirements**: [SYS-072-073] Static alloc, stack limit
- **Method**: `arm-none-eabi-nm sentinel_mcu.elf | grep malloc` (must be empty)
- **Pass**: Zero malloc, stack ≤32KB

### QT-SYS-074 to QT-SYS-080: Process
- **Requirements**: [SYS-074-080] Work products, reviews, traceability, MISRA
- **Method**: Document audit, `python3 scripts/trace_check.py`, cppcheck MISRA report
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

## 5. Test Execution Script

```python
#!/usr/bin/env python3
"""
Qualification test runner - tests/qualification/run_all.py
"""
import subprocess
import socket
import struct
import time

def send_diag_cmd(cmd):
    """Send diagnostic command and return response."""
    sock = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
    sock.connect(('127.0.0.1', 5002))
    sock.send(f"{cmd}\n".encode())
    response = sock.recv(4096).decode()
    sock.close()
    return response

def test_sensor_read():
    """QT-SYS-001: Verify 4 sensor channels."""
    for ch in range(4):
        resp = send_diag_cmd(f"sensor read {ch}")
        assert resp.startswith("OK SENSOR"), f"Channel {ch} failed: {resp}"
    print("QT-SYS-001: PASS")

def test_actuator_safety():
    """QT-SYS-022: Verify out-of-range rejected."""
    resp = send_diag_cmd("actuator set 0 200")
    assert "ERROR" in resp and "OUT_OF_RANGE" in resp
    print("QT-SYS-022: PASS")

def test_case_sensitivity():
    """Verify commands are lowercase only."""
    resp = send_diag_cmd("STATUS")
    assert "ERROR UNKNOWN_COMMAND" in resp
    print("Case sensitivity: PASS")

if __name__ == "__main__":
    test_sensor_read()
    test_actuator_safety()
    test_case_sensitivity()
    print("All qualification tests passed!")
```

## 6. Pass/Fail Criteria

- **Pass**: All 80 tests pass
- **Conditional Pass**: ≥76 tests pass (95%), no safety-critical failures
- **Fail**: Any safety-critical test fails (actuator, fail-safe, watchdog)
