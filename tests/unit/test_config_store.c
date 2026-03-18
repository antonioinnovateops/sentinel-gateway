/**
 * @file test_config_store.c
 * @brief Unit tests for MCU config store (CRC, validation, defaults)
 * @implements UTP-001 test cases TC-CS-001 through TC-CS-010
 */

#include "vendor/unity.h"
#include "../../src/mcu/config_store.h"
#include "../../src/mcu/hal/flash_driver.h"
#include <string.h>

void setUp(void)
{
    flash_init(); /* Reset simulated flash to 0xFF */
}

void tearDown(void) {}

/* TC-CS-001: Default config values */
void test_defaults_valid(void)
{
    system_config_t config;
    config_store_get_defaults(&config);

    TEST_ASSERT_EQUAL(10, config.sensor_rates_hz[0]);
    TEST_ASSERT_EQUAL(10, config.sensor_rates_hz[3]);
    TEST_ASSERT_FLOAT_WITHIN(0.01f, 0.0f, config.actuator_min_percent[0]);
    TEST_ASSERT_FLOAT_WITHIN(0.01f, 95.0f, config.actuator_max_percent[0]);
    TEST_ASSERT_EQUAL(1000, config.heartbeat_interval_ms);
    TEST_ASSERT_EQUAL(3000, config.comm_timeout_ms);
}

/* TC-CS-002: Save and load round-trip */
void test_save_load_roundtrip(void)
{
    system_config_t saved, loaded;
    config_store_get_defaults(&saved);
    saved.sensor_rates_hz[0] = 50;
    saved.actuator_max_percent[1] = 80.0f;

    sentinel_err_t err = config_store_save(&saved);
    TEST_ASSERT_EQUAL(SENTINEL_OK, err);

    err = config_store_load(&loaded);
    TEST_ASSERT_EQUAL(SENTINEL_OK, err);
    TEST_ASSERT_EQUAL(50, loaded.sensor_rates_hz[0]);
    TEST_ASSERT_FLOAT_WITHIN(0.01f, 80.0f, loaded.actuator_max_percent[1]);
}

/* TC-CS-003: Load from blank flash returns defaults */
void test_load_blank_flash_returns_defaults(void)
{
    system_config_t config;
    sentinel_err_t err = config_store_load(&config);

    /* Flash is all 0xFF → magic mismatch → defaults */
    TEST_ASSERT_NOT_EQUAL(SENTINEL_OK, err);
    TEST_ASSERT_EQUAL(10, config.sensor_rates_hz[0]); /* Got defaults */
}

/* TC-CS-004: Validate rejects zero rate */
void test_validate_rejects_zero_rate(void)
{
    system_config_t config;
    config_store_get_defaults(&config);
    config.sensor_rates_hz[2] = 0;

    TEST_ASSERT_EQUAL(SENTINEL_ERR_OUT_OF_RANGE, config_store_validate(&config));
}

/* TC-CS-005: Validate rejects rate > 100 */
void test_validate_rejects_high_rate(void)
{
    system_config_t config;
    config_store_get_defaults(&config);
    config.sensor_rates_hz[1] = 101;

    TEST_ASSERT_EQUAL(SENTINEL_ERR_OUT_OF_RANGE, config_store_validate(&config));
}

/* TC-CS-006: Validate rejects min >= max */
void test_validate_rejects_inverted_limits(void)
{
    system_config_t config;
    config_store_get_defaults(&config);
    config.actuator_min_percent[0] = 50.0f;
    config.actuator_max_percent[0] = 30.0f;

    TEST_ASSERT_EQUAL(SENTINEL_ERR_INVALID_ARG, config_store_validate(&config));
}

/* TC-CS-007: Validate rejects max > 100% */
void test_validate_rejects_over_100(void)
{
    system_config_t config;
    config_store_get_defaults(&config);
    config.actuator_max_percent[0] = 101.0f;

    TEST_ASSERT_EQUAL(SENTINEL_ERR_OUT_OF_RANGE, config_store_validate(&config));
}

/* TC-CS-008: Save rejects invalid config */
void test_save_rejects_invalid(void)
{
    system_config_t config;
    config_store_get_defaults(&config);
    config.sensor_rates_hz[0] = 0; /* Invalid */

    TEST_ASSERT_EQUAL(SENTINEL_ERR_OUT_OF_RANGE, config_store_save(&config));
}

/* TC-CS-009: NULL pointer handling */
void test_null_pointers(void)
{
    TEST_ASSERT_EQUAL(SENTINEL_ERR_INVALID_ARG, config_store_load(NULL));
    TEST_ASSERT_EQUAL(SENTINEL_ERR_INVALID_ARG, config_store_save(NULL));
    TEST_ASSERT_EQUAL(SENTINEL_ERR_INVALID_ARG, config_store_validate(NULL));
}

/* TC-CS-010: CRC corruption detection */
void test_crc_corruption_detected(void)
{
    system_config_t saved, loaded;
    config_store_get_defaults(&saved);

    config_store_save(&saved);

    /* Corrupt a byte in the config data area (after magic+version header = offset 8) */
    uint8_t byte = 0x42;
    flash_write(FLASH_CONFIG_ADDR + 8, &byte, 1);

    sentinel_err_t err = config_store_load(&loaded);
    /* Should fail: either CRC mismatch or validation failure */
    TEST_ASSERT_NOT_EQUAL(SENTINEL_OK, err);
    /* Loaded defaults as fallback */
    TEST_ASSERT_EQUAL(10, loaded.sensor_rates_hz[0]);
}

int main(void)
{
    UNITY_BEGIN();
    RUN_TEST(test_defaults_valid);
    RUN_TEST(test_save_load_roundtrip);
    RUN_TEST(test_load_blank_flash_returns_defaults);
    RUN_TEST(test_validate_rejects_zero_rate);
    RUN_TEST(test_validate_rejects_high_rate);
    RUN_TEST(test_validate_rejects_inverted_limits);
    RUN_TEST(test_validate_rejects_over_100);
    RUN_TEST(test_save_rejects_invalid);
    RUN_TEST(test_null_pointers);
    RUN_TEST(test_crc_corruption_detected);
    return UNITY_END();
}
