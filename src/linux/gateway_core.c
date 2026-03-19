/**
 * @file gateway_core.c
 * @brief SW-01: Gateway application core — epoll event loop
 * @implements SWE.3 Gateway Core Design
 */

#define _POSIX_C_SOURCE 200809L

#include "gateway_core.h"
#include "tcp_transport.h"
#include "sensor_proxy.h"
#include "actuator_proxy.h"
#include "health_monitor.h"
#include "diagnostics.h"
#include "config_manager.h"
#include "logger.h"
#include "../common/wire_frame.h"

#include <stdio.h>
#include <signal.h>
#include <time.h>
#include <string.h>
#include <unistd.h>
#include <stdlib.h>

static volatile sig_atomic_t g_running = 1;

static void signal_handler(int sig)
{
    (void)sig;
    g_running = 0;
}

static uint64_t get_ms(void)
{
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (uint64_t)ts.tv_sec * 1000ULL + (uint64_t)(ts.tv_nsec / 1000000L);
}

/* Diagnostic text command callback */
static void on_diag_command(int client_fd, const char *cmd, void *ctx)
{
    (void)ctx;
    char response[512];
    diagnostics_process_command(cmd, response, sizeof(response));

    /* Send response back to diagnostic client */
    (void)write(client_fd, response, strlen(response));
}

/* Message dispatch callback from transport layer */
static void on_message_received(uint8_t msg_type, const uint8_t *payload,
                                 size_t payload_len, void *ctx)
{
    (void)ctx;

    switch (msg_type) {
        case MSG_TYPE_SENSOR_DATA:
            sensor_proxy_process(payload, payload_len);
            break;
        case MSG_TYPE_HEALTH_STATUS:
            health_monitor_process_health(payload, payload_len);
            break;
        case MSG_TYPE_ACTUATOR_RSP:
            actuator_proxy_process_response(payload, payload_len);
            break;
        case MSG_TYPE_CONFIG_RSP:
            config_manager_process_response(payload, payload_len);
            break;
        default:
            fprintf(stderr, "[GW] Unknown message type: 0x%02X\n", msg_type);
            break;
    }
}

sentinel_err_t gateway_init(void)
{
    printf("[GW] Sentinel Gateway %s starting...\n", SENTINEL_VERSION);

    /* Signal handling */
    struct sigaction sa;
    (void)memset(&sa, 0, sizeof(sa));
    sa.sa_handler = signal_handler;
    (void)sigaction(SIGINT, &sa, NULL);
    (void)sigaction(SIGTERM, &sa, NULL);

    /* Initialize subsystems */
    sentinel_err_t err;

    err = logger_init("/var/log/sentinel/sensor_data.jsonl",
                       "/var/log/sentinel/events.jsonl");
    if (err != SENTINEL_OK) {
        /* Fallback to current directory */
        err = logger_init("sensor_data.jsonl", "events.jsonl");
    }

    err = config_manager_init(NULL);
    if (err != SENTINEL_OK) { return err; }

    err = sensor_proxy_init();
    if (err != SENTINEL_OK) { return err; }

    err = actuator_proxy_init();
    if (err != SENTINEL_OK) { return err; }

    err = health_monitor_init();
    if (err != SENTINEL_OK) { return err; }

    err = diagnostics_init();
    if (err != SENTINEL_OK) { return err; }

    /* Initialize transport */
    err = transport_init();
    if (err != SENTINEL_OK) { return err; }

    transport_set_recv_callback(on_message_received, NULL);
    transport_set_diag_callback(on_diag_command, NULL);

    /* Start listening for telemetry and diagnostics */
    err = transport_listen_telemetry();
    if (err != SENTINEL_OK) {
        fprintf(stderr, "[GW] Failed to listen on telemetry port\n");
        return err;
    }

    err = transport_listen_diagnostics();
    if (err != SENTINEL_OK) {
        fprintf(stderr, "[GW] Failed to listen on diagnostics port\n");
        return err;
    }

    printf("[GW] Listening on ports %u (telemetry) and %u (diagnostics)\n",
           SENTINEL_PORT_TELEMETRY, SENTINEL_PORT_DIAG);

    /* Connect to MCU command channel (allow env override for SIL testing) */
    const char *mcu_ip = getenv("SENTINEL_MCU_HOST");
    if (mcu_ip == NULL) { mcu_ip = SENTINEL_MCU_IP; }
    err = transport_connect_command(mcu_ip);
    if (err != SENTINEL_OK) {
        fprintf(stderr, "[GW] Warning: MCU not yet available on %s:%u\n",
                mcu_ip, SENTINEL_PORT_COMMAND);
        /* Non-fatal — will retry */
    }

    logger_write_event("STARTUP", "Gateway initialized");
    return SENTINEL_OK;
}

sentinel_err_t gateway_run(void)
{
    printf("[GW] Entering main event loop\n");

    while (g_running) {
        /* Poll for I/O events with 100ms timeout */
        transport_poll(100);

        /* Periodic health check */
        uint64_t now_ms = get_ms();
        health_monitor_tick(now_ms);
    }

    printf("[GW] Shutting down...\n");
    logger_write_event("SHUTDOWN", "Gateway stopping");
    transport_close();
    logger_close();

    return SENTINEL_OK;
}

void gateway_shutdown(void)
{
    g_running = 0;
}
