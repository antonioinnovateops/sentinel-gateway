---
title: "Detailed Design — Linux Config Manager (SW-07)"
document_id: "DD-SW07"
project: "Sentinel Gateway"
version: "1.1.0"
date: 2026-03-18
status: "Approved"
aspice_process: "SWE.3 Software Detailed Design"
component: "SW-07"
implements: ["SWE-058-2", "SWE-061-1"]
---

# Detailed Design — Linux Config Manager (SW-07)

## 1. Purpose

Manages system configuration on the Linux side. Loads defaults on startup, optionally parses JSON config file, sends ConfigUpdate messages to MCU, processes ConfigResponse, and provides configuration query interface for diagnostics.

## 2. Implementation Reference

| Item | Value |
|------|-------|
| Source file | `src/linux/config_manager.c` |
| Header file | `src/linux/config_manager.h` |
| Lines | ~70 |
| CMake target | `sentinel-gw` |
| Build command | `docker compose run --rm linux-build` |
| Test command | `docker compose run --rm linux-build ctest --output-on-failure` |

## 3. Module Interface

### 3.1 Function Signatures (Exact)

```c
#include "../common/sentinel_types.h"
#include <stddef.h>

sentinel_err_t config_manager_init(const char *config_file_path);

sentinel_err_t config_manager_update_sensor_rate(uint8_t channel, uint32_t rate_hz);

sentinel_err_t config_manager_update_actuator_limits(uint8_t actuator,
                                                      float min_pct, float max_pct);

sentinel_err_t config_manager_get_current(system_config_t *out);

sentinel_err_t config_manager_process_response(const uint8_t *payload, size_t len);
```

### 3.2 Error Handling Contract

| Function | Returns | Recovery |
|----------|---------|----------|
| `config_manager_init()` | `SENTINEL_OK` | Always succeeds (loads defaults) |
| `config_manager_update_sensor_rate()` | `SENTINEL_ERR_INVALID_ARG` | channel ≥ `SENTINEL_MAX_CHANNELS` (4) |
| `config_manager_update_sensor_rate()` | `SENTINEL_ERR_OUT_OF_RANGE` | rate_hz < 1 or > 100 |
| `config_manager_update_actuator_limits()` | `SENTINEL_ERR_INVALID_ARG` | actuator ≥ `SENTINEL_MAX_ACTUATORS` (2) |
| `config_manager_update_actuator_limits()` | `SENTINEL_ERR_INVALID_ARG` | min_pct ≥ max_pct |
| `config_manager_get_current()` | `SENTINEL_ERR_INVALID_ARG` | NULL output pointer |
| `config_manager_process_response()` | `SENTINEL_OK` | Placeholder for protobuf decode |

## 4. Data Structure

```c
/* Static configuration (config_manager.c:12) */
static system_config_t g_config;

/* From sentinel_types.h */
typedef struct {
    uint32_t sensor_rates_hz[SENTINEL_MAX_CHANNELS];     /* 4 channels */
    float    actuator_min_percent[SENTINEL_MAX_ACTUATORS]; /* 2 actuators */
    float    actuator_max_percent[SENTINEL_MAX_ACTUATORS];
    uint32_t heartbeat_interval_ms;
    uint32_t comm_timeout_ms;
} system_config_t;
```

## 5. Processing Logic

### 5.1 Initialization (`config_manager_init`)

```c
static void load_defaults(void)
{
    for (uint8_t i = 0U; i < SENTINEL_MAX_CHANNELS; i++) {
        g_config.sensor_rates_hz[i] = 10U;
    }
    g_config.actuator_min_percent[0] = 0.0f;
    g_config.actuator_min_percent[1] = 0.0f;
    g_config.actuator_max_percent[0] = 95.0f;
    g_config.actuator_max_percent[1] = 95.0f;
    g_config.heartbeat_interval_ms = SENTINEL_HEALTH_INTERVAL_MS;  /* 1000 */
    g_config.comm_timeout_ms = SENTINEL_COMM_TIMEOUT_MS;           /* 3000 */
}
```

1. Always load defaults first
2. If `config_file_path != NULL`, parse JSON and override defaults (**TODO**)
3. Return `SENTINEL_OK`

### 5.2 Update Operations

Sensor rate update:
1. Validate channel < 4
2. Validate rate in range 1–100 Hz
3. Update `g_config.sensor_rates_hz[channel]`
4. **TODO**: Send ConfigUpdate protobuf to MCU

Actuator limits update:
1. Validate actuator < 2
2. Validate min_pct < max_pct
3. Update both limits in `g_config`
4. **TODO**: Send ConfigUpdate protobuf to MCU

### 5.3 Default Values

| Parameter | Default | Valid Range |
|-----------|---------|-------------|
| Sensor rates | 10 Hz (all channels) | 1–100 Hz |
| Actuator min | 0% (both) | 0–94% |
| Actuator max | 95% (both) | 1–95% |
| Heartbeat interval | 1000 ms | Compile-time constant |
| Comm timeout | 3000 ms | Compile-time constant |

## 6. Dependencies

| Dependency | Include | Purpose |
|------------|---------|---------|
| sentinel_types.h | `#include "../common/sentinel_types.h"` | Types, constants |
| string.h | `#include <string.h>` | memset |
| stdio.h | `#include <stdio.h>` | Future: JSON parsing |

## 7. Implementation Lessons

1. **No JSON parsing yet**: The `config_file_path` parameter is accepted but not used. Implementation uses defaults only.

2. **No MCU communication**: Config updates are stored locally but not transmitted to MCU. Requires protobuf integration.

3. **Thread safety**: Single-threaded design. If multi-threading is added, `g_config` access needs mutex protection.

4. **Symmetric with MCU config_store**: Linux config_manager matches MCU config_store defaults for consistency.

## 8. Traceability

| Requirement | Function | Status |
|-------------|----------|--------|
| SWE-058-2 | `config_manager_update_*()` — send config updates to MCU | Local only (no protobuf) |
| SWE-061-1 | `config_manager_get_current()` — read-back current config | Complete |
