---
title: "Detailed Design — Linux Health Monitor (SW-05)"
document_id: "DD-SW05"
project: "Sentinel Gateway"
version: "1.1.0"
date: 2026-03-18
status: "Approved"
aspice_process: "SWE.3 Software Detailed Design"
component: "SW-05"
implements: ["SWE-042-1", "SWE-043-1", "SWE-043-2", "SWE-044-1", "SWE-045-1"]
---

# Detailed Design — Linux Health Monitor (SW-05)

## 1. Purpose

Monitors MCU health via HealthStatus messages on TCP:5001. Detects communication loss within 3 seconds. Implements recovery state machine with USB power cycle, TCP reconnect, and state sync capabilities.

## 2. Implementation Reference

| Item | Value |
|------|-------|
| Source file | `src/linux/health_monitor.c` |
| Header file | `src/linux/health_monitor.h` |
| Lines | ~100 |
| CMake target | `sentinel-gw` |
| Build command | `docker compose run --rm linux-build` |
| Test command | `docker compose run --rm linux-build ctest --output-on-failure` |

## 3. Module Interface

### 3.1 Function Signatures (Exact)

```c
#include "../common/sentinel_types.h"
#include <stdbool.h>

/* From health_monitor.h (internal structure) */
typedef struct {
    system_state_t state;
    uint32_t uptime_s;
    uint32_t watchdog_resets;
    bool comm_ok;
    uint64_t last_health_ms;
} linux_health_state_t;

sentinel_err_t health_monitor_init(void);

sentinel_err_t health_monitor_process_health(const uint8_t *payload, size_t len);

void health_monitor_tick(uint64_t now_ms);

sentinel_err_t health_monitor_get_state(linux_health_state_t *out);
```

### 3.2 Error Handling Contract

| Function | Returns | Recovery |
|----------|---------|----------|
| `health_monitor_init()` | `SENTINEL_OK` | Always succeeds (state reset) |
| `health_monitor_process_health()` | `SENTINEL_OK` | Placeholder for protobuf decode |
| `health_monitor_get_state()` | `SENTINEL_ERR_INVALID_ARG` | NULL output pointer |

## 4. State Machine

```c
typedef enum {
    HM_MONITORING = 0,  /* Normal operation, watching for health messages */
    HM_COMM_LOSS,       /* Health timeout detected, starting recovery */
    HM_RECOVERING,      /* Active recovery in progress */
    HM_FAULT            /* Recovery failed, manual intervention needed */
} hm_state_t;
```

### 4.1 State Transitions

| From | To | Trigger | Action |
|------|----|---------|--------|
| HM_MONITORING | HM_COMM_LOSS | 3s without HealthStatus | Log "COMM_LOSS" event |
| HM_COMM_LOSS | HM_RECOVERING | Immediate | Increment recovery attempts, log "RECOVERY_START" |
| HM_RECOVERING | HM_MONITORING | Health message received | Reset attempt counter, log "RECOVERY" |
| HM_RECOVERING | HM_FAULT | 3 failed attempts | Log "FAULT" event |
| HM_FAULT | — | Manual reset only | Stay in fault state |

![State Machine Diagram](../../architecture/diagrams/state_machine.drawio.svg)

## 5. Processing Logic

### 5.1 Health Message Processing (`health_monitor_process_health`)

```c
sentinel_err_t health_monitor_process_health(const uint8_t *payload, size_t len)
{
    /* TODO: Decode HealthStatus protobuf */
    (void)payload;
    (void)len;

    g_last_health_received_ms = get_ms();
    g_health.last_health_ms = g_last_health_received_ms;
    g_health.comm_ok = true;

    /* Recovery: if we were in COMM_LOSS/RECOVERING, MCU is back */
    if (g_hm_state == HM_COMM_LOSS || g_hm_state == HM_RECOVERING) {
        g_hm_state = HM_MONITORING;
        g_recovery_attempts = 0U;
        logger_write_event("RECOVERY", "Communication restored");
    }

    return SENTINEL_OK;
}
```

### 5.2 Periodic Tick (`health_monitor_tick`)

Called from gateway_core's event loop with current timestamp:

```c
void health_monitor_tick(uint64_t now_ms)
{
    switch (g_hm_state) {
        case HM_MONITORING:
            if ((now_ms - g_last_health_received_ms) >= SENTINEL_COMM_TIMEOUT_MS) {
                g_hm_state = HM_COMM_LOSS;
                g_health.comm_ok = false;
                logger_write_event("COMM_LOSS", "MCU health timeout");
            }
            break;

        case HM_COMM_LOSS:
            g_hm_state = HM_RECOVERING;
            g_recovery_attempts++;
            logger_write_event("RECOVERY_START", "Attempting reconnection");
            break;

        case HM_RECOVERING:
            if (g_recovery_attempts >= 3U) {
                g_hm_state = HM_FAULT;
                logger_write_event("FAULT", "Recovery failed after 3 attempts");
            }
            /* TODO: USB power cycle, TCP reconnect, state sync */
            break;

        case HM_FAULT:
            /* Stay in fault until manual intervention */
            break;

        default:
            g_hm_state = HM_FAULT;
            break;
    }
}
```

### 5.3 Timeout Constant

```c
/* From sentinel_types.h */
#define SENTINEL_COMM_TIMEOUT_MS  3000U  /* 3 seconds */
```

## 6. Recovery Sequence (Future Implementation)

When fully implemented, recovery will:
1. USB power cycle via QEMU monitor (or `uhubctl` on real hardware)
2. Wait for USB re-enumeration (timeout 5s)
3. Wait for TCP reconnection (timeout 5s)
4. Send StateSyncRequest
5. Receive StateSyncResponse → verify state
6. If success → transition to HM_MONITORING
7. If failure → increment attempt counter, retry (max 3)

## 7. Dependencies

| Dependency | Include | Purpose |
|------------|---------|---------|
| sentinel_types.h | `#include "../common/sentinel_types.h"` | Types, constants |
| logger.h | `#include "logger.h"` | Event logging |
| time.h | `#include <time.h>` | clock_gettime |
| string.h | `#include <string.h>` | memset |
| stdio.h | `#include <stdio.h>` | Debug output |

## 8. Static State

```c
static hm_state_t g_hm_state = HM_MONITORING;
static linux_health_state_t g_health;
static uint32_t g_recovery_attempts = 0U;
static uint64_t g_last_health_received_ms = 0U;
```

## 9. Implementation Lessons

1. **`_POSIX_C_SOURCE` required**: Set to 200809L at top of file for `clock_gettime()`.

2. **Protobuf decode placeholder**: `health_monitor_process_health()` currently ignores payload content but updates timing and state.

3. **Recovery not implemented**: USB power cycle and state sync are marked TODO. Recovery logic transitions states but doesn't perform actual recovery actions.

4. **Immediate state transition**: COMM_LOSS → RECOVERING happens on next tick, not after a delay.

5. **Default state on unknown**: Any unexpected state value triggers transition to HM_FAULT for safety.

## 10. Traceability

| Requirement | Implementation | Status |
|-------------|----------------|--------|
| SWE-042-1 | `health_monitor_process_health()` — process MCU health | Partial (no protobuf decode) |
| SWE-043-1 | `health_monitor_tick()` — detect communication loss | Complete |
| SWE-043-2 | Recovery sequence — TCP reconnection | Stub (state machine only) |
| SWE-044-1 | Recovery sequence — USB power cycle | Not implemented |
| SWE-045-1 | Recovery sequence — state sync after reconnect | Not implemented |
