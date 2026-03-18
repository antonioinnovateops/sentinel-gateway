/**
 * @file watchdog_driver_qemu.c
 * @brief Watchdog driver for QEMU MPS2-AN505
 *
 * Maps STM32 IWDG interface to CMSDK watchdog on MPS2-AN505.
 * The CMSDK watchdog is simpler but provides similar functionality.
 */

#include "../watchdog_driver.h"
#include "mps2_an505.h"
#include "qemu_hal_shim.h"

static bool g_watchdog_started = false;

sentinel_err_t iwdg_init(uint32_t timeout_ms)
{
    /* Initialize simulated IWDG registers */
    g_qemu_iwdg.KR = 0;
    g_qemu_iwdg.PR = IWDG_PR_DIV256;
    g_qemu_iwdg.RLR = timeout_ms / 8U;  /* Same calculation as STM32 */
    g_qemu_iwdg.SR = 0;

    /* Also configure CMSDK watchdog if we want real watchdog behavior */
    /* Unlock the watchdog */
    WDOG->LOCK = CMSDK_WDOG_UNLOCK_KEY;

    /* Calculate reload value
     * CMSDK watchdog runs at system clock (25 MHz)
     * For timeout_ms: reload = (timeout_ms * 25000000) / 1000 */
    uint32_t reload = (timeout_ms * (SYSTEM_CLOCK_HZ / 1000U));
    WDOG->LOAD = reload;

    /* Enable watchdog with reset */
    WDOG->CTRL = CMSDK_WDOG_CTRL_RESEN | CMSDK_WDOG_CTRL_INTEN;

    /* Lock the watchdog */
    WDOG->LOCK = 0;

    g_qemu_iwdg.KR = IWDG_KEY_START;
    g_watchdog_started = true;

    return SENTINEL_OK;
}

void iwdg_feed(void)
{
    /* Update simulated IWDG */
    g_qemu_iwdg.KR = IWDG_KEY_RELOAD;

    /* Also feed CMSDK watchdog */
    if (g_watchdog_started) {
        WDOG->LOCK = CMSDK_WDOG_UNLOCK_KEY;
        WDOG->INTCLR = 1;  /* Clear interrupt and reload */
        WDOG->LOCK = 0;
    }
}

bool iwdg_was_reset_cause(void)
{
    /* Check simulated RCC_CSR flag */
    return (g_qemu_rcc_csr & RCC_CSR_IWDGRSTF) != 0U;
}
