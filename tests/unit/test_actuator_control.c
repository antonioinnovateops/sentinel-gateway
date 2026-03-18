/**
 * @file test_actuator_control.c
 * @brief Unit tests for MCU actuator control with safety limits
 * @implements UTP-001 test cases TC-AC-001 through TC-AC-010
 */

#include "vendor/unity.h"
#include "../../src/mcu/actuator_control.h"

static system_config_t g_test_config;

void setUp(void)
{
    g_test_config.actuator_min_percent[0] = 0.0f;
    g_test_config.actuator_min_percent[1] = 0.0f;
    g_test_config.actuator_max_percent[0] = 95.0f;
    g_test_config.actuator_max_percent[1] = 95.0f;
    /* Also set sensor rates to avoid uninitialized issues */
    for (int i = 0; i < SENTINEL_MAX_CHANNELS; i++) {
        g_test_config.sensor_rates_hz[i] = 10;
    }
    g_test_config.heartbeat_interval_ms = 1000;
    g_test_config.comm_timeout_ms = 3000;
    actuator_init(&g_test_config);
}

void tearDown(void) {}

/* TC-AC-001: Set valid duty cycle */
void test_set_valid_duty(void)
{
    uint32_t status = 99;
    sentinel_err_t err = actuator_set(0, 50.0f, &status);
    TEST_ASSERT_EQUAL(SENTINEL_OK, err);
    TEST_ASSERT_EQUAL(0, status); /* STATUS_OK */
}

/* TC-AC-002: Get duty matches set */
void test_get_matches_set(void)
{
    uint32_t status;
    actuator_set(0, 75.0f, &status);

    float actual;
    sentinel_err_t err = actuator_get(0, &actual);
    TEST_ASSERT_EQUAL(SENTINEL_OK, err);
    TEST_ASSERT_FLOAT_WITHIN(1.0f, 75.0f, actual);
}

/* TC-AC-003: Reject above max limit */
void test_reject_above_max(void)
{
    uint32_t status;
    sentinel_err_t err = actuator_set(0, 96.0f, &status);
    TEST_ASSERT_EQUAL(SENTINEL_ERR_OUT_OF_RANGE, err);
    TEST_ASSERT_EQUAL(2, status); /* STATUS_ERROR_OUT_OF_RANGE */
}

/* TC-AC-004: Reject below min limit */
void test_reject_below_min(void)
{
    actuator_set_limits(0, 10.0f, 90.0f);
    uint32_t status;
    sentinel_err_t err = actuator_set(0, 5.0f, &status);
    TEST_ASSERT_EQUAL(SENTINEL_ERR_OUT_OF_RANGE, err);
}

/* TC-AC-005: Reject invalid actuator ID */
void test_reject_invalid_id(void)
{
    uint32_t status;
    sentinel_err_t err = actuator_set(SENTINEL_MAX_ACTUATORS, 50.0f, &status);
    TEST_ASSERT_EQUAL(SENTINEL_ERR_INVALID_ARG, err);
}

/* TC-AC-006: Failsafe sets all to zero */
void test_failsafe_zeros_all(void)
{
    uint32_t status;
    actuator_set(0, 50.0f, &status);
    actuator_set(1, 75.0f, &status);

    actuator_failsafe();

    float duty0, duty1;
    actuator_get(0, &duty0);
    actuator_get(1, &duty1);
    TEST_ASSERT_FLOAT_WITHIN(0.1f, 0.0f, duty0);
    TEST_ASSERT_FLOAT_WITHIN(0.1f, 0.0f, duty1);
}

/* TC-AC-007: Set limits updates constraints */
void test_set_limits(void)
{
    sentinel_err_t err = actuator_set_limits(0, 20.0f, 80.0f);
    TEST_ASSERT_EQUAL(SENTINEL_OK, err);

    uint32_t status;
    TEST_ASSERT_EQUAL(SENTINEL_ERR_OUT_OF_RANGE, actuator_set(0, 15.0f, &status));
    TEST_ASSERT_EQUAL(SENTINEL_OK, actuator_set(0, 50.0f, &status));
    TEST_ASSERT_EQUAL(SENTINEL_ERR_OUT_OF_RANGE, actuator_set(0, 85.0f, &status));
}

/* TC-AC-008: Reject inverted limits */
void test_reject_inverted_limits(void)
{
    TEST_ASSERT_EQUAL(SENTINEL_ERR_INVALID_ARG, actuator_set_limits(0, 80.0f, 20.0f));
}

/* TC-AC-009: Verify readback after set */
void test_verify_readback(void)
{
    uint32_t status;
    actuator_set(0, 50.0f, &status);
    TEST_ASSERT_TRUE(actuator_verify_readback(0));
}

/* TC-AC-010: NULL pointer handling */
void test_null_pointers(void)
{
    TEST_ASSERT_EQUAL(SENTINEL_ERR_INVALID_ARG, actuator_get(0, NULL));
}

int main(void)
{
    UNITY_BEGIN();
    RUN_TEST(test_set_valid_duty);
    RUN_TEST(test_get_matches_set);
    RUN_TEST(test_reject_above_max);
    RUN_TEST(test_reject_below_min);
    RUN_TEST(test_reject_invalid_id);
    RUN_TEST(test_failsafe_zeros_all);
    RUN_TEST(test_set_limits);
    RUN_TEST(test_reject_inverted_limits);
    RUN_TEST(test_verify_readback);
    RUN_TEST(test_null_pointers);
    return UNITY_END();
}
