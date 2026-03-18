---
title: "Detailed Design — MCU Safety Monitor (FW-07)"
document_id: "DD-FW07"
project: "Sentinel Gateway"
version: "1.1.0"
date: 2026-03-18
status: "Approved"
aspice_process: "SWE.3 Software Detailed Design"
component: "FW-07"
implements: ["SWE-026-1", "SWE-027-1", "SWE-028-1", "SWE-029-1", "SWE-038-1", "SWE-039-1", "SWE-040-1", "SWE-041-1"]
---

# Detailed Design — MCU Safety Monitor (FW-07)

## 1. Purpose

Central safety component implementing the operational state machine (INIT → NORMAL → FAILSAFE), health status generation, fault detection, and fail-safe enforcement. **ASIL-B** classified.

## 2. Implementation Reference

| Item | Value |
|------|-------|
| Source file | `src/mcu/health_reporter.c` |
| Header file | `src/mcu/health_reporter.h` |
| Lines | ~110 |
| CMake target | `sentinel-mcu` or `sentinel-mcu-qemu` |
| Build command | `docker compose run --rm mcu-build` |
| QEMU build | `docker compose run --rm mcu-build-qemu` |

**Note**: Implementation is in `health_reporter.c` which combines safety monitor state machine with health reporting functionality.

## 3. Module Interface

### 3.1 Function Signatures (Exact)

```c
#include "../common/sentinel_types.h"
#include <stdbool.h>

/* Health data for encoding */
typedef struct {
    system_state_t state;
    uint32_t uptime_s;
    uint32_t watchdog_resets;
    float fan_duty;
    float valve_duty;
    fault_id_t last_fault;
    bool comm_ok;
    uint32_t free_stack;
} health_data_t;

sentinel_err_t safety_monitor_init(void);
void safety_monitor_tick(uint32_t now_ms);
void safety_monitor_report_fault(fault_id_t fault);
system_state_t safety_monitor_get_state(void);
bool safety_monitor_is_failsafe(void);
void safety_monitor_set_comm_ok(bool ok);
void safety_monitor_set_tcp_connected(bool connected);
sentinel_err_t safety_monitor_get_health(health_data_t *data);
```

### 3.2 Error Handling Contract

| Function | Returns | Condition |
|----------|---------|-----------|
| `safety_monitor_init()` | `SENTINEL_OK` | Always succeeds |
| `safety_monitor_get_health()` | `SENTINEL_ERR_INVALID_ARG` | NULL output pointer |
| Other functions | void | No error returns |

## 4. State Machine

```c
/* From sentinel_types.h */
typedef enum {
    STATE_INIT     = 0,  /* Waiting for TCP connection */
    STATE_NORMAL   = 1,  /* Full operation */
    STATE_DEGRADED = 2,  /* Partial operation (unused) */
    STATE_FAILSAFE = 3,  /* Safe state, actuators at 0% */
    STATE_ERROR    = 4   /* Fatal error (unused) */
} system_state_t;
```

![State Machine Diagram](../../architecture/diagrams/state_machine.drawio.svg)

### 4.1 State Transitions

| From | To | Trigger | Entry Action |
|------|----|---------|--------------|
| STATE_INIT | STATE_NORMAL | TCP connected | Update `g_last_comm_ms` |
| STATE_NORMAL | STATE_FAILSAFE | Comm timeout (3s) | `actuator_failsafe()` |
| STATE_NORMAL | STATE_FAILSAFE | Actuator readback fail | `actuator_failsafe()` |
| STATE_NORMAL | STATE_FAILSAFE | `safety_monitor_report_fault()` | `actuator_failsafe()` |
| STATE_FAILSAFE | STATE_NORMAL | State sync from Linux | Via `safety_monitor_set_tcp_connected()` |
| Any unknown | STATE_FAILSAFE | Default case | `actuator_failsafe()` |

### 4.2 Transition Implementation

```c
void safety_monitor_tick(uint32_t now_ms)
{
    switch (g_state) {
        case STATE_INIT:
            if (g_tcp_connected) {
                g_state = STATE_NORMAL;
                g_last_comm_ms = now_ms;
            }
            break;

        case STATE_NORMAL:
            /* Check communication timeout */
            if ((now_ms - g_last_comm_ms) >= SENTINEL_COMM_TIMEOUT_MS) {
                safety_monitor_report_fault(FAULT_COMM_LOSS);
            }

            /* Verify actuator readback */
            for (uint8_t i = 0U; i < SENTINEL_MAX_ACTUATORS; i++) {
                if (!actuator_verify_readback(i)) {
                    safety_monitor_report_fault(FAULT_ACTUATOR_READBACK);
                }
            }
            break;

        case STATE_FAILSAFE:
            /* Keep actuators at zero, wait for recovery */
            (void)actuator_failsafe();
            break;

        case STATE_DEGRADED:
        case STATE_ERROR:
        default:
            /* Unknown state → failsafe */
            g_state = STATE_FAILSAFE;
            (void)actuator_failsafe();
            break;
    }
}
```

## 5. Fault Handling

### 5.1 Fault Catalogue

