---
title: "System Integration Test Plan"
document_id: "SITP-001"
project: "Sentinel Gateway"
version: "1.0.0"
date: 2026-03-18
status: "Approved"
aspice_process: "SYS.4 System Integration and Integration Verification"
verifies: "SAD-001 (System Architecture)"
---

# System Integration Test Plan — Sentinel Gateway

## 1. Purpose

This document specifies the system-level integration verification measures per ASPICE SYS.4. It verifies that system elements (SE-01 Linux Gateway, SE-02 MCU, SE-03 Communication Link, SE-04 QEMU SIL) integrate correctly and satisfy system requirements (SRS-001).

**Distinction from SWE.5**: SWE.5 (ITP-001) tests software component interactions within each processor. SYS.4 tests the integration of entire system elements across processor boundaries.

## 2. Integration Strategy

### 2.1 Approach

**Bottom-up integration**: Individual system elements are verified standalone first, then combined incrementally.

### 2.2 Integration Sequence

| Phase | Elements Integrated | Prerequisite |
|-------|-------------------|-------------|
| IS-1 | SE-04 (QEMU SIL) standalone | QEMU installed, images built |
| IS-2 | SE-01 (Linux) on SE-04 | IS-1 pass, Yocto image boots |
| IS-3 | SE-02 (MCU) on SE-04 | IS-1 pass, firmware ELF loads |
| IS-4 | SE-03 (Comm Link) between SE-01 + SE-02 | IS-2 + IS-3 pass |
| IS-5 | Full system: SE-01 + SE-02 + SE-03 on SE-04 | IS-4 pass |

### 2.3 Test Environment

All tests execute in the containerized SIL environment per SIL-001. No physical hardware required.

## 3. Integration Test Specifications

### SIT-01: QEMU SIL Environment Boot

**Verifies**: SYS-050 (QEMU SIL), Integration Phase IS-1
**Precondition**: Docker containers built
**Steps**:
1. Start Linux QEMU VM (`qemu-system-aarch64 -machine virt`)
2. Verify kernel boots to login prompt within 30 seconds
3. Start MCU QEMU VM (`qemu-system-arm -machine netduinoplus2`)
4. Verify firmware init message on serial console within 10 seconds
**Expected**: Both VMs boot independently
**Pass Criteria**: Boot messages appear within timeout

### SIT-02: USB CDC-ECM Link Establishment

**Verifies**: SYS-030, SYS-031 (Communication), Integration Phase IS-4
**Precondition**: SIT-01 pass
**Steps**:
1. Start both QEMU VMs with TAP bridge
2. Wait for USB CDC-ECM enumeration on Linux side
3. Verify `usb0` interface appears on Linux
4. Verify IP 192.168.7.1 assigned on Linux
5. Verify IP 192.168.7.2 assigned on MCU
6. Ping 192.168.7.2 from Linux
**Expected**: Layer 2 + Layer 3 connectivity established
**Pass Criteria**: Ping succeeds with <10ms latency

### SIT-03: TCP Channel Establishment

**Verifies**: SYS-032, SYS-033, SYS-034 (TCP ports), Integration Phase IS-4
**Precondition**: SIT-02 pass
**Steps**:
1. Verify MCU listens on TCP port 5000 (commands)
2. Verify Linux connects to MCU port 5000
3. Verify MCU connects to Linux port 5001 (telemetry)
4. Verify Linux listens on TCP port 5002 (diagnostics)
5. Verify external connection to Linux port 5002
**Expected**: All 3 TCP channels active
**Pass Criteria**: TCP connections established, no RST

### SIT-04: End-to-End Sensor Data Flow

**Verifies**: SYS-001 to SYS-008 (Sensor Acquisition), Integration Phase IS-5
**Precondition**: SIT-03 pass
**Steps**:
1. Inject known ADC values via QEMU QMP (4 channels)
2. Wait for MCU to sample and transmit SensorData
3. Verify Linux receives SensorData protobuf on TCP:5001
4. Verify sensor values match injected values (within calibration tolerance)
5. Verify data logged to `sensor_data.jsonl` on Linux filesystem
6. Verify timestamp resolution ≤ 1ms
**Expected**: Complete data path: ADC → MCU → protobuf → TCP → Linux → log
**Pass Criteria**: 4 sensor values received and logged correctly

### SIT-05: End-to-End Actuator Command Flow

**Verifies**: SYS-016 to SYS-020 (Actuator Control), Integration Phase IS-5
**Precondition**: SIT-03 pass
**Steps**:
1. Linux sends ActuatorCommand (actuator 0, value 50%) via TCP:5000
2. Verify MCU receives and decodes protobuf
3. Verify MCU applies PWM duty cycle (read timer compare register via QMP)
4. Verify MCU sends ActuatorResponse with STATUS_OK
5. Verify Linux receives response within 20ms
6. Measure end-to-end latency (Linux send → PWM change)
**Expected**: Complete command path: Linux → TCP → MCU → PWM
**Pass Criteria**: Latency ≤ 20ms, PWM register = 50%

### SIT-06: Heartbeat and Health Monitoring

**Verifies**: SYS-038 to SYS-041 (Health Monitoring), Integration Phase IS-5
**Precondition**: SIT-03 pass
**Steps**:
1. Verify MCU sends HealthStatus every 1 second on TCP:5001
2. Verify Linux health_monitor receives and logs health messages
3. Verify health message contains: state, uptime, watchdog count, sensor status
4. Run for 30 seconds, verify ≥28 health messages received (allow 2 lost)
**Expected**: Continuous health monitoring active
**Pass Criteria**: ≥28/30 health messages received

