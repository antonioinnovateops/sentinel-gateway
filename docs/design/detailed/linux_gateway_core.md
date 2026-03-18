---
title: "Detailed Design — Linux Gateway Core (SW-01)"
document_id: "DD-SW01"
project: "Sentinel Gateway"
version: "1.1.0"
date: 2026-03-18
status: "Approved"
aspice_process: "SWE.3 Software Detailed Design"
component: "SW-01"
implements: ["SWE-046-1", "SWE-044-1", "SWE-066-1", "SWE-057-1"]
---

# Detailed Design — Linux Gateway Core (SW-01)

## 1. Purpose

The gateway_core module is the main entry point for the Linux gateway application (`sentinel-gw` daemon). It owns the epoll event loop, startup/shutdown sequence, signal handling, and coordinates all other modules via message dispatch.

## 2. Implementation Reference

| Item | Value |
|------|-------|
| Source file | `src/linux/gateway_core.c` |
| Header file | `src/linux/gateway_core.h` |
| Main entry | `src/linux/main.c` |
| Lines | ~170 |
| CMake target | `sentinel-gw` |
| Build command | `docker compose run --rm linux-build` |
| Run command | `./build/sentinel-gw` or via Docker |

## 3. Module Interface

### 3.1 Function Signatures (Exact)

```c
#include "gateway_core.h"

sentinel_err_t gateway_init(void);
sentinel_err_t gateway_run(void);
void gateway_shutdown(void);
```

### 3.2 Error Handling Contract

| Function | Returns | Recovery |
|----------|---------|----------|
| `gateway_init()` | `SENTINEL_OK` | All subsystems initialized |
| `gateway_init()` | `SENTINEL_ERR_*` | Subsystem init failure, logged |
| `gateway_run()` | `SENTINEL_OK` | Normal shutdown via signal |
| `gateway_shutdown()` | void | Sets `g_running = 0` |

## 4. Architecture

![Linux Gateway Process Architecture](../../architecture/diagrams/linux_process_arch.drawio.svg)

## 5. Initialization Sequence

```c
sentinel_err_t gateway_init(void)
{
    printf("[GW] Sentinel Gateway %s starting...\n", SENTINEL_VERSION);

    /* Signal handling */
    struct sigaction sa;
    (void)memset(&sa, 0, sizeof(sa));
    sa.sa_handler = signal_handler;
    (void)sigaction(SIGINT, &sa, NULL);
    (void)sigaction(SIGTERM, &sa, NULL);

    /* Initialize subsystems in order */
    sentinel_err_t err;

    /* 1. Logger (fallback to current dir if /var/log not writable) */
    err = logger_init("/var/log/sentinel/sensor_data.jsonl",
                       "/var/log/sentinel/events.jsonl");
    if (err != SENTINEL_OK) {
        err = logger_init("sensor_data.jsonl", "events.jsonl");
    }

    /* 2-6. Subsystems */
    err = config_manager_init(NULL);      /* No config file, use defaults */
    err = sensor_proxy_init();
    err = actuator_proxy_init();
    err = health_monitor_init();
    err = diagnostics_init();

    /* 7. Transport layer */
    err = transport_init();
    transport_set_recv_callback(on_message_received, NULL);
    transport_set_diag_callback(on_diag_command, NULL);

    /* 8. Listen on ports */
    err = transport_listen_telemetry();   /* Port 5001 */
    err = transport_listen_diagnostics(); /* Port 5002 */

    /* 9. Connect to MCU command channel */
    const char *mcu_ip = getenv("SENTINEL_MCU_HOST");
    if (mcu_ip == NULL) { mcu_ip = SENTINEL_MCU_IP; }  /* "192.168.7.2" */
    err = transport_connect_command(mcu_ip);
    /* Non-fatal if MCU not yet available */

    logger_write_event("STARTUP", "Gateway initialized");
    return SENTINEL_OK;
}
```

### 5.1 Environment Variable Override

The `SENTINEL_MCU_HOST` environment variable allows overriding the default MCU IP address. This is essential for:
- **SIL testing**: Point to localhost or Docker container
- **HIL testing**: Different network topology
- **Development**: Connect to simulator

```bash
# Example: Connect to MCU simulator on localhost
export SENTINEL_MCU_HOST=127.0.0.1
./sentinel-gw
```

## 6. Event Loop Design

