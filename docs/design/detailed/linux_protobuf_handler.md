---
title: "Detailed Design — Linux Protobuf Handler / TCP Transport (SW-02)"
document_id: "DD-SW02"
project: "Sentinel Gateway"
version: "1.1.0"
date: 2026-03-18
status: "Approved"
aspice_process: "SWE.3 Software Detailed Design"
component: "SW-02"
implements: ["SWE-035-3", "SWE-035-4", "SWE-036-2"]
---

# Detailed Design — Linux Protobuf Handler / TCP Transport (SW-02)

## 1. Purpose

Implements the wire frame format (4-byte length + 1-byte type + payload) for communication between Linux gateway and MCU. Manages TCP connections via epoll, handles message reassembly, and dispatches to appropriate handlers.

**Note**: This component combines wire frame handling (common) with TCP transport (Linux-specific). Full protobuf encoding/decoding using protobuf-c is planned but not yet integrated.

## 2. Implementation Reference

| Item | Value |
|------|-------|
| Wire frame source | `src/common/wire_frame.c` |
| Wire frame header | `src/common/wire_frame.h` |
| TCP transport source | `src/linux/tcp_transport.c` |
| TCP transport header | `src/linux/tcp_transport.h` |
| Lines | wire_frame: ~80, tcp_transport: ~250 |
| CMake target | `sentinel-gw` |
| Build command | `docker compose run --rm linux-build` |

## 3. Wire Frame Format

![Wire Frame Format](../../architecture/diagrams/wire_frame_format.drawio.svg)

```
+--------+--------+--------+--------+--------+-- ... --+
| Length (4 bytes, LE)     | Type   | Payload (N bytes)|
+--------+--------+--------+--------+--------+-- ... --+
     Bytes 0-3                B4        B5 ... B(4+N)
```

### 3.1 Constants

```c
/* From sentinel_types.h */
#define WIRE_FRAME_HEADER_SIZE  5U    /* 4-byte length + 1-byte type */
#define WIRE_FRAME_MAX_PAYLOAD  507U  /* Max protobuf payload */
#define WIRE_FRAME_MAX_SIZE     (WIRE_FRAME_HEADER_SIZE + WIRE_FRAME_MAX_PAYLOAD)
```

## 4. Wire Frame Interface

### 4.1 Function Signatures (Exact)

```c
/* From wire_frame.h */
#include "../common/sentinel_types.h"
#include <stddef.h>

sentinel_err_t wire_frame_encode(uint8_t msg_type,
                                  const uint8_t *payload, size_t payload_len,
                                  uint8_t *out_buf, size_t *out_len);

sentinel_err_t wire_frame_decode_header(const uint8_t *frame, size_t frame_len,
                                         uint8_t *out_type, uint32_t *out_payload_len);
```

### 4.2 Error Handling Contract

| Function | Returns | Condition |
|----------|---------|-----------|
| `wire_frame_encode()` | `SENTINEL_OK` | Success |
| `wire_frame_encode()` | `SENTINEL_ERR_INVALID_ARG` | NULL pointers |
| `wire_frame_encode()` | `SENTINEL_ERR_OUT_OF_RANGE` | payload_len > 507 |
| `wire_frame_decode_header()` | `SENTINEL_OK` | Valid header |
| `wire_frame_decode_header()` | `SENTINEL_ERR_INVALID_ARG` | NULL pointers or frame_len < 5 |
| `wire_frame_decode_header()` | `SENTINEL_ERR_OUT_OF_RANGE` | Decoded length > 507 |

## 5. TCP Transport Interface

### 5.1 Function Signatures (Exact)

```c
/* From tcp_transport.h */
typedef void (*transport_recv_cb_t)(uint8_t msg_type, const uint8_t *payload,
                                     size_t payload_len, void *ctx);
typedef void (*transport_diag_cb_t)(int client_fd, const char *cmd, void *ctx);

sentinel_err_t transport_init(void);
sentinel_err_t transport_connect_command(const char *mcu_ip);
sentinel_err_t transport_listen_telemetry(void);
sentinel_err_t transport_listen_diagnostics(void);
void transport_set_recv_callback(transport_recv_cb_t cb, void *ctx);
void transport_set_diag_callback(transport_diag_cb_t cb, void *ctx);
sentinel_err_t transport_poll(int timeout_ms);
void transport_close(void);
```

### 5.2 Error Handling Contract

