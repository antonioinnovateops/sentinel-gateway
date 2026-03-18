/**
 * @file wire_frame.h
 * @brief Wire frame encoding/decoding (shared between Linux and MCU)
 * @implements SYS-035 Message Framing
 *
 * Frame format: [4-byte LE length][1-byte type][protobuf payload]
 */

#ifndef SENTINEL_WIRE_FRAME_H
#define SENTINEL_WIRE_FRAME_H

#include <stdint.h>
#include <stddef.h>
#include "sentinel_types.h"

/**
 * Encode a wire frame: prepend header to payload
 * @param msg_type Message type ID (MSG_TYPE_*)
 * @param payload  Protobuf-encoded payload
 * @param payload_len Length of payload
 * @param out_buf  Output buffer (must be >= WIRE_FRAME_HEADER_SIZE + payload_len)
 * @param out_len  Output: total frame length
 * @return SENTINEL_OK on success
 */
sentinel_err_t wire_frame_encode(uint8_t msg_type,
                                  const uint8_t *payload, size_t payload_len,
                                  uint8_t *out_buf, size_t *out_len);

/**
 * Decode a wire frame header
 * @param buf      Input buffer (at least WIRE_FRAME_HEADER_SIZE bytes)
 * @param buf_len  Available bytes
 * @param msg_type Output: message type
 * @param payload_len Output: payload length
 * @return SENTINEL_OK on success
 */
sentinel_err_t wire_frame_decode_header(const uint8_t *buf, size_t buf_len,
                                         uint8_t *msg_type, uint32_t *payload_len);

#endif /* SENTINEL_WIRE_FRAME_H */
