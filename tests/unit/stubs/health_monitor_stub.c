/**
 * @file health_monitor_stub.c
 * @brief Stub for diagnostics tests
 */
#include "health_monitor.h"
#include <string.h>

static linux_health_state_t g_health;

sentinel_err_t health_monitor_init(void) {
    memset(&g_health, 0, sizeof(g_health));
    g_health.state = STATE_NORMAL;
    g_health.comm_ok = true;
    return SENTINEL_OK;
}

sentinel_err_t health_monitor_process_health(const uint8_t *p, size_t l) {
    (void)p; (void)l; return SENTINEL_OK;
}

sentinel_err_t health_monitor_get_state(linux_health_state_t *out) {
    if (out == NULL) return SENTINEL_ERR_INVALID_ARG;
    *out = g_health;
    return SENTINEL_OK;
}

void health_monitor_tick(uint64_t now_ms) {
    (void)now_ms;
}
