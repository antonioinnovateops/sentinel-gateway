/**
 * @file health_reporter.c
 * @brief FW-07: Safety monitor state machine + health reporting
 * @implements SWE-026 through SWE-029, SWE-038 through SWE-041
 */

#include "health_reporter.h"
#include "actuator_control.h"
#include "watchdog_mgr.h"
#include "hal/systick.h"

static system_state_t g_state = STATE_INIT;
static fault_id_t g_last_fault = FAULT_NONE;
static bool g_comm_ok = false;
static bool g_tcp_connected = false;
static uint32_t g_last_comm_ms = 0U;
static uint32_t g_boot_ms = 0U;

/* Stack canary — check remaining stack space */
extern uint32_t _estack;  /* From linker script */
extern uint32_t _sstack;

sentinel_err_t safety_monitor_init(void)
{
    g_state = STATE_INIT;
    g_last_fault = FAULT_NONE;
    g_comm_ok = false;
    g_tcp_connected = false;
    g_boot_ms = systick_get_ms();

    /* Ensure actuators start at safe state */
    (void)actuator_failsafe();

    return SENTINEL_OK;
}

void safety_monitor_tick(uint32_t now_ms)
{
    switch (g_state) {
        case STATE_INIT:
            if (g_tcp_connected) {
                g_state = STATE_NORMAL;
                g_last_comm_ms = now_ms;
            }
            break;

        case STATE_NORMAL:
            /* Check communication timeout */
            if ((now_ms - g_last_comm_ms) >= SENTINEL_COMM_TIMEOUT_MS) {
                safety_monitor_report_fault(FAULT_COMM_LOSS);
            }

            /* Verify actuator readback */
            for (uint8_t i = 0U; i < SENTINEL_MAX_ACTUATORS; i++) {
                if (!actuator_verify_readback(i)) {
                    safety_monitor_report_fault(FAULT_ACTUATOR_READBACK);
                }
            }
            break;

        case STATE_FAILSAFE:
            /* In failsafe: keep actuators at zero, wait for recovery */
            (void)actuator_failsafe();
            break;

        case STATE_DEGRADED:
        case STATE_ERROR:
        default:
            /* Unknown state → failsafe */
            g_state = STATE_FAILSAFE;
            (void)actuator_failsafe();
            break;
    }
}

void safety_monitor_report_fault(fault_id_t fault)
{
    g_last_fault = fault;
    g_state = STATE_FAILSAFE;
    (void)actuator_failsafe();
}

system_state_t safety_monitor_get_state(void)
{
    return g_state;
}

bool safety_monitor_is_failsafe(void)
{
    return (g_state == STATE_FAILSAFE);
}

void safety_monitor_set_comm_ok(bool ok)
{
    g_comm_ok = ok;
    if (ok) {
        g_last_comm_ms = systick_get_ms();
    }
}

void safety_monitor_set_tcp_connected(bool connected)
{
    g_tcp_connected = connected;
    if (connected && (g_state == STATE_FAILSAFE)) {
        /* Recovery requires explicit state sync — stay in failsafe
         * until state sync is processed */
    }
    if (connected && (g_state == STATE_INIT)) {
        g_state = STATE_NORMAL;
        g_last_comm_ms = systick_get_ms();
    }
}

sentinel_err_t safety_monitor_get_health(health_data_t *data)
{
    if (data == NULL) {
        return SENTINEL_ERR_INVALID_ARG;
    }

    uint32_t now_ms = systick_get_ms();

    data->state = g_state;
    data->uptime_s = (now_ms - g_boot_ms) / 1000U;
    data->watchdog_resets = watchdog_mgr_get_reset_count();
    data->last_fault = g_last_fault;
    data->comm_ok = g_comm_ok;

    /* Read current actuator duties */
    (void)actuator_get(0U, &data->fan_duty);
    (void)actuator_get(1U, &data->valve_duty);

    /* Estimate free stack (simplified) */
    data->free_stack = 0U; /* Would need stack pointer intrinsic */

    return SENTINEL_OK;
}
