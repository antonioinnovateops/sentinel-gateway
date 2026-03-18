/**
 * @file adc_driver_qemu.c
 * @brief ADC driver for QEMU MPS2-AN505 (software simulation)
 *
 * MPS2-AN505 does not have an ADC peripheral, so we simulate
 * ADC readings in software. The test harness can inject values
 * via the qemu_adc_set_value() function.
 */

#include "../adc_driver.h"
#include "mps2_an505.h"
#include "qemu_hal_shim.h"
#include <stdbool.h>

static bool g_adc_initialized = false;

sentinel_err_t adc_init(void)
{
    /* Initialize simulated ADC registers */
    g_qemu_adc1.CR = 0;
    g_qemu_adc1.CFGR = 0;
    g_qemu_adc1.ISR = ADC_ISR_EOC | ADC_ISR_EOS;
    g_qemu_adc1.SQR1 = 0;
    g_qemu_adc1.DR = 0;

    /* Enable ADC */
    g_qemu_adc1.CR |= ADC_CR_ADEN;

    /* Configure 12-bit resolution, right-aligned */
    g_qemu_adc1.CFGR = 0U;

    /* Configure scan sequence: channels 0-3 */
    g_qemu_adc1.SQR1 = (3UL << 0)    /* 4 conversions */
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
    g_qemu_adc1.SQR1 = ((uint32_t)channel << 6);

    /* Start conversion */
    g_qemu_adc1.CR |= ADC_CR_ADSTART;

    /* Simulate the ADC conversion */
    qemu_adc_simulate_conversion();

    /* Clear start flag */
    g_qemu_adc1.CR &= ~ADC_CR_ADSTART;

    *value = g_qemu_adc1.DR & ADC_RESOLUTION;
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
