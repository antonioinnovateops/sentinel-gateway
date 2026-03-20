/**
 * @file test_wire_frame.c
 * @brief Unit tests for wire frame encode/decode
 * @implements UTP-001 test cases TC-WF-001 through TC-WF-008
 */

#include "vendor/unity.h"
#include "../../src/common/wire_frame.h"
#include <string.h>

void setUp(void) {}
void tearDown(void) {}

/* TC-WF-001: Encode valid frame */
void test_wire_frame_encode_valid(void)
{
    uint8_t payload[] = {0x01, 0x02, 0x03};
    uint8_t out[WIRE_FRAME_MAX_SIZE];
    size_t out_len = 0;

    sentinel_err_t err = wire_frame_encode(MSG_TYPE_SENSOR_DATA, payload, 3, out, &out_len);

    TEST_ASSERT_EQUAL(SENTINEL_OK, err);
    TEST_ASSERT_EQUAL(8, out_len); /* 5 header + 3 payload */
    TEST_ASSERT_EQUAL(3, out[0]);  /* Length LE byte 0 */
    TEST_ASSERT_EQUAL(0, out[1]);
    TEST_ASSERT_EQUAL(0, out[2]);
    TEST_ASSERT_EQUAL(0, out[3]);
    TEST_ASSERT_EQUAL(MSG_TYPE_SENSOR_DATA, out[4]); /* Type */
    TEST_ASSERT_EQUAL(0x01, out[5]);
    TEST_ASSERT_EQUAL(0x02, out[6]);
    TEST_ASSERT_EQUAL(0x03, out[7]);
}

/* TC-WF-002: Encode empty payload */
void test_wire_frame_encode_empty_payload(void)
{
    uint8_t payload[] = {0};
    uint8_t out[WIRE_FRAME_MAX_SIZE];
    size_t out_len = 0;

    sentinel_err_t err = wire_frame_encode(MSG_TYPE_HEALTH_STATUS, payload, 0, out, &out_len);

    TEST_ASSERT_EQUAL(SENTINEL_OK, err);
    TEST_ASSERT_EQUAL(WIRE_FRAME_HEADER_SIZE, out_len);
}

/* TC-WF-003: Encode rejects oversized payload */
void test_wire_frame_encode_oversized(void)
{
    uint8_t payload[WIRE_FRAME_MAX_PAYLOAD + 1];
    uint8_t out[WIRE_FRAME_MAX_SIZE + 10];
    size_t out_len = 0;

    sentinel_err_t err = wire_frame_encode(0x01, payload, WIRE_FRAME_MAX_PAYLOAD + 1, out, &out_len);

    TEST_ASSERT_EQUAL(SENTINEL_ERR_OUT_OF_RANGE, err);
}

/* TC-WF-004: Encode rejects NULL args */
void test_wire_frame_encode_null_args(void)
{
    uint8_t buf[16];
    size_t len;

    TEST_ASSERT_EQUAL(SENTINEL_ERR_INVALID_ARG, wire_frame_encode(0x01, NULL, 0, buf, &len));
    TEST_ASSERT_EQUAL(SENTINEL_ERR_INVALID_ARG, wire_frame_encode(0x01, buf, 1, NULL, &len));
    TEST_ASSERT_EQUAL(SENTINEL_ERR_INVALID_ARG, wire_frame_encode(0x01, buf, 1, buf, NULL));
}

/* TC-WF-005: Decode valid header */
void test_wire_frame_decode_valid(void)
{
    uint8_t frame[] = {0x0A, 0x00, 0x00, 0x00, MSG_TYPE_ACTUATOR_CMD};
    uint8_t msg_type;
    uint32_t payload_len;

    sentinel_err_t err = wire_frame_decode_header(frame, sizeof(frame), &msg_type, &payload_len);

    TEST_ASSERT_EQUAL(SENTINEL_OK, err);
    TEST_ASSERT_EQUAL(MSG_TYPE_ACTUATOR_CMD, msg_type);
    TEST_ASSERT_EQUAL(10, payload_len);
}

/* TC-WF-006: Decode rejects short buffer */
void test_wire_frame_decode_short_buffer(void)
{
    uint8_t frame[] = {0x01, 0x00, 0x00};
    uint8_t msg_type;
    uint32_t payload_len;

    sentinel_err_t err = wire_frame_decode_header(frame, 3, &msg_type, &payload_len);

    TEST_ASSERT_EQUAL(SENTINEL_ERR_INVALID_ARG, err);
}

/* TC-WF-007: Decode rejects oversized length */
void test_wire_frame_decode_oversized_length(void)
{
    uint8_t frame[] = {0xFF, 0xFF, 0x00, 0x00, 0x01};
    uint8_t msg_type;
    uint32_t payload_len;

    sentinel_err_t err = wire_frame_decode_header(frame, 5, &msg_type, &payload_len);

    TEST_ASSERT_EQUAL(SENTINEL_ERR_OUT_OF_RANGE, err);
}

/* TC-WF-008: Round-trip encode/decode */
void test_wire_frame_round_trip(void)
{
    uint8_t payload[] = {0xDE, 0xAD, 0xBE, 0xEF};
    uint8_t frame[WIRE_FRAME_MAX_SIZE];
    size_t frame_len = 0;
    uint8_t msg_type;
    uint32_t decoded_len;

    wire_frame_encode(MSG_TYPE_CONFIG_UPDATE, payload, 4, frame, &frame_len);
    wire_frame_decode_header(frame, frame_len, &msg_type, &decoded_len);

    TEST_ASSERT_EQUAL(MSG_TYPE_CONFIG_UPDATE, msg_type);
    TEST_ASSERT_EQUAL(4, decoded_len);
    TEST_ASSERT_EQUAL_MEMORY(payload, frame + WIRE_FRAME_HEADER_SIZE, 4);
}

