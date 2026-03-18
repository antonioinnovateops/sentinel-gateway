/**
 * @file pwm_stub.c
 * @brief PWM driver stub for host-side unit tests
 * Replaces HAL pwm_driver.c — stores duty in memory instead of hardware
 */

#include "../../../src/mcu/hal/pwm_driver.h"
#include <stdbool.h>

static bool g_initialized = false;
static float g_duty[PWM_MAX_CHANNELS];

sentinel_err_t pwm_init(void)
{
    g_duty[0] = 0.0f;
    g_duty[1] = 0.0f;
    g_initialized = true;
    return SENTINEL_OK;
}

sentinel_err_t pwm_set_duty(uint8_t channel, float percent)
{
    if (!g_initialized) { return SENTINEL_ERR_NOT_READY; }
    if (channel >= PWM_MAX_CHANNELS) { return SENTINEL_ERR_INVALID_ARG; }
    if ((percent < 0.0f) || (percent > 100.0f)) { return SENTINEL_ERR_OUT_OF_RANGE; }
    g_duty[channel] = percent;
    return SENTINEL_OK;
}

sentinel_err_t pwm_get_duty(uint8_t channel, float *percent)
{
    if (!g_initialized || (percent == NULL)) { return SENTINEL_ERR_INVALID_ARG; }
    if (channel >= PWM_MAX_CHANNELS) { return SENTINEL_ERR_INVALID_ARG; }
    *percent = g_duty[channel];
    return SENTINEL_OK;
}

sentinel_err_t pwm_set_all_zero(void)
{
    g_duty[0] = 0.0f;
    g_duty[1] = 0.0f;
    return SENTINEL_OK;
}
