/**
 * @file watchdog_driver.c
 * @brief IWDG driver for STM32U575
 */

#include "watchdog_driver.h"
#include "stm32u575.h"

sentinel_err_t iwdg_init(uint32_t timeout_ms)
{
    /* IWDG clock = LSI (~32 kHz) / prescaler
     * With /256: 32000/256 = 125 Hz → 8ms per tick
     * For 2000ms timeout: reload = 2000/8 = 250 */
    uint32_t reload = timeout_ms / 8U;
    if (reload > 0xFFFU) {
        reload = 0xFFFU;
    }

    IWDG->KR = IWDG_KEY_ACCESS;  /* Enable register access */
    IWDG->PR = IWDG_PR_DIV256;   /* Prescaler /256 */
    IWDG->RLR = reload;          /* Reload value */
    IWDG->KR = IWDG_KEY_START;   /* Start IWDG */

    return SENTINEL_OK;
}

void iwdg_feed(void)
{
    IWDG->KR = IWDG_KEY_RELOAD;
}

bool iwdg_was_reset_cause(void)
{
    return (RCC_CSR & RCC_CSR_IWDGRSTF) != 0U;
}
