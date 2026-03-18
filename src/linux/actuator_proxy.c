/**
 * @file actuator_proxy.c
 * @brief SW-04: Actuator command/response handling
 * @implements SWE-018, SWE-019
 */

#include "actuator_proxy.h"
#include "tcp_transport.h"
#include "../common/wire_frame.h"
#include <string.h>
#include <stdio.h>

static actuator_state_t g_actuators[SENTINEL_MAX_ACTUATORS];
static uint32_t g_sequence = 0U;
static uint32_t g_timeout_count = 0U;

sentinel_err_t actuator_proxy_init(void)
{
    (void)memset(g_actuators, 0, sizeof(g_actuators));
    g_sequence = 0U;
    g_timeout_count = 0U;
    return SENTINEL_OK;
}

sentinel_err_t actuator_proxy_send_command(uint8_t actuator_id, float value_percent,
                                            actuator_response_cb_t cb, void *ctx)
{
    (void)cb;
    (void)ctx;

    if (actuator_id >= SENTINEL_MAX_ACTUATORS) {
        return SENTINEL_ERR_INVALID_ARG;
    }
    if ((value_percent < 0.0f) || (value_percent > 100.0f)) {
        return SENTINEL_ERR_OUT_OF_RANGE;
    }

    /* TODO: Encode ActuatorCommand protobuf and send via transport
     * Sequence: g_sequence++
     * Start response timeout timer (500ms)
     */

    g_actuators[actuator_id].actuator_id = actuator_id;
    g_actuators[actuator_id].commanded_percent = value_percent;
    g_sequence++;

    return SENTINEL_OK;
}

sentinel_err_t actuator_proxy_process_response(const uint8_t *payload, size_t len)
{
    /* TODO: Decode ActuatorResponse protobuf
     * Match by sequence number
     * Update actuator state
     * Call pending callback
     */
    (void)payload;
    (void)len;
    return SENTINEL_OK;
}

sentinel_err_t actuator_proxy_get_state(uint8_t actuator_id, actuator_state_t *out)
{
    if ((actuator_id >= SENTINEL_MAX_ACTUATORS) || (out == NULL)) {
        return SENTINEL_ERR_INVALID_ARG;
    }
    *out = g_actuators[actuator_id];
    return SENTINEL_OK;
}
