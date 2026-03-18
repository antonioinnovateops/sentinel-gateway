---
title: "Detailed Design — Linux Sensor Proxy (SW-03)"
document_id: "DD-SW03"
project: "Sentinel Gateway"
version: "1.0.0"
date: 2026-03-18
status: "Approved"
aspice_process: "SWE.3 Software Detailed Design"
component: "SW-03"
implements: ["SWE-013-1", "SWE-013-2", "SWE-015-1"]
---

# Detailed Design — Linux Sensor Proxy (SW-03)

## 1. Purpose

Receives SensorData protobuf messages from MCU via TCP:5001, validates data integrity, forwards to logger, and maintains latest readings cache for diagnostic queries.

## 2. Module Interface

### 2.1 Public API

```c
/** Initialize sensor proxy, register with event loop */
sentinel_err_t sensor_proxy_init(event_loop_t *loop, logger_t *logger);

/** Process incoming SensorData message (called by tcp_transport on receipt) */
sentinel_err_t sensor_proxy_process(const SensorData *msg);

/** Get latest reading for a specific channel (diagnostic query) */
sentinel_err_t sensor_proxy_get_latest(uint8_t channel, sensor_reading_t *out);

/** Get all latest readings (diagnostic query) */
sentinel_err_t sensor_proxy_get_all(sensor_reading_t readings[4], uint32_t *count);
```

### 2.2 Dependencies

| Dependency | Interface | Direction |
|-----------|-----------|-----------|
| tcp_transport | Message dispatch callback | Inbound |
| logger | `logger_write_sensor()` | Outbound |
| protobuf_handler | `decode_sensor_data()` | Inbound |
| diagnostics | `sensor_proxy_get_latest()` | Query |

## 3. Data Structures

```c
typedef struct {
    sensor_reading_t latest[SENTINEL_MAX_CHANNELS];  /* 4 channels */
    uint32_t message_count;
    uint32_t error_count;
    uint64_t last_receive_timestamp_ms;
} sensor_proxy_state_t;

/* Static allocation — no heap */
static sensor_proxy_state_t g_sensor_state;
```

## 4. Processing Logic

### 4.1 Message Reception (`sensor_proxy_process`)

1. Validate message: sequence number > last seen (detect duplicates/reorder)
2. Validate channel count: 1 ≤ count ≤ `SENTINEL_MAX_CHANNELS`
3. For each sensor reading in message:
   a. Validate channel ID (0–3)
   b. Validate value range (per channel calibration bounds)
   c. Update `latest[channel]` cache
4. Forward complete message to logger via `logger_write_sensor()`
5. Increment `message_count`
6. Update `last_receive_timestamp_ms`

### 4.2 Error Handling

| Error | Response |
|-------|----------|
| Invalid channel ID | Log warning, skip reading, increment `error_count` |
| Duplicate sequence | Log info, skip entire message |
| Decode failure | Log error, increment `error_count` |
| Logger write failure | Log error, continue (don't block sensor path) |

## 5. Traceability

| Requirement | Function |
|-------------|----------|
| SWE-013-1 | `sensor_proxy_process()` — receive and validate SensorData |
| SWE-013-2 | `sensor_proxy_process()` → `logger_write_sensor()` — log all readings |
| SWE-015-1 | `sensor_proxy_get_latest()` — provide readings to diagnostics |
