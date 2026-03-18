---
title: "System Integration Test Plan"
document_id: "SITP-001"
project: "Sentinel Gateway"
version: "2.0.0"
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

### 2.2 QEMU User-Mode Architecture

**Critical Implementation Note**: The SIL environment uses QEMU **user-mode emulation** (qemu-arm-static), NOT full system emulation. This means:
- No TAP bridges or virtual network interfaces
- Both processes run on the same host, communicating via localhost TCP
- MCU process uses POSIX sockets (not lwIP)
- Communication loss is simulated by process termination, not network disruption

```
┌─────────────────────────────────────────────────────────────┐
│                    Docker Container                          │
│  ┌──────────────────────┐    ┌────────────────────────────┐ │
│  │  Linux Gateway        │    │  MCU QEMU User-Mode        │ │
│  │  (x86_64 native)      │◄──►│  (qemu-arm-static)         │ │
│  │  sentinel_gateway     │    │  sentinel_mcu_qemu         │ │
│  └──────────┬────────────┘    └─────────────┬──────────────┘ │
│             │                               │                │
│             └───────────┬───────────────────┘                │
│                         │                                    │
│              localhost:5000, 5001, 5002                       │
└─────────────────────────────────────────────────────────────┘
```

### 2.3 Integration Sequence

| Phase | Elements Integrated | Prerequisite |
|-------|-------------------|-------------|
| IS-1 | SE-04 (Docker SIL) standalone | Docker installed, images built |
| IS-2 | SE-01 (Linux Gateway) in Docker | IS-1 pass, gateway binary built |
| IS-3 | SE-02 (MCU) via qemu-arm-static | IS-1 pass, MCU binary built |
| IS-4 | SE-03 (TCP Comm) between SE-01 + SE-02 | IS-2 + IS-3 pass |
| IS-5 | Full system: SE-01 + SE-02 + SE-03 on SE-04 | IS-4 pass |

### 2.4 Test Environment

All tests execute in the containerized SIL environment per SIL-001.

```bash
# Build SIL container
docker build -f docker/Dockerfile.qemu-sil -t sentinel-sil .

# Run SIL tests
docker run --rm sentinel-sil ./run_system_tests.sh
```

## 3. Integration Test Specifications

### SIT-01: Docker SIL Environment Boot

**Verifies**: SYS-050 (QEMU SIL), Integration Phase IS-1
**Precondition**: Docker images built
**Steps**:
1. Start Docker container: `docker run -it sentinel-sil /bin/bash`
2. Verify qemu-arm-static installed: `qemu-arm-static --version`
3. Verify ARM libraries present: `ls /usr/arm-linux-gnueabihf/lib/`
4. Verify build artifacts exist: `ls build-linux/ build-qemu-mcu/`
**Expected**: Container boots with all required tools
**Pass Criteria**: All binaries and tools present

### SIT-02: Localhost TCP Link Establishment

**Verifies**: SYS-030, SYS-031 (Communication), Integration Phase IS-4
**Precondition**: SIT-01 pass
**Implementation Note**: No USB CDC-ECM in user-mode. TCP over localhost instead.
**Steps**:
1. Start Linux gateway: `./build-linux/sentinel_gateway &`
2. Wait 2 seconds for port binding
3. Verify gateway listens: `ss -tln | grep 5002`
4. Start MCU QEMU: `qemu-arm-static ./build-qemu-mcu/sentinel_mcu_qemu &`
5. Wait for connection (check gateway log for "MCU connected")
6. Verify MCU connects to gateway on port 5000
7. Verify MCU opens telemetry port 5001
**Expected**: TCP connectivity established via localhost
**Pass Criteria**: Logs show bidirectional connection established

### SIT-03: TCP Channel Establishment

**Verifies**: SYS-032, SYS-033, SYS-034 (TCP ports), Integration Phase IS-4
**Precondition**: SIT-02 pass
**Steps**:
1. Verify gateway listens on TCP port 5000 (commands): `ss -tln | grep 5000`
2. Verify MCU connects to port 5000 (check logs)
3. Verify gateway listens on TCP port 5001 (telemetry): `ss -tln | grep 5001`
4. Verify MCU connects to port 5001 (check logs)
5. Verify gateway listens on TCP port 5002 (diagnostics): `ss -tln | grep 5002`
6. Verify external connection: `nc -z localhost 5002`
**Expected**: All 3 TCP channels active
**Pass Criteria**: TCP connections established, ports open

