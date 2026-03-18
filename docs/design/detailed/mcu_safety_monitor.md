---
title: "Detailed Design — MCU Safety Monitor (FW-07)"
document_id: "DD-FW07"
project: "Sentinel Gateway"
version: "1.0.0"
date: 2026-03-18
status: "Approved"
aspice_process: "SWE.3 Software Detailed Design"
component: "FW-07"
implements: ["SWE-026-1", "SWE-027-1", "SWE-028-1", "SWE-029-1", "SWE-038-1", "SWE-039-1", "SWE-040-1", "SWE-041-1"]
---

# Detailed Design — MCU Safety Monitor (FW-07)

## 1. Purpose

Central safety component. Monitors system health, manages operational state machine (INIT → NORMAL → FAILSAFE), generates HealthStatus messages, and enforces fail-safe behavior. **ASIL-B** classified.

## 2. Module Interface

```c
sentinel_err_t safety_monitor_init(void);
void safety_monitor_tick(void);              /* Called every super-loop iteration */
void safety_monitor_report_fault(fault_id_t fault);
system_state_t safety_monitor_get_state(void);
sentinel_err_t safety_monitor_encode_health(uint8_t *buf, size_t *len);
bool safety_monitor_is_failsafe(void);
```

## 3. State Machine

See diagram: `../../architecture/diagrams/state_machine.drawio.svg`

| State | Entry Action | Behavior |
|-------|-------------|----------|
| INIT | Set all PWM=0%, start watchdog | Accept no commands, wait for TCP |
| NORMAL | Enable command processing | Full operation |
| FAILSAFE | Set all PWM=0%, reject commands | Only health reporting and recovery |

### 3.1 Transition Rules

| From | To | Condition |
|------|----|-----------|
| INIT | NORMAL | TCP connected + first heartbeat sent |
| NORMAL | FAILSAFE | Comm timeout (3s) OR actuator readback fail OR explicit fault |
| FAILSAFE | NORMAL | State sync from Linux (only via full recovery) |

## 4. Health Reporting

Every 1 second (configurable), encode and send HealthStatus containing:
- `state`: INIT / NORMAL / FAILSAFE
- `uptime_ms`: milliseconds since boot
- `watchdog_reset_count`: from watchdog_mgr
- `sensor_status`: per-channel OK/FAULT
- `actuator_status`: per-actuator OK/FAULT/FAILSAFE
- `last_fault`: most recent fault code
- `comm_ok`: true if Linux messages received recently

## 5. Fault Catalogue

| Fault ID | Trigger | Action |
|----------|---------|--------|
| FAULT_COMM_LOSS | No Linux messages for 3s | Enter FAILSAFE |
| FAULT_ACTUATOR_READBACK | PWM register != commanded value | Enter FAILSAFE |
| FAULT_ADC_TIMEOUT | ADC scan not complete within 100ms | Log warning |
| FAULT_FLASH_CRC | Config CRC mismatch on load | Use defaults, log error |
| FAULT_STACK_OVERFLOW | Stack pointer near limit | Enter FAILSAFE |

## 6. Safety Mechanisms

- **Defensive programming**: All state transitions validated
- **Fail-safe default**: Unknown state → FAILSAFE
- **Independent monitoring**: safety_monitor checks actuator_control output
- **Redundant timeout**: Both safety_monitor and actuator_control track comm timeout

## 7. Traceability

| Requirement | Function |
|-------------|----------|
| SWE-026-1 | State machine — enter FAILSAFE on fault |
| SWE-027-1 | FAILSAFE entry action — all PWM to 0% |
| SWE-028-1 | `safety_monitor_is_failsafe()` — reject commands in FAILSAFE |
| SWE-029-1 | Recovery — FAILSAFE → NORMAL only via state sync |
| SWE-038-1 | `safety_monitor_encode_health()` — health message generation |
| SWE-039-1 | `safety_monitor_tick()` — periodic health check |
| SWE-040-1 | Health message content (state, uptime, faults) |
| SWE-041-1 | 1-second health reporting interval |
