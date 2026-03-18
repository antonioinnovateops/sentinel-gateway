---
title: "Detailed Design — MCU Protobuf Handler (FW-03)"
document_id: "DD-FW03"
project: "Sentinel Gateway"
version: "1.0.0"
date: 2026-03-18
status: "Approved"
aspice_process: "SWE.3 Software Detailed Design"
component: "FW-03"
implements: ["SWE-035-1", "SWE-035-2", "SWE-036-1", "SWE-037-1"]
---

# Detailed Design — MCU Protobuf Handler (FW-03)

## 1. Purpose

Encodes and decodes protobuf messages using nanopb on the MCU. All buffers statically allocated. Wire frame format: 4-byte LE length + 1-byte type + protobuf payload.

## 2. Module Interface

```c
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

## 3. Static Buffer Allocation

```c
static uint8_t g_encode_buf[PROTOBUF_ENCODE_BUF_SIZE];  /* 256 bytes */
static uint8_t g_decode_buf[PROTOBUF_DECODE_BUF_SIZE];  /* 256 bytes */
static uint32_t g_sequence_counter;                       /* monotonic */
```

**Zero heap**: nanopb configured with `PB_BUFFER_ONLY` and `PB_NO_ERRMSG`. All field sizes bounded by `.options` file.

## 4. Processing Logic

### 4.1 Encoding

1. Fill nanopb struct from application data
2. Set protocol version (major=1, minor=0) per SWE-037-1
3. Set sequence number (increment `g_sequence_counter`)
4. Call `pb_encode()` into `g_encode_buf`
5. Prepend wire frame header (4-byte length + 1-byte type)
6. Return frame in caller's buffer

### 4.2 Decoding

1. Parse wire frame header (length + type)
2. Validate length ≤ max payload
3. Call `pb_decode()` from payload into caller's struct
4. Validate decoded fields (range checks per message type)
5. Return decoded message type

## 5. Traceability

| Requirement | Function |
|-------------|----------|
| SWE-035-1 | `protobuf_encode_*()` — encode wire frames |
| SWE-035-2 | `protobuf_decode_command()` — decode wire frames |
| SWE-036-1 | `protobuf_get_sequence()` — sequence numbering |
| SWE-037-1 | Protocol version embedding in all outgoing messages |
