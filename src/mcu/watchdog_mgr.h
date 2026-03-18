/**
 * @file watchdog_mgr.h
 * @brief FW-05: Watchdog Manager
 */

#ifndef WATCHDOG_MGR_H
#define WATCHDOG_MGR_H

#include "../common/sentinel_types.h"

sentinel_err_t watchdog_mgr_init(void);
void watchdog_mgr_feed(void);
uint32_t watchdog_mgr_get_reset_count(void);

#endif /* WATCHDOG_MGR_H */