| Function | Returns | Recovery |
|----------|---------|----------|
| `transport_init()` | `SENTINEL_ERR_INTERNAL` | epoll_create1 failed |
| `transport_connect_command()` | `SENTINEL_ERR_COMM` | TCP connect failed |
| `transport_connect_command()` | `SENTINEL_ERR_INVALID_ARG` | Invalid IP address |
| `transport_listen_*()` | `SENTINEL_ERR_COMM` | bind/listen failed |
| `transport_poll()` | `SENTINEL_OK` | Normal operation |

## 6. TCP Socket Architecture

```
g_epoll_fd (epoll instance)
    ├── g_tel_listen_fd (port 5001, listen)
    │       └── g_tel_fd (accepted MCU telemetry connection)
    ├── g_diag_listen_fd (port 5002, listen)
    │       └── g_diag_fd (accepted diagnostic client)
    └── g_cmd_fd (port 5000, connected to MCU)
```

### 6.1 Port Assignments

| Port | Variable | Direction | Purpose |
|------|----------|-----------|---------|
| 5000 | `g_cmd_fd` | Gateway → MCU | Actuator commands, config updates |
| 5001 | `g_tel_fd` | MCU → Gateway | Sensor data, health status |
| 5002 | `g_diag_fd` | Client → Gateway | Text diagnostic commands |

## 7. Message Reassembly

TCP is a stream protocol, so wire frames may arrive fragmented. The transport layer reassembles them:

```c
static uint8_t g_rx_buf[RX_BUF_SIZE];  /* 2048 bytes */
static size_t g_rx_pos = 0U;

static void process_received_data(int fd)
{
    ssize_t n = read(fd, g_rx_buf + g_rx_pos, RX_BUF_SIZE - g_rx_pos);
    if (n <= 0) { return; }
    g_rx_pos += (size_t)n;

    /* Extract complete frames */
    while (g_rx_pos >= WIRE_FRAME_HEADER_SIZE) {
        uint8_t msg_type;
        uint32_t payload_len;
        sentinel_err_t err = wire_frame_decode_header(g_rx_buf, g_rx_pos,
                                                       &msg_type, &payload_len);
        if (err != SENTINEL_OK) {
            g_rx_pos = 0U;  /* Discard corrupt data */
            break;
        }

        size_t frame_len = WIRE_FRAME_HEADER_SIZE + payload_len;
        if (g_rx_pos < frame_len) {
            break;  /* Incomplete, wait for more data */
        }

        /* Dispatch complete frame */
        if (g_recv_cb != NULL) {
            g_recv_cb(msg_type,
                      g_rx_buf + WIRE_FRAME_HEADER_SIZE,
                      payload_len,
                      g_recv_ctx);
        }

        /* Shift remaining data */
        if (g_rx_pos > frame_len) {
            (void)memmove(g_rx_buf, g_rx_buf + frame_len, g_rx_pos - frame_len);
        }
        g_rx_pos -= frame_len;
    }
}
```

## 8. Diagnostic Text Protocol

Port 5002 uses a simple text protocol instead of protobuf:

```c
static uint8_t g_diag_rx_buf[512];
static size_t g_diag_rx_pos = 0U;
```

Commands are newline-terminated. Response is sent back on the same connection.

## 9. Dependencies

| Dependency | Include | Purpose |
|------------|---------|---------|
| sys/socket.h | `#include <sys/socket.h>` | Socket API |
| sys/epoll.h | `#include <sys/epoll.h>` | Epoll for multiplexing |
| netinet/in.h | `#include <netinet/in.h>` | sockaddr_in |
| netinet/tcp.h | `#include <netinet/tcp.h>` | TCP_NODELAY |
| arpa/inet.h | `#include <arpa/inet.h>` | inet_pton |
| fcntl.h | `#include <fcntl.h>` | Non-blocking sockets |
| unistd.h | `#include <unistd.h>` | read/write/close |

## 10. Implementation Lessons

1. **`_POSIX_C_SOURCE` required**: Set to 200809L for POSIX socket functions.

2. **Non-blocking sockets**: All sockets set to `O_NONBLOCK` for use with epoll.

3. **TCP_NODELAY**: Disabled Nagle's algorithm on command socket for low latency.

4. **SO_REUSEADDR**: Set on listen sockets to allow quick restart after crash.

5. **Partial read handling**: `memmove()` used to shift remaining bytes after extracting complete frames.

6. **No protobuf-c yet**: Wire frame encoding/decoding is implemented, but payload interpretation (protobuf-c) is placeholder.

## 11. Traceability

| Requirement | Implementation | Status |
|-------------|----------------|--------|
| SWE-035-3 | `wire_frame_encode()` — encode wire frames | Complete |
| SWE-035-4 | `wire_frame_decode_header()` — decode wire frames | Complete |
| SWE-036-2 | Sequence tracking in protobuf messages | MCU side only |
