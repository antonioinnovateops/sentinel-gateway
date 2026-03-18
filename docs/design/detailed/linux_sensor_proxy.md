---
title: "Detailed Design — Linux Sensor Proxy (SW-03)"
document_id: "DD-SW03"
project: "Sentinel Gateway"
version: "1.1.0"
date: 2026-03-18
status: "Approved"
aspice_process: "SWE.3 Software Detailed Design"
component: "SW-03"
implements: ["SWE-013-1", "SWE-013-2", "SWE-015-1"]
---

# Detailed Design — Linux Sensor Proxy (SW-03)

## 1. Purpose

Receives SensorData protobuf messages from MCU via TCP:5001, validates data integrity, forwards to logger, and maintains latest readings cache for diagnostic queries.

## 2. Implementation Reference

| Item | Value |
|------|-------|
| Source file | `src/linux/sensor_proxy.c` |
| Header file | `src/linux/sensor_proxy.h` |
| Lines | ~55 |
| CMake target | `sentinel-gw` |
| Build command | `docker compose run --rm linux-build` |
| Test command | `docker compose run --rm linux-build ctest --output-on-failure` |

## 3. Module Interface

### 3.1 Function Signatures (Exact)

```c
#include "../common/sentinel_types.h"
#include <stddef.h>
#include <stdint.h>

sentinel_err_t sensor_proxy_init(void);

sentinel_err_t sensor_proxy_process(const uint8_t *payload, size_t len);

sentinel_err_t sensor_proxy_get_latest(uint8_t channel, sensor_reading_t *out);

sentinel_err_t sensor_proxy_get_all(sensor_reading_t readings[4], uint32_t *count);
```

### 3.2 Error Handling Contract

| Function | Returns | Condition |
|----------|---------|-----------|
| `sensor_proxy_init()` | `SENTINEL_OK` | Always succeeds (memset only) |
| `sensor_proxy_process()` | `SENTINEL_OK` | Placeholder for protobuf decode |
| `sensor_proxy_get_latest()` | `SENTINEL_ERR_INVALID_ARG` | channel ≥ 4 or NULL output |
| `sensor_proxy_get_all()` | `SENTINEL_ERR_INVALID_ARG` | NULL readings or count pointer |

## 4. Data Structures

```c
/* From sentinel_types.h */
typedef struct {
    uint8_t  channel;
    uint32_t raw_value;
    float    calibrated_value;
    uint64_t timestamp_ms;
} sensor_reading_t;

/* Static state (sensor_proxy.c:13-15) */
static sensor_reading_t g_latest[SENTINEL_MAX_CHANNELS];  /* 4 channels */
static uint32_t g_msg_count = 0U;
static uint32_t g_err_count = 0U;
```

## 5. Processing Logic

### 5.1 Initialization (`sensor_proxy_init`)

```c
sentinel_err_t sensor_proxy_init(void)
{
    (void)memset(g_latest, 0, sizeof(g_latest));
    g_msg_count = 0U;
    g_err_count = 0U;
    return SENTINEL_OK;
}
```

### 5.2 Message Processing (`sensor_proxy_process`)

Current implementation (placeholder):

```c
sentinel_err_t sensor_proxy_process(const uint8_t *payload, size_t len)
{
    /* TODO: Decode SensorData protobuf into sensor_reading_t array
     * For each reading: validate channel, update g_latest, call logger_write_sensor
     */
    (void)payload;
    (void)len;
    g_msg_count++;
    return SENTINEL_OK;
}
```

When protobuf integration is complete:
1. Decode SensorData protobuf message
2. Validate sequence number > last seen (detect duplicates/reorder)
3. Validate channel count: 1 ≤ count ≤ `SENTINEL_MAX_CHANNELS`
4. For each sensor reading:
   - Validate channel ID (0–3)
   - Validate value range (per channel calibration bounds)
   - Update `g_latest[channel]` cache
5. Forward complete message to logger via `logger_write_sensor()`
6. Increment `g_msg_count`

### 5.3 Query Interface

```c
sentinel_err_t sensor_proxy_get_latest(uint8_t channel, sensor_reading_t *out)
{
    if ((channel >= SENTINEL_MAX_CHANNELS) || (out == NULL)) {
        return SENTINEL_ERR_INVALID_ARG;
    }
    *out = g_latest[channel];
    return SENTINEL_OK;
}

sentinel_err_t sensor_proxy_get_all(sensor_reading_t readings[4], uint32_t *count)
{
    if ((readings == NULL) || (count == NULL)) {
        return SENTINEL_ERR_INVALID_ARG;
    }
    (void)memcpy(readings, g_latest, sizeof(g_latest));
    *count = SENTINEL_MAX_CHANNELS;
    return SENTINEL_OK;
}
```

## 6. Sensor Channel Mapping

| Channel | Type | Unit | Calibration Range |
|---------|------|------|-------------------|
| 0 | Temperature | °C | -40 to +125 |
| 1 | Humidity | %RH | 0 to 100 |
| 2 | Pressure | hPa | 300 to 1100 |
| 3 | Light | lux | 0 to 100,000 |

## 7. Dependencies

| Dependency | Include | Purpose |
|------------|---------|---------|
| sentinel_types.h | `#include "../common/sentinel_types.h"` | Types, constants |
| logger.h | `#include "logger.h"` | Sensor data logging |
| string.h | `#include <string.h>` | memset, memcpy |
| stdio.h | `#include <stdio.h>` | Debug output |

## 8. Error Handling

| Error | Response |
|-------|----------|
| Invalid channel ID | Return `SENTINEL_ERR_INVALID_ARG`, skip reading |
| Duplicate sequence | Log info, skip entire message |
| Decode failure | Log error, increment `g_err_count` |
| Logger write failure | Log error, continue (don't block sensor path) |

## 9. Implementation Lessons

1. **No protobuf decode**: Currently just increments counter. Full implementation requires protobuf-c integration.

2. **Static allocation**: Uses fixed arrays for 4 channels. No heap allocation.

3. **Thread safety**: Single-threaded design. If multi-threading is added, `g_latest` access needs protection.

4. **Error counters**: `g_msg_count` and `g_err_count` available for diagnostics but not exposed via API yet.

## 10. Traceability

| Requirement | Implementation | Status |
|-------------|----------------|--------|
| SWE-013-1 | `sensor_proxy_process()` — receive and validate SensorData | Stub (no protobuf) |
| SWE-013-2 | `sensor_proxy_process()` → `logger_write_sensor()` | Not connected |
| SWE-015-1 | `sensor_proxy_get_latest()` — provide readings to diagnostics | Complete |
