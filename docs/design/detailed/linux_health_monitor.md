---
title: "Detailed Design — Linux Health Monitor (SW-05)"
document_id: "DD-SW05"
project: "Sentinel Gateway"
version: "1.0.0"
date: 2026-03-18
status: "Approved"
aspice_process: "SWE.3 Software Detailed Design"
component: "SW-05"
implements: ["SWE-042-1", "SWE-043-1", "SWE-043-2", "SWE-044-1", "SWE-045-1"]
---

# Detailed Design — Linux Health Monitor (SW-05)

## 1. Purpose

Monitors MCU health via HealthStatus messages on TCP:5001. Detects communication loss within 3 seconds. Triggers recovery sequence (USB power cycle, TCP reconnect, state sync).

## 2. Module Interface

```c
sentinel_err_t health_monitor_init(event_loop_t *loop, recovery_config_t *cfg);
sentinel_err_t health_monitor_process_health(const HealthStatus *msg);
sentinel_err_t health_monitor_get_state(health_state_t *out);
void health_monitor_tick(void);  /* Called every 500ms by event loop timer */
```

## 3. State Machine

States: `MONITORING` → `COMM_LOSS` → `RECOVERING` → `MONITORING` (or `FAULT`)

| From | To | Trigger |
|------|----|---------|
| MONITORING | COMM_LOSS | 3 seconds without HealthStatus |
| COMM_LOSS | RECOVERING | Auto (immediate recovery attempt) |
| RECOVERING | MONITORING | TCP reconnect + state sync success |
| RECOVERING | FAULT | 3 recovery attempts failed |
| FAULT | MONITORING | Manual reset via diagnostics |

See state machine diagram: `../../architecture/diagrams/state_machine.drawio.svg`

## 4. Processing Logic

### 4.1 Health Message Processing

1. Reset watchdog timer (3-second countdown)
2. Parse MCU state (NORMAL, FAILSAFE, INIT)
3. Log MCU uptime, watchdog reset count, sensor status
4. If MCU reports FAILSAFE → log alert, notify diagnostics

### 4.2 Communication Loss Detection (`health_monitor_tick`)

1. Check time since last HealthStatus
2. If ≥ 3 seconds → transition to COMM_LOSS
3. Log COMM_LOSS event with timestamp

### 4.3 Recovery Sequence

1. USB power cycle via QEMU monitor (or `uhubctl` on real HW)
2. Wait for USB re-enumeration (timeout 5s)
3. Wait for TCP reconnection (timeout 5s)
4. Send StateSyncRequest
5. Receive StateSyncResponse → verify state
6. If success → transition to MONITORING
7. If failure → increment attempt counter, retry (max 3)

## 5. Traceability

| Requirement | Function |
|-------------|----------|
| SWE-042-1 | `health_monitor_process_health()` — process MCU health messages |
| SWE-043-1 | `health_monitor_tick()` — detect communication loss |
| SWE-043-2 | Recovery sequence — TCP reconnection logic |
| SWE-044-1 | Recovery sequence — USB power cycle |
| SWE-045-1 | Recovery sequence — state sync after reconnect |
