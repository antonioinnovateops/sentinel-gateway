/**
 * @file test_health_monitor.c
 * @brief Unit tests for health_monitor module
 * @implements UTP-001 test cases for SWE-042 through SWE-045
 */

#include "unity.h"
#include "health_monitor.h"
#include <string.h>

void setUp(void)
{
    health_monitor_init();
}

void tearDown(void) {}

/* UT-HM-001: Init sets healthy state */
void test_init_healthy_state(void)
{
    linux_health_state_t state;
    sentinel_err_t err = health_monitor_get_state(&state);
    TEST_ASSERT_EQUAL(SENTINEL_OK, err);
    TEST_ASSERT_EQUAL(STATE_INIT, state.state);
}

/* UT-HM-002: Get state with NULL returns error */
void test_get_state_null(void)
{
    TEST_ASSERT_EQUAL(SENTINEL_ERR_INVALID_ARG,
                      health_monitor_get_state(NULL));
}

/* UT-HM-003: Process NULL payload returns error */
void test_process_null_payload(void)
{
    TEST_ASSERT_EQUAL(SENTINEL_ERR_INVALID_ARG,
                      health_monitor_process_health(NULL, 10));
}

/* UT-HM-004: Process zero-length payload returns error */
void test_process_zero_length(void)
{
    uint8_t buf[1] = {0};
    TEST_ASSERT_EQUAL(SENTINEL_ERR_INVALID_ARG,
                      health_monitor_process_health(buf, 0));
}

/* UT-HM-005: Process valid HealthStatus sets state */
void test_process_valid_health(void)
{
    /* Minimal HealthStatus protobuf:
     * Field 2 (varint): state = 1 (NORMAL)
     * Field 4 (varint): uptime_s = 100
     * Field 5 (varint): watchdog_resets = 0
     *
     * Encoding:
     * 10 01 - Field 2: state = 1
     * 20 64 - Field 4: uptime_s = 100
     * 28 00 - Field 5: watchdog_resets = 0
     */
    uint8_t payload[] = {
        0x10, 0x01,  /* Field 2: state = NORMAL */
        0x20, 0x64,  /* Field 4: uptime_s = 100 */
        0x28, 0x00   /* Field 5: watchdog_resets = 0 */
    };

    sentinel_err_t err = health_monitor_process_health(payload, sizeof(payload));
    TEST_ASSERT_EQUAL(SENTINEL_OK, err);

    linux_health_state_t state;
    err = health_monitor_get_state(&state);
    TEST_ASSERT_EQUAL(SENTINEL_OK, err);
    TEST_ASSERT_EQUAL(STATE_NORMAL, state.state);
    TEST_ASSERT_EQUAL(100, state.uptime_s);
    TEST_ASSERT_EQUAL(0, state.watchdog_resets);
    TEST_ASSERT_TRUE(state.comm_ok);
}

/* UT-HM-006: MCU version accessors work */
void test_mcu_version_accessors(void)
{
    uint32_t major = 99, minor = 99;
    health_monitor_get_mcu_version(&major, &minor);
    /* After init, version should be 0.0 */
    TEST_ASSERT_EQUAL(0, major);
    TEST_ASSERT_EQUAL(0, minor);
}

/* UT-HM-007: MCU version with NULL args is safe */
void test_mcu_version_null_args(void)
{
    /* Should not crash */
    health_monitor_get_mcu_version(NULL, NULL);
    uint32_t major = 99;
    health_monitor_get_mcu_version(&major, NULL);
    TEST_ASSERT_EQUAL(0, major);
}

/* UT-HM-008: Tick does not crash with valid timestamp */
void test_tick_valid(void)
{
    /* Should not crash */
    health_monitor_tick(1000);
    health_monitor_tick(2000);
    health_monitor_tick(3000);

    linux_health_state_t state;
    health_monitor_get_state(&state);
    /* State may have changed, but should be valid */
    TEST_ASSERT_TRUE(state.state <= STATE_ERROR);
}

int main(void)
{
    UNITY_BEGIN();
    RUN_TEST(test_init_healthy_state);
    RUN_TEST(test_get_state_null);
    RUN_TEST(test_process_null_payload);
    RUN_TEST(test_process_zero_length);
    RUN_TEST(test_process_valid_health);
    RUN_TEST(test_mcu_version_accessors);
    RUN_TEST(test_mcu_version_null_args);
    RUN_TEST(test_tick_valid);
    return UNITY_END();
}
