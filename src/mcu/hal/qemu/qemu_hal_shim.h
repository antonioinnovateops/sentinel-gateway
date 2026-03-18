/**
 * @file qemu_hal_shim.h
 * @brief QEMU HAL shim layer interface
 */

#ifndef QEMU_HAL_SHIM_H
#define QEMU_HAL_SHIM_H

#include <stdint.h>

/**
 * Initialize QEMU HAL shim layer
 * Must be called before any peripheral initialization
 */
void qemu_hal_shim_init(void);

/**
 * Simulate ADC conversion (called when ADSTART is set)
 */
void qemu_adc_simulate_conversion(void);

/**
 * Set simulated ADC value for a channel
 * @param channel Channel number (0-3)
 * @param value 12-bit ADC value (0-4095)
 */
void qemu_adc_set_value(uint8_t channel, uint32_t value);

/**
 * Sync GPIO state to FPGAIO LEDs
 */
void qemu_gpio_led_sync(void);

/**
 * Process IWDG key writes
 * @param key IWDG key value
 */
void qemu_iwdg_process_key(uint32_t key);

/**
 * Get millisecond tick from FPGAIO 100Hz counter
 * @return Milliseconds elapsed
 */
uint32_t qemu_get_tick_ms_fpgaio(void);

/**
 * Debug output via UART0
 */
void qemu_uart_putchar(char c);
void qemu_uart_puts(const char *str);

#endif /* QEMU_HAL_SHIM_H */
