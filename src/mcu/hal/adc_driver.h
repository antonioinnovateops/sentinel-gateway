/**
 * @file adc_driver.h
 * @brief ADC hardware abstraction for STM32U575
 */

#ifndef ADC_DRIVER_H
#define ADC_DRIVER_H

#include <stdint.h>
#include "../../common/error_codes.h"

#define ADC_MAX_CHANNELS  4U
#define ADC_RESOLUTION    4095U  /* 12-bit */

sentinel_err_t adc_init(void);
sentinel_err_t adc_read_channel(uint8_t channel, uint32_t *value);
sentinel_err_t adc_scan_all(uint32_t values[ADC_MAX_CHANNELS]);

#endif /* ADC_DRIVER_H */
