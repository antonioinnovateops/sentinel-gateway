---
title: "Integration Test Plan"
document_id: "ITP-001"
project: "Sentinel Gateway"
version: "2.0.0"
date: 2026-03-18
status: "Approved"
aspice_process: "SWE.5 Software Integration Test"
---

# Integration Test Plan — Sentinel Gateway

## 1. Test Environment

### 1.1 QEMU User-Mode SIL Architecture

**Critical Implementation Note**: Integration tests run in QEMU **user-mode emulation** (qemu-arm-static), NOT full system emulation with TAP bridges.

```
┌────────────────────────────────────────────────────────────────┐
│                    Host Machine (x86_64)                       │
│  ┌──────────────────────┐    ┌─────────────────────────────┐  │
│  │   Linux Gateway       │    │   MCU Firmware (QEMU)       │  │
│  │   (Native x86_64)     │◄──►│   qemu-arm-static           │  │
│  │                       │    │   (User-mode emulation)     │  │
│  │  - sentinel_gateway   │    │  - sentinel_mcu_qemu        │  │
│  │  - Port 5002 (diag)   │    │  - Connects to 127.0.0.1    │  │
│  └───────────┬───────────┘    └──────────────┬──────────────┘  │
│              │                               │                 │
│              └───────────────┬───────────────┘                 │
│                              │                                 │
│                      localhost:5000 (cmd)                      │
│                      localhost:5001 (telemetry)                │
└────────────────────────────────────────────────────────────────┘
```

### 1.2 Network Configuration

| Port | Direction | Purpose |
|------|-----------|---------|
| 5000 | Linux → MCU | Actuator commands, config updates |
| 5001 | MCU → Linux | Sensor telemetry, health status |
| 5002 | External → Linux | Diagnostic CLI (single client) |

**MCU Address**: `127.0.0.1` (localhost) or override via `SENTINEL_MCU_HOST` environment variable.

### 1.3 Test Harness

- **Language**: Python 3
- **Location**: `tests/integration/`
- **Framework**: pytest
- **Dependencies**: None external (uses stdlib socket/struct)

```bash
# Run integration tests
cd tests/integration
pytest -v test_integration.py
```

## 2. Wire Frame Protocol

All TCP messages use this exact framing:

```
┌─────────────────┬──────────────┬─────────────────────────┐
│ Length (4 bytes)│ Type (1 byte)│ Payload (N bytes)       │
│ Little-endian   │              │                         │
│ uint32          │ msg_type_t   │ Protobuf-encoded        │
└─────────────────┴──────────────┴─────────────────────────┘
```

**Message Type IDs**:
- 0x01: MSG_SENSOR_DATA
- 0x02: MSG_ACTUATOR_CMD
- 0x03: MSG_ACTUATOR_RESP
- 0x04: MSG_HEALTH_STATUS
- 0x05: MSG_CONFIG_UPDATE
- 0x06: MSG_CONFIG_RESP
- 0x07: MSG_STATE_SYNC_REQ
- 0x08: MSG_STATE_SYNC_RESP

## 3. Integration Test Scenarios

### IT-01: Sensor Data End-to-End Flow
- **Objective**: Verify sensor data flows from MCU ADC stub to Linux log file
- **Requirements**: SYS-001 to SYS-014
- **Steps**:
  1. Start Linux gateway: `./build-linux/sentinel_gateway`
  2. Start QEMU MCU: `qemu-arm-static ./build-qemu-mcu/sentinel_mcu_qemu`
  3. Wait for TCP connection established (check log)
  4. Wait 5 seconds for sensor data at default 10 Hz
  5. Connect to Linux diagnostic port 5002
  6. Send `sensor read 0` through `sensor read 3` (lowercase!)
  7. Verify all 4 channels return valid readings
  8. Check `sensor_data.jsonl` has ≥ 40 entries (4 ch × 10 Hz × 1s)
- **Pass Criteria**: All channels read, log file populated, no errors

