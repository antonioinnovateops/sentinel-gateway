/**
 * @file test_logger.c
 * @brief Unit tests for JSON Lines logger
 */

#include "unity.h"
#include "logger.h"
#include <stdio.h>
#include <string.h>
#include <unistd.h>

static const char *SENSOR_FILE = "/tmp/test_sensor.jsonl";
static const char *EVENT_FILE = "/tmp/test_events.jsonl";

void setUp(void)
{
    unlink(SENSOR_FILE);
    unlink(EVENT_FILE);
    logger_init(SENSOR_FILE, EVENT_FILE);
}

void tearDown(void)
{
    logger_close();
    unlink(SENSOR_FILE);
    unlink(EVENT_FILE);
}

/* UT-LG-001: Sensor log written */
void test_sensor_log(void)
{
    sensor_reading_t r = {.channel = 0, .raw_value = 2048, .calibrated_value = 25.5f};
    sentinel_err_t err = logger_write_sensor(&r);
    TEST_ASSERT_EQUAL(SENTINEL_OK, err);

    /* Read back file */
    FILE *fp = fopen(SENSOR_FILE, "r");
    TEST_ASSERT_NOT_NULL(fp);
    char line[512];
    TEST_ASSERT_NOT_NULL(fgets(line, sizeof(line), fp));
    fclose(fp);

    TEST_ASSERT_NOT_NULL(strstr(line, "\"ch\":0"));
    TEST_ASSERT_NOT_NULL(strstr(line, "\"raw\":2048"));
}

/* UT-LG-002: Event log written */
void test_event_log(void)
{
    sentinel_err_t err = logger_write_event("STARTUP", "test message");
    TEST_ASSERT_EQUAL(SENTINEL_OK, err);

    FILE *fp = fopen(EVENT_FILE, "r");
    TEST_ASSERT_NOT_NULL(fp);
    char line[512];
    TEST_ASSERT_NOT_NULL(fgets(line, sizeof(line), fp));
    fclose(fp);

    TEST_ASSERT_NOT_NULL(strstr(line, "\"type\":\"STARTUP\""));
    TEST_ASSERT_NOT_NULL(strstr(line, "test message"));
}

/* UT-LG-003: NULL args handled */
void test_null_args(void)
{
    TEST_ASSERT_EQUAL(SENTINEL_ERR_INVALID_ARG, logger_write_sensor(NULL));
    TEST_ASSERT_EQUAL(SENTINEL_ERR_INVALID_ARG, logger_write_event(NULL, "x"));
}

/* UT-LG-004: Multiple log entries */
void test_multiple_entries(void)
{
    sensor_reading_t r = {.channel = 1, .raw_value = 100, .calibrated_value = 10.0f};
    for (int i = 0; i < 10; i++) {
        r.raw_value = (uint32_t)(100 + i);
        logger_write_sensor(&r);
    }

    FILE *fp = fopen(SENSOR_FILE, "r");
    TEST_ASSERT_NOT_NULL(fp);
    int count = 0;
    char line[512];
    while (fgets(line, sizeof(line), fp)) { count++; }
    fclose(fp);
    TEST_ASSERT_EQUAL(10, count);
}

int main(void)
{
    UNITY_BEGIN();
    RUN_TEST(test_sensor_log);
    RUN_TEST(test_event_log);
    RUN_TEST(test_null_args);
    RUN_TEST(test_multiple_entries);
    return UNITY_END();
}
