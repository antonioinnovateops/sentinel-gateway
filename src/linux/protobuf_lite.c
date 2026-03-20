/**
 * @file protobuf_lite.c
 * @brief Lightweight protobuf decoding utilities for Linux gateway
 *
 * Shared decoder functions to avoid code duplication between
 * sensor_proxy.c and health_monitor.c.
 */

#include "protobuf_lite.h"
#include <string.h>

size_t pb_dec_varint(const uint8_t *buf, size_t len, uint64_t *val)
{
    *val = 0;
    size_t shift = 0;
    for (size_t i = 0; i < len && i < 10U; i++) {
        *val |= (uint64_t)(buf[i] & 0x7FU) << shift;
        shift += 7;
        if ((buf[i] & 0x80U) == 0) {
            return i + 1;
        }
    }
    return 0; /* Error: incomplete or too long varint */
}

float pb_dec_float32(const uint8_t *buf)
{
    uint32_t bits = (uint32_t)buf[0]
                  | ((uint32_t)buf[1] << 8)
                  | ((uint32_t)buf[2] << 16)
                  | ((uint32_t)buf[3] << 24);
    float val;
    memcpy(&val, &bits, sizeof(val));
    return val;
}
