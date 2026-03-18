---
title: "Detailed Design — Linux Actuator Proxy (SW-04)"
document_id: "DD-SW04"
project: "Sentinel Gateway"
version: "1.1.0"
date: 2026-03-18
status: "Approved"
aspice_process: "SWE.3 Software Detailed Design"
component: "SW-04"
implements: ["SWE-018-2", "SWE-019-2"]
---

# Detailed Design — Linux Actuator Proxy (SW-04)

## 1. Purpose

Sends ActuatorCommand protobuf messages to MCU via TCP:5000, tracks pending commands, processes ActuatorResponse messages, and provides command interface for diagnostics.

## 2. Implementation Reference

| Item | Value |
|------|-------|
| Source file | `src/linux/actuator_proxy.c` |
| Header file | `src/linux/actuator_proxy.h` |
| Lines | ~65 |
| CMake target | `sentinel-gw` |
| Build command | `docker compose run --rm linux-build` |
| Test command | `docker compose run --rm linux-build ctest --output-on-failure` |

## 3. Module Interface

### 3.1 Function Signatures (Exact)

```c
#include "../common/sentinel_types.h"
#include <stddef.h>

typedef void (*actuator_response_cb_t)(uint8_t actuator_id, float applied,
                                        uint32_t status, void *ctx);

sentinel_err_t actuator_proxy_init(void);

sentinel_err_t actuator_proxy_send_command(uint8_t actuator_id, float value_percent,
                                            actuator_response_cb_t cb, void *ctx);

sentinel_err_t actuator_proxy_process_response(const uint8_t *payload, size_t len);

sentinel_err_t actuator_proxy_get_state(uint8_t actuator_id, actuator_state_t *out);
```

### 3.2 Error Handling Contract

| Function | Returns | Recovery |
|----------|---------|----------|
| `actuator_proxy_init()` | `SENTINEL_OK` | Always succeeds (memset only) |
| `actuator_proxy_send_command()` | `SENTINEL_ERR_INVALID_ARG` | Invalid actuator_id (≥2) |
| `actuator_proxy_send_command()` | `SENTINEL_ERR_OUT_OF_RANGE` | value_percent outside 0.0–100.0 |
| `actuator_proxy_process_response()` | `SENTINEL_OK` | Placeholder for protobuf decode |
| `actuator_proxy_get_state()` | `SENTINEL_ERR_INVALID_ARG` | NULL output or invalid actuator_id |

## 4. Data Structures

```c
/* From sentinel_types.h */
typedef struct {
    uint8_t  actuator_id;
    float    commanded_percent;
    float    applied_percent;     /* from MCU response */
    uint32_t status;              /* STATUS_OK, STATUS_ERROR, etc. */
    uint64_t command_timestamp_ms;
    uint64_t response_timestamp_ms;
} actuator_state_t;

/* Static state (actuator_proxy.c:12-14) */
static actuator_state_t g_actuators[SENTINEL_MAX_ACTUATORS];  /* 2 actuators */
static uint32_t g_sequence = 0U;
static uint32_t g_timeout_count = 0U;
```

## 5. Processing Logic

### 5.1 Command Sending (`actuator_proxy_send_command`)

1. Validate actuator_id < `SENTINEL_MAX_ACTUATORS` (2)
2. Validate value_percent in range 0.0–100.0
3. Store commanded value in `g_actuators[actuator_id]`
4. Increment sequence counter
5. **TODO**: Encode ActuatorCommand protobuf and send via transport
6. **TODO**: Start response timeout timer (500ms)

### 5.2 Response Processing (`actuator_proxy_process_response`)

Currently a placeholder. Full implementation will:
1. Decode ActuatorResponse protobuf
2. Match response to pending command by sequence number
3. Update actuator state with `applied_percent` and `status`
4. Cancel timeout timer
5. Invoke caller's callback with result
6. Log command/response pair to event log

### 5.3 Timeout Handling

When protobuf integration is complete:
- If no response within 500ms, invoke callback with `SENTINEL_ERR_TIMEOUT`
- Increment `g_timeout_count`
- Log warning (may indicate MCU overload or comm issue)

## 6. Dependencies

| Dependency | Include | Purpose |
|------------|---------|---------|
| sentinel_types.h | `#include "../common/sentinel_types.h"` | Types, constants |
| tcp_transport.h | `#include "tcp_transport.h"` | Network I/O |
| wire_frame.h | `#include "../common/wire_frame.h"` | Frame encoding |
| string.h | `#include <string.h>` | memset |
| stdio.h | `#include <stdio.h>` | fprintf for debug |

## 7. Implementation Lessons

1. **Callback parameters unused**: The current implementation casts `cb` and `ctx` to void since protobuf encoding is not yet integrated.

2. **No heap allocation**: Uses static arrays for actuator state. Maximum of 2 actuators defined by `SENTINEL_MAX_ACTUATORS`.

3. **Sequence number management**: Global `g_sequence` counter increments per command; response matching will use this for correlation.

4. **Testing with stubs**: Unit tests require PWM stub (`tests/unit/stubs/pwm_stub.c`) since actuator_proxy indirectly references actuator_control.

## 8. Safety Considerations

- **ASIL-B**: This component sends safety-relevant actuator commands
- Value clamping is done on MCU side (defense in depth), but proxy also validates range
- Sequence numbers prevent replay/duplication
- Timeout detection ensures stuck commands don't go unnoticed

## 9. Traceability

| Requirement | Function | Status |
|-------------|----------|--------|
| SWE-018-2 | `actuator_proxy_send_command()` — send commands to MCU | Partial (no protobuf) |
| SWE-019-2 | `actuator_proxy_process_response()` — handle MCU response | Stub |
