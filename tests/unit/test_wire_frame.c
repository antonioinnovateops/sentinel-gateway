/**
 * @file test_wire_frame.c
 * @brief Unit tests for wire frame encode/decode
 * @implements UTP-001 test cases for SWE-035
 */

#include "unity.h"
#include "wire_frame.h"
#include "sentinel_types.h"
#include <string.h>

void setUp(void) {}
void tearDown(void) {}

/* UT-WF-001: Encode minimal payload */
void test_encode_minimal_payload(void)
{
    uint8_t payload[] = {0x01, 0x02, 0x03};
    uint8_t out[WIRE_FRAME_MAX_SIZE];
    size_t out_len = 0;

    sentinel_err_t err = wire_frame_encode(MSG_TYPE_SENSOR_DATA,
                                            payload, sizeof(payload),
                                            out, &out_len);
    TEST_ASSERT_EQUAL(SENTINEL_OK, err);
    TEST_ASSERT_EQUAL(WIRE_FRAME_HEADER_SIZE + 3, out_len);

    /* Verify LE length */
    TEST_ASSERT_EQUAL(3, out[0]);
    TEST_ASSERT_EQUAL(0, out[1]);
    TEST_ASSERT_EQUAL(0, out[2]);
    TEST_ASSERT_EQUAL(0, out[3]);

    /* Verify type */
    TEST_ASSERT_EQUAL(MSG_TYPE_SENSOR_DATA, out[4]);

    /* Verify payload */
    TEST_ASSERT_EQUAL_MEMORY(payload, out + 5, 3);
}

/* UT-WF-002: Encode empty payload */
void test_encode_empty_payload(void)
{
    uint8_t out[WIRE_FRAME_MAX_SIZE];
    size_t out_len = 0;
    uint8_t dummy = 0;

    sentinel_err_t err = wire_frame_encode(MSG_TYPE_HEALTH_STATUS,
                                            &dummy, 0,
                                            out, &out_len);
    TEST_ASSERT_EQUAL(SENTINEL_OK, err);
    TEST_ASSERT_EQUAL(WIRE_FRAME_HEADER_SIZE, out_len);
}

/* UT-WF-003: Encode max payload */
void test_encode_max_payload(void)
{
    uint8_t payload[WIRE_FRAME_MAX_PAYLOAD];
    memset(payload, 0xAA, sizeof(payload));
    uint8_t out[WIRE_FRAME_MAX_SIZE];
    size_t out_len = 0;

    sentinel_err_t err = wire_frame_encode(MSG_TYPE_SENSOR_DATA,
                                            payload, WIRE_FRAME_MAX_PAYLOAD,
                                            out, &out_len);
    TEST_ASSERT_EQUAL(SENTINEL_OK, err);
    TEST_ASSERT_EQUAL(WIRE_FRAME_MAX_SIZE, out_len);
}

/* UT-WF-004: Encode oversized payload rejected */
void test_encode_oversized_payload(void)
{
    uint8_t payload[WIRE_FRAME_MAX_PAYLOAD + 1];
    uint8_t out[WIRE_FRAME_MAX_SIZE + 1];
    size_t out_len = 0;

    sentinel_err_t err = wire_frame_encode(MSG_TYPE_SENSOR_DATA,
                                            payload, WIRE_FRAME_MAX_PAYLOAD + 1,
                                            out, &out_len);
    TEST_ASSERT_EQUAL(SENTINEL_ERR_OUT_OF_RANGE, err);
}

/* UT-WF-005: Encode NULL args rejected */
void test_encode_null_args(void)
{
    uint8_t buf[16];
    size_t len = 0;

    TEST_ASSERT_EQUAL(SENTINEL_ERR_INVALID_ARG,
                      wire_frame_encode(0, NULL, 1, buf, &len));
    TEST_ASSERT_EQUAL(SENTINEL_ERR_INVALID_ARG,
                      wire_frame_encode(0, buf, 1, NULL, &len));
    TEST_ASSERT_EQUAL(SENTINEL_ERR_INVALID_ARG,
                      wire_frame_encode(0, buf, 1, buf, NULL));
}