### SIT-07: Communication Loss Detection

**Verifies**: SYS-038, SYS-039 (Loss Detection), Integration Phase IS-5
**Precondition**: SIT-06 pass
**Steps**:
1. Normal operation for 5 seconds (baseline)
2. Disconnect TAP bridge (`ip link set tap-mcu down`)
3. Start timer
4. Verify Linux detects loss within 3 seconds
5. Verify Linux logs COMM_LOSS event
6. Verify MCU enters FAILSAFE state within 3 seconds
7. Verify MCU sets all PWM outputs to 0%
**Expected**: Both sides detect loss independently
**Pass Criteria**: Detection ≤ 3 seconds on both sides

### SIT-08: Automatic Recovery Sequence

**Verifies**: SYS-042 to SYS-045 (Recovery), Integration Phase IS-5
**Precondition**: SIT-07 pass
**Steps**:
1. Trigger communication loss (SIT-07 steps 1-7)
2. Restore TAP bridge (`ip link set tap-mcu up`)
3. Verify TCP reconnection
4. Verify Linux sends StateSyncRequest
5. Verify MCU responds with full state (StateSyncResponse)
6. Verify both sides return to NORMAL state
7. Verify sensor data and health messages resume
**Expected**: Full automatic recovery without human intervention
**Pass Criteria**: Recovery complete within 10 seconds of link restoration

### SIT-09: Configuration Update Across Boundary

**Verifies**: SYS-058 to SYS-065 (Configuration), Integration Phase IS-5
**Precondition**: SIT-03 pass
**Steps**:
1. Default sensor rate = 10 Hz (verify baseline)
2. Linux sends ConfigUpdate {channel:0, rate:50}
3. Verify MCU validates and applies new rate
4. Verify MCU stores config in flash (CRC-protected)
5. Verify MCU sends ConfigResponse {status: OK}
6. Verify sensor data now arrives at ~50 Hz (measure interval)
7. Reset MCU via QEMU `system_reset`
8. After reboot, verify config persists (rate still 50 Hz)
**Expected**: Config applied, persisted, survives reset
**Pass Criteria**: Rate change visible, persists across reset

### SIT-10: Diagnostic Interface End-to-End

**Verifies**: SYS-046 to SYS-051 (Diagnostics), Integration Phase IS-5
**Precondition**: SIT-03 pass
**Steps**:
1. Connect to Linux TCP:5002 via TCP client
2. Send `status` command — verify system state response
3. Send `sensor read 0` — verify live sensor reading
4. Send `actuator set 0 75` — verify actuator set, PWM changes on MCU
5. Send `version` — verify both Linux and MCU firmware versions
6. Send `reset mcu` — verify MCU resets and recovers
7. Disconnect and reconnect — verify new session works
**Expected**: Diagnostic commands affect real system state
**Pass Criteria**: All commands produce correct responses and effects

### SIT-11: Watchdog Recovery

**Verifies**: SYS-069 to SYS-071 (Watchdog), Integration Phase IS-5
**Precondition**: SIT-03 pass
**Steps**:
1. Freeze MCU via QEMU QMP `stop` command
2. Wait 2 seconds (watchdog timeout)
3. Verify QEMU detects watchdog reset (MCU restarts)
4. Verify MCU boots to safe state (PWM = 0%)
5. Verify MCU re-establishes TCP connections
6. Verify Linux detects reconnection and syncs state
**Expected**: Watchdog triggers reset, system recovers
**Pass Criteria**: Reset within 2 seconds, full recovery within 10 seconds

### SIT-12: Full System Stress Test

**Verifies**: SYS-066 to SYS-068 (Performance), Integration Phase IS-5
**Precondition**: All SIT-01 through SIT-11 pass
**Steps**:
1. Configure all 4 sensors at 100 Hz
2. Send actuator commands at 50/sec (max rate)
3. Run continuously for 5 minutes
4. Monitor: CPU usage, memory usage, message loss rate, latency
5. Inject 3 communication faults during the run
6. Verify recovery from each fault
**Expected**: System maintains operation under full load with fault recovery
**Pass Criteria**: Zero message corruption, latency ≤ 50ms (p99), recovery ≤ 10s

## 4. Traceability

| Test ID | System Requirements Verified |
|---------|------------------------------|
| SIT-01 | SYS-050 |
| SIT-02 | SYS-030, SYS-031 |
| SIT-03 | SYS-032, SYS-033, SYS-034 |
| SIT-04 | SYS-001 to SYS-008, SYS-013 to SYS-015 |
| SIT-05 | SYS-016 to SYS-020 |
| SIT-06 | SYS-038 to SYS-041 |
| SIT-07 | SYS-038, SYS-039, SYS-026 to SYS-029 |
| SIT-08 | SYS-042 to SYS-045 |
| SIT-09 | SYS-058 to SYS-065 |
| SIT-10 | SYS-046 to SYS-051 |
| SIT-11 | SYS-069 to SYS-071 |
| SIT-12 | SYS-066 to SYS-068 |

## 5. Pass/Fail Criteria

- **Pass**: All 12 tests pass
- **Conditional Pass**: ≥10 tests pass, failing tests are non-safety-critical
- **Fail**: Any safety-critical test fails (SIT-05, SIT-07, SIT-08, SIT-11)
