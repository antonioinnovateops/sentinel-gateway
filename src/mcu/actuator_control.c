/**
 * @file actuator_control.c
 * @brief FW-02: PWM actuator control with safety limits
 * @implements SWE-016 through SWE-029
 */

#include "actuator_control.h"
#include "hal/pwm_driver.h"
#include <math.h>

static float g_min_percent[SENTINEL_MAX_ACTUATORS];
static float g_max_percent[SENTINEL_MAX_ACTUATORS];
static float g_commanded[SENTINEL_MAX_ACTUATORS];
static bool g_initialized = false;

sentinel_err_t actuator_init(const system_config_t *config)
{
    if (config == NULL) {
        return SENTINEL_ERR_INVALID_ARG;
    }

    sentinel_err_t err = pwm_init();
    if (err != SENTINEL_OK) {
        return err;
    }

    for (uint8_t i = 0U; i < SENTINEL_MAX_ACTUATORS; i++) {
        g_min_percent[i] = config->actuator_min_percent[i];
        g_max_percent[i] = config->actuator_max_percent[i];
        g_commanded[i] = 0.0f;
    }

    g_initialized = true;
    return SENTINEL_OK;
}

sentinel_err_t actuator_set(uint8_t id, float percent, uint32_t *status)
{
    if (!g_initialized || (status == NULL)) {
        return SENTINEL_ERR_NOT_READY;
    }
    if (id >= SENTINEL_MAX_ACTUATORS) {
        *status = 1U; /* STATUS_ERROR_INVALID_ACTUATOR */
        return SENTINEL_ERR_INVALID_ARG;
    }

    /* Clamp to safety limits */
    if (percent < g_min_percent[id]) {
        *status = 2U; /* STATUS_ERROR_OUT_OF_RANGE */
        return SENTINEL_ERR_OUT_OF_RANGE;
    }
    if (percent > g_max_percent[id]) {
        *status = 2U; /* STATUS_ERROR_OUT_OF_RANGE */
        return SENTINEL_ERR_OUT_OF_RANGE;
    }

    sentinel_err_t err = pwm_set_duty(id, percent);
    if (err != SENTINEL_OK) {
        *status = 6U; /* STATUS_ERROR_INTERNAL */
        return err;
    }

    g_commanded[id] = percent;
    *status = 0U; /* STATUS_OK */
    return SENTINEL_OK;
}

sentinel_err_t actuator_get(uint8_t id, float *percent)
{
    if (!g_initialized || (percent == NULL)) {
        return SENTINEL_ERR_INVALID_ARG;
    }
    if (id >= SENTINEL_MAX_ACTUATORS) {
        return SENTINEL_ERR_INVALID_ARG;
    }
    return pwm_get_duty(id, percent);
}

sentinel_err_t actuator_set_limits(uint8_t id, float min_pct, float max_pct)
{
    if (id >= SENTINEL_MAX_ACTUATORS) {
        return SENTINEL_ERR_INVALID_ARG;
    }
    if (min_pct >= max_pct) {
        return SENTINEL_ERR_INVALID_ARG;
    }
    g_min_percent[id] = min_pct;
    g_max_percent[id] = max_pct;
    return SENTINEL_OK;
}

sentinel_err_t actuator_failsafe(void)
{
    sentinel_err_t err = pwm_set_all_zero();
    for (uint8_t i = 0U; i < SENTINEL_MAX_ACTUATORS; i++) {
        g_commanded[i] = 0.0f;
    }
    return err;
}

bool actuator_verify_readback(uint8_t id)
{
    if (id >= SENTINEL_MAX_ACTUATORS) {
        return false;
    }
    float actual = 0.0f;
    if (pwm_get_duty(id, &actual) != SENTINEL_OK) {
        return false;
    }
    /* Allow 1% tolerance */
    return (fabsf(actual - g_commanded[id]) < 1.0f);
}
