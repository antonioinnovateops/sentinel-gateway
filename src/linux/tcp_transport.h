/**
 * @file tcp_transport.h
 * @brief TCP transport layer for Linux gateway
 */

#ifndef TCP_TRANSPORT_H
#define TCP_TRANSPORT_H

#include "../common/sentinel_types.h"
#include <stddef.h>

/** Callback for received wire frames */
typedef void (*transport_recv_cb_t)(uint8_t msg_type, const uint8_t *payload,
                                     size_t payload_len, void *ctx);

/** Initialize transport layer */
sentinel_err_t transport_init(void);

/** Connect to MCU command channel (port 5000) */
sentinel_err_t transport_connect_command(const char *mcu_ip);

/** Start listening for telemetry (port 5001) */
sentinel_err_t transport_listen_telemetry(void);

/** Start listening for diagnostics (port 5002) */
sentinel_err_t transport_listen_diagnostics(void);

/** Send a wire frame to MCU command channel */
sentinel_err_t transport_send_command(const uint8_t *frame, size_t len);

/** Callback for diagnostic text commands */
typedef void (*transport_diag_cb_t)(int client_fd, const char *cmd, void *ctx);

/** Register callback for received messages */
void transport_set_recv_callback(transport_recv_cb_t cb, void *ctx);

/** Register callback for diagnostic text commands */
void transport_set_diag_callback(transport_diag_cb_t cb, void *ctx);

/** Poll for I/O events (call from event loop) */
sentinel_err_t transport_poll(int timeout_ms);

/** Check if command channel is connected */
bool transport_is_connected(void);

/** Close all connections */
void transport_close(void);

#endif /* TCP_TRANSPORT_H */