```c
sentinel_err_t gateway_run(void)
{
    printf("[GW] Entering main event loop\n");

    while (g_running) {
        /* Poll for I/O events with 100ms timeout */
        transport_poll(100);

        /* Periodic health check */
        uint64_t now_ms = get_ms();
        health_monitor_tick(now_ms);
    }

    printf("[GW] Shutting down...\n");
    logger_write_event("SHUTDOWN", "Gateway stopping");
    transport_close();
    logger_close();

    return SENTINEL_OK;
}
```

The event loop uses epoll internally (via `transport_poll()`) with a 100ms timeout to ensure periodic health checks run even without I/O activity.

## 7. Message Dispatch

When `tcp_transport` receives a complete wire frame, it calls `on_message_received()`:

```c
static void on_message_received(uint8_t msg_type, const uint8_t *payload,
                                 size_t payload_len, void *ctx)
{
    switch (msg_type) {
        case MSG_TYPE_SENSOR_DATA:     /* 0x01 */
            sensor_proxy_process(payload, payload_len);
            break;
        case MSG_TYPE_HEALTH_STATUS:   /* 0x02 */
            health_monitor_process_health(payload, payload_len);
            break;
        case MSG_TYPE_ACTUATOR_RSP:    /* 0x11 */
            actuator_proxy_process_response(payload, payload_len);
            break;
        case MSG_TYPE_CONFIG_RSP:      /* 0x21 */
            config_manager_process_response(payload, payload_len);
            break;
        default:
            fprintf(stderr, "[GW] Unknown message type: 0x%02X\n", msg_type);
            break;
    }
}
```

| Message Type | Value | Handler Module |
|-------------|-------|----------------|
| MSG_TYPE_SENSOR_DATA | 0x01 | sensor_proxy |
| MSG_TYPE_HEALTH_STATUS | 0x02 | health_monitor |
| MSG_TYPE_ACTUATOR_RSP | 0x11 | actuator_proxy |
| MSG_TYPE_CONFIG_RSP | 0x21 | config_manager |
| MSG_TYPE_DIAG_RSP | 0x31 | gateway_core (future) |
| MSG_TYPE_STATE_SYNC_RSP | 0x41 | gateway_core (future) |

## 8. Signal Handling

```c
static volatile sig_atomic_t g_running = 1;

static void signal_handler(int sig)
{
    (void)sig;
    g_running = 0;
}
```

- `SIGINT` (Ctrl+C) and `SIGTERM` trigger graceful shutdown
- No cleanup in signal handler (deferred to main loop exit)
- `volatile sig_atomic_t` ensures atomic access from signal context

## 9. Dependencies

| Dependency | Include | Purpose |
|------------|---------|---------|
| All subsystem headers | Various | Module initialization |
| signal.h | `#include <signal.h>` | Signal handling |
| time.h | `#include <time.h>` | clock_gettime for timestamps |
| stdlib.h | `#include <stdlib.h>` | getenv for MCU host override |
| unistd.h | `#include <unistd.h>` | write() for diagnostic responses |

## 10. Implementation Lessons

1. **Logger fallback**: Production path `/var/log/sentinel/` may not exist or be writable. Code falls back to current directory.

2. **MCU connection non-fatal**: Gateway starts even if MCU is not reachable. Health monitor will track connection status.

3. **Required includes**: Added `<stddef.h>`, `<stdbool.h>`, `<string.h>` during build integration.

4. **`_POSIX_C_SOURCE` requirement**: Set to 200809L at file top for `clock_gettime()` and `sigaction()`.

5. **Diagnostic text protocol**: Diagnostic clients connect to port 5002 and send newline-terminated text commands. Responses are sent back on the same connection.

## 11. TCP Port Summary

| Port | Direction | Purpose |
|------|-----------|---------|
| 5000 | Gateway → MCU | Command channel (actuator, config) |
| 5001 | MCU → Gateway | Telemetry (sensor, health) |
| 5002 | Client → Gateway | Text diagnostics |

## 12. Traceability

| Requirement | Implementation | Status |
|-------------|----------------|--------|
| SWE-046-1 | `gateway_run()` — epoll event loop | Complete |
| SWE-044-1 | Recovery via health_monitor | Partial (no USB power cycle) |
| SWE-066-1 | `gateway_init()` — startup sequence | Complete |
| SWE-057-1 | Version query | Future (MCU version not queried) |