### IT-02: Actuator Command End-to-End Flow
- **Objective**: Verify actuator commands flow from Linux to MCU PWM stub
- **Requirements**: SYS-016 to SYS-020
- **Steps**:
  1. Connect to diagnostic port 5002
  2. Send `actuator set 0 50` (fan to 50%)
  3. Verify response: `OK ACTUATOR 0 SET 50.0%`
  4. Send `actuator set 1 75` (valve to 75%)
  5. Verify response: `OK ACTUATOR 1 SET 75.0%`
  6. Send `status` and verify actuator states
- **Pass Criteria**: Both actuators at commanded values, latency ≤ 20ms

### IT-03: Actuator Safety Validation
- **Objective**: Verify safety limits prevent out-of-range actuator commands
- **Requirements**: SYS-022 to SYS-025
- **Steps**:
  1. Send `actuator set 0 100` (above 95% default fan max)
  2. Verify clamped to 95% (response shows clamped value)
  3. Send `actuator set 0 -5` (below 0% min)
  4. Verify rejected with error: `ERROR OUT_OF_RANGE`
- **Pass Criteria**: Limits enforced, appropriate error codes returned

### IT-04: Heartbeat Monitoring
- **Objective**: Verify Linux detects MCU heartbeat and monitors health
- **Requirements**: SYS-038, SYS-039
- **Steps**:
  1. Start both processes, wait for normal operation
  2. Send `status` — verify MCU state = NORMAL, comm = CONNECTED
  3. Monitor for 10 seconds — no spurious alerts
  4. Verify HealthStatus messages logged at ~1 Hz
- **Pass Criteria**: Heartbeat regular, status correct

### IT-05: Communication Loss and Fail-Safe
- **Objective**: Verify fail-safe triggers on MCU communication loss
- **Requirements**: SYS-026, SYS-039, SYS-040
- **Implementation Note**: Since QEMU user-mode uses localhost sockets, simulate comm loss by killing MCU process.
- **Steps**:
  1. Set actuators to non-zero values via diagnostic CLI
  2. Kill MCU QEMU process: `pkill sentinel_mcu_qemu`
  3. Wait 5 seconds
  4. Restart MCU QEMU process
  5. Send `status`
  6. Verify MCU went to FAILSAFE, actuators reset to 0%
- **Pass Criteria**: Fail-safe triggered, actuators safe, recovery initiated

### IT-06: Recovery Sequence
- **Objective**: Verify automatic recovery after communication loss
- **Requirements**: SYS-042 to SYS-045
- **Steps**:
  1. Cause communication loss (kill MCU process)
  2. Wait for Linux to detect loss (3 seconds timeout)
  3. Restart MCU process
  4. Verify TCP reconnection (check logs)
  5. Verify state sync occurs (SyncRequest/Response in logs)
  6. Verify normal operation resumes (sensor data flowing)
- **Pass Criteria**: Recovery successful, state synchronized

### IT-07: Configuration Update Flow
- **Objective**: Verify configuration changes propagate and persist
- **Requirements**: SYS-058 to SYS-065
- **Steps**:
  1. Send config update: `config set channel0_rate 50` (via diagnostic interface)
  2. Verify CH0 now streams at ~50 Hz (measure timing)
  3. Kill and restart MCU process
  4. After reconnect, verify CH0 still at 50 Hz (persisted in flash stub)
  5. Send invalid config: `config set channel0_rate 200`
  6. Verify rejected with error: `ERROR OUT_OF_RANGE`
- **Pass Criteria**: Config applied, persisted, invalid rejected

### IT-08: Diagnostic Interface Complete Test
- **Objective**: Exercise all diagnostic commands
- **Requirements**: SYS-046 to SYS-054
- **Critical**: Commands are **lowercase only, case-sensitive**.
- **Steps**:
  1. Connect to port 5002
  2. Send each command (valid):
     - `sensor read 0` → `OK SENSOR 0 ...`
     - `actuator set 0 50` → `OK ACTUATOR 0 SET 50.0%`
     - `status` → system state JSON
     - `version` → Linux + MCU versions
     - `log 10` → last 10 log entries
     - `config get` → current config
     - `reset mcu` → `OK MCU_RESET_INITIATED`
     - `help` → command list
  3. Test invalid commands:
     - `STATUS` (uppercase) → `ERROR UNKNOWN_COMMAND`
     - `invalid_cmd` → `ERROR UNKNOWN_COMMAND`
     - `sensor read 99` → `ERROR INVALID_CHANNEL`
