/**
 * @file pwm_driver_stub.c
 * @brief Stub PWM driver for unit testing actuator_control
 */

#include "pwm_driver.h"

static float g_duty[PWM_MAX_CHANNELS] = {0};
static bool g_initialized = false;

sentinel_err_t pwm_init(void)
{
    g_initialized = true;
    g_duty[0] = 0.0f;
    g_duty[1] = 0.0f;
    return SENTINEL_OK;
}

sentinel_err_t pwm_set_duty(uint8_t channel, float percent)
{
    if (!g_initialized || channel >= PWM_MAX_CHANNELS) return SENTINEL_ERR_INVALID_ARG;
    if (percent < 0.0f || percent > 100.0f) return SENTINEL_ERR_OUT_OF_RANGE;
    g_duty[channel] = percent;
    return SENTINEL_OK;
}

sentinel_err_t pwm_get_duty(uint8_t channel, float *percent)
{
    if (!g_initialized || channel >= PWM_MAX_CHANNELS || percent == NULL)
        return SENTINEL_ERR_INVALID_ARG;
    *percent = g_duty[channel];
    return SENTINEL_OK;
}

sentinel_err_t pwm_set_all_zero(void)
{
    g_duty[0] = 0.0f;
    g_duty[1] = 0.0f;
    return SENTINEL_OK;
}
