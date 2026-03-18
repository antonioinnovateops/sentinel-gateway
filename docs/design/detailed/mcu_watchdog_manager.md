---
title: "Detailed Design — MCU Watchdog Manager (FW-05)"
document_id: "DD-FW05"
project: "Sentinel Gateway"
version: "1.1.0"
date: 2026-03-18
status: "Approved"
aspice_process: "SWE.3 Software Detailed Design"
component: "FW-05"
implements: ["SWE-069-1", "SWE-070-1", "SWE-071-1"]
---

# Detailed Design — MCU Watchdog Manager (FW-05)

## 1. Purpose

Manages the Independent Watchdog (IWDG) with 2-second timeout. Fed from the super-loop to detect firmware hangs. Tracks reset count in a persistent RAM variable for diagnostics.

## 2. Implementation Reference

| Item | Value |
|------|-------|
| Source file | `src/mcu/watchdog_mgr.c` |
| Header file | `src/mcu/watchdog_mgr.h` |
| HAL driver | `src/mcu/hal/watchdog_driver.c` |
| QEMU HAL | `src/mcu/hal/qemu/watchdog_driver_qemu.c` |
| Lines | ~30 (mgr), ~60 (driver) |
| CMake target | `sentinel-mcu` or `sentinel-mcu-qemu` |
| Build command | `docker compose run --rm mcu-build` |
| QEMU build | `docker compose run --rm mcu-build-qemu` |

## 3. Module Interface

### 3.1 Function Signatures (Exact)

```c
#include "../common/sentinel_types.h"

sentinel_err_t watchdog_mgr_init(void);
void watchdog_mgr_feed(void);
uint32_t watchdog_mgr_get_reset_count(void);
```

### 3.2 Error Handling Contract

| Function | Returns | Condition |
|----------|---------|-----------|
| `watchdog_mgr_init()` | `SENTINEL_OK` | Always succeeds (IWDG started) |
| `watchdog_mgr_init()` | HAL error | `iwdg_init()` failed (rare) |
| `watchdog_mgr_feed()` | void | Always succeeds |
| `watchdog_mgr_get_reset_count()` | uint32_t | Count since power-on |

## 4. Implementation

### 4.1 Watchdog Manager

```c
/* Placed in .noinit section — survives reset but not power cycle */
static uint32_t g_reset_count __attribute__((section(".noinit")));

sentinel_err_t watchdog_mgr_init(void)
{
    /* Check if last reset was IWDG-caused */
    if (iwdg_was_reset_cause()) {
        g_reset_count++;
    }

    /* Start IWDG with 2-second timeout */
    return iwdg_init(SENTINEL_WATCHDOG_TIMEOUT_MS);
}

void watchdog_mgr_feed(void)
{
    iwdg_feed();
}

uint32_t watchdog_mgr_get_reset_count(void)
{
    return g_reset_count;
}
```

### 4.2 Watchdog Timeout

```c
/* From sentinel_types.h */
#define SENTINEL_WATCHDOG_TIMEOUT_MS  2000U  /* 2 seconds */
```

## 5. HAL Interface

### 5.1 IWDG Driver API

```c
/* From watchdog_driver.h */
sentinel_err_t iwdg_init(uint32_t timeout_ms);
void iwdg_feed(void);
bool iwdg_was_reset_cause(void);
```

### 5.2 STM32 Implementation

```c
sentinel_err_t iwdg_init(uint32_t timeout_ms)
{
    /* IWDG clock: ~32 kHz LSI
     * Prescaler /256 = 125 Hz
     * Reload = timeout_ms * 125 / 1000 */
    uint32_t reload = (timeout_ms * 125U) / 1000U;

    IWDG->KR = IWDG_KEY_ACCESS;   /* Enable register write */
    IWDG->PR = IWDG_PR_DIV256;    /* Prescaler /256 */
    IWDG->RLR = reload;           /* Reload value */
    IWDG->KR = IWDG_KEY_START;    /* Start watchdog */

    return SENTINEL_OK;
}

void iwdg_feed(void)
{
    IWDG->KR = IWDG_KEY_RELOAD;
}

bool iwdg_was_reset_cause(void)
{
    return (RCC_CSR & RCC_CSR_IWDGRSTF) != 0U;
}
```

