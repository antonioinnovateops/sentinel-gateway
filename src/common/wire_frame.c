/**
 * @file wire_frame.c
 * @brief Wire frame encoding/decoding
 * @implements SYS-035 Message Framing
 */

#include "wire_frame.h"

sentinel_err_t wire_frame_encode(uint8_t msg_type,
                                  const uint8_t *payload, size_t payload_len,
                                  uint8_t *out_buf, size_t *out_len)
{
    if ((payload == NULL) || (out_buf == NULL) || (out_len == NULL)) {
        return SENTINEL_ERR_INVALID_ARG;
    }
    if (payload_len > WIRE_FRAME_MAX_PAYLOAD) {
        return SENTINEL_ERR_OUT_OF_RANGE;
    }

    /* 4-byte little-endian length (payload only, excludes type byte) */
    uint32_t len = (uint32_t)payload_len;
    out_buf[0] = (uint8_t)(len & 0xFFU);
    out_buf[1] = (uint8_t)((len >> 8U) & 0xFFU);
    out_buf[2] = (uint8_t)((len >> 16U) & 0xFFU);
    out_buf[3] = (uint8_t)((len >> 24U) & 0xFFU);

    /* 1-byte message type */
    out_buf[4] = msg_type;

    /* Copy payload */
    for (size_t i = 0U; i < payload_len; i++) {
        out_buf[WIRE_FRAME_HEADER_SIZE + i] = payload[i];
    }

    *out_len = WIRE_FRAME_HEADER_SIZE + payload_len;
    return SENTINEL_OK;
}

sentinel_err_t wire_frame_decode_header(const uint8_t *buf, size_t buf_len,
                                         uint8_t *msg_type, uint32_t *payload_len)
{
    if ((buf == NULL) || (msg_type == NULL) || (payload_len == NULL)) {
        return SENTINEL_ERR_INVALID_ARG;
    }
    if (buf_len < WIRE_FRAME_HEADER_SIZE) {
        return SENTINEL_ERR_INVALID_ARG;
    }

    /* Decode 4-byte little-endian length */
    *payload_len = (uint32_t)buf[0]
                 | ((uint32_t)buf[1] << 8U)
                 | ((uint32_t)buf[2] << 16U)
                 | ((uint32_t)buf[3] << 24U);

    if (*payload_len > WIRE_FRAME_MAX_PAYLOAD) {
        return SENTINEL_ERR_OUT_OF_RANGE;
    }

    *msg_type = buf[4];
    return SENTINEL_OK;
}
