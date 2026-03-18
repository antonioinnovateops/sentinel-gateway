---
title: "Detailed Design — MCU Watchdog Manager (FW-05)"
document_id: "DD-FW05"
project: "Sentinel Gateway"
version: "1.0.0"
date: 2026-03-18
status: "Approved"
aspice_process: "SWE.3 Software Detailed Design"
component: "FW-05"
implements: ["SWE-069-1", "SWE-070-1", "SWE-071-1"]
---

# Detailed Design — MCU Watchdog Manager (FW-05)

## 1. Purpose

Manages the Independent Watchdog (IWDG) with 2-second timeout. Fed from the super-loop to detect firmware hangs. Tracks reset count in a persistent RAM variable.

## 2. Module Interface

```c
sentinel_err_t watchdog_mgr_init(void);    /* Configure IWDG, 2s timeout */
void watchdog_mgr_feed(void);              /* Feed watchdog — call every loop iteration */
uint32_t watchdog_mgr_get_reset_count(void); /* Number of watchdog resets since power-on */
```

## 3. Implementation Details

### 3.1 IWDG Configuration

- Prescaler: `/256`
- Reload: calculated for 2-second timeout
- Once started, IWDG cannot be stopped (hardware feature)
- Feed interval must be ≤ 500ms under worst-case loop timing

### 3.2 Feed Strategy

- `watchdog_mgr_feed()` called at end of each super-loop iteration
- If any loop iteration takes > 2 seconds, MCU resets automatically
- Safety: watchdog is the last line of defense against firmware hangs

### 3.3 Reset Counter

```c
/* Placed in .noinit section — survives reset but not power cycle */
static uint32_t __attribute__((section(".noinit"))) g_watchdog_reset_count;
```

- On init: if reset reason register indicates IWDG reset, increment counter
- Counter reported in HealthStatus messages
- Useful for diagnosing intermittent hangs

## 4. Safety Classification

**ASIL-B**: Watchdog is a safety mechanism. Failure to feed → MCU reset → safe state (PWM=0%).

## 5. Traceability

| Requirement | Function |
|-------------|----------|
| SWE-069-1 | `watchdog_mgr_init()` — IWDG configuration |
| SWE-070-1 | `watchdog_mgr_feed()` — periodic feed |
| SWE-071-1 | `watchdog_mgr_get_reset_count()` — reset tracking |
