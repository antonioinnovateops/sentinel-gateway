/**
 * @file pwm_driver.c
 * @brief PWM driver using TIM2 CH1/CH2
 */

#include "pwm_driver.h"
#include "stm32u575.h"

static bool g_pwm_initialized = false;

sentinel_err_t pwm_init(void)
{
    /* Configure TIM2 for PWM mode */
    /* Prescaler: system_clock / (ARR+1) / prescaler = PWM freq */
    TIM2->PSC = 3U;  /* Divide by 4 → 1 MHz tick at 4 MHz clock */
    TIM2->ARR = PWM_ARR_VALUE;  /* 1 kHz PWM frequency */

    /* PWM mode 1, output compare CH1 + CH2 */
    TIM2->CCMR1 = (6UL << 4)   /* OC1M = PWM mode 1 */
                | (6UL << 12);  /* OC2M = PWM mode 1 */

    /* Set initial duty to 0% */
    TIM2->CCR1 = 0U;
    TIM2->CCR2 = 0U;

    /* Enable outputs */
    TIM2->CCER = TIM_CCER_CC1E | TIM_CCER_CC2E;

    /* Start timer */
    TIM2->CR1 |= TIM_CR1_CEN;

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
        TIM2->CCR1 = ccr;
    } else {
        TIM2->CCR2 = ccr;
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

    uint32_t ccr = (channel == 0U) ? TIM2->CCR1 : TIM2->CCR2;
    *percent = ((float)ccr * 100.0f) / (float)PWM_ARR_VALUE;
    return SENTINEL_OK;
}

sentinel_err_t pwm_set_all_zero(void)
{
    TIM2->CCR1 = 0U;
    TIM2->CCR2 = 0U;
    return SENTINEL_OK;
}
