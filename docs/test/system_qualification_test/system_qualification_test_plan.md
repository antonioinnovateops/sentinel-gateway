---
title: "System Qualification Test Plan"
document_id: "SQTP-001"
project: "Sentinel Gateway"
version: "2.0.0"
date: 2026-03-18
status: "Approved"
aspice_process: "SYS.5 System Qualification Test"
verifies: "STKH-REQ-001 (Stakeholder Requirements)"
---

# System Qualification Test Plan — Sentinel Gateway

## 1. Purpose

This document specifies system-level qualification tests per ASPICE SYS.5. These tests validate that the integrated system satisfies the original stakeholder requirements (STKH-REQ-001). Each test traces directly to a stakeholder requirement.

**Distinction from SYS.4**: SYS.4 (SITP-001) verifies system element integration. SYS.5 validates the complete system against stakeholder needs — the top of the V-model's right side.

## 2. Test Strategy

### 2.1 Approach

Black-box testing against stakeholder acceptance criteria. Tests are written from the stakeholder perspective — not implementation details.

### 2.2 Prerequisites

- All SYS.4 tests (SITP-001) pass
- All SWE.6 tests (QTP-001) pass
- Complete system running in QEMU SIL environment

### 2.3 Test Environment

**QEMU User-Mode Architecture**: Tests run with MCU firmware executing via `qemu-arm-static` (user-mode emulation), communicating with native Linux gateway over localhost TCP.

```bash
# Start system for qualification testing
./build-linux/sentinel_gateway &
qemu-arm-static ./build-qemu-mcu/sentinel_mcu_qemu &
```

### 2.4 Diagnostic Interface

All human interaction uses the diagnostic CLI on port 5002.

**Critical**: Commands are **lowercase only, case-sensitive**:
- Valid: `status`, `sensor read 0`, `actuator set 0 50`, `version`, `help`
- Invalid: `STATUS`, `SENSOR READ 0`, `GET_STATUS`

## 3. Qualification Tests

### SQT-01: Multi-Sensor Data Collection

**Validates**: STKH-001
**Acceptance**: ≥4 sensor channels, configurable sample rates
**Steps**:
1. Connect to diagnostic CLI: `nc localhost 5002`
2. Configure 4 sensor channels at different rates:
   - `config set channel0_rate 10`
   - `config set channel1_rate 20`
   - `config set channel2_rate 50`
   - `config set channel3_rate 100`
3. Run for 60 seconds
4. Retrieve logged data: `cat sensor_data.jsonl | wc -l`
5. Verify all 4 channels present with correct sample counts (±5%):
   - CH0: ~600 samples, CH1: ~1200, CH2: ~3000, CH3: ~6000
**Verdict Criteria**: All 4 channels collecting at configured rates

### SQT-02: Real-Time Sensor Sampling

**Validates**: STKH-002
**Acceptance**: Jitter ≤ 500 µs at 100 Hz
**Steps**:
1. Configure all channels at 100 Hz: `config set channel0_rate 100` (repeat for all)
2. Collect 1000 consecutive samples from telemetry port
3. Parse timestamps from wire frames (type=0x01)
4. Compute inter-sample interval statistics (mean, stddev, max)
5. Verify jitter ≤ 500 µs
**Verdict Criteria**: Statistical jitter within specification

### SQT-03: Sensor Data Logging

**Validates**: STKH-003
**Acceptance**: Data logged with ≤1ms timestamp resolution
**Steps**:
1. Run system for 10 minutes
2. Retrieve `sensor_data.jsonl` from Linux filesystem
3. Parse JSON lines, verify format: `{"channel": N, "raw": N, "calibrated": N.N, "timestamp": "..."}`
4. Verify timestamp monotonicity (each > previous)
5. Verify timestamp resolution (sub-millisecond granularity available)
**Verdict Criteria**: All data logged with correct timestamps

### SQT-04: Remote Actuator Command

**Validates**: STKH-004
**Acceptance**: Commands applied within 20ms
**Steps**:
1. Connect to diagnostic CLI: `nc localhost 5002`
2. Send 100 actuator commands: `actuator set 0 N` for various N
3. Measure round-trip time for each (send → response)
4. Verify 99th percentile latency ≤ 20ms
5. Send `status` and verify actuator values match last commands
**Verdict Criteria**: Latency and accuracy within specification

### SQT-05: Actuator Safety Limits

**Validates**: STKH-005
**Acceptance**: Out-of-range commands rejected with error
**Steps**:
1. Default actuator limits: min=0%, max=95%
2. Send `actuator set 0 50` — verify response: `OK ACTUATOR 0 SET 50.0%`
3. Send `actuator set 0 100` — verify response: `ERROR OUT_OF_RANGE` or clamped
4. Send `actuator set 0 -10` — verify response: `ERROR OUT_OF_RANGE`
5. Send `status` — verify actuator did not exceed limits
**Verdict Criteria**: Invalid commands rejected, actuator unchanged

### SQT-06: Actuator Fail-Safe

