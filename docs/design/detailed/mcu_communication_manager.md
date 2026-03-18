---
title: "Detailed Design — MCU Communication Manager (FW-04)"
document_id: "DD-FW04"
project: "Sentinel Gateway"
version: "2.0.0"
date: 2026-03-18
status: "Approved"
aspice_process: "SWE.3 Software Detailed Design"
component: "FW-04"
implements: ["SWE-030-1", "SWE-031-1", "SWE-032-1", "SWE-033-1", "SWE-043-1", "SWE-043-2"]
implementation_files:
  - "src/mcu/tcp_stack_qemu.c (QEMU SIL build)"
  - "src/mcu/tcp_stack_lwip.c (Hardware build)"
---

# Detailed Design — MCU Communication Manager (FW-04)

## 1. Purpose

Manages TCP connections on ports 5000 (commands, MCU listens) and 5001 (telemetry, MCU connects to Linux). Handles connection lifecycle and reconnection.

**CRITICAL IMPLEMENTATION NOTE**: Two implementations exist:
- **Hardware builds**: lwIP TCP/IP stack over USB CDC-ECM
- **QEMU SIL builds**: POSIX sockets (no lwIP, no USB)

## 2. Build Configuration

```cmake
# From CMakeLists.txt
if(BUILD_QEMU_MCU)
    target_sources(sentinel_mcu PRIVATE tcp_stack_qemu.c)
    target_compile_definitions(sentinel_mcu PRIVATE USE_POSIX_TCP=1)
else()
    target_sources(sentinel_mcu PRIVATE tcp_stack_lwip.c)
endif()
```

## 3. Module Interface

```c
// Common interface for both implementations
sentinel_err_t tcp_stack_init(const system_config_t *config);
void tcp_stack_poll(void);  // Call from super-loop
sentinel_err_t tcp_stack_send(uint16_t port, const uint8_t *frame, size_t len);
bool tcp_stack_is_connected(uint16_t port);
void tcp_stack_register_recv_callback(uint16_t port, tcp_recv_cb_t callback);
sentinel_err_t tcp_stack_reconnect(void);
```

## 4. QEMU SIL Implementation (tcp_stack_qemu.c)

### 4.1 Architecture

```
┌─────────────────────────────────────────────────────────────┐
│                    QEMU User-Mode                           │
│  ┌────────────────────────────────────────────────────────┐ │
│  │  MCU Firmware (qemu-arm-static)                        │ │
│  │                                                        │ │
│  │  tcp_stack_qemu.c                                      │ │
│  │    - POSIX sockets (socket, connect, send, recv)       │ │
│  │    - Non-blocking I/O (O_NONBLOCK)                     │ │
│  │    - poll() for event multiplexing                     │ │
│  │                                                        │ │
│  │  Connects to: 127.0.0.1:5000, 127.0.0.1:5001          │ │
│  │  (or SENTINEL_MCU_HOST environment variable)           │ │
│  └────────────────────────────────────────────────────────┘ │
└─────────────────────────────────────────────────────────────┘
```

### 4.2 Host Address Configuration

```c
// From tcp_stack_qemu.c
static const char* get_host_address(void) {
    const char *host = getenv("SENTINEL_MCU_HOST");
    return host ? host : "127.0.0.1";  // Default to localhost
}
```

### 4.3 Implementation

```c
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <fcntl.h>
#include <poll.h>

static int cmd_socket = -1;       // Port 5000 - commands
static int telemetry_socket = -1; // Port 5001 - telemetry

sentinel_err_t tcp_stack_init(const system_config_t *config) {
    const char *host = get_host_address();

    // Connect to command port (Linux listens)
    cmd_socket = socket(AF_INET, SOCK_STREAM, 0);
    fcntl(cmd_socket, F_SETFL, O_NONBLOCK);

    struct sockaddr_in addr = {
        .sin_family = AF_INET,
        .sin_port = htons(5000),
    };
    inet_pton(AF_INET, host, &addr.sin_addr);

    // Non-blocking connect (will complete asynchronously)
    connect(cmd_socket, (struct sockaddr*)&addr, sizeof(addr));

    // Similar for telemetry port 5001
    // ...

    return SENTINEL_OK;
}

void tcp_stack_poll(void) {
    struct pollfd fds[2] = {
        { .fd = cmd_socket, .events = POLLIN },
        { .fd = telemetry_socket, .events = POLLIN },
    };

    int ret = poll(fds, 2, 0);  // Non-blocking poll

    if (ret > 0) {
        if (fds[0].revents & POLLIN) {
            // Handle incoming commands
            handle_cmd_recv();
        }
        // ...
    }
}
```

## 5. Hardware Implementation (tcp_stack_lwip.c)

### 5.1 Architecture

```
┌─────────────────────────────────────────────────────────────┐
│  MCU Hardware (STM32 / MPS2-AN505)                          │
│  ┌────────────────────────────────────────────────────────┐ │
│  │  USB CDC-ECM Driver                                    │ │
│  │    - USB OTG FS peripheral                             │ │
│  │    - Ethernet over USB                                 │ │
│  └────────────────────────────────────────────────────────┘ │
│                          │                                  │
│  ┌────────────────────────────────────────────────────────┐ │
│  │  lwIP TCP/IP Stack (NO_SYS=1 mode)                     │ │
│  │    - Static memory pool (128 KB)                       │ │
│  │    - No malloc/free                                    │ │
│  │    - tcp_bind, tcp_listen, tcp_connect                 │ │
│  └────────────────────────────────────────────────────────┘ │
│                                                             │
│  MCU IP: 192.168.7.2                                        │
│  Linux IP: 192.168.7.1                                      │
└─────────────────────────────────────────────────────────────┘
```

