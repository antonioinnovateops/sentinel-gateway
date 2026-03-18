/**
 * @file test_config_store.c
 * @brief Unit tests for MCU config store (flash-backed, CRC protected)
 * @implements UTP-001 test cases for SWE-058 through SWE-065
 */

#include "unity.h"
#include "config_store.h"
#include <string.h>

void setUp(void)
{
    /* Re-init flash for each test */
    extern sentinel_err_t flash_init(void);
    flash_init();
}

void tearDown(void) {}

/* UT-CS-001: Load defaults on empty flash */
void test_load_defaults_on_empty(void)
{
    system_config_t config;
    sentinel_err_t err = config_store_load(&config);

    /* Should fail (no valid data) but populate defaults */
    TEST_ASSERT_NOT_EQUAL(SENTINEL_OK, err);
    TEST_ASSERT_EQUAL(10, config.sensor_rates_hz[0]);
    TEST_ASSERT_EQUAL(10, config.sensor_rates_hz[3]);
    TEST_ASSERT_FLOAT_WITHIN(0.1f, 95.0f, config.actuator_max_percent[0]);
}

/* UT-CS-002: Save and load roundtrip */
void test_save_load_roundtrip(void)
{
    system_config_t config;
    config_store_get_defaults(&config);
    config.sensor_rates_hz[0] = 50;
    config.sensor_rates_hz[1] = 25;
    config.actuator_max_percent[0] = 80.0f;
    config.heartbeat_interval_ms = 2000;

    sentinel_err_t err = config_store_save(&config);
    TEST_ASSERT_EQUAL(SENTINEL_OK, err);

    system_config_t loaded;
    err = config_store_load(&loaded);
    TEST_ASSERT_EQUAL(SENTINEL_OK, err);
    TEST_ASSERT_EQUAL(50, loaded.sensor_rates_hz[0]);
    TEST_ASSERT_EQUAL(25, loaded.sensor_rates_hz[1]);
    TEST_ASSERT_FLOAT_WITHIN(0.1f, 80.0f, loaded.actuator_max_percent[0]);
    TEST_ASSERT_EQUAL(2000, loaded.heartbeat_interval_ms);
}

/* UT-CS-003: Validate rejects zero rate */
void test_validate_rejects_zero_rate(void)
{
    system_config_t config;
    config_store_get_defaults(&config);
    config.sensor_rates_hz[2] = 0;

    TEST_ASSERT_EQUAL(SENTINEL_ERR_OUT_OF_RANGE,
                      config_store_validate(&config));
}

/* UT-CS-004: Validate rejects rate > 100 */
void test_validate_rejects_high_rate(void)
{
    system_config_t config;
    config_store_get_defaults(&config);
    config.sensor_rates_hz[0] = 101;

    TEST_ASSERT_EQUAL(SENTINEL_ERR_OUT_OF_RANGE,
                      config_store_validate(&config));
}

/* UT-CS-005: Validate rejects min >= max */
void test_validate_rejects_bad_limits(void)
{
    system_config_t config;
    config_store_get_defaults(&config);
    config.actuator_min_percent[0] = 95.0f;
    config.actuator_max_percent[0] = 50.0f;

    TEST_ASSERT_EQUAL(SENTINEL_ERR_INVALID_ARG,
                      config_store_validate(&config));
}

/* UT-CS-006: NULL arg rejected */
void test_null_args(void)
{
    TEST_ASSERT_EQUAL(SENTINEL_ERR_INVALID_ARG, config_store_load(NULL));
    TEST_ASSERT_EQUAL(SENTINEL_ERR_INVALID_ARG, config_store_save(NULL));
    TEST_ASSERT_EQUAL(SENTINEL_ERR_INVALID_ARG, config_store_validate(NULL));
}

/* UT-CS-007: Defaults are valid */
void test_defaults_valid(void)
{
    system_config_t config;
    config_store_get_defaults(&config);
    TEST_ASSERT_EQUAL(SENTINEL_OK, config_store_validate(&config));
}

/* UT-CS-008: Overwrite existing config */
void test_overwrite_config(void)
{
    system_config_t c1, c2, loaded;
    config_store_get_defaults(&c1);
    c1.sensor_rates_hz[0] = 20;
    config_store_save(&c1);

    config_store_get_defaults(&c2);
    c2.sensor_rates_hz[0] = 75;
    config_store_save(&c2);

    config_store_load(&loaded);
    TEST_ASSERT_EQUAL(75, loaded.sensor_rates_hz[0]);
}

int main(void)
{
    UNITY_BEGIN();
    RUN_TEST(test_load_defaults_on_empty);
    RUN_TEST(test_save_load_roundtrip);
    RUN_TEST(test_validate_rejects_zero_rate);
    RUN_TEST(test_validate_rejects_high_rate);
    RUN_TEST(test_validate_rejects_bad_limits);
    RUN_TEST(test_null_args);
    RUN_TEST(test_defaults_valid);
    RUN_TEST(test_overwrite_config);
    return UNITY_END();
}
