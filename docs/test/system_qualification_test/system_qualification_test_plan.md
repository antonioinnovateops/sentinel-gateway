---
title: "System Qualification Test Plan"
document_id: "SQTP-001"
project: "Sentinel Gateway"
version: "1.0.0"
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
- Complete system running in SIL environment

## 3. Qualification Tests

### SQT-01: Multi-Sensor Data Collection

**Validates**: STKH-001
**Acceptance**: ≥4 sensor channels, configurable sample rates
**Steps**:
1. Configure 4 sensor channels at different rates (10, 20, 50, 100 Hz)
2. Run for 60 seconds
3. Retrieve logged data from Linux filesystem
4. Verify all 4 channels present with correct sample counts (±5%)
**Verdict Criteria**: All 4 channels collecting at configured rates

### SQT-02: Real-Time Sensor Sampling

**Validates**: STKH-002
**Acceptance**: Jitter ≤ 500 µs at 100 Hz
**Steps**:
1. Configure all channels at 100 Hz
2. Collect 1000 consecutive samples
3. Compute inter-sample interval statistics (mean, stddev, max)
4. Verify jitter ≤ 500 µs
**Verdict Criteria**: Statistical jitter within specification

### SQT-03: Sensor Data Logging

**Validates**: STKH-003
**Acceptance**: Data logged with ≤1ms timestamp resolution
**Steps**:
1. Run system for 10 minutes
2. Retrieve `sensor_data.jsonl` from Linux filesystem
3. Verify timestamp monotonicity
4. Verify timestamp resolution (sub-millisecond granularity)
5. Verify data format is parseable JSON Lines
**Verdict Criteria**: All data logged with correct timestamps

### SQT-04: Remote Actuator Command

**Validates**: STKH-004
**Acceptance**: Commands applied within 20ms
**Steps**:
1. Send 100 actuator commands via TCP
2. Measure round-trip time for each (send → response)
3. Verify 99th percentile latency ≤ 20ms
4. Verify PWM outputs match commanded values
**Verdict Criteria**: Latency and accuracy within specification

### SQT-05: Actuator Safety Limits

**Validates**: STKH-005
**Acceptance**: Out-of-range commands rejected with error
**Steps**:
1. Configure actuator limits: min=0%, max=95%
2. Send command with value=50% — verify accepted
3. Send command with value=100% — verify rejected with error
4. Send command with value=-10% — verify rejected with error
5. Verify actuator did not change from 50% after rejected commands
**Verdict Criteria**: Invalid commands rejected, actuator unchanged

### SQT-06: Actuator Fail-Safe

**Validates**: STKH-006
**Acceptance**: Safe state within 2 seconds of fault
**Steps**:
1. Set actuator to 75%
2. Trigger communication fault (disconnect bridge)
3. Measure time until MCU sets actuator to 0%
4. Verify time ≤ 2 seconds
5. Verify actuator remains at 0% until recovery
**Verdict Criteria**: Safe state reached within timeout

### SQT-07: Structured Communication Protocol

**Validates**: STKH-007
**Acceptance**: Versioned, type-safe protocol
**Steps**:
1. Capture TCP traffic between Linux and MCU
2. Verify all messages use protobuf encoding (not raw binary)
3. Verify protocol version field present in messages
4. Verify message type field matches content
5. Send malformed protobuf — verify rejection (no crash)
**Verdict Criteria**: Protocol is structured, versioned, robust

### SQT-08: Communication Loss Detection

**Validates**: STKH-008
**Acceptance**: Loss detected within 3 seconds
**Steps**:
1. Establish normal communication
2. Disconnect at random time during operation
3. Measure detection time on both Linux and MCU
4. Repeat 5 times
5. All detection times must be ≤ 3 seconds
**Verdict Criteria**: 100% detection within 3 second window

### SQT-09: Automatic Recovery

**Validates**: STKH-009
**Acceptance**: Recovery within 5 seconds, ≥90% success for transient faults
**Steps**:
1. Trigger 10 transient communication faults (disconnect 2s, reconnect)
2. Count successful automatic recoveries
3. Measure recovery time for each
4. Verify ≥9 of 10 recover successfully
5. Verify recovery times ≤ 5 seconds
**Verdict Criteria**: ≥90% success rate, recovery time within spec

### SQT-10: Remote Diagnostic Access