### 5.2 lwIP Configuration

```c
// lwipopts.h
#define NO_SYS                  1   // No OS
#define MEM_SIZE                (128*1024)
#define MEMP_NUM_TCP_PCB        4
#define MEMP_NUM_PBUF           32
#define TCP_MSS                 1460
#define TCP_WND                 (8*TCP_MSS)
```

### 5.3 Implementation

```c
#include "lwip/tcp.h"
#include "lwip/sys.h"

static struct tcp_pcb *cmd_pcb = NULL;
static struct tcp_pcb *telemetry_pcb = NULL;

sentinel_err_t tcp_stack_init(const system_config_t *config) {
    // Initialize lwIP
    lwip_init();

    // Set MCU IP address
    IP4_ADDR(&gw, 0, 0, 0, 0);
    IP4_ADDR(&ipaddr, 192, 168, 7, 2);
    IP4_ADDR(&netmask, 255, 255, 255, 0);

    // Listen on port 5000
    cmd_pcb = tcp_new();
    tcp_bind(cmd_pcb, IP_ADDR_ANY, 5000);
    cmd_pcb = tcp_listen(cmd_pcb);
    tcp_accept(cmd_pcb, cmd_accept_callback);

    // Connect to Linux port 5001
    ip_addr_t linux_ip;
    IP4_ADDR(&linux_ip, 192, 168, 7, 1);
    telemetry_pcb = tcp_new();
    tcp_connect(telemetry_pcb, &linux_ip, 5001, telemetry_connected);

    return SENTINEL_OK;
}

void tcp_stack_poll(void) {
    sys_check_timeouts();  // Drive lwIP timers
    // Process any pending TCP callbacks
}
```

## 6. Connection Management

### 6.1 Port 5000 (Command Channel)

- MCU listens on port 5000 (QEMU) or MCU binds (hardware)
- Accepts single connection from Linux
- Dispatches received messages to protobuf_handler → actuator_control / config_store

### 6.2 Port 5001 (Telemetry Channel)

- MCU connects to Linux 192.168.7.1:5001 (hardware) or 127.0.0.1:5001 (QEMU)
- Sends SensorData and HealthStatus messages
- If connection lost, attempts reconnect with exponential backoff

## 7. Reconnection Logic (SWE-043-1, SWE-043-2)

```c
typedef struct {
    int attempt_count;
    uint32_t backoff_ms;
    uint32_t last_attempt_tick;
} reconnect_state_t;

#define INITIAL_BACKOFF_MS    100
#define MAX_BACKOFF_MS        5000
#define MAX_ATTEMPTS          10

sentinel_err_t tcp_stack_reconnect(void) {
    static reconnect_state_t state = {0, INITIAL_BACKOFF_MS, 0};

    uint32_t now = HAL_GetTick();
    if (now - state.last_attempt_tick < state.backoff_ms) {
        return SENTINEL_ERR_NOT_READY;  // Too soon
    }

    state.last_attempt_tick = now;

    // Attempt reconnection
    sentinel_err_t err = do_reconnect();

    if (err == SENTINEL_OK) {
        state.attempt_count = 0;
        state.backoff_ms = INITIAL_BACKOFF_MS;
        return SENTINEL_OK;
    }

    // Exponential backoff
    state.attempt_count++;
    state.backoff_ms = MIN(state.backoff_ms * 2, MAX_BACKOFF_MS);

    if (state.attempt_count >= MAX_ATTEMPTS) {
        return SENTINEL_ERR_COMM_FAILED;  // Give up
    }

    return SENTINEL_ERR_NOT_READY;
}
```

## 8. Wire Frame Handling

Both implementations use the common wire frame format from `wire_frame.h`:

```c
// Send a message with proper framing
sentinel_err_t tcp_stack_send(uint16_t port, const uint8_t *payload, size_t len) {
    uint8_t header[5];

    // 4-byte little-endian length
    header[0] = (len >> 0) & 0xFF;
    header[1] = (len >> 8) & 0xFF;
    header[2] = (len >> 16) & 0xFF;
    header[3] = (len >> 24) & 0xFF;

    // Message type is first byte of payload (already encoded)

    // Send header + payload
    send_data(port, header, 4);
    send_data(port, payload, len);

    return SENTINEL_OK;
}
```

## 9. Traceability

| Requirement | Function | Implementation |
|-------------|----------|----------------|
| SWE-030-1 | `tcp_stack_init()` | USB CDC-ECM (hw) or POSIX sockets (QEMU) |
| SWE-031-1 | `tcp_stack_init()` | IP configuration |
| SWE-032-1 | Port 5000 listen/accept | Both implementations |
| SWE-033-1 | Port 5001 connect | Both implementations |
| SWE-043-1 | `tcp_stack_reconnect()` | Exponential backoff |
| SWE-043-2 | `tcp_stack_reconnect()` | MCU-side reconnection |
