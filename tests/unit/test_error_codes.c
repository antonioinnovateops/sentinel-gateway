/**
 * @file test_error_codes.c
 * @brief Unit tests verifying error code values
 */

#include "vendor/unity.h"
#include "../../src/common/error_codes.h"

void setUp(void) {}
void tearDown(void) {}

void test_sentinel_ok_is_zero(void)
{
    TEST_ASSERT_EQUAL(0, SENTINEL_OK);
}

void test_errors_are_negative(void)
{
    TEST_ASSERT_TRUE(SENTINEL_ERR_INVALID_ARG < 0);
    TEST_ASSERT_TRUE(SENTINEL_ERR_OUT_OF_RANGE < 0);
    TEST_ASSERT_TRUE(SENTINEL_ERR_TIMEOUT < 0);
    TEST_ASSERT_TRUE(SENTINEL_ERR_COMM < 0);
    TEST_ASSERT_TRUE(SENTINEL_ERR_DECODE < 0);
    TEST_ASSERT_TRUE(SENTINEL_ERR_ENCODE < 0);
    TEST_ASSERT_TRUE(SENTINEL_ERR_FULL < 0);
    TEST_ASSERT_TRUE(SENTINEL_ERR_NOT_READY < 0);
    TEST_ASSERT_TRUE(SENTINEL_ERR_AUTH < 0);
    TEST_ASSERT_TRUE(SENTINEL_ERR_INTERNAL < 0);
}

void test_error_codes_unique(void)
{
    TEST_ASSERT_NOT_EQUAL(SENTINEL_ERR_INVALID_ARG, SENTINEL_ERR_TIMEOUT);
    TEST_ASSERT_NOT_EQUAL(SENTINEL_ERR_COMM, SENTINEL_ERR_DECODE);
    TEST_ASSERT_NOT_EQUAL(SENTINEL_ERR_AUTH, SENTINEL_ERR_INTERNAL);
}

int main(void)
{
    UNITY_BEGIN();
    RUN_TEST(test_sentinel_ok_is_zero);
    RUN_TEST(test_errors_are_negative);
    RUN_TEST(test_error_codes_unique);
    return UNITY_END();
}
