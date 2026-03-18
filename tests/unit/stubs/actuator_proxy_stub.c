/**
 * @file actuator_proxy_stub.c
 * @brief Stub for diagnostics tests
 */
#include "actuator_proxy.h"
#include <string.h>

static actuator_state_t g_state[SENTINEL_MAX_ACTUATORS];

sentinel_err_t actuator_proxy_init(void) {
    memset(g_state, 0, sizeof(g_state));
    return SENTINEL_OK;
}

sentinel_err_t actuator_proxy_send_command(uint8_t id, float val,
                                            actuator_response_cb_t cb, void *ctx) {
    (void)cb; (void)ctx;
    if (id >= SENTINEL_MAX_ACTUATORS) return SENTINEL_ERR_INVALID_ARG;
    if (val < 0.0f || val > 100.0f) return SENTINEL_ERR_OUT_OF_RANGE;
    g_state[id].commanded_percent = val;
    return SENTINEL_OK;
}

sentinel_err_t actuator_proxy_process_response(const uint8_t *p, size_t l) {
    (void)p; (void)l; return SENTINEL_OK;
}

sentinel_err_t actuator_proxy_get_state(uint8_t id, actuator_state_t *out) {
    if (id >= SENTINEL_MAX_ACTUATORS || out == NULL) return SENTINEL_ERR_INVALID_ARG;
    *out = g_state[id];
    return SENTINEL_OK;
}
