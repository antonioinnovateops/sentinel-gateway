---
title: "Detailed Design — Linux Protobuf Handler and TCP Transport (SW-02)"
document_id: "DD-SW02"
project: "Sentinel Gateway"
version: "1.0.0"
date: 2026-03-18
status: "Approved"
aspice_process: "SWE.3 Software Detailed Design"
component: "SW-02"
implements: ["SWE-035-3", "SWE-035-4", "SWE-036-2"]
---

# Detailed Design — Linux Protobuf Handler / TCP Transport (SW-02)

## 1. Purpose

Encodes and decodes protobuf messages using protobuf-c. Manages wire frame format (4-byte length + 1-byte type + payload). Tracks message sequence numbers.

## 2. Module Interface

```c
/** Encode a protobuf message into wire frame format */
sentinel_err_t wire_frame_encode(msg_type_t type, const void *protobuf_msg,
                                  uint8_t *out_buf, size_t buf_size, size_t *out_len);

/** Decode a wire frame into protobuf message */
sentinel_err_t wire_frame_decode(const uint8_t *frame, size_t frame_len,
                                  msg_type_t *out_type, void **out_msg);

/** Free decoded protobuf message */
void wire_frame_free(msg_type_t type, void *msg);

/** Get/increment sequence number */
uint32_t sequence_next(void);
```

## 3. Wire Frame Processing

See wire frame format diagram: `../../architecture/diagrams/wire_frame_format.drawio.svg`

### 3.1 Encoding

1. Serialize protobuf message via `protobuf_c_message_pack()`
2. Compute payload length
3. Write 4-byte LE length prefix
4. Write 1-byte message type
5. Write payload bytes
6. Return total frame length

### 3.2 Decoding

1. Read 4-byte LE length (validate ≤ max payload size 507)
2. Read 1-byte message type (validate known type)
3. Deserialize payload via `protobuf_c_message_unpack()`
4. Validate protobuf decode success
5. Return message pointer (caller must free)

### 3.3 Sequence Tracking

- Atomic uint32_t counter, incremented per outgoing message
- Incoming messages validated: sequence > last_seen (warn on gap/reorder)

## 4. Traceability

| Requirement | Function |
|-------------|----------|
| SWE-035-3 | `wire_frame_encode()` — encode wire frames |
| SWE-035-4 | `wire_frame_decode()` — decode wire frames |
| SWE-036-2 | `sequence_next()` — sequence number tracking |
