/**
 * @file pwm_driver.h
 * @brief PWM hardware abstraction for STM32U575 TIM2
 */

#ifndef PWM_DRIVER_H
#define PWM_DRIVER_H

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include "../../common/error_codes.h"

#define PWM_MAX_CHANNELS 2U
#define PWM_ARR_VALUE    999U  /* 0-999 for 0.0-100.0% resolution */

sentinel_err_t pwm_init(void);
sentinel_err_t pwm_set_duty(uint8_t channel, float percent);
sentinel_err_t pwm_get_duty(uint8_t channel, float *percent);
sentinel_err_t pwm_set_all_zero(void);

#endif /* PWM_DRIVER_H */