### SIT-04: End-to-End Sensor Data Flow

**Verifies**: SYS-001 to SYS-008 (Sensor Acquisition), Integration Phase IS-5
**Precondition**: SIT-03 pass
**Steps**:
1. MCU generates sensor data from ADC stub (4 channels)
2. Wait for MCU to transmit SensorData via TCP:5001
3. Verify Linux receives wire frame: `[4-byte len][1-byte type=0x01][payload]`
4. Decode protobuf payload, verify all fields present
5. Verify data logged to `sensor_data.jsonl` on Linux filesystem
6. Connect diagnostic CLI: `nc localhost 5002`
7. Send `sensor read 0` through `sensor read 3`
8. Verify readings match logged values (within timing tolerance)
**Expected**: Complete data path: ADC stub → MCU → wire frame → TCP → Linux → log
**Pass Criteria**: 4 sensor values received and logged correctly

### SIT-05: End-to-End Actuator Command Flow

**Verifies**: SYS-016 to SYS-020 (Actuator Control), Integration Phase IS-5
**Precondition**: SIT-03 pass
**Steps**:
1. Connect to diagnostic CLI: `nc localhost 5002`
2. Send `actuator set 0 50` (fan to 50%)
3. Verify gateway sends wire frame: `[4-byte len][1-byte type=0x02][protobuf]`
4. Verify MCU receives and decodes command
5. Verify MCU applies PWM duty cycle (check MCU debug log)
6. Verify MCU sends ActuatorResponse with status=OK
7. Verify CLI response: `OK ACTUATOR 0 SET 50.0%`
8. Measure end-to-end latency (CLI send → response)
**Expected**: Complete command path: Linux CLI → TCP → MCU → PWM stub → response
**Pass Criteria**: Latency ≤ 20ms, PWM stub shows 50%

### SIT-06: Heartbeat and Health Monitoring

**Verifies**: SYS-038 to SYS-041 (Health Monitoring), Integration Phase IS-5
**Precondition**: SIT-03 pass
**Steps**:
1. Verify MCU sends HealthStatus every 1 second via TCP:5001
2. Verify wire frame type = 0x04 (MSG_HEALTH_STATUS)
3. Verify Linux health_monitor receives and logs health messages
4. Verify health message contains: state, uptime, watchdog count, sensor status
5. Run for 30 seconds, count health messages
6. Verify ≥28 health messages received (allow 2 lost)
**Expected**: Continuous health monitoring active
**Pass Criteria**: ≥28/30 health messages received

### SIT-07: Communication Loss Detection

**Verifies**: SYS-038, SYS-039 (Loss Detection), Integration Phase IS-5
**Precondition**: SIT-06 pass
**Implementation Note**: Simulate comm loss by killing MCU process (no TAP bridge to disconnect).
**Steps**:
1. Normal operation for 5 seconds (baseline)
2. Kill MCU process: `pkill sentinel_mcu_qemu`
3. Start timer
4. Verify Linux detects loss within 3 seconds (check logs for "COMM_LOSS")
5. Verify Linux logs COMM_LOSS event to events.jsonl
6. Verify Linux enters recovery mode
**Expected**: Linux detects loss within timeout
**Pass Criteria**: Detection ≤ 3 seconds

### SIT-08: Automatic Recovery Sequence

**Verifies**: SYS-042 to SYS-045 (Recovery), Integration Phase IS-5
**Precondition**: SIT-07 pass
**Steps**:
1. Trigger communication loss (kill MCU process)
2. Wait for Linux to detect loss (3 seconds)
3. Restart MCU process: `qemu-arm-static ./build-qemu-mcu/sentinel_mcu_qemu &`
4. Verify TCP reconnection (check logs)
5. Verify Linux sends StateSyncRequest (type=0x07)
6. Verify MCU responds with StateSyncResponse (type=0x08)
7. Verify both sides return to NORMAL state
8. Verify sensor data and health messages resume
**Expected**: Full automatic recovery
**Pass Criteria**: Recovery complete within 10 seconds of MCU restart

### SIT-09: Configuration Update Across Boundary

