/**
 * @file test_actuator_control.c
 * @brief Unit tests for MCU actuator control
 * @implements UTP-001 test cases for SWE-016 through SWE-029
 */

#include "unity.h"
#include "actuator_control.h"

static system_config_t default_config(void)
{
    system_config_t c = {0};
    c.actuator_min_percent[0] = 0.0f;
    c.actuator_min_percent[1] = 0.0f;
    c.actuator_max_percent[0] = 95.0f;
    c.actuator_max_percent[1] = 95.0f;
    return c;
}

void setUp(void)
{
    system_config_t c = default_config();
    actuator_init(&c);
}
void tearDown(void) {}

/* UT-AC-001: Set valid duty cycle */
void test_set_valid_duty(void)
{
    uint32_t status = 99;
    sentinel_err_t err = actuator_set(0, 50.0f, &status);
    TEST_ASSERT_EQUAL(SENTINEL_OK, err);
    TEST_ASSERT_EQUAL(0, status);

    float duty = 0;
    actuator_get(0, &duty);
    TEST_ASSERT_FLOAT_WITHIN(1.0f, 50.0f, duty);
}

/* UT-AC-002: Reject out of range (above max) */
void test_reject_above_max(void)
{
    uint32_t status = 0;
    sentinel_err_t err = actuator_set(0, 96.0f, &status);
    TEST_ASSERT_EQUAL(SENTINEL_ERR_OUT_OF_RANGE, err);
    TEST_ASSERT_EQUAL(2, status);
}

/* UT-AC-003: Reject out of range (below min) */
void test_reject_below_min(void)
{
    actuator_set_limits(0, 10.0f, 95.0f);
    uint32_t status = 0;
    sentinel_err_t err = actuator_set(0, 5.0f, &status);
    TEST_ASSERT_EQUAL(SENTINEL_ERR_OUT_OF_RANGE, err);
}

/* UT-AC-004: Invalid actuator ID */
void test_invalid_actuator_id(void)
{
    uint32_t status = 0;
    sentinel_err_t err = actuator_set(5, 50.0f, &status);
    TEST_ASSERT_EQUAL(SENTINEL_ERR_INVALID_ARG, err);
}

/* UT-AC-005: Failsafe sets all to zero */
void test_failsafe(void)
{
    uint32_t status = 0;
    actuator_set(0, 75.0f, &status);
    actuator_set(1, 50.0f, &status);

    actuator_failsafe();

    float d0 = 99, d1 = 99;
    actuator_get(0, &d0);
    actuator_get(1, &d1);
    TEST_ASSERT_FLOAT_WITHIN(0.1f, 0.0f, d0);
    TEST_ASSERT_FLOAT_WITHIN(0.1f, 0.0f, d1);
}

/* UT-AC-006: Readback verify */
void test_readback_verify(void)
{
    uint32_t status = 0;
    actuator_set(0, 60.0f, &status);
    TEST_ASSERT_TRUE(actuator_verify_readback(0));
}

/* UT-AC-007: Set limits */
void test_set_limits(void)
{
    sentinel_err_t err = actuator_set_limits(0, 10.0f, 80.0f);
    TEST_ASSERT_EQUAL(SENTINEL_OK, err);

    uint32_t status = 0;
    err = actuator_set(0, 85.0f, &status);
    TEST_ASSERT_EQUAL(SENTINEL_ERR_OUT_OF_RANGE, err);

    err = actuator_set(0, 5.0f, &status);
    TEST_ASSERT_EQUAL(SENTINEL_ERR_OUT_OF_RANGE, err);

    err = actuator_set(0, 50.0f, &status);
    TEST_ASSERT_EQUAL(SENTINEL_OK, err);
}

/* UT-AC-008: Invalid limits rejected */
void test_invalid_limits(void)
{
    sentinel_err_t err = actuator_set_limits(0, 80.0f, 50.0f);
    TEST_ASSERT_EQUAL(SENTINEL_ERR_INVALID_ARG, err);
}

int main(void)
{
    UNITY_BEGIN();
    RUN_TEST(test_set_valid_duty);
    RUN_TEST(test_reject_above_max);
    RUN_TEST(test_reject_below_min);
    RUN_TEST(test_invalid_actuator_id);
    RUN_TEST(test_failsafe);
    RUN_TEST(test_readback_verify);
    RUN_TEST(test_set_limits);
    RUN_TEST(test_invalid_limits);
    return UNITY_END();
}
