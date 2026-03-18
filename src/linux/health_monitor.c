/**
 * @file health_monitor.c
 * @brief SW-05: MCU health monitoring, comm loss detection, recovery
 * @implements SWE-042 through SWE-045
 */

#define _POSIX_C_SOURCE 200809L

#include "health_monitor.h"
#include "logger.h"
#include <string.h>
#include <time.h>
#include <stdio.h>

typedef enum {
    HM_MONITORING = 0,
    HM_COMM_LOSS,
    HM_RECOVERING,
    HM_FAULT
} hm_state_t;

static hm_state_t g_hm_state = HM_MONITORING;
static linux_health_state_t g_health;
static uint32_t g_recovery_attempts = 0U;
static uint64_t g_last_health_received_ms = 0U;

static uint64_t get_ms(void)
{
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (uint64_t)ts.tv_sec * 1000ULL + (uint64_t)(ts.tv_nsec / 1000000L);
}

sentinel_err_t health_monitor_init(void)
{
    (void)memset(&g_health, 0, sizeof(g_health));
    g_hm_state = HM_MONITORING;
    g_recovery_attempts = 0U;
    g_last_health_received_ms = get_ms();
    return SENTINEL_OK;
}

sentinel_err_t health_monitor_process_health(const uint8_t *payload, size_t len)
{
    /* TODO: Decode HealthStatus protobuf */
    (void)payload;
    (void)len;

    g_last_health_received_ms = get_ms();
    g_health.last_health_ms = g_last_health_received_ms;
    g_health.comm_ok = true;

    /* If we were in COMM_LOSS and got health message, MCU is back */
    if (g_hm_state == HM_COMM_LOSS || g_hm_state == HM_RECOVERING) {
        g_hm_state = HM_MONITORING;
        g_recovery_attempts = 0U;
        logger_write_event("RECOVERY", "Communication restored");
    }

    return SENTINEL_OK;
}

void health_monitor_tick(uint64_t now_ms)
{
    switch (g_hm_state) {
        case HM_MONITORING:
            if ((now_ms - g_last_health_received_ms) >= SENTINEL_COMM_TIMEOUT_MS) {
                g_hm_state = HM_COMM_LOSS;
                g_health.comm_ok = false;
                logger_write_event("COMM_LOSS", "MCU health timeout");
            }
            break;

        case HM_COMM_LOSS:
            g_hm_state = HM_RECOVERING;
            g_recovery_attempts++;
            logger_write_event("RECOVERY_START", "Attempting reconnection");
            break;

        case HM_RECOVERING:
            if (g_recovery_attempts >= 3U) {
                g_hm_state = HM_FAULT;
                logger_write_event("FAULT", "Recovery failed after 3 attempts");
            }
            /* Recovery logic: USB power cycle, TCP reconnect, state sync */
            break;

        case HM_FAULT:
            /* Stay in fault until manual intervention */
            break;

        default:
            g_hm_state = HM_FAULT;
            break;
    }
}

sentinel_err_t health_monitor_get_state(linux_health_state_t *out)
{
    if (out == NULL) {
        return SENTINEL_ERR_INVALID_ARG;
    }
    *out = g_health;
    return SENTINEL_OK;
}