```c
/* From sentinel_types.h */
typedef enum {
    FAULT_NONE             = 0,
    FAULT_COMM_LOSS        = 1,  /* No Linux messages for 3s */
    FAULT_ACTUATOR_READBACK = 2, /* PWM register != commanded */
    FAULT_ADC_TIMEOUT      = 3,  /* ADC not complete in 100ms */
    FAULT_FLASH_CRC        = 4,  /* Config CRC mismatch */
    FAULT_STACK_OVERFLOW   = 5   /* Stack pointer near limit */
} fault_id_t;
```

### 5.2 Fault Reporting

```c
void safety_monitor_report_fault(fault_id_t fault)
{
    g_last_fault = fault;
    g_state = STATE_FAILSAFE;
    (void)actuator_failsafe();  /* PWM to 0% immediately */
}
```

All faults result in immediate transition to FAILSAFE with actuators set to 0%.

## 6. Health Reporting

### 6.1 Health Data Collection

```c
sentinel_err_t safety_monitor_get_health(health_data_t *data)
{
    if (data == NULL) {
        return SENTINEL_ERR_INVALID_ARG;
    }

    uint32_t now_ms = systick_get_ms();

    data->state = g_state;
    data->uptime_s = (now_ms - g_boot_ms) / 1000U;
    data->watchdog_resets = watchdog_mgr_get_reset_count();
    data->last_fault = g_last_fault;
    data->comm_ok = g_comm_ok;

    /* Read current actuator duties */
    (void)actuator_get(0U, &data->fan_duty);
    (void)actuator_get(1U, &data->valve_duty);

    /* Stack monitoring (simplified) */
    data->free_stack = 0U;  /* Would need SP intrinsic */

    return SENTINEL_OK;
}
```

### 6.2 Health Message Contents

| Field | Source | Update Rate |
|-------|--------|-------------|
| state | `g_state` | Every message |
| uptime_s | `systick_get_ms() - g_boot_ms` | Every message |
| watchdog_resets | `watchdog_mgr_get_reset_count()` | Every message |
| fan_duty | `actuator_get(0, ...)` | Every message |
| valve_duty | `actuator_get(1, ...)` | Every message |
| last_fault | `g_last_fault` | Every message |
| comm_ok | `g_comm_ok` | Every message |
| free_stack | Not implemented | Always 0 |

## 7. Communication Tracking

### 7.1 TCP Connection Status

```c
void safety_monitor_set_tcp_connected(bool connected)
{
    g_tcp_connected = connected;
    if (connected && (g_state == STATE_FAILSAFE)) {
        /* Recovery requires explicit state sync — stay in failsafe
         * until state sync is processed */
    }
    if (connected && (g_state == STATE_INIT)) {
        g_state = STATE_NORMAL;
        g_last_comm_ms = systick_get_ms();
    }
}
```

### 7.2 Message Reception Tracking

```c
void safety_monitor_set_comm_ok(bool ok)
{
    g_comm_ok = ok;
    if (ok) {
        g_last_comm_ms = systick_get_ms();
    }
}
```

Called from `on_command_received()` in main.c when any valid command arrives.

## 8. Static State

```c
static system_state_t g_state = STATE_INIT;
static fault_id_t g_last_fault = FAULT_NONE;
static bool g_comm_ok = false;
static bool g_tcp_connected = false;
static uint32_t g_last_comm_ms = 0U;
static uint32_t g_boot_ms = 0U;
```

## 9. Dependencies

| Dependency | Include | Purpose |
|------------|---------|---------|
| sentinel_types.h | `#include "../common/sentinel_types.h"` | Types, enums |
| actuator_control.h | `#include "actuator_control.h"` | Failsafe, readback verify |
| watchdog_mgr.h | `#include "watchdog_mgr.h"` | Reset count |
| systick.h | `#include "hal/systick.h"` | Timestamps |

## 10. Safety Mechanisms

| Mechanism | Implementation |
|-----------|----------------|
| Defensive state transition | Unknown state → FAILSAFE |
| Fail-safe default | All faults → FAILSAFE |
| Independent actuator monitoring | `actuator_verify_readback()` |
| Redundant timeout | Both safety_monitor and actuator_control track comm |
| Immediate failsafe action | `actuator_failsafe()` called on fault |

## 11. Implementation Lessons

1. **Recovery requires state sync**: TCP reconnection alone doesn't exit FAILSAFE. Explicit MSG_TYPE_STATE_SYNC_REQ required.

2. **Actuator readback in NORMAL only**: Readback verification runs every tick in NORMAL state.

3. **Boot time tracking**: `g_boot_ms` captured in init for accurate uptime calculation.

4. **Stack monitoring not implemented**: `free_stack` field always returns 0. Would require SP intrinsic and linker symbol for stack base.

5. **Comm timeout constant**: `SENTINEL_COMM_TIMEOUT_MS = 3000` from sentinel_types.h.

## 12. Traceability

| Requirement | Implementation | Status |
|-------------|----------------|--------|
| SWE-026-1 | State machine — enter FAILSAFE on fault | Complete |
| SWE-027-1 | FAILSAFE entry — all PWM to 0% | Complete |
| SWE-028-1 | `safety_monitor_is_failsafe()` — reject commands | Complete |
| SWE-029-1 | Recovery — FAILSAFE → NORMAL via state sync | Complete |
| SWE-038-1 | `safety_monitor_get_health()` — health data | Complete |
| SWE-039-1 | `safety_monitor_tick()` — periodic health check | Complete |
| SWE-040-1 | Health message content (state, uptime, faults) | Complete |
| SWE-041-1 | 1-second health reporting interval | Via main loop |
