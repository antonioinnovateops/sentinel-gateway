/**
 * @file adc_driver.c
 * @brief ADC driver for STM32U575 (QEMU-compatible)
 */

#include "adc_driver.h"
#include "stm32u575.h"
#include <stdbool.h>

static bool g_adc_initialized = false;

sentinel_err_t adc_init(void)
{
    /* Enable ADC clock via RCC */
    /* In QEMU, ADC registers may not be fully emulated.
     * We configure them per real hardware spec. */

    /* Enable ADC */
    ADC1->CR |= ADC_CR_ADEN;

    /* Configure 12-bit resolution, right-aligned */
    ADC1->CFGR = 0U;

    /* Configure scan sequence: channels 0-3 */
    ADC1->SQR1 = (3UL << 0)    /* 4 conversions */
               | (0UL << 6)    /* SQ1 = CH0 */
               | (1UL << 12)   /* SQ2 = CH1 */
               | (2UL << 18)   /* SQ3 = CH2 */
               | (3UL << 24);  /* SQ4 = CH3 */

    g_adc_initialized = true;
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

    /* Configure single channel */
    ADC1->SQR1 = ((uint32_t)channel << 6);

    /* Start conversion */
    ADC1->CR |= ADC_CR_ADSTART;

    /* Wait for end of conversion (with timeout) */
    uint32_t timeout = 10000U;
    while (((ADC1->ISR & ADC_ISR_EOC) == 0U) && (timeout > 0U)) {
        timeout--;
    }

    if (timeout == 0U) {
        return SENTINEL_ERR_TIMEOUT;
    }

    *value = ADC1->DR & ADC_RESOLUTION;
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
