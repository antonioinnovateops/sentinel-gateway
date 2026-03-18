/**
 * @file pwm_driver_qemu.c
 * @brief PWM driver for QEMU MPS2-AN505 (software simulation)
 *
 * MPS2-AN505 does not have the same timer configuration as STM32,
 * so we simulate PWM in software using RAM-backed registers.
 */

#include "../pwm_driver.h"
#include "mps2_an505.h"
#include <stdbool.h>

static bool g_pwm_initialized = false;

sentinel_err_t pwm_init(void)
{
    /* Initialize simulated TIM2 registers */
    g_qemu_tim2.CR1 = 0;
    g_qemu_tim2.PSC = 3U;  /* Divide by 4 */
    g_qemu_tim2.ARR = PWM_ARR_VALUE;
    g_qemu_tim2.CCR1 = 0U;
    g_qemu_tim2.CCR2 = 0U;

    /* PWM mode 1, output compare CH1 + CH2 */
    g_qemu_tim2.CCMR1 = (6UL << 4)   /* OC1M = PWM mode 1 */
                      | (6UL << 12);  /* OC2M = PWM mode 1 */

    /* Enable outputs */
    g_qemu_tim2.CCER = TIM_CCER_CC1E | TIM_CCER_CC2E;

    /* Start timer */
    g_qemu_tim2.CR1 |= TIM_CR1_CEN;

    g_pwm_initialized = true;
    return SENTINEL_OK;
}

sentinel_err_t pwm_set_duty(uint8_t channel, float percent)
{
    if (!g_pwm_initialized) {
        return SENTINEL_ERR_NOT_READY;
    }
    if (channel >= PWM_MAX_CHANNELS) {
        return SENTINEL_ERR_INVALID_ARG;
    }
    if ((percent < 0.0f) || (percent > 100.0f)) {
        return SENTINEL_ERR_OUT_OF_RANGE;
    }

    uint32_t ccr = (uint32_t)((percent * (float)PWM_ARR_VALUE) / 100.0f);

    if (channel == 0U) {
        g_qemu_tim2.CCR1 = ccr;
    } else {
        g_qemu_tim2.CCR2 = ccr;
    }
    return SENTINEL_OK;
}

sentinel_err_t pwm_get_duty(uint8_t channel, float *percent)
{
    if (!g_pwm_initialized || (percent == NULL)) {
        return SENTINEL_ERR_INVALID_ARG;
    }
    if (channel >= PWM_MAX_CHANNELS) {
        return SENTINEL_ERR_INVALID_ARG;
    }

    uint32_t ccr = (channel == 0U) ? g_qemu_tim2.CCR1 : g_qemu_tim2.CCR2;
    *percent = ((float)ccr * 100.0f) / (float)PWM_ARR_VALUE;
    return SENTINEL_OK;
}

sentinel_err_t pwm_set_all_zero(void)
{
    g_qemu_tim2.CCR1 = 0U;
    g_qemu_tim2.CCR2 = 0U;
    return SENTINEL_OK;
}
