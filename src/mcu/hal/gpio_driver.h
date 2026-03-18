/**
 * @file gpio_driver.h
 * @brief GPIO driver for status LED
 */

#ifndef GPIO_DRIVER_H
#define GPIO_DRIVER_H

#include "../../common/error_codes.h"
#include <stdbool.h>

sentinel_err_t gpio_init(void);
void gpio_led_set(bool on);
void gpio_led_toggle(void);

#endif /* GPIO_DRIVER_H */
