/**
 * @file gpio_driver.c
 * @brief GPIO driver — LED on PA5 (Netduino standard)
 */

#include "gpio_driver.h"
#include "stm32u575.h"

sentinel_err_t gpio_init(void)
{
    /* PA5 as output (LED) */
    GPIOA->MODER = (GPIOA->MODER & ~(3UL << 10)) | (1UL << 10);
    return SENTINEL_OK;
}

void gpio_led_set(bool on)
{
    if (on) {
        GPIOA->BSRR = (1UL << 5);
    } else {
        GPIOA->BSRR = (1UL << (5 + 16));
    }
}

void gpio_led_toggle(void)
{
    GPIOA->ODR ^= (1UL << 5);
}
