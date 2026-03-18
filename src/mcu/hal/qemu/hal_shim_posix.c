/**
 * @file hal_shim_posix.c
 * @brief HAL shims for QEMU user-mode (POSIX sockets, no hardware access)
 *
 * This file provides pure-software implementations of HAL functions
 * for use in QEMU user-mode emulation (qemu-arm-static). Unlike the
 * bare-metal QEMU HAL shims (which access MPS2-AN505 registers), these
 * implementations are entirely software-based.
 *
 * This file contains:
 * - GPIO driver (pure software)
 * - Flash driver (RAM-backed)
 * - ADC driver (simulated values)
 * - PWM driver (tracked in software)
 * - Watchdog driver (no-op)
 */

#include "../gpio_driver.h"
#include "../flash_driver.h"
#include "../adc_driver.h"
#include "../pwm_driver.h"
#include "../watchdog_driver.h"
#include <string.h>
#include <stdbool.h>
#include <stdio.h>

/* ============================================================================
 * GPIO Driver (pure software simulation)
 * ============================================================================ */

static bool g_led_state = false;

sentinel_err_t gpio_init(void)
{
    g_led_state = false;
    return SENTINEL_OK;
}

void gpio_led_set(bool on)
{
    g_led_state = on;
}

void gpio_led_toggle(void)
{
    g_led_state = !g_led_state;
}

/* ============================================================================
 * Flash Driver (RAM-backed simulation)
 * ============================================================================ */

static uint8_t g_simulated_flash[FLASH_CONFIG_SIZE] __attribute__((aligned(4)));
static bool g_flash_initialized = false;

sentinel_err_t flash_init(void)
{
    memset(g_simulated_flash, 0xFF, sizeof(g_simulated_flash));
    g_flash_initialized = true;
    return SENTINEL_OK;
}

sentinel_err_t flash_erase_page(uint32_t addr)
{
    if (!g_flash_initialized) {
        return SENTINEL_ERR_NOT_READY;
    }
    if (addr != FLASH_CONFIG_ADDR) {
        return SENTINEL_ERR_INVALID_ARG;
    }
    memset(g_simulated_flash, 0xFF, FLASH_CONFIG_SIZE);
    return SENTINEL_OK;
}

sentinel_err_t flash_write(uint32_t addr, const uint8_t *data, size_t len)
{
    if (!g_flash_initialized || (data == NULL)) {
        return SENTINEL_ERR_INVALID_ARG;
    }
    uint32_t offset = addr - FLASH_CONFIG_ADDR;
    if ((offset + len) > FLASH_CONFIG_SIZE) {
        return SENTINEL_ERR_OUT_OF_RANGE;
    }
    memcpy(&g_simulated_flash[offset], data, len);
    return SENTINEL_OK;
}

sentinel_err_t flash_read(uint32_t addr, uint8_t *data, size_t len)
{
    if (!g_flash_initialized || (data == NULL)) {
        return SENTINEL_ERR_INVALID_ARG;
    }
    uint32_t offset = addr - FLASH_CONFIG_ADDR;
    if ((offset + len) > FLASH_CONFIG_SIZE) {
        return SENTINEL_ERR_OUT_OF_RANGE;
    }
    memcpy(data, &g_simulated_flash[offset], len);
    return SENTINEL_OK;
}

/* ============================================================================
 * ADC Driver (simulated with drifting values)
 * ============================================================================ */

static bool g_adc_initialized = false;
static uint32_t g_adc_base_values[ADC_MAX_CHANNELS] = {2048, 1024, 3000, 500};
static uint32_t g_adc_read_count = 0;

sentinel_err_t adc_init(void)
{
    g_adc_initialized = true;
    g_adc_read_count = 0;
    return SENTINEL_OK;
}

sentinel_err_t adc_read_channel(uint8_t channel, uint32_t *value)
{
    if (!g_adc_initialized) {
        return SENTINEL_ERR_NOT_READY;
    }
    if ((channel >= ADC_MAX_CHANNELS) || (value == NULL)) {
        return SENTINEL_ERR_INVALID_ARG;
    }

    /* Simulate some drift to make readings realistic */
    int32_t drift = ((int32_t)(g_adc_read_count % 11) - 5);
    int32_t val = (int32_t)g_adc_base_values[channel] + drift;
    if (val < 0) val = 0;
    if (val > 4095) val = 4095;

    *value = (uint32_t)val;
    g_adc_read_count++;
    return SENTINEL_OK;
}

sentinel_err_t adc_scan_all(uint32_t values[ADC_MAX_CHANNELS])
{
    if (!g_adc_initialized || (values == NULL)) {
        return SENTINEL_ERR_INVALID_ARG;
    }
    for (uint8_t ch = 0U; ch < ADC_MAX_CHANNELS; ch++) {
        sentinel_err_t err = adc_read_channel(ch, &values[ch]);
        if (err != SENTINEL_OK) {
            return err;
        }
    }
    return SENTINEL_OK;
}

/* ============================================================================
 * PWM Driver (pure software tracking)
 * ============================================================================ */

static bool g_pwm_initialized = false;
static float g_pwm_duty[2] = {0.0f, 0.0f};

sentinel_err_t pwm_init(void)
{
    g_pwm_initialized = true;
    g_pwm_duty[0] = 0.0f;
    g_pwm_duty[1] = 0.0f;
    return SENTINEL_OK;
}

sentinel_err_t pwm_set_duty(uint8_t channel, float duty_percent)
{
    if (!g_pwm_initialized) {
        return SENTINEL_ERR_NOT_READY;
    }
    if (channel >= 2U) {
        return SENTINEL_ERR_INVALID_ARG;
    }

    /* Clamp duty */
    if (duty_percent < 0.0f) duty_percent = 0.0f;
    if (duty_percent > 100.0f) duty_percent = 100.0f;

    g_pwm_duty[channel] = duty_percent;
    return SENTINEL_OK;
}

sentinel_err_t pwm_get_duty(uint8_t channel, float *duty_percent)
{
    if (!g_pwm_initialized) {
        return SENTINEL_ERR_NOT_READY;
    }
    if ((channel >= 2U) || (duty_percent == NULL)) {
        return SENTINEL_ERR_INVALID_ARG;
    }

    *duty_percent = g_pwm_duty[channel];
    return SENTINEL_OK;
}

sentinel_err_t pwm_set_all_zero(void)
{
    if (!g_pwm_initialized) {
        return SENTINEL_ERR_NOT_READY;
    }
    g_pwm_duty[0] = 0.0f;
    g_pwm_duty[1] = 0.0f;
    return SENTINEL_OK;
}

/* ============================================================================
 * Watchdog Driver (no-op for user-mode)
 * ============================================================================ */

static bool g_watchdog_started = false;

sentinel_err_t iwdg_init(uint32_t timeout_ms)
{
    (void)timeout_ms;
    g_watchdog_started = true;
    return SENTINEL_OK;
}

void iwdg_feed(void)
{
    /* No-op in user mode */
}

bool iwdg_was_reset_cause(void)
{
    return false;
}
