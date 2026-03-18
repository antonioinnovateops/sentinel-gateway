/**
 * @file health_monitor.h
 * @brief SW-05: MCU health monitoring and recovery
 */

#ifndef HEALTH_MONITOR_H
#define HEALTH_MONITOR_H

#include "../common/sentinel_types.h"
#include <stddef.h>

typedef struct {
    system_state_t state;
    uint32_t uptime_s;
    uint32_t watchdog_resets;
    bool comm_ok;
    uint64_t last_health_ms;
} linux_health_state_t;

sentinel_err_t health_monitor_init(void);
sentinel_err_t health_monitor_process_health(const uint8_t *payload, size_t len);
sentinel_err_t health_monitor_get_state(linux_health_state_t *out);
void health_monitor_tick(uint64_t now_ms);

#endif /* HEALTH_MONITOR_H */
