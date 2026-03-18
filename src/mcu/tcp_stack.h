/**
 * @file tcp_stack.h
 * @brief FW-04: TCP/IP communication manager
 *
 * In the full implementation, this wraps lwIP in NO_SYS mode.
 * For QEMU SIL, we use POSIX sockets (the QEMU virtual network
 * presents a standard Ethernet interface to the firmware).
 */

#ifndef TCP_STACK_H
#define TCP_STACK_H

#include "../common/sentinel_types.h"
#include <stddef.h>

/** Callback for received frames */
typedef void (*tcp_recv_cb_t)(const uint8_t *frame, size_t len);

sentinel_err_t tcp_stack_init(const system_config_t *config);
void tcp_stack_poll(void);
sentinel_err_t tcp_stack_send_telemetry(const uint8_t *frame, size_t len);
sentinel_err_t tcp_stack_send_command_response(const uint8_t *frame, size_t len);
bool tcp_stack_is_connected(void);
void tcp_stack_register_command_callback(tcp_recv_cb_t callback);
sentinel_err_t tcp_stack_reconnect(void);

#endif /* TCP_STACK_H */
