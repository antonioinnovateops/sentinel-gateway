---
title: "Detailed Design — MCU Protobuf Handler (FW-03)"
document_id: "DD-FW03"
project: "Sentinel Gateway"
version: "2.0.0"
date: 2026-03-18
status: "Approved"
aspice_process: "SWE.3 Software Detailed Design"
component: "FW-03"
implements: ["SWE-035-1", "SWE-035-2", "SWE-036-1", "SWE-037-1"]
implementation: "src/common/wire_frame.h, src/common/wire_frame.c"
---

# Detailed Design — MCU Protobuf Handler (FW-03)

## 1. Purpose

Encodes and decodes protobuf messages using nanopb on the MCU. All buffers statically allocated.

## 2. Wire Frame Format (CRITICAL)

**The wire frame header is 5 bytes total:**

```
┌─────────────────┬──────────────┬─────────────────────────┐
│ Length (4 bytes)│ Type (1 byte)│ Payload (N bytes)       │
│ Little-endian   │              │                         │
│ uint32          │ msg_type_t   │ Protobuf-encoded        │
└─────────────────┴──────────────┴─────────────────────────┘
Offset: 0-3         Offset: 4      Offset: 5+

CRITICAL: Length field contains ONLY the payload size (NOT including the 5-byte header).
```

**Example: SensorData message (32 bytes payload)**
```
Bytes:  20 00 00 00 01 [32 bytes of protobuf]
        └─ length ──┘ └─ type (MSG_SENSOR_DATA)
```

**Message Type IDs** (from `sentinel_types.h`):
```c
typedef enum {
    MSG_SENSOR_DATA      = 0x01,
    MSG_ACTUATOR_CMD     = 0x02,
    MSG_ACTUATOR_RESP    = 0x03,
    MSG_HEALTH_STATUS    = 0x04,
    MSG_CONFIG_UPDATE    = 0x05,
    MSG_CONFIG_RESP      = 0x06,
    MSG_STATE_SYNC_REQ   = 0x07,
    MSG_STATE_SYNC_RESP  = 0x08,
    MSG_DIAG_REQ         = 0x09,
    MSG_DIAG_RESP        = 0x0A
} msg_type_t;
```

## 3. Module Interface

```c
// From wire_frame.h
#define WIRE_FRAME_HEADER_SIZE  5   // 4-byte length + 1-byte type
#define WIRE_FRAME_MAX_PAYLOAD  256

typedef struct {
    uint32_t length;      // Payload length (NOT including header)
    msg_type_t msg_type;  // Message type ID
    uint8_t payload[WIRE_FRAME_MAX_PAYLOAD];
} wire_frame_t;

sentinel_err_t wire_frame_encode(const wire_frame_t *frame,
                                  uint8_t *out_buf, size_t *out_len);
sentinel_err_t wire_frame_decode(const uint8_t *buf, size_t len,
                                  wire_frame_t *frame);

// Protobuf encoding helpers
sentinel_err_t protobuf_encode_sensor_data(const sensor_reading_t readings[4],
                                            uint8_t *out_buf, size_t *out_len);
sentinel_err_t protobuf_encode_health(const health_state_t *state,
                                       uint8_t *out_buf, size_t *out_len);
sentinel_err_t protobuf_encode_response(msg_type_t type, uint32_t status,
                                         const char *error_msg,
                                         uint8_t *out_buf, size_t *out_len);
sentinel_err_t protobuf_decode_command(const uint8_t *buf, size_t len,
                                        msg_type_t *type, void *out_msg);
uint32_t protobuf_get_sequence(void);
```

## 4. Static Buffer Allocation

```c
static uint8_t g_encode_buf[PROTOBUF_ENCODE_BUF_SIZE];  /* 256 bytes */
static uint8_t g_decode_buf[PROTOBUF_DECODE_BUF_SIZE];  /* 256 bytes */
static uint32_t g_sequence_counter;                       /* monotonic */
```

**Zero heap**: nanopb configured with `PB_BUFFER_ONLY` and `PB_NO_ERRMSG`. All field sizes bounded by `.options` file.

## 5. Processing Logic

### 5.1 Wire Frame Encoding

```c
sentinel_err_t wire_frame_encode(const wire_frame_t *frame,
                                  uint8_t *out_buf, size_t *out_len) {
    if (frame->length > WIRE_FRAME_MAX_PAYLOAD) {
        return SENTINEL_ERR_FRAME_TOO_LARGE;
    }

    // 4-byte little-endian length
    out_buf[0] = (frame->length >> 0) & 0xFF;
    out_buf[1] = (frame->length >> 8) & 0xFF;
    out_buf[2] = (frame->length >> 16) & 0xFF;
    out_buf[3] = (frame->length >> 24) & 0xFF;

    // 1-byte message type
    out_buf[4] = (uint8_t)frame->msg_type;

    // Copy payload
    memcpy(&out_buf[5], frame->payload, frame->length);

    *out_len = WIRE_FRAME_HEADER_SIZE + frame->length;
    return SENTINEL_OK;
}
```

### 5.2 Wire Frame Decoding

```c
sentinel_err_t wire_frame_decode(const uint8_t *buf, size_t len,
                                  wire_frame_t *frame) {
    if (len < WIRE_FRAME_HEADER_SIZE) {
        return SENTINEL_ERR_PROTO_ERROR;  // Incomplete header
    }

    // Parse little-endian length
    frame->length = (uint32_t)buf[0]
                  | ((uint32_t)buf[1] << 8)
                  | ((uint32_t)buf[2] << 16)
                  | ((uint32_t)buf[3] << 24);

    if (frame->length > WIRE_FRAME_MAX_PAYLOAD) {
        return SENTINEL_ERR_FRAME_TOO_LARGE;
    }

    // Parse message type
    frame->msg_type = (msg_type_t)buf[4];

    // Validate we have complete payload
    if (len < WIRE_FRAME_HEADER_SIZE + frame->length) {
        return SENTINEL_ERR_PROTO_ERROR;  // Incomplete payload
    }

    // Copy payload
    memcpy(frame->payload, &buf[5], frame->length);

    return SENTINEL_OK;
}
```

### 5.3 Protobuf Encoding Flow

1. Fill nanopb struct from application data
2. Set protocol version (major=1, minor=0) per SWE-037-1
3. Set sequence number (increment `g_sequence_counter`)
4. Call `pb_encode()` into `g_encode_buf`
5. Build wire frame with appropriate message type
6. Return complete frame in caller's buffer

### 5.4 Protobuf Decoding Flow

1. Parse wire frame header (length + type)
2. Validate length ≤ max payload
3. Validate message type is known
4. Call `pb_decode()` from payload into caller's struct
5. Validate decoded fields (range checks per message type)
6. Return decoded message type

## 6. Error Handling

```c
// From error_codes.h
typedef enum {
    SENTINEL_OK = 0,
    SENTINEL_ERR_PROTO_ERROR = -5,    // Protobuf decode failure
    SENTINEL_ERR_FRAME_TOO_LARGE = -9,
    SENTINEL_ERR_UNKNOWN_MSG_TYPE = -10
} sentinel_err_t;
```

On any decode error, the caller (actuator_control) triggers fail-safe per SWE-028-1.

## 7. Traceability

| Requirement | Function |
|-------------|----------|
| SWE-035-1 | `wire_frame_encode()`, `protobuf_encode_*()` |
| SWE-035-2 | `wire_frame_decode()`, `protobuf_decode_command()` |
| SWE-036-1 | `protobuf_get_sequence()` — sequence numbering |
| SWE-037-1 | Protocol version embedding in all outgoing messages |
