/**
 * @file health_reporter.h
 * @brief FW-07: Safety Monitor and Health Reporter
 */

#ifndef HEALTH_REPORTER_H
#define HEALTH_REPORTER_H

#include "../common/sentinel_types.h"

sentinel_err_t safety_monitor_init(void);
void safety_monitor_tick(uint32_t now_ms);
void safety_monitor_report_fault(fault_id_t fault);
system_state_t safety_monitor_get_state(void);
bool safety_monitor_is_failsafe(void);
void safety_monitor_set_comm_ok(bool ok);
void safety_monitor_set_tcp_connected(bool connected);

/* Health data for encoding */
typedef struct {
    system_state_t state;
    uint32_t uptime_s;
    uint32_t watchdog_resets;
    float fan_duty;
    float valve_duty;
    fault_id_t last_fault;
    bool comm_ok;
    uint32_t free_stack;
} health_data_t;

sentinel_err_t safety_monitor_get_health(health_data_t *data);

#endif /* HEALTH_REPORTER_H */
