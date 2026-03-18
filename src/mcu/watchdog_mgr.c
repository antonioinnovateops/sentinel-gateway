/**
 * @file watchdog_mgr.c
 * @brief FW-05: IWDG management with reset counter
 * @implements SWE-069 through SWE-071
 */

#include "watchdog_mgr.h"
#include "hal/watchdog_driver.h"

/* Placed in .noinit section — survives reset but not power cycle */
static uint32_t g_reset_count __attribute__((section(".noinit")));

sentinel_err_t watchdog_mgr_init(void)
{
    /* Check if last reset was IWDG-caused */
    if (iwdg_was_reset_cause()) {
        g_reset_count++;
    }

    /* Start IWDG with 2-second timeout */
    return iwdg_init(SENTINEL_WATCHDOG_TIMEOUT_MS);
}

void watchdog_mgr_feed(void)
{
    iwdg_feed();
}

uint32_t watchdog_mgr_get_reset_count(void)
{
    return g_reset_count;
}
