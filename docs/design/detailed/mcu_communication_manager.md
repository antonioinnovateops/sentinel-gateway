---
title: "Detailed Design — MCU Communication Manager (FW-04)"
document_id: "DD-FW04"
project: "Sentinel Gateway"
version: "1.0.0"
date: 2026-03-18
status: "Approved"
aspice_process: "SWE.3 Software Detailed Design"
component: "FW-04"
implements: ["SWE-030-1", "SWE-031-1", "SWE-032-1", "SWE-033-1", "SWE-043-1"]
---

# Detailed Design — MCU Communication Manager (FW-04)

## 1. Purpose

Manages USB CDC-ECM initialization, lwIP TCP/IP stack (NO_SYS mode), TCP connections on ports 5000 (commands, MCU listens) and 5001 (telemetry, MCU connects to Linux). Handles connection lifecycle and reconnection.

## 2. Module Interface

```c
sentinel_err_t tcp_stack_init(const system_config_t *config);
void tcp_stack_poll(void);  /* Call from super-loop — processes lwIP timers + TCP */
sentinel_err_t tcp_stack_send(uint16_t port, const uint8_t *frame, size_t len);
bool tcp_stack_is_connected(uint16_t port);
void tcp_stack_register_recv_callback(uint16_t port, tcp_recv_cb_t callback);
sentinel_err_t tcp_stack_reconnect(void);
```

## 3. Connection Management

### 3.1 Port 5000 (Command Channel)

- MCU listens on port 5000 (`tcp_bind` + `tcp_listen`)
- Accepts single connection from Linux
- Dispatches received messages to protobuf_handler → actuator_control / config_store

### 3.2 Port 5001 (Telemetry Channel)

- MCU connects to Linux 192.168.7.1:5001 (`tcp_connect`)
- Sends SensorData and HealthStatus messages
- If connection lost, attempts reconnect every 1 second

### 3.3 lwIP Integration

- `NO_SYS=1` mode — no OS threads
- `tcp_stack_poll()` called from super-loop, drives `sys_check_timeouts()`
- Static memory pool (128 KB) — no `malloc`

## 4. Reconnection Logic (SWE-043-1)

1. Detect TCP disconnect (lwIP `ERR_CONN` or `ERR_RST`)
2. Close old PCBs
3. Wait 1 second
4. Re-listen on port 5000, re-connect to port 5001
5. If reconnect succeeds, wait for StateSyncRequest from Linux

## 5. Traceability

| Requirement | Function |
|-------------|----------|
| SWE-030-1 | `tcp_stack_init()` — USB CDC-ECM + lwIP initialization |
| SWE-031-1 | `tcp_stack_poll()` — lwIP stack polling |
| SWE-032-1 | Port 5000 listen/accept |
| SWE-033-1 | Port 5001 connect |
| SWE-043-1 | `tcp_stack_reconnect()` — reconnection after loss |