### 5.3 QEMU Implementation

QEMU's MPS2-AN505 uses the CMSDK watchdog peripheral:

```c
/* Maps to CMSDK Watchdog at 0x40081000 */
sentinel_err_t iwdg_init(uint32_t timeout_ms)
{
    /* Unlock watchdog */
    WDOG->LOCK = CMSDK_WDOG_UNLOCK_KEY;

    /* Set reload value (25 MHz clock) */
    uint32_t reload = (timeout_ms * 25000U);
    WDOG->LOAD = reload;

    /* Enable with reset capability */
    WDOG->CTRL = CMSDK_WDOG_CTRL_INTEN | CMSDK_WDOG_CTRL_RESEN;

    return SENTINEL_OK;
}
```

## 6. Feed Strategy

### 6.1 Super-Loop Integration

```c
/* From main.c super-loop */
for (;;) {
    tcp_stack_poll();
    /* ... other processing ... */
    safety_monitor_tick(now_ms);

    /* Feed watchdog — MUST be last */
    watchdog_mgr_feed();
}
```

**Critical**: Watchdog feed is the LAST operation in the loop. This ensures all loop steps completed successfully before feeding.

### 6.2 Timing Requirements

| Metric | Value | Notes |
|--------|-------|-------|
| Watchdog timeout | 2000 ms | IWDG fires if not fed |
| Max loop time | ~3 ms | Well within timeout |
| Safety margin | 1997 ms | Handles occasional delays |

## 7. Reset Counter

### 7.1 Persistent Storage

```c
static uint32_t g_reset_count __attribute__((section(".noinit")));
```

The `.noinit` section is not zeroed on startup, so the counter survives watchdog resets. It IS zeroed on power cycle (RAM contents lost).

### 7.2 Linker Script Support

```ld
/* From mps2_an505.ld / stm32u575.ld */
.noinit (NOLOAD) :
{
    *(.noinit)
} > RAM
```

### 7.3 Reset Cause Detection

On STM32:
```c
#define RCC_CSR_IWDGRSTF  (1UL << 29)
bool iwdg_was_reset_cause(void)
{
    return (RCC_CSR & RCC_CSR_IWDGRSTF) != 0U;
}
```

On QEMU (simulated):
```c
bool iwdg_was_reset_cause(void)
{
    return g_qemu_rcc_csr != 0U;  /* Simulated flag */
}
```

## 8. Dependencies

| Dependency | Include | Purpose |
|------------|---------|---------|
| sentinel_types.h | `#include "../common/sentinel_types.h"` | Constants |
| watchdog_driver.h | `#include "hal/watchdog_driver.h"` | IWDG HAL |

## 9. Safety Classification

**ASIL-B**: Watchdog is a safety mechanism. Failure to feed triggers MCU reset, which:
1. Stops all PWM output (hardware default = 0%)
2. Increments reset counter (diagnostic)
3. Re-initializes firmware in safe state

## 10. Implementation Lessons

1. **Once started, IWDG cannot be stopped**: This is a hardware security feature. Debug builds may use longer timeouts.

2. **QEMU watchdog behavior**: QEMU's CMSDK watchdog works but may have timing differences. Test on real hardware for production validation.

3. **Reset count overflow**: Counter is uint32_t. At worst case (reset every 2s), overflow takes 272 years.

4. **Feed timing margin**: 2000ms timeout with ~3ms loop time provides >99% margin. Even blocking operations up to 1.9s would not trigger reset.

5. **No watchdog in unit tests**: Unit tests on host don't initialize hardware. Watchdog functions are stubbed or not called.

## 11. Traceability

| Requirement | Implementation | Status |
|-------------|----------------|--------|
| SWE-069-1 | `watchdog_mgr_init()` — IWDG configuration | Complete |
| SWE-070-1 | `watchdog_mgr_feed()` — periodic feed | Complete |
| SWE-071-1 | `watchdog_mgr_get_reset_count()` — reset tracking | Complete |