**Validates**: STKH-006
**Acceptance**: Safe state within 3 seconds of fault
**Steps**:
1. Set actuator to 75%: `actuator set 0 75`
2. Verify via `status`: actuator at 75%
3. Kill MCU process: `pkill sentinel_mcu_qemu`
4. Start timer
5. Wait for Linux to detect loss and report FAILSAFE
6. Restart MCU: `qemu-arm-static ./build-qemu-mcu/sentinel_mcu_qemu &`
7. Send `status` — verify actuator at 0% (safe state)
8. Verify time from kill to safe state ≤ 3 seconds
**Verdict Criteria**: Safe state reached within timeout

### SQT-07: Structured Communication Protocol

**Validates**: STKH-007
**Acceptance**: Versioned, type-safe protocol
**Steps**:
1. Capture TCP traffic on port 5001 (telemetry)
2. Verify wire frame format: `[4-byte LE length][1-byte type][protobuf payload]`
3. Verify message type field (0x01=SensorData, 0x04=HealthStatus)
4. Verify protobuf decodes correctly with schema
5. Send malformed data to MCU command port — verify rejection (no crash)
**Verdict Criteria**: Protocol is structured, versioned, robust

### SQT-08: Communication Loss Detection

**Validates**: STKH-008
**Acceptance**: Loss detected within 3 seconds
**Steps**:
1. Establish normal communication (verify `status` shows CONNECTED)
2. Kill MCU process (simulate comm loss)
3. Start timer
4. Monitor Linux logs for "COMM_LOSS" event
5. Verify detection within 3 seconds
6. Repeat 5 times, all must detect within 3 seconds
**Verdict Criteria**: 100% detection within 3 second window

### SQT-09: Automatic Recovery

**Validates**: STKH-009
**Acceptance**: Recovery within 10 seconds, ≥90% success for transient faults
**Steps**:
1. Trigger 10 transient communication faults:
   - Kill MCU process
   - Wait 2 seconds
   - Restart MCU process
2. Count successful automatic recoveries (status returns to NORMAL)
3. Measure recovery time for each
4. Verify ≥9 of 10 recover successfully
5. Verify recovery times ≤ 10 seconds
**Verdict Criteria**: ≥90% success rate, recovery time within spec

### SQT-10: Remote Diagnostic Access

**Validates**: STKH-010
**Acceptance**: CLI accessible via network
**Steps**:
1. Connect to TCP:5002: `nc localhost 5002`
2. Execute each diagnostic command:
   - `status` — system state
   - `sensor read 0` — live reading
   - `actuator set 0 50` — command actuator
   - `version` — firmware versions
   - `reset mcu` — trigger MCU reset
   - `help` — command list
3. Verify each returns meaningful response
4. **Note**: Only single client supported (epoll design limitation)
**Verdict Criteria**: All commands work

### SQT-11: Comprehensive Logging

**Validates**: STKH-011
**Acceptance**: Logs persist, retrievable via diagnostic interface
**Steps**:
1. Generate various events:
   - Sensor reads (automatic)
   - Actuator commands: `actuator set 0 50`
   - Config changes: `config set channel0_rate 50`
   - Errors: `actuator set 0 999` (generates error)
2. Verify events in `events.jsonl`
3. Kill and restart Linux gateway process
4. Verify logs persist after restart
5. Query logs via `log 10` command
**Verdict Criteria**: All event types logged, persistent

### SQT-12: Firmware Version Reporting

**Validates**: STKH-012
**Acceptance**: Versions include build date and commit hash
**Steps**:
1. Send `version` command via diagnostic CLI
2. Verify response format:
   ```
   Linux: 1.0.0-abc1234
   MCU: 1.0.0-def5678
   ```
3. Verify both versions are non-empty
4. Verify hash portion matches git commit
**Verdict Criteria**: Both versions reportable

### SQT-13: Runtime Configuration

**Validates**: STKH-013
**Acceptance**: Config applied within 1 second, persists across resets
**Steps**:
1. Query current config: `config get`
2. Change sensor rate: `config set channel0_rate 50`
3. Measure time until rate change observed (count messages/second)
4. Verify application within 1 second
5. Kill and restart MCU process
6. After recovery, verify config persisted: `config get` shows rate=50
**Verdict Criteria**: Fast application, persistent storage

### SQT-14: Configuration Validation

**Validates**: STKH-014
**Acceptance**: Invalid values rejected with clear error
**Steps**:
1. Send valid config: `config set channel0_rate 50` — verify `OK`
2. Send invalid config: `config set channel0_rate 0` — verify `ERROR OUT_OF_RANGE`
3. Send invalid config: `config set channel0_rate 999` — verify `ERROR OUT_OF_RANGE`
4. Send `status` — verify system still operates with last valid config
**Verdict Criteria**: Invalid configs rejected, system stable

### SQT-15: Continuous Operation

