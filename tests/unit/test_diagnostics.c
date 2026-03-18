/**
 * @file test_diagnostics.c
 * @brief Unit tests for Linux diagnostic interface
 * @implements UTP-001 test cases for SWE-046 through SWE-051
 */

#include "unity.h"
#include "diagnostics.h"
#include <string.h>

void setUp(void)
{
    diagnostics_init();
}
void tearDown(void) {}

/* UT-DG-001: Status command */
void test_status_command(void)
{
    char response[256];
    sentinel_err_t err = diagnostics_process_command("status", response, sizeof(response));
    TEST_ASSERT_EQUAL(SENTINEL_OK, err);
    TEST_ASSERT_NOT_NULL(strstr(response, "state="));
    TEST_ASSERT_NOT_NULL(strstr(response, "comm="));
}

/* UT-DG-002: Version command */
void test_version_command(void)
{
    char response[256];
    sentinel_err_t err = diagnostics_process_command("version", response, sizeof(response));
    TEST_ASSERT_EQUAL(SENTINEL_OK, err);
    TEST_ASSERT_NOT_NULL(strstr(response, "linux=1.0.0"));
}

/* UT-DG-003: Help command */
void test_help_command(void)
{
    char response[256];
    sentinel_err_t err = diagnostics_process_command("help", response, sizeof(response));
    TEST_ASSERT_EQUAL(SENTINEL_OK, err);
    TEST_ASSERT_NOT_NULL(strstr(response, "Commands:"));
}

/* UT-DG-004: Unknown command */
void test_unknown_command(void)
{
    char response[256];
    sentinel_err_t err = diagnostics_process_command("foobar", response, sizeof(response));
    TEST_ASSERT_EQUAL(SENTINEL_OK, err);
    TEST_ASSERT_NOT_NULL(strstr(response, "ERROR"));
}

/* UT-DG-005: NULL args */
void test_null_args(void)
{
    char buf[64];
    TEST_ASSERT_EQUAL(SENTINEL_ERR_INVALID_ARG,
                      diagnostics_process_command(NULL, buf, sizeof(buf)));
    TEST_ASSERT_EQUAL(SENTINEL_ERR_INVALID_ARG,
                      diagnostics_process_command("status", NULL, 0));
}

/* UT-DG-006: Sensor read command */
void test_sensor_read(void)
{
    char response[256];
    sentinel_err_t err = diagnostics_process_command("sensor read 0", response, sizeof(response));
    TEST_ASSERT_EQUAL(SENTINEL_OK, err);
    TEST_ASSERT_NOT_NULL(strstr(response, "ch=0"));
}

int main(void)
{
    UNITY_BEGIN();
    RUN_TEST(test_status_command);
    RUN_TEST(test_version_command);
    RUN_TEST(test_help_command);
    RUN_TEST(test_unknown_command);
    RUN_TEST(test_null_args);
    RUN_TEST(test_sensor_read);
    return UNITY_END();
}
