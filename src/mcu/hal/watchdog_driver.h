/**
 * @file watchdog_driver.h
 * @brief IWDG hardware abstraction
 */

#ifndef WATCHDOG_DRIVER_H
#define WATCHDOG_DRIVER_H

#include "../../common/error_codes.h"
#include <stdbool.h>

sentinel_err_t iwdg_init(uint32_t timeout_ms);
void iwdg_feed(void);
bool iwdg_was_reset_cause(void);

#endif /* WATCHDOG_DRIVER_H */
