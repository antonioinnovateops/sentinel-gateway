/**
 * @file qemu_hal_shim.c
 * @brief QEMU HAL shim layer for MPS2-AN505
 *
 * Provides software simulation of STM32U575 peripherals on the MPS2-AN505
 * platform. The MPS2-AN505 is a Cortex-M33 FPGA image that QEMU supports,
 * but it has different peripheral addresses than STM32U575.
 *
 * This shim layer:
 * 1. Provides RAM-backed "registers" for GPIO, ADC, PWM, etc.
 * 2. Implements software simulation for ADC readings
 * 3. Maps watchdog operations to CMSDK watchdog or software
 * 4. Uses FPGAIO for LED indication
 */

#include "mps2_an505.h"
#include <string.h>

/* RAM-backed peripheral simulation structures */
GPIO_Type g_qemu_gpioa;
GPIO_Type g_qemu_gpiob;
ADC_Type g_qemu_adc1;
TIM_Type g_qemu_tim2;
FLASH_Type g_qemu_flash;
IWDG_Type g_qemu_iwdg;
RCC_Type g_qemu_rcc;
volatile uint32_t g_qemu_rcc_csr = 0;

/* Simulated ADC values (will be set by test harness via semihosting or UART) */
static uint32_t g_simulated_adc_values[4] = {2048, 1024, 3000, 500};
static uint32_t g_adc_read_count = 0;

/**
 * Initialize QEMU HAL shim layer
 * Called before any peripheral initialization
 */
void qemu_hal_shim_init(void)
{
    /* Zero out all simulated registers */
    memset(&g_qemu_gpioa, 0, sizeof(g_qemu_gpioa));
    memset(&g_qemu_gpiob, 0, sizeof(g_qemu_gpiob));
    memset(&g_qemu_adc1, 0, sizeof(g_qemu_adc1));
    memset(&g_qemu_tim2, 0, sizeof(g_qemu_tim2));
    memset(&g_qemu_flash, 0, sizeof(g_qemu_flash));
    memset(&g_qemu_iwdg, 0, sizeof(g_qemu_iwdg));
    memset(&g_qemu_rcc, 0, sizeof(g_qemu_rcc));
    g_qemu_rcc_csr = 0;

    /* Set ADC ready flag */
    g_qemu_adc1.ISR = ADC_ISR_EOC | ADC_ISR_EOS;
}

/**
 * Simulate ADC conversion
 * This is called when ADC_CR_ADSTART is set
 */
void qemu_adc_simulate_conversion(void)
{
    /* Extract channel from SQR1 (bits 6-10) */
    uint32_t channel = (g_qemu_adc1.SQR1 >> 6) & 0x1FU;
    if (channel < 4U) {
        /* Add some drift to simulate real sensor */
        int32_t drift = ((int32_t)(g_adc_read_count % 11) - 5);
        int32_t value = (int32_t)g_simulated_adc_values[channel] + drift;
        if (value < 0) value = 0;
        if (value > 4095) value = 4095;
        g_qemu_adc1.DR = (uint32_t)value;
    }
    g_qemu_adc1.ISR |= ADC_ISR_EOC;
    g_adc_read_count++;
}

/**
 * Set simulated ADC value for a channel
 */
void qemu_adc_set_value(uint8_t channel, uint32_t value)
{
    if (channel < 4U) {
        g_simulated_adc_values[channel] = value & 0xFFFU;
    }
}

/**
 * Simulate GPIO LED on FPGAIO
 */
void qemu_gpio_led_sync(void)
{
    /* Map PA5 (bit 5) to FPGAIO LED 0 */
    if (g_qemu_gpioa.ODR & (1UL << 5)) {
        FPGAIO->LED |= (1UL << 0);
    } else {
        FPGAIO->LED &= ~(1UL << 0);
    }
}

/**
 * Process IWDG key writes
 */
void qemu_iwdg_process_key(uint32_t key)
{
    if (key == IWDG_KEY_START) {
        /* Start watchdog - map to CMSDK watchdog if desired */
        /* For QEMU testing, we just track state */
    } else if (key == IWDG_KEY_RELOAD) {
        /* Feed watchdog */
        /* In a real test, this would reset the CMSDK watchdog */
    }
}

/**
 * Get millisecond tick from FPGAIO 100Hz counter
 * This is more reliable than SysTick in some QEMU versions
 */
uint32_t qemu_get_tick_ms_fpgaio(void)
{
    return FPGAIO->CLK100HZ * 10U;
}

/**
 * Debug output via UART0
 */
void qemu_uart_putchar(char c)
{
    /* Wait for TX not full */
    while (UART0->STATE & CMSDK_UART_STATE_TXFULL) {
        /* Busy wait */
    }
    UART0->DATA = (uint32_t)c;
}

void qemu_uart_puts(const char *str)
{
    while (*str) {
        if (*str == '\n') {
            qemu_uart_putchar('\r');
        }
        qemu_uart_putchar(*str++);
    }
}
