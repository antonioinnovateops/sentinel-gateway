/**
 * @file tcp_stack.c
 * @brief FW-04: TCP communication using lwIP NO_SYS mode
 * @implements SWE-030 through SWE-033, SWE-043
 *
 * For QEMU SIL environment, lwIP operates on a TAP virtual
 * Ethernet interface. This module initializes the stack,
 * manages TCP connections on ports 5000 (command listen)
 * and 5001 (telemetry connect to Linux).
 *
 * NOTE: Full lwIP integration requires the lwIP source tree.
 * This implementation provides the interface and state machine;
 * lwIP API calls are stubbed for compilation without lwIP headers.
 * The Docker build (Phase 3) includes lwIP and enables the real calls.
 */

#include "tcp_stack.h"
#include "hal/systick.h"
#include <string.h>

/* Connection state */
typedef enum {
    CONN_DISCONNECTED = 0,
    CONN_LISTENING,
    CONN_CONNECTING,
    CONN_CONNECTED
} conn_state_t;

static conn_state_t g_cmd_state = CONN_DISCONNECTED;
static conn_state_t g_tel_state = CONN_DISCONNECTED;
static tcp_recv_cb_t g_command_callback = NULL;
static uint32_t g_last_reconnect_ms = 0U;

/* TX/RX buffers (static allocation) */
static uint8_t g_tx_buf[WIRE_FRAME_MAX_SIZE];
static uint8_t g_rx_buf[WIRE_FRAME_MAX_SIZE];
static size_t g_rx_len = 0U;

sentinel_err_t tcp_stack_init(const system_config_t *config)
{
    (void)config;

    /* In full implementation:
     * 1. Initialize USB CDC-ECM interface
     * 2. Initialize lwIP (mem_init, memp_init, netif_add, etc.)
     * 3. Set static IP 192.168.7.2
     * 4. Start listening on port 5000
     * 5. Begin connecting to 192.168.7.1:5001
     */

    g_cmd_state = CONN_LISTENING;
    g_tel_state = CONN_CONNECTING;
    g_rx_len = 0U;

    return SENTINEL_OK;
}

void tcp_stack_poll(void)
{
    /* In full implementation:
     * 1. Call sys_check_timeouts() for lwIP timers
     * 2. Process received TCP data
     * 3. Handle connection state changes
     */

    uint32_t now_ms = systick_get_ms();

    /* Handle telemetry reconnection */
    if (g_tel_state == CONN_CONNECTING) {
        if ((now_ms - g_last_reconnect_ms) >= 1000U) {
            g_last_reconnect_ms = now_ms;
            /* Attempt connection to Linux 192.168.7.1:5001 */
            /* On success: g_tel_state = CONN_CONNECTED */
        }
    }

    /* Process received command data */
    if ((g_cmd_state == CONN_CONNECTED) && (g_rx_len > 0U) &&
        (g_command_callback != NULL)) {
        g_command_callback(g_rx_buf, g_rx_len);
        g_rx_len = 0U;
    }
}

sentinel_err_t tcp_stack_send_telemetry(const uint8_t *frame, size_t len)
{
    if ((frame == NULL) || (len == 0U)) {
        return SENTINEL_ERR_INVALID_ARG;
    }
    if (g_tel_state != CONN_CONNECTED) {
        return SENTINEL_ERR_COMM;
    }

    /* In full implementation: tcp_write() + tcp_output() on telemetry PCB */
    (void)memcpy(g_tx_buf, frame, len);
    return SENTINEL_OK;
}

sentinel_err_t tcp_stack_send_command_response(const uint8_t *frame, size_t len)
{
    if ((frame == NULL) || (len == 0U)) {
        return SENTINEL_ERR_INVALID_ARG;
    }
    if (g_cmd_state != CONN_CONNECTED) {
        return SENTINEL_ERR_COMM;
    }

    /* In full implementation: tcp_write() + tcp_output() on command PCB */
    return SENTINEL_OK;
}

bool tcp_stack_is_connected(void)
{
    return (g_cmd_state == CONN_CONNECTED) && (g_tel_state == CONN_CONNECTED);
}

void tcp_stack_register_command_callback(tcp_recv_cb_t callback)
{
    g_command_callback = callback;
}

sentinel_err_t tcp_stack_reconnect(void)
{
    g_cmd_state = CONN_LISTENING;
    g_tel_state = CONN_CONNECTING;
    g_last_reconnect_ms = 0U;
    g_rx_len = 0U;
    return SENTINEL_OK;
}
