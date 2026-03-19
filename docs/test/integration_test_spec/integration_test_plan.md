---
title: "Integration Test Plan"
document_id: "ITP-001"
project: "Sentinel Gateway"
version: "1.0.0"
date: 2026-03-17
status: "Approved"
aspice_process: "SWE.5 Software Integration Test"
---

# Integration Test Plan — Sentinel Gateway

## 1. Test Environment

Two execution modes are available (both via Docker):

| Mode | Command | MCU | Gateway | Network |
|------|---------|-----|---------|---------|
| **Fast SIL** (default) | `docker-compose run --rm sil` | x86 MCU simulator | Native x86 gateway | localhost TCP |
| **QEMU SIL** (optional) | `docker-compose run --rm qemu-sil` | ARM binary via qemu-arm-static | aarch64/x86 gateway | localhost TCP |

- **Test Harness**: Python pytest (`tests/integration/test_sil.py`) connecting via diagnostic port TCP:5002
- **Test Report**: JUnit XML at `results/sil/sil-results.xml`

## 2. Integration Test Scenarios

### IT-01: Sensor Data End-to-End Flow
- **Objective**: Verify sensor data flows from MCU ADC to Linux log file
- **Requirements**: SYS-001 to SYS-014
- **Steps**:
  1. Boot both VMs
  2. Wait for TCP connection established
  3. Wait 5 seconds for sensor data at default 10 Hz
  4. Connect to Linux diagnostic port 5002
  5. Send `READ_SENSOR 0` through `READ_SENSOR 3`
  6. Verify all 4 channels return valid readings
  7. Check sensor log file has ≥ 40 entries (4 ch × 10 Hz × 1s)
- **Pass Criteria**: All channels read, log file populated, no errors

### IT-02: Actuator Command End-to-End Flow
- **Objective**: Verify actuator commands flow from Linux to MCU PWM output
- **Requirements**: SYS-016 to SYS-020
- **Steps**:
  1. Connect to diagnostic port 5002
  2. Send `SET_ACTUATOR 0 50` (fan to 50%)
  3. Verify response: `OK ACTUATOR 0 SET 50.0%`
  4. Send `SET_ACTUATOR 1 75` (valve to 75%)
  5. Verify response: `OK ACTUATOR 1 SET 75.0%`
  6. Send `GET_STATUS` and verify actuator states
- **Pass Criteria**: Both actuators at commanded values, latency ≤ 20ms

### IT-03: Actuator Safety Validation
- **Objective**: Verify safety limits prevent out-of-range actuator commands
- **Requirements**: SYS-022 to SYS-025
- **Steps**:
  1. Send `SET_ACTUATOR 0 100` (above 95% default fan max)
  2. Verify clamped to 95%
  3. Send `SET_ACTUATOR 0 -5` (below 0% min)
  4. Verify rejected with error
- **Pass Criteria**: Limits enforced, appropriate error codes returned

### IT-04: Heartbeat Monitoring
- **Objective**: Verify Linux detects MCU heartbeat and monitors health
- **Requirements**: SYS-038, SYS-039
- **Steps**:
  1. Boot both VMs, wait for normal operation
  2. Send `GET_STATUS` — verify MCU state = NORMAL, comm = CONNECTED
  3. Monitor for 10 seconds — no spurious alerts
  4. Verify HealthStatus messages logged at ~1 Hz
- **Pass Criteria**: Heartbeat regular, status correct

### IT-05: Communication Loss and Fail-Safe
- **Objective**: Verify fail-safe triggers on MCU communication loss
- **Requirements**: SYS-026, SYS-039, SYS-040
- **Steps**:
  1. Set actuators to non-zero values
  2. Disconnect network bridge (simulate comm loss)
  3. Wait 5 seconds
  4. Reconnect network bridge
  5. Send `GET_STATUS`
  6. Verify MCU went to FAILSAFE, actuators at 0%
- **Pass Criteria**: Fail-safe triggered, actuators safe, recovery initiated

### IT-06: Recovery Sequence
- **Objective**: Verify automatic recovery after communication loss
- **Requirements**: SYS-042 to SYS-045
- **Steps**:
  1. Cause communication loss (disconnect bridge)
  2. Wait for Linux to detect loss (3 seconds)
  3. Wait for recovery attempt (5 seconds)
  4. Restore network bridge
  5. Verify state sync occurs
  6. Verify normal operation resumes
- **Pass Criteria**: Recovery successful, state synchronized

### IT-07: Configuration Update Flow
- **Objective**: Verify configuration changes propagate and persist
- **Requirements**: SYS-058 to SYS-065
- **Steps**:
  1. Send config update: CH0 rate = 50 Hz (via diagnostic interface)
  2. Verify CH0 now streams at ~50 Hz
  3. Reset MCU
  4. After reconnect, verify CH0 still at 50 Hz (persisted in flash)
  5. Send invalid config (rate = 200 Hz)
  6. Verify rejected with error
- **Pass Criteria**: Config applied, persisted, invalid rejected

### IT-08: Diagnostic Interface Complete Test
- **Objective**: Exercise all diagnostic commands
- **Requirements**: SYS-046 to SYS-054
- **Steps**:
  1. Connect to port 5002
  2. Send each command: READ_SENSOR, SET_ACTUATOR, GET_STATUS, GET_VERSION, GET_LOG, GET_CONFIG, RESET_MCU, HELP
  3. Verify each response format
  4. Test invalid commands
- **Pass Criteria**: All commands return expected format, errors handled

### IT-09: Multiple Diagnostic Clients
- **Objective**: Verify up to 3 concurrent diagnostic clients
- **Requirements**: SWE-046-2
- **Steps**:
  1. Connect 3 clients to port 5002
  2. Send commands from all 3 concurrently
  3. Attempt 4th connection
  4. Verify 4th rejected (max clients)
- **Pass Criteria**: 3 clients work, 4th rejected

### IT-10: Watchdog Recovery
- **Objective**: Verify watchdog resets MCU on firmware hang
- **Requirements**: SYS-069, SYS-027
- **Steps**:
  1. Send diagnostic command to simulate MCU hang (if supported)
  2. Or: inject delay via QEMU (if possible)
  3. Wait for watchdog reset (2 seconds)
  4. Verify MCU reboots to safe state
  5. Verify reset counter incremented
- **Pass Criteria**: MCU resets, safe state on boot, counter incremented

### IT-11: Protocol Version Compatibility
- **Objective**: Verify version field handling
- **Requirements**: SYS-037
- **Steps**:
  1. Normal operation with matching versions
  2. Inject message with different minor version
  3. Verify accepted (backward compatible)
  4. Check for version mismatch warnings in log
- **Pass Criteria**: Compatible versions accepted, warnings logged

### IT-12: TCP Throughput
- **Objective**: Verify ≥ 1 Mbps sustained throughput
- **Requirements**: SYS-068
- **Steps**:
  1. Set all 4 channels to 100 Hz
  2. Measure data rate on telemetry port
  3. Run for 60 seconds
- **Pass Criteria**: Sustained throughput ≥ 1 Mbps

## 3. Test Summary

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
| IT-09 | SWE-046-2 | Low | 10s |
| IT-10 | SYS-069, SYS-027 | High | 30s |
| IT-11 | SYS-037 | Low | 10s |
| IT-12 | SYS-068 | Medium | 90s |
| **Total** | | | **~6 min** |
