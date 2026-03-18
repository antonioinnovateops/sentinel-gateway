/**
 * @file protobuf_handler.c
 * @brief FW-03: nanopb protobuf encoding/decoding
 * @implements SWE-035 through SWE-037
 *
 * NOTE: This file uses manual byte-level encoding rather than nanopb
 * pb_encode/pb_decode to avoid the nanopb library dependency during
 * initial build verification. Once nanopb is integrated in the Docker
 * build, this will be replaced with proper nanopb calls using the
 * generated sentinel.pb.h/sentinel.pb.c files.
 *
 * For now, we implement a minimal wire-compatible encoder that produces
 * valid wire frames with simplified payload encoding.
 */

#include "protobuf_handler.h"
#include "../common/wire_frame.h"
#include "hal/systick.h"
#include <string.h>

static uint32_t g_sequence = 0U;

/* Simple varint encoder (protobuf wire format) */
static size_t encode_varint(uint8_t *buf, uint64_t value)
{
    size_t n = 0U;
    while (value > 0x7FU) {
        buf[n] = (uint8_t)(value & 0x7FU) | 0x80U;
        value >>= 7;
        n++;
    }
    buf[n] = (uint8_t)value;
    return n + 1U;
}

/* Encode field tag + varint */
static size_t encode_field_varint(uint8_t *buf, uint32_t field, uint64_t value)
{
    size_t n = encode_varint(buf, ((uint64_t)field << 3) | 0U); /* wire type 0 */
    n += encode_varint(buf + n, value);
    return n;
}

/* Encode field tag + fixed32 (for float) */
static size_t encode_field_float(uint8_t *buf, uint32_t field, float value)
{
    size_t n = encode_varint(buf, ((uint64_t)field << 3) | 5U); /* wire type 5 */
    uint32_t bits;
    (void)memcpy(&bits, &value, sizeof(bits));
    buf[n + 0U] = (uint8_t)(bits & 0xFFU);
    buf[n + 1U] = (uint8_t)((bits >> 8U) & 0xFFU);
    buf[n + 2U] = (uint8_t)((bits >> 16U) & 0xFFU);
    buf[n + 3U] = (uint8_t)((bits >> 24U) & 0xFFU);
    return n + 4U;
}

/* Encode a MessageHeader submessage */
static size_t encode_header(uint8_t *buf, uint32_t field_num)
{
    /* Encode header fields into temp buffer */
    uint8_t hdr[32];
    size_t hlen = 0U;

    g_sequence++;
    hlen += encode_field_varint(hdr + hlen, 1, g_sequence);       /* sequence */
    hlen += encode_field_varint(hdr + hlen, 2, systick_get_us()); /* timestamp */

    /* Version submessage (field 3) */
    uint8_t ver[4];
    size_t vlen = 0U;
    vlen += encode_field_varint(ver + vlen, 1, SENTINEL_PROTO_VERSION_MAJOR);
    vlen += encode_field_varint(ver + vlen, 2, SENTINEL_PROTO_VERSION_MINOR);
    hlen += encode_varint(hdr + hlen, ((uint64_t)3 << 3) | 2U); /* field 3, wire type 2 */
    hlen += encode_varint(hdr + hlen, vlen);
    (void)memcpy(hdr + hlen, ver, vlen);
    hlen += vlen;

    /* Write as length-delimited submessage */
    size_t n = encode_varint(buf, ((uint64_t)field_num << 3) | 2U);
    n += encode_varint(buf + n, hlen);
    (void)memcpy(buf + n, hdr, hlen);
    return n + hlen;
}

