/**
 * @file test_sensor_proxy.c
 * @brief Unit tests for sensor_proxy module
 * @implements UTP-001 test cases for SWE-013, SWE-015
 */

#include "unity.h"
#include "sensor_proxy.h"
#include <string.h>

void setUp(void)
{
    sensor_proxy_init();
}

void tearDown(void) {}

/* UT-SP-001: Init clears all readings */
void test_init_clears_state(void)
{
    sensor_reading_t r;
    sentinel_err_t err = sensor_proxy_get_latest(0, &r);
    TEST_ASSERT_EQUAL(SENTINEL_OK, err);
    TEST_ASSERT_EQUAL(0, r.channel);
    TEST_ASSERT_EQUAL(0, r.raw_value);
    TEST_ASSERT_FLOAT_WITHIN(0.001f, 0.0f, r.calibrated_value);
}

/* UT-SP-002: Invalid channel returns error */
void test_invalid_channel(void)
{
    sensor_reading_t r;
    TEST_ASSERT_EQUAL(SENTINEL_ERR_INVALID_ARG,
                      sensor_proxy_get_latest(SENTINEL_MAX_CHANNELS, &r));
    TEST_ASSERT_EQUAL(SENTINEL_ERR_INVALID_ARG,
                      sensor_proxy_get_latest(255, &r));
}

/* UT-SP-003: NULL pointer returns error */
void test_null_pointer(void)
{
    TEST_ASSERT_EQUAL(SENTINEL_ERR_INVALID_ARG,
                      sensor_proxy_get_latest(0, NULL));
}

/* UT-SP-004: Process NULL payload returns error */
void test_process_null_payload(void)
{
    TEST_ASSERT_EQUAL(SENTINEL_ERR_INVALID_ARG,
                      sensor_proxy_process(NULL, 10));
}

/* UT-SP-005: Process zero-length payload returns error */
void test_process_zero_length(void)
{
    uint8_t buf[1] = {0};
    TEST_ASSERT_EQUAL(SENTINEL_ERR_INVALID_ARG,
                      sensor_proxy_process(buf, 0));
}

/* UT-SP-006: Process valid SensorData protobuf */
void test_process_valid_sensor_data(void)
{
    /* Minimal SensorData protobuf with one SensorReading:
     * Field 2 (LEN): SensorReading { 1: channel=1, 2: raw=2048, 3: cal=1.5f }
     *
     * Encoding:
     * 12 0B - Field 2 (tag=0x12), length 11
     *   08 01 - Field 1 varint: channel = 1
     *   10 80 10 - Field 2 varint: raw = 2048 (0x800)
     *   1D 00 00 C0 3F - Field 3 fixed32: 1.5f (0x3FC00000)
     */
    uint8_t payload[] = {
        0x12, 0x0B,             /* Field 2, length 11 */
        0x08, 0x01,             /* Field 1: channel = 1 */
        0x10, 0x80, 0x10,       /* Field 2: raw = 2048 */
        0x1D, 0x00, 0x00, 0xC0, 0x3F  /* Field 3: cal = 1.5f */
    };

    sentinel_err_t err = sensor_proxy_process(payload, sizeof(payload));
    TEST_ASSERT_EQUAL(SENTINEL_OK, err);

    sensor_reading_t r;
    err = sensor_proxy_get_latest(1, &r);
    TEST_ASSERT_EQUAL(SENTINEL_OK, err);
    TEST_ASSERT_EQUAL(1, r.channel);
    TEST_ASSERT_EQUAL(2048, r.raw_value);
    TEST_ASSERT_FLOAT_WITHIN(0.001f, 1.5f, r.calibrated_value);
}

/* UT-SP-007: Get all readings */
void test_get_all_readings(void)
{
    sensor_reading_t readings[4];
    uint32_t count = 0;

    sentinel_err_t err = sensor_proxy_get_all(readings, &count);
    TEST_ASSERT_EQUAL(SENTINEL_OK, err);
    TEST_ASSERT_EQUAL(SENTINEL_MAX_CHANNELS, count);
}

/* UT-SP-008: Get all with NULL args returns error */
void test_get_all_null_args(void)
{
    sensor_reading_t readings[4];
    uint32_t count;

    TEST_ASSERT_EQUAL(SENTINEL_ERR_INVALID_ARG,
                      sensor_proxy_get_all(NULL, &count));
    TEST_ASSERT_EQUAL(SENTINEL_ERR_INVALID_ARG,
                      sensor_proxy_get_all(readings, NULL));
}

int main(void)
{
    UNITY_BEGIN();
    RUN_TEST(test_init_clears_state);
    RUN_TEST(test_invalid_channel);
    RUN_TEST(test_null_pointer);
    RUN_TEST(test_process_null_payload);
    RUN_TEST(test_process_zero_length);
    RUN_TEST(test_process_valid_sensor_data);
    RUN_TEST(test_get_all_readings);
    RUN_TEST(test_get_all_null_args);
    return UNITY_END();
}