**Validates**: STKH-010
**Acceptance**: CLI accessible via network
**Steps**:
1. Connect to TCP:5002 from test harness
2. Execute each diagnostic command: `status`, `sensor read`, `actuator set`, `version`, `reset`, `help`
3. Verify each returns meaningful response
4. Verify multiple simultaneous connections (up to 3)
**Verdict Criteria**: All commands work, concurrent access supported

### SQT-11: Comprehensive Logging

**Validates**: STKH-011
**Acceptance**: Logs persist, retrievable via diagnostic interface
**Steps**:
1. Generate various events (sensor reads, commands, errors, state changes)
2. Verify all events appear in `events.jsonl`
3. Reset Linux gateway process
4. Verify logs persist after restart
5. Query logs via diagnostic interface
**Verdict Criteria**: All event types logged, persistent

### SQT-12: Firmware Version Reporting

**Validates**: STKH-012
**Acceptance**: Versions include build date and commit hash
**Steps**:
1. Query version via diagnostic interface (`version` command)
2. Verify Linux version string format: `major.minor.patch-hash`
3. Verify MCU version string format: `major.minor.patch-hash`
4. Verify both versions are non-empty
**Verdict Criteria**: Both versions reportable

### SQT-13: Runtime Configuration

**Validates**: STKH-013
**Acceptance**: Config applied within 1 second, persists across resets
**Steps**:
1. Change sensor rate via ConfigUpdate
2. Measure time to apply (observe rate change)
3. Verify application within 1 second
4. Reset MCU
5. Verify config persists after reset
**Verdict Criteria**: Fast application, persistent storage

### SQT-14: Configuration Validation

**Validates**: STKH-014
**Acceptance**: Invalid values rejected with clear error
**Steps**:
1. Send valid config (rate=50 Hz) — verify accepted
2. Send invalid config (rate=0 Hz) — verify rejected with error message
3. Send invalid config (rate=999 Hz) — verify rejected with error message
4. Verify system still operates with last valid config
**Verdict Criteria**: Invalid configs rejected, system stable

### SQT-15: Continuous Operation

**Validates**: STKH-015
**Acceptance**: MTBF ≥ 8,760 hours (demonstrated by stability test)
**Steps**:
1. Run full system for 4 hours continuously
2. Monitor: memory usage, CPU usage, message counts, error counts
3. Verify no memory leaks (RSS stable ±5%)
4. Verify no message loss
5. Verify no unrecoverable errors
**Verdict Criteria**: 4-hour stability with no degradation

### SQT-16: Watchdog Protection

**Validates**: STKH-016
**Acceptance**: Watchdog triggers within 2 seconds of hang
**Steps**:
1. Freeze MCU via QEMU `stop` command
2. Measure time to watchdog reset
3. Verify ≤ 2 seconds
4. Verify MCU reboots to safe state after reset
**Verdict Criteria**: Watchdog fires, MCU recovers

### SQT-17: No Dynamic Memory on MCU

**Validates**: STKH-017
**Acceptance**: Static analysis confirms zero heap usage
**Steps**:
1. Run `arm-none-eabi-nm` on firmware ELF — search for malloc/free symbols
2. Verify linker script has no .heap section
3. Verify `__HEAP_SIZE=0` in compilation flags
4. Run cppcheck with MISRA Rule 21.3 check
**Verdict Criteria**: Zero evidence of heap allocation

### SQT-18: ASPICE CL2 Compliance

**Validates**: STKH-018
**Acceptance**: All work products complete
**Steps**:
1. Verify all ASPICE process work products exist (SYS.1–SYS.5, SWE.1–SWE.6, SUP, MAN)
2. Verify all documents have YAML front matter metadata
3. Verify document cross-references are valid
4. Verify traceability matrices are complete (zero orphans)
**Verdict Criteria**: Full documentation set with no gaps

### SQT-19: Full Traceability

**Validates**: STKH-019
**Acceptance**: Zero orphaned requirements
**Steps**:
1. Parse STKH→SYS traceability — verify all 20 STKH map to SYS
2. Parse SYS→SWE traceability — verify all 80 SYS map to SWE
3. Parse SWE→ARCH traceability — verify all 80 SWE map to components
4. Verify test cases exist for all requirements
5. Count orphans in each direction
**Verdict Criteria**: Zero orphans in any direction

### SQT-20: MISRA C Compliance

**Validates**: STKH-020
**Acceptance**: Zero Required violations, documented Advisory deviations
**Steps**:
1. Run cppcheck with MISRA C:2012 addon on all source files
2. Count Required rule violations (must be 0)
3. Count Advisory rule violations
4. Verify all Advisory violations have documented deviations
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
