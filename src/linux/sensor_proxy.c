/**
 * @file sensor_proxy.c
 * @brief SW-03: Sensor data reception and logging
 * @implements SWE-013, SWE-015
 *
 * Decodes SensorData protobuf from MCU, caches latest readings per channel.
 * Protobuf field layout (matches MCU protobuf_handler.c):
 *   Field 1 (LEN): MessageHeader (skipped)
 *   Field 2 (LEN): repeated SensorReading {1:channel, 2:raw, 3:cal}
 *   Field 3 (VARINT): sample_rate_hz
 */

#include "sensor_proxy.h"
#include "logger.h"
#include <string.h>
#include <stdio.h>

static sensor_reading_t g_latest[SENTINEL_MAX_CHANNELS];
static uint32_t g_msg_count = 0U;
static uint32_t g_err_count = 0U;

/* ---- Minimal protobuf decoder ---- */

static size_t dec_varint(const uint8_t *buf, size_t len, uint64_t *val)
{
    *val = 0;
    size_t shift = 0;
    for (size_t i = 0; i < len && i < 10U; i++) {
        *val |= (uint64_t)(buf[i] & 0x7FU) << shift;
        shift += 7;
        if ((buf[i] & 0x80U) == 0) { return i + 1; }
    }
    return 0; /* error */
}

static float dec_float32(const uint8_t *buf)
{
    uint32_t bits = (uint32_t)buf[0] | ((uint32_t)buf[1] << 8) |
                    ((uint32_t)buf[2] << 16) | ((uint32_t)buf[3] << 24);
    float val;
    memcpy(&val, &bits, sizeof(val));
    return val;
}

/* Decode a SensorReading sub-message: {1:channel, 2:raw, 3:cal} */
static void decode_reading(const uint8_t *buf, size_t len)
{
    size_t pos = 0;
    uint8_t channel = 0;
    uint32_t raw = 0;
    float cal = 0.0f;

    while (pos < len) {
        uint64_t tag;
        size_t n = dec_varint(buf + pos, len - pos, &tag);
        if (n == 0) { break; }
        pos += n;
        uint32_t field = (uint32_t)(tag >> 3);
        uint32_t wtype = (uint32_t)(tag & 0x07);

        if (wtype == 0) { /* varint */
            uint64_t v;
            n = dec_varint(buf + pos, len - pos, &v);
            if (n == 0) { break; }
            pos += n;
            if (field == 1) { channel = (uint8_t)v; }
            else if (field == 2) { raw = (uint32_t)v; }
        } else if (wtype == 5) { /* fixed32 */
            if (pos + 4 > len) { break; }
            if (field == 3) { cal = dec_float32(buf + pos); }
            pos += 4;
        } else if (wtype == 2) { /* length-delimited — skip */
            uint64_t slen;
            n = dec_varint(buf + pos, len - pos, &slen);
            if (n == 0) { break; }
            pos += n + (size_t)slen;
        } else {
            break;
        }
    }

    if (channel < SENTINEL_MAX_CHANNELS) {
        g_latest[channel].channel = channel;
        g_latest[channel].raw_value = raw;
        g_latest[channel].calibrated_value = cal;
    }
}

sentinel_err_t sensor_proxy_init(void)
{
    (void)memset(g_latest, 0, sizeof(g_latest));
    g_msg_count = 0U;
    g_err_count = 0U;
    return SENTINEL_OK;
}

sentinel_err_t sensor_proxy_process(const uint8_t *payload, size_t len)
{
    if (payload == NULL || len == 0) {
        g_err_count++;
        return SENTINEL_ERR_INVALID_ARG;
    }

    /* Iterate protobuf fields in the SensorData message */
    size_t pos = 0;
    while (pos < len) {
        uint64_t tag;
        size_t n = dec_varint(payload + pos, len - pos, &tag);
        if (n == 0) { break; }
        pos += n;
        uint32_t field = (uint32_t)(tag >> 3);
        uint32_t wtype = (uint32_t)(tag & 0x07);

        if (wtype == 0) { /* varint */
            uint64_t v;
            n = dec_varint(payload + pos, len - pos, &v);
            if (n == 0) { break; }
            pos += n;
            /* field 3 = sample_rate_hz — ignored for now */
        } else if (wtype == 2) { /* length-delimited */
            uint64_t slen;
            n = dec_varint(payload + pos, len - pos, &slen);
            if (n == 0) { break; }
            pos += n;
            if (pos + slen > len) { break; }

            if (field == 2) {
                /* SensorReading sub-message */
                decode_reading(payload + pos, (size_t)slen);
            }
            /* field 1 = header — skip */
            pos += (size_t)slen;
        } else if (wtype == 5) { /* fixed32 — skip */
            pos += 4;
        } else {
            break;
        }
    }

    g_msg_count++;
    return SENTINEL_OK;
}

sentinel_err_t sensor_proxy_get_latest(uint8_t channel, sensor_reading_t *out)
{
    if ((channel >= SENTINEL_MAX_CHANNELS) || (out == NULL)) {
        return SENTINEL_ERR_INVALID_ARG;
    }
    *out = g_latest[channel];
    return SENTINEL_OK;
}

sentinel_err_t sensor_proxy_get_all(sensor_reading_t readings[4], uint32_t *count)
{
    if ((readings == NULL) || (count == NULL)) {
        return SENTINEL_ERR_INVALID_ARG;
    }
    (void)memcpy(readings, g_latest, sizeof(g_latest));
    *count = SENTINEL_MAX_CHANNELS;
    return SENTINEL_OK;
}
