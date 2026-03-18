/**
 * @file gpio_driver_qemu.c
 * @brief GPIO driver for QEMU MPS2-AN505
 *
 * Maps GPIO operations to FPGAIO LEDs on MPS2-AN505.
 * LED is visible in QEMU monitor and can be queried by test harness.
 */

#include "../gpio_driver.h"
#include "mps2_an505.h"
#include "qemu_hal_shim.h"

sentinel_err_t gpio_init(void)
{
    /* Initialize simulated GPIO */
    g_qemu_gpioa.MODER = 0;
    g_qemu_gpioa.ODR = 0;

    /* PA5 as output (LED) */
    g_qemu_gpioa.MODER = (g_qemu_gpioa.MODER & ~(3UL << 10)) | (1UL << 10);

    /* Initialize FPGAIO for LED output */
    FPGAIO->LED = 0;

    return SENTINEL_OK;
}

void gpio_led_set(bool on)
{
    if (on) {
        g_qemu_gpioa.BSRR = (1UL << 5);
        g_qemu_gpioa.ODR |= (1UL << 5);
        FPGAIO->LED |= (1UL << 0);
    } else {
        g_qemu_gpioa.BSRR = (1UL << (5 + 16));
        g_qemu_gpioa.ODR &= ~(1UL << 5);
        FPGAIO->LED &= ~(1UL << 0);
    }
}

void gpio_led_toggle(void)
{
    g_qemu_gpioa.ODR ^= (1UL << 5);
    if (g_qemu_gpioa.ODR & (1UL << 5)) {
        FPGAIO->LED |= (1UL << 0);
    } else {
        FPGAIO->LED &= ~(1UL << 0);
    }
}