/* TC-WF-009: Decode rejects maximum uint32 payload length */
void test_wire_frame_decode_max_payload_len(void)
{
    uint8_t frame[] = {0xFF, 0xFF, 0xFF, 0xFF, 0x01};
    uint8_t msg_type;
    uint32_t payload_len;

    sentinel_err_t err = wire_frame_decode_header(frame, 5, &msg_type, &payload_len);

    TEST_ASSERT_EQUAL(SENTINEL_ERR_OUT_OF_RANGE, err);
}

/* TC-WF-010: Decode rejects NULL output pointers */
void test_wire_frame_decode_null_outputs(void)
{
    uint8_t frame[] = {0x01, 0x00, 0x00, 0x00, 0x01};
    uint8_t msg_type;
    uint32_t payload_len;

    TEST_ASSERT_EQUAL(SENTINEL_ERR_INVALID_ARG,
        wire_frame_decode_header(NULL, 5, &msg_type, &payload_len));
    TEST_ASSERT_EQUAL(SENTINEL_ERR_INVALID_ARG,
        wire_frame_decode_header(frame, 5, NULL, &payload_len));
    TEST_ASSERT_EQUAL(SENTINEL_ERR_INVALID_ARG,
        wire_frame_decode_header(frame, 5, &msg_type, NULL));
}

/* TC-WF-011: Decode with zero-length buffer */
void test_wire_frame_decode_zero_length_buffer(void)
{
    uint8_t frame[] = {0x00};
    uint8_t msg_type;
    uint32_t payload_len;

    sentinel_err_t err = wire_frame_decode_header(frame, 0, &msg_type, &payload_len);

    TEST_ASSERT_EQUAL(SENTINEL_ERR_INVALID_ARG, err);
}

/* TC-WF-012: Decode header with zero payload length is valid */
void test_wire_frame_decode_zero_payload_len(void)
{
    uint8_t frame[] = {0x00, 0x00, 0x00, 0x00, MSG_TYPE_HEALTH_STATUS};
    uint8_t msg_type;
    uint32_t payload_len;

    sentinel_err_t err = wire_frame_decode_header(frame, 5, &msg_type, &payload_len);

    TEST_ASSERT_EQUAL(SENTINEL_OK, err);
    TEST_ASSERT_EQUAL(MSG_TYPE_HEALTH_STATUS, msg_type);
    TEST_ASSERT_EQUAL(0, payload_len);
}

/* TC-WF-013: Decode exactly at max valid payload boundary */
void test_wire_frame_decode_boundary_payload_len(void)
{
    /* WIRE_FRAME_MAX_PAYLOAD encoded as LE uint32 */
    uint32_t max_payload = WIRE_FRAME_MAX_PAYLOAD;
    uint8_t frame[5];
    frame[0] = (uint8_t)(max_payload & 0xFF);
    frame[1] = (uint8_t)((max_payload >> 8) & 0xFF);
    frame[2] = (uint8_t)((max_payload >> 16) & 0xFF);
    frame[3] = (uint8_t)((max_payload >> 24) & 0xFF);
    frame[4] = MSG_TYPE_SENSOR_DATA;

    uint8_t msg_type;
    uint32_t payload_len;

    sentinel_err_t err = wire_frame_decode_header(frame, 5, &msg_type, &payload_len);

    TEST_ASSERT_EQUAL(SENTINEL_OK, err);
    TEST_ASSERT_EQUAL(max_payload, payload_len);

    /* One above max should fail */
    uint32_t over = max_payload + 1;
    frame[0] = (uint8_t)(over & 0xFF);
    frame[1] = (uint8_t)((over >> 8) & 0xFF);
    frame[2] = (uint8_t)((over >> 16) & 0xFF);
    frame[3] = (uint8_t)((over >> 24) & 0xFF);

    err = wire_frame_decode_header(frame, 5, &msg_type, &payload_len);
    TEST_ASSERT_EQUAL(SENTINEL_ERR_OUT_OF_RANGE, err);
}

int main(void)
{
    UNITY_BEGIN();
    RUN_TEST(test_wire_frame_encode_valid);
    RUN_TEST(test_wire_frame_encode_empty_payload);
    RUN_TEST(test_wire_frame_encode_oversized);
    RUN_TEST(test_wire_frame_encode_null_args);
    RUN_TEST(test_wire_frame_decode_valid);
    RUN_TEST(test_wire_frame_decode_short_buffer);
    RUN_TEST(test_wire_frame_decode_oversized_length);
    RUN_TEST(test_wire_frame_round_trip);
    /* Negative / boundary tests [T-04] */
    RUN_TEST(test_wire_frame_decode_max_payload_len);
    RUN_TEST(test_wire_frame_decode_null_outputs);
    RUN_TEST(test_wire_frame_decode_zero_length_buffer);
    RUN_TEST(test_wire_frame_decode_zero_payload_len);
    RUN_TEST(test_wire_frame_decode_boundary_payload_len);
    return UNITY_END();
}
