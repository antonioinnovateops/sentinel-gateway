---
title: "Detailed Design — Linux Actuator Proxy (SW-04)"
document_id: "DD-SW04"
project: "Sentinel Gateway"
version: "1.0.0"
date: 2026-03-18
status: "Approved"
aspice_process: "SWE.3 Software Detailed Design"
component: "SW-04"
implements: ["SWE-018-2", "SWE-019-2"]
---

# Detailed Design — Linux Actuator Proxy (SW-04)

## 1. Purpose

Sends ActuatorCommand protobuf messages to MCU via TCP:5000, tracks pending commands, processes ActuatorResponse messages, and provides command interface for diagnostics.

## 2. Module Interface

```c
/** Initialize actuator proxy */
sentinel_err_t actuator_proxy_init(event_loop_t *loop);

/** Send actuator command to MCU. Non-blocking; response via callback. */
sentinel_err_t actuator_proxy_send_command(uint8_t actuator_id, float value_percent,
                                           actuator_response_cb_t callback, void *ctx);

/** Process incoming ActuatorResponse (called by tcp_transport) */
sentinel_err_t actuator_proxy_process_response(const ActuatorResponse *msg);

/** Get last known actuator state (for diagnostics) */
sentinel_err_t actuator_proxy_get_state(uint8_t actuator_id, actuator_state_t *out);
```

## 3. Data Structures

```c
typedef struct {
    uint8_t actuator_id;
    float commanded_percent;
    float applied_percent;     /* from MCU response */
    uint32_t status;           /* STATUS_OK, STATUS_ERROR, etc. */
    uint64_t command_timestamp_ms;
    uint64_t response_timestamp_ms;
} actuator_state_t;

typedef struct {
    actuator_state_t actuators[SENTINEL_MAX_ACTUATORS];  /* 2 actuators */
    uint32_t sequence_counter;
    uint32_t pending_count;
    uint32_t timeout_count;
} actuator_proxy_state_t;

static actuator_proxy_state_t g_actuator_state;
```

## 4. Processing Logic

### 4.1 Command Sending

1. Validate actuator_id (0 or 1)
2. Validate value_percent (0.0–100.0)
3. Encode ActuatorCommand protobuf with sequence number
4. Send via tcp_transport on port 5000
5. Record pending command with timestamp
6. Start response timeout timer (500ms)

### 4.2 Response Processing

1. Match response to pending command by sequence number
2. Update actuator state with applied_percent and status
3. Cancel timeout timer
4. Invoke caller's callback with result
5. Log command/response pair to event log

### 4.3 Timeout Handling

- If no response within 500ms, invoke callback with `SENTINEL_ERR_TIMEOUT`
- Increment `timeout_count`
- Log warning (may indicate MCU overload or comm issue)

## 5. Safety Considerations

- **ASIL-B**: This component sends safety-relevant actuator commands
- Value clamping is done on MCU side (defense in depth), but proxy also validates range
- Sequence numbers prevent replay/duplication
- Timeout detection ensures stuck commands don't go unnoticed

## 6. Traceability

| Requirement | Function |
|-------------|----------|
| SWE-018-2 | `actuator_proxy_send_command()` — send commands to MCU |
| SWE-019-2 | `actuator_proxy_process_response()` — handle MCU response |