**Validates**: STKH-015
**Acceptance**: MTBF ≥ 8,760 hours (demonstrated by stability test)
**Steps**:
1. Run full system for 4 hours continuously
2. Monitor:
   - Memory usage: `ps aux | grep sentinel`
   - Message counts: count lines in logs
   - Error counts: grep ERROR in logs
3. Verify no memory leaks (RSS stable ±5%)
4. Verify no message loss
5. Verify no unrecoverable errors
**Verdict Criteria**: 4-hour stability with no degradation

### SQT-16: Watchdog Protection

**Validates**: STKH-016
**Acceptance**: Watchdog triggers within 2 seconds of hang
**Steps**:
1. Pause MCU process: `kill -STOP $(pgrep sentinel_mcu_qemu)`
2. Start timer
3. Wait for MCU process to exit (watchdog in user-mode triggers exit)
4. Measure time — verify ≤ 2 seconds
5. Restart MCU: `qemu-arm-static ./build-qemu-mcu/sentinel_mcu_qemu &`
6. Verify MCU boots to safe state (actuators at 0%)
**Verdict Criteria**: Watchdog fires, MCU recovers

### SQT-17: No Dynamic Memory on MCU

**Validates**: STKH-017
**Acceptance**: Static analysis confirms zero heap usage
**Steps**:
1. Run `arm-none-eabi-nm build-mcu/sentinel_mcu.elf | grep -E "(malloc|free|realloc|calloc)"`
2. Verify output is empty (no heap functions)
3. Check linker script for `.heap` section — must not exist or size=0
4. Run cppcheck with MISRA Rule 21.3 check (stdlib dynamic memory)
**Verdict Criteria**: Zero evidence of heap allocation

### SQT-18: ASPICE CL2 Compliance

**Validates**: STKH-018
**Acceptance**: All work products complete
**Steps**:
1. Verify all ASPICE process work products exist:
   - SYS.1–SYS.5: stakeholder_requirements.md, SRS.md, SAD.md, SITP-001, SQTP-001
   - SWE.1–SWE.6: SWRS.md, SWAD.md, detailed designs, UTP-001, ITP-001, QTP-001
   - SUP: traceability matrices
   - MAN: project plans
2. Verify all documents have YAML front matter metadata
3. Verify document cross-references are valid
4. Verify traceability matrices are complete
**Verdict Criteria**: Full documentation set with no gaps

### SQT-19: Full Traceability

**Validates**: STKH-019
**Acceptance**: Zero orphaned requirements
**Steps**:
1. Run traceability checker: `python3 scripts/trace_check.py`
2. Parse STKH→SYS: verify all 20 STKH map to SYS
3. Parse SYS→SWE: verify all 80 SYS map to SWE
4. Parse SWE→ARCH: verify all SWE map to components
5. Verify test cases exist for all requirements
6. Count orphans — must be zero
**Verdict Criteria**: Zero orphans in any direction

### SQT-20: MISRA C Compliance

**Validates**: STKH-020
**Acceptance**: Zero Required violations, documented Advisory deviations
**Steps**:
1. Run cppcheck with MISRA C:2012 addon:
   ```bash
   cppcheck --addon=misra src/mcu/ --xml 2> misra_report.xml
   ```
2. Parse report, count Required rule violations (must be 0)
3. Count Advisory rule violations
4. Verify all Advisory violations have documented deviations in `docs/compliance/misra_deviations.md`
**Verdict Criteria**: Zero Required, all Advisory documented

## 4. Traceability Matrix

| Test | STKH Req | Priority | Safety-Critical |
|------|----------|----------|:---:|
| SQT-01 | STKH-001 | Must Have | No |
| SQT-02 | STKH-002 | Must Have | No |
| SQT-03 | STKH-003 | Must Have | No |
| SQT-04 | STKH-004 | Must Have | No |
| SQT-05 | STKH-005 | Must Have | Yes |
| SQT-06 | STKH-006 | Must Have | Yes |
| SQT-07 | STKH-007 | Must Have | No |
| SQT-08 | STKH-008 | Must Have | Yes |
| SQT-09 | STKH-009 | Must Have | Yes |
| SQT-10 | STKH-010 | Must Have | No |
| SQT-11 | STKH-011 | Must Have | No |
| SQT-12 | STKH-012 | Should Have | No |
| SQT-13 | STKH-013 | Must Have | No |
| SQT-14 | STKH-014 | Must Have | No |
| SQT-15 | STKH-015 | Must Have | No |
| SQT-16 | STKH-016 | Must Have | Yes |
| SQT-17 | STKH-017 | Must Have | No |
| SQT-18 | STKH-018 | Must Have | No |
| SQT-19 | STKH-019 | Must Have | No |
| SQT-20 | STKH-020 | Must Have | No |

**Coverage**: 20/20 stakeholder requirements (100%)

## 5. Pass/Fail Criteria

- **Pass**: All 20 tests pass
- **Conditional Pass**: ≥18 pass, no safety-critical test fails
- **Fail**: Any safety-critical test fails (SQT-05, SQT-06, SQT-08, SQT-09, SQT-16)