/* UT-WF-006: Decode valid header */
void test_decode_valid_header(void)
{
    uint8_t frame[] = {0x0A, 0x00, 0x00, 0x00, MSG_TYPE_ACTUATOR_CMD};
    uint8_t msg_type = 0;
    uint32_t payload_len = 0;

    sentinel_err_t err = wire_frame_decode_header(frame, sizeof(frame),
                                                    &msg_type, &payload_len);
    TEST_ASSERT_EQUAL(SENTINEL_OK, err);
    TEST_ASSERT_EQUAL(MSG_TYPE_ACTUATOR_CMD, msg_type);
    TEST_ASSERT_EQUAL(10, payload_len);
}

/* UT-WF-007: Decode oversized length rejected */
void test_decode_oversized_length(void)
{
    uint8_t frame[] = {0xFF, 0xFF, 0x00, 0x00, 0x01};
    uint8_t msg_type = 0;
    uint32_t payload_len = 0;

    sentinel_err_t err = wire_frame_decode_header(frame, sizeof(frame),
                                                    &msg_type, &payload_len);
    TEST_ASSERT_EQUAL(SENTINEL_ERR_OUT_OF_RANGE, err);
}

/* UT-WF-008: Decode too-short buffer rejected */
void test_decode_short_buffer(void)
{
    uint8_t frame[] = {0x01, 0x00, 0x00};
    uint8_t msg_type = 0;
    uint32_t payload_len = 0;

    sentinel_err_t err = wire_frame_decode_header(frame, sizeof(frame),
                                                    &msg_type, &payload_len);
    TEST_ASSERT_EQUAL(SENTINEL_ERR_INVALID_ARG, err);
}

/* UT-WF-009: Roundtrip encode→decode */
void test_roundtrip(void)
{
    uint8_t payload[] = {0xDE, 0xAD, 0xBE, 0xEF};
    uint8_t frame[WIRE_FRAME_MAX_SIZE];
    size_t frame_len = 0;

    sentinel_err_t err = wire_frame_encode(MSG_TYPE_CONFIG_UPDATE,
                                            payload, sizeof(payload),
                                            frame, &frame_len);
    TEST_ASSERT_EQUAL(SENTINEL_OK, err);

    uint8_t msg_type = 0;
    uint32_t payload_len = 0;
    err = wire_frame_decode_header(frame, frame_len, &msg_type, &payload_len);
    TEST_ASSERT_EQUAL(SENTINEL_OK, err);
    TEST_ASSERT_EQUAL(MSG_TYPE_CONFIG_UPDATE, msg_type);
    TEST_ASSERT_EQUAL(4, payload_len);
    TEST_ASSERT_EQUAL_MEMORY(payload, frame + WIRE_FRAME_HEADER_SIZE, 4);
}

/* UT-WF-010: All message types */
void test_all_message_types(void)
{
    uint8_t types[] = {
        MSG_TYPE_SENSOR_DATA, MSG_TYPE_HEALTH_STATUS,
        MSG_TYPE_ACTUATOR_CMD, MSG_TYPE_ACTUATOR_RSP,
        MSG_TYPE_CONFIG_UPDATE, MSG_TYPE_CONFIG_RSP,
        MSG_TYPE_DIAG_REQ, MSG_TYPE_DIAG_RSP,
        MSG_TYPE_STATE_SYNC_REQ, MSG_TYPE_STATE_SYNC_RSP
    };
    uint8_t payload = 0x42;
    uint8_t frame[WIRE_FRAME_MAX_SIZE];

    for (int i = 0; i < 10; i++) {
        size_t frame_len = 0;
        sentinel_err_t err = wire_frame_encode(types[i], &payload, 1,
                                                frame, &frame_len);
        TEST_ASSERT_EQUAL(SENTINEL_OK, err);
        TEST_ASSERT_EQUAL(types[i], frame[4]);
    }
}

int main(void)
{
    UNITY_BEGIN();
    RUN_TEST(test_encode_minimal_payload);
    RUN_TEST(test_encode_empty_payload);
    RUN_TEST(test_encode_max_payload);
    RUN_TEST(test_encode_oversized_payload);
    RUN_TEST(test_encode_null_args);
    RUN_TEST(test_decode_valid_header);
    RUN_TEST(test_decode_oversized_length);
    RUN_TEST(test_decode_short_buffer);
    RUN_TEST(test_roundtrip);
    RUN_TEST(test_all_message_types);
    return UNITY_END();
}
