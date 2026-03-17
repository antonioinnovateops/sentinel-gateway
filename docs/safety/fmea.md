---
title: "Failure Mode and Effects Analysis (FMEA)"
document_id: "FMEA-001"
project: "Sentinel Gateway"
version: "1.0.0"
date: 2026-03-17
status: "Approved"
---

# FMEA — Sentinel Gateway

## Severity / Occurrence / Detection Scale

**Severity (S)**: 1 = No effect, 5 = Loss of function, 10 = Safety hazard
**Occurrence (O)**: 1 = Extremely unlikely, 5 = Moderate, 10 = Certain
**Detection (D)**: 1 = Certain detection, 5 = Moderate detection, 10 = No detection
**RPN** = S × O × D (Risk Priority Number, action needed if RPN > 100)

## FMEA Table

| ID | Component | Failure Mode | Effect | S | Cause | O | Detection Method | D | RPN | Mitigation | Req |
|----|-----------|-------------|--------|---|-------|---|-----------------|---|-----|------------|-----|
| FM-01 | ADC Driver | Wrong channel reading | Incorrect sensor data logged | 4 | Register misconfiguration | 2 | Cross-check with known reference | 3 | 24 | Self-test on init, verify channel mapping | SYS-001 |
| FM-02 | ADC Driver | ADC conversion stuck | No sensor data | 5 | Peripheral hang | 2 | Watchdog timer, conversion timeout | 2 | 20 | Timeout + watchdog reset | SYS-011 |
| FM-03 | Sensor Acquisition | Sample timing drift | Jitter exceeds spec | 3 | Interrupt latency | 3 | Statistical timing analysis | 4 | 36 | Hardware timer interrupt, measure jitter | SYS-010 |
| FM-04 | Protobuf Handler (MCU) | Message encode failure | No telemetry data sent | 5 | Buffer overflow, schema mismatch | 2 | Return code check, sequence gap detection | 2 | 20 | Static buffer allocation, version check | SYS-034 |
| FM-05 | Protobuf Handler (MCU) | Message decode failure | Invalid command executed | 8 | Corrupted data, version mismatch | 2 | CRC/checksum, protobuf validation | 2 | 32 | Validate all decoded fields, fail-safe on error | SYS-028 |
| FM-06 | Actuator Control | PWM output stuck high | Fan/valve at 100% | 8 | Timer register corruption | 2 | PWM output readback verification | 2 | 32 | Readback check, fail-safe transition | SYS-016 |
| FM-07 | Actuator Control | PWM output stuck low | Fan/valve off (safe state) | 3 | Timer disable, GPIO fault | 2 | Health status reporting | 3 | 18 | Report via HealthStatus, no safety impact (IS safe state) | SYS-027 |
| FM-08 | Actuator Control | Out-of-range command applied | Equipment damage | 9 | Range validation bypass | 1 | Dual validation (input + output check) | 2 | 18 | Double range check: on receive AND before PWM write | SYS-022 |
| FM-09 | USB CDC-ECM | USB link drops | Communication loss | 6 | USB enumeration failure, cable issue | 3 | Heartbeat timeout detection | 2 | 36 | Reconnection with exponential backoff | SYS-043 |
| FM-10 | TCP Stack | TCP connection reset | Temporary data loss | 5 | Network buffer overflow | 3 | Sequence number gap detection | 2 | 30 | Reconnection + state resync | SYS-044 |
| FM-11 | Watchdog Manager | Watchdog not fed | MCU reset | 4 | Main loop blocked | 3 | Reset counter tracking | 1 | 12 | By design (watchdog IS the safety mechanism) | SYS-069 |
| FM-12 | Watchdog Manager | Watchdog disabled | No reset on firmware hang | 9 | Configuration error | 1 | Code review, IWDG cannot be disabled after start | 1 | 9 | Use IWDG (independent, non-disableable) | SYS-069 |
| FM-13 | Config Store | Flash CRC mismatch | Config corruption detected | 4 | Power loss during write | 2 | CRC-32 validation on read | 1 | 8 | Revert to factory defaults | SYS-065 |
| FM-14 | Config Store | Flash write failure | Config not persisted | 4 | Flash wear-out, write error | 1 | Write verification (read-back) | 2 | 8 | Retry with error logging | SYS-060 |
| FM-15 | Health Monitor (Linux) | False comm loss detection | Unnecessary recovery cycle | 4 | Processing delay, missed heartbeat | 2 | 3-consecutive-miss policy | 2 | 16 | Require 3 missed heartbeats, not just 1 | SYS-039 |
| FM-16 | Health Monitor (Linux) | Missed real comm loss | Actuators remain active without supervision | 8 | Heartbeat falsely generated | 1 | MCU-side independent comm monitor | 2 | 16 | MCU independently monitors Linux silence (SYS-040) | SYS-040 |
| FM-17 | Diagnostics Server | Unauthorized command | Unintended actuator change | 7 | No authentication on diag port | 3 | Auth token validation | 2 | 42 | Require auth token for write commands | SYS-025 |
| FM-18 | Main Loop (MCU) | Infinite loop / hang | All functions stop | 8 | Software bug | 2 | Hardware watchdog | 1 | 16 | IWDG resets MCU, safe state on boot | SYS-069 |

## High-RPN Actions

All RPNs are below 100 threshold. The highest RPNs are:

1. **FM-17** (RPN 42): Diagnostics authentication — addressed by auth token requirement
2. **FM-03** (RPN 36): Sample timing drift — addressed by hardware timer interrupt
3. **FM-09** (RPN 36): USB link drops — addressed by reconnection mechanism
4. **FM-05** (RPN 32): Protobuf decode failure — addressed by validation + fail-safe
5. **FM-06** (RPN 32): PWM stuck high — addressed by output readback

## Safety Mechanisms Summary

| Mechanism | Protects Against | Independent? | Reference |
|-----------|-----------------|:---:|-----------|
| IWDG Watchdog | Firmware hang | ✓ (independent clock) | SYS-069 |
| Comm timeout (MCU) | Linux failure | ✓ (MCU-side timer) | SYS-040 |
| Comm timeout (Linux) | MCU failure | ✓ (Linux-side timer) | SYS-039 |
| Range validation | Out-of-range commands | ✓ (MCU enforced) | SYS-022 |
| Config CRC | Flash corruption | ✓ (math check) | SYS-065 |
| Sequence numbers | Message loss/replay | ✓ (per message) | SYS-036 |
| Safe state on boot | Post-reset danger | ✓ (hardware init) | SYS-027 |