- **Pass Criteria**: All commands return expected format, errors handled

### IT-09: Single Diagnostic Client Limitation
- **Objective**: Verify diagnostic port accepts only one client
- **Requirements**: SWE-046-2
- **Design Note**: Epoll implementation uses single-slot design for simplicity.
- **Steps**:
  1. Connect first client to port 5002
  2. Verify connection accepted, `help` works
  3. Attempt second connection to port 5002
  4. Verify second connection refused (ECONNREFUSED or immediate close)
  5. Disconnect first client
  6. Connect new client
  7. Verify new client works
- **Pass Criteria**: Single client enforced, graceful rejection

### IT-10: Watchdog Recovery
- **Objective**: Verify watchdog resets MCU on firmware hang
- **Requirements**: SYS-069, SYS-027
- **Implementation Note**: In QEMU user-mode, watchdog is simulated by a timer thread that calls `exit(1)` on timeout. Recovery tested by process restart.
- **Steps**:
  1. Send diagnostic command to freeze MCU main loop (if debug build supports)
  2. Or: Use gdb attach to pause MCU process
  3. Wait for watchdog timeout (2 seconds)
  4. Verify MCU process exits with non-zero code
  5. Restart MCU process
  6. Verify MCU boots to safe state (actuators at 0%)
  7. Verify Linux detects reconnection
- **Pass Criteria**: MCU exits, safe state on reboot, recovery succeeds

### IT-11: Protocol Version Compatibility
- **Objective**: Verify version field handling
- **Requirements**: SYS-037
- **Steps**:
  1. Normal operation with matching versions
  2. Modify MCU to send different minor version (if configurable)
  3. Verify accepted (backward compatible)
  4. Check for version mismatch warnings in log
- **Pass Criteria**: Compatible versions accepted, warnings logged

### IT-12: TCP Throughput
- **Objective**: Verify ≥ 1 Mbps sustained throughput
- **Requirements**: SYS-068
- **Steps**:
  1. Configure all 4 channels to 100 Hz via `config set`
  2. Measure data rate on telemetry port (capture bytes/second)
  3. Run for 60 seconds
  4. Calculate average throughput
- **Pass Criteria**: Sustained throughput ≥ 1 Mbps

## 4. Test Execution Commands

```bash
# Start Linux gateway
./build-linux/sentinel_gateway

# Start QEMU MCU (in separate terminal)
qemu-arm-static ./build-qemu-mcu/sentinel_mcu_qemu

# Or with custom MCU address (if running on different host)
SENTINEL_MCU_HOST=192.168.1.100 ./build-linux/sentinel_gateway

# Connect diagnostic CLI
nc localhost 5002
# Or use Python test harness:
python3 tests/integration/diag_client.py
```

## 5. Test Summary

| Test ID | Requirements | Priority | Estimated Duration |
|---------|-------------|----------|-------------------|
| IT-01 | SYS-001 to SYS-014 | High | 30s |
| IT-02 | SYS-016 to SYS-020 | High | 15s |
| IT-03 | SYS-022 to SYS-025 | High | 10s |
| IT-04 | SYS-038, SYS-039 | High | 15s |
| IT-05 | SYS-026, SYS-039, SYS-040 | Critical | 30s |
| IT-06 | SYS-042 to SYS-045 | Critical | 60s |
| IT-07 | SYS-058 to SYS-065 | Medium | 30s |
| IT-08 | SYS-046 to SYS-054 | Medium | 20s |
| IT-09 | SWE-046-2 | Medium | 10s |
| IT-10 | SYS-069, SYS-027 | High | 30s |
| IT-11 | SYS-037 | Low | 10s |
| IT-12 | SYS-068 | Medium | 90s |
| **Total** | | | **~6 min** |

## 6. Pass/Fail Criteria

- **Pass**: All 12 tests pass
- **Conditional Pass**: ≥10 tests pass, failing tests are non-critical
- **Fail**: Any safety-critical test fails (IT-03, IT-05, IT-06, IT-10)
