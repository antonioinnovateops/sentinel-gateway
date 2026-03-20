/**
 * @file protobuf_lite.h
 * @brief Lightweight protobuf decoding utilities for Linux gateway
 *
 * Shared decoder functions used by sensor_proxy and health_monitor.
 * These are hand-rolled decoders to avoid protobuf-c dependency.
 */

#ifndef PROTOBUF_LITE_H
#define PROTOBUF_LITE_H

#include <stdint.h>
#include <stddef.h>

/**
 * Decode a varint from a buffer.
 * @param buf Input buffer
 * @param len Available bytes
 * @param val Output: decoded value
 * @return Number of bytes consumed, or 0 on error
 */
size_t pb_dec_varint(const uint8_t *buf, size_t len, uint64_t *val);

/**
 * Decode a fixed32 (little-endian float) from a buffer.
 * @param buf Input buffer (must have at least 4 bytes)
 * @return Decoded float value
 */
float pb_dec_float32(const uint8_t *buf);

#endif /* PROTOBUF_LITE_H */