sentinel_err_t pb_encode_sensor_data(const sensor_reading_t *readings,
                                      uint32_t count, uint32_t rate_hz,
                                      uint8_t *out_buf, size_t *out_len)
{
    if ((readings == NULL) || (out_buf == NULL) || (out_len == NULL)) {
        return SENTINEL_ERR_INVALID_ARG;
    }

    uint8_t payload[PB_ENCODE_BUF_SIZE];
    size_t plen = 0U;

    /* Field 1: header */
    plen += encode_header(payload + plen, 1U);

    /* Field 2: repeated SensorReading */
    for (uint32_t i = 0U; i < count; i++) {
        uint8_t reading[32];
        size_t rlen = 0U;
        rlen += encode_field_varint(reading + rlen, 1, readings[i].channel);
        rlen += encode_field_varint(reading + rlen, 2, readings[i].raw_value);
        rlen += encode_field_float(reading + rlen, 3, readings[i].calibrated_value);

        /* Write as length-delimited submessage (field 2) */
        plen += encode_varint(payload + plen, (2ULL << 3) | 2U);
        plen += encode_varint(payload + plen, rlen);
        (void)memcpy(payload + plen, reading, rlen);
        plen += rlen;
    }

    /* Field 3: sample_rate_hz */
    plen += encode_field_varint(payload + plen, 3, rate_hz);

    /* Wrap in wire frame */
    return wire_frame_encode(MSG_TYPE_SENSOR_DATA, payload, plen,
                              out_buf, out_len);
}

sentinel_err_t pb_encode_health_status(const health_data_t *health,
                                        uint8_t *out_buf, size_t *out_len)
{
    if ((health == NULL) || (out_buf == NULL) || (out_len == NULL)) {
        return SENTINEL_ERR_INVALID_ARG;
    }

    uint8_t payload[PB_ENCODE_BUF_SIZE];
    size_t plen = 0U;

    plen += encode_header(payload + plen, 1U);
    plen += encode_field_varint(payload + plen, 2, (uint64_t)health->state);
    plen += encode_field_varint(payload + plen, 3, health->comm_ok ? 0ULL : 2ULL);
    plen += encode_field_varint(payload + plen, 4, health->uptime_s);
    plen += encode_field_varint(payload + plen, 5, health->watchdog_resets);
    plen += encode_field_varint(payload + plen, 7, health->free_stack);
    plen += encode_field_varint(payload + plen, 8, (uint64_t)health->last_fault);
    plen += encode_field_float(payload + plen, 9, health->fan_duty);
    plen += encode_field_float(payload + plen, 10, health->valve_duty);

    return wire_frame_encode(MSG_TYPE_HEALTH_STATUS, payload, plen,
                              out_buf, out_len);
}

sentinel_err_t pb_encode_actuator_response(uint8_t actuator_id,
                                            float applied_percent,
                                            uint32_t status,
                                            uint8_t *out_buf, size_t *out_len)
{
    if ((out_buf == NULL) || (out_len == NULL)) {
        return SENTINEL_ERR_INVALID_ARG;
    }

    uint8_t payload[PB_ENCODE_BUF_SIZE];
    size_t plen = 0U;

    plen += encode_header(payload + plen, 1U);
    plen += encode_field_varint(payload + plen, 2, actuator_id);
    plen += encode_field_float(payload + plen, 3, applied_percent);
    plen += encode_field_varint(payload + plen, 4, status);

    return wire_frame_encode(MSG_TYPE_ACTUATOR_RSP, payload, plen,
                              out_buf, out_len);
}

sentinel_err_t pb_encode_config_response(uint32_t status,
                                          const char *error_msg,
                                          uint8_t *out_buf, size_t *out_len)
{
    if ((out_buf == NULL) || (out_len == NULL)) {
        return SENTINEL_ERR_INVALID_ARG;
    }

    uint8_t payload[PB_ENCODE_BUF_SIZE];
    size_t plen = 0U;

    plen += encode_header(payload + plen, 1U);
    plen += encode_field_varint(payload + plen, 2, status);

    /* Field 3: error_message (length-delimited string) */
    if ((error_msg != NULL) && (error_msg[0] != '\0')) {
        size_t msg_len = strlen(error_msg);
        if (msg_len > 64U) { msg_len = 64U; }
        plen += encode_varint(payload + plen, (3ULL << 3) | 2U);
        plen += encode_varint(payload + plen, msg_len);
        (void)memcpy(payload + plen, error_msg, msg_len);
        plen += msg_len;
    }

    return wire_frame_encode(MSG_TYPE_CONFIG_RSP, payload, plen,
                              out_buf, out_len);
}

uint32_t pb_get_sequence(void)
{
    return g_sequence;
}