**Verifies**: SYS-058 to SYS-065 (Configuration), Integration Phase IS-5
**Precondition**: SIT-03 pass
**Steps**:
1. Default sensor rate = 10 Hz (verify by counting messages/second)
2. Send `config set channel0_rate 50` via diagnostic CLI
3. Verify gateway sends ConfigUpdate (type=0x05)
4. Verify MCU validates and applies new rate
5. Verify MCU sends ConfigResponse with status=OK
6. Verify sensor CH0 now arrives at ~50 Hz (measure interval)
7. Kill and restart MCU process
8. After reconnect, verify CH0 still at 50 Hz (persisted in flash stub)
**Expected**: Config applied, persisted, survives restart
**Pass Criteria**: Rate change visible, persists across restart

### SIT-10: Diagnostic Interface End-to-End

**Verifies**: SYS-046 to SYS-051 (Diagnostics), Integration Phase IS-5
**Precondition**: SIT-03 pass
**Critical**: Commands are **lowercase only**.
**Steps**:
1. Connect to diagnostic port: `nc localhost 5002`
2. Send `status` — verify system state response
3. Send `sensor read 0` — verify live sensor reading
4. Send `actuator set 0 75` — verify actuator set, PWM changes on MCU
5. Send `version` — verify both Linux and MCU firmware versions
6. Send `reset mcu` — verify MCU resets (process exits, restarts)
7. Disconnect and reconnect — verify new session works
8. Test invalid commands:
   - `STATUS` → `ERROR UNKNOWN_COMMAND` (case-sensitive!)
   - `invalid` → `ERROR UNKNOWN_COMMAND`
**Expected**: Diagnostic commands affect real system state
**Pass Criteria**: All commands produce correct responses and effects

### SIT-11: Watchdog Recovery

**Verifies**: SYS-069 to SYS-071 (Watchdog), Integration Phase IS-5
**Precondition**: SIT-03 pass
**Implementation Note**: In user-mode QEMU, watchdog simulated by timer thread that calls exit(1).
**Steps**:
1. Pause MCU process with SIGSTOP: `kill -STOP $(pgrep sentinel_mcu_qemu)`
2. Wait 2 seconds (watchdog timeout)
3. Verify MCU process exits (check for exit code)
4. Resume/restart MCU: `qemu-arm-static ./build-qemu-mcu/sentinel_mcu_qemu &`
5. Verify MCU boots to safe state (actuators at 0%)
6. Verify MCU re-establishes TCP connections
7. Verify Linux detects reconnection and syncs state
**Expected**: Watchdog triggers exit, system recovers
**Pass Criteria**: Exit within 2 seconds, full recovery within 10 seconds

### SIT-12: Full System Stress Test

**Verifies**: SYS-066 to SYS-068 (Performance), Integration Phase IS-5
**Precondition**: All SIT-01 through SIT-11 pass
**Steps**:
1. Configure all 4 sensors at 100 Hz via `config set`
2. Start actuator command script (50 cmds/sec, max rate)
3. Run continuously for 5 minutes
4. Monitor: CPU usage, memory usage, message loss rate, latency
5. Kill MCU process 3 times during the run (simulate comm faults)
6. Verify automatic recovery from each fault
7. Collect statistics
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

## 5. Test Execution Script

```bash
#!/bin/bash
# run_system_tests.sh

set -e

echo "=== SIT-01: Environment Check ==="
qemu-arm-static --version
test -x build-linux/sentinel_gateway
test -x build-qemu-mcu/sentinel_mcu_qemu

echo "=== SIT-02/03: Starting System ==="
./build-linux/sentinel_gateway &
GATEWAY_PID=$!
sleep 2

qemu-arm-static ./build-qemu-mcu/sentinel_mcu_qemu &
MCU_PID=$!
sleep 3

echo "=== SIT-04: Sensor Data Test ==="
# Python test script handles wire frame parsing
python3 tests/integration/test_sensor_flow.py

echo "=== SIT-05: Actuator Command Test ==="
echo "actuator set 0 50" | nc -q1 localhost 5002 | grep "OK ACTUATOR"

echo "=== Cleanup ==="
kill $MCU_PID $GATEWAY_PID 2>/dev/null || true

echo "=== ALL TESTS PASSED ==="
```

## 6. Pass/Fail Criteria

- **Pass**: All 12 tests pass
- **Conditional Pass**: ≥10 tests pass, failing tests are non-safety-critical
- **Fail**: Any safety-critical test fails (SIT-05, SIT-07, SIT-08, SIT-11)
