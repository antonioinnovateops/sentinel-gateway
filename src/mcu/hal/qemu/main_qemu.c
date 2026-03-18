/**
 * @file main_qemu.c
 * @brief MCU firmware entry point for QEMU user-mode emulation
 *
 * This replaces main.c when building for qemu-arm-static (user-mode).
 * Uses POSIX APIs for timing and signals, but runs the SAME application
 * logic (sensor acquisition, actuator control, safety monitor, etc.)
 *
 * Key differences from bare-metal main.c:
 * - Uses clock_gettime/usleep instead of SysTick interrupts
 * - Uses POSIX signals for clean shutdown
 * - tcp_stack_qemu.c provides real TCP via POSIX sockets
 * - HAL shims simulate ADC/PWM/GPIO behavior in software
 */

#include "../../common/sentinel_types.h"
#include "../../common/wire_frame.h"
#include "../systick.h"
#include "../gpio_driver.h"
#include "../flash_driver.h"
#include "../../sensor_acquisition.h"
#include "../../actuator_control.h"
#include "../../protobuf_handler.h"
#include "../../tcp_stack.h"
#include "../../watchdog_mgr.h"
#include "../../config_store.h"
#include "../../health_reporter.h"

#include <stdio.h>
#include <stdlib.h>
#include <signal.h>
#include <time.h>
#include <unistd.h>
#include <stdbool.h>

/* Global running flag (set to false by SIGINT/SIGTERM) */
static volatile sig_atomic_t g_running = 1;

/* Static buffers - no heap allocation (same as bare-metal) */
static system_config_t g_config;
static sensor_reading_t g_readings[SENTINEL_MAX_CHANNELS];
static uint8_t g_frame_buf[WIRE_FRAME_MAX_SIZE];
static uint32_t g_last_health_ms = 0U;

/* POSIX time tracking */
static struct timespec g_start_time;
static uint32_t g_tick_ms = 0U;

/* Command dispatch callback - forward declaration */
static void on_command_received(const uint8_t *frame, size_t len);

/**
 * Signal handler for graceful shutdown
 */
static void signal_handler(int signum)
{
    (void)signum;
    fprintf(stderr, "\n[QEMU-MCU] Received signal %d, shutting down...\n", signum);
    g_running = 0;
}

/**
 * Initialize POSIX systick replacement using clock_gettime
 */
static void systick_posix_init(void)
{
    clock_gettime(CLOCK_MONOTONIC, &g_start_time);
    g_tick_ms = 0U;
}

/**
 * Get elapsed milliseconds since init (POSIX replacement for systick_get_ms)
 * This function is called by the main loop and by modules that include systick.h
 */
uint32_t systick_get_ms(void)
{
    struct timespec now;
    clock_gettime(CLOCK_MONOTONIC, &now);

    long sec_diff = now.tv_sec - g_start_time.tv_sec;
    long nsec_diff = now.tv_nsec - g_start_time.tv_nsec;

    if (nsec_diff < 0) {
        sec_diff -= 1;
        nsec_diff += 1000000000L;
    }

    g_tick_ms = (uint32_t)(sec_diff * 1000 + nsec_diff / 1000000);
    return g_tick_ms;
}

/**
 * Get elapsed microseconds since init
 */
uint64_t systick_get_us(void)
{
    struct timespec now;
    clock_gettime(CLOCK_MONOTONIC, &now);

    long sec_diff = now.tv_sec - g_start_time.tv_sec;
    long nsec_diff = now.tv_nsec - g_start_time.tv_nsec;

    if (nsec_diff < 0) {
        sec_diff -= 1;
        nsec_diff += 1000000000L;
    }

    return (uint64_t)(sec_diff * 1000000LL + nsec_diff / 1000);
}

/**
 * POSIX systick_init shim - just calls our POSIX implementation
 */
void systick_init(void)
{
    systick_posix_init();
}

/**
 * Command dispatch callback - identical to bare-metal main.c
 * Called from tcp_stack_qemu when data arrives on port 5000.
 */
static void on_command_received(const uint8_t *frame, size_t len)
{
    if ((frame == NULL) || (len < WIRE_FRAME_HEADER_SIZE)) {
        return;
    }

    uint8_t msg_type = 0U;
    uint32_t payload_len = 0U;

    sentinel_err_t err = wire_frame_decode_header(frame, len,
                                                    &msg_type, &payload_len);
    if (err != SENTINEL_OK) {
        return;
    }

    /* Mark communication as alive */
    safety_monitor_set_comm_ok(true);

    /* Reject commands in failsafe (except state sync) */
    if (safety_monitor_is_failsafe() && (msg_type != MSG_TYPE_STATE_SYNC_REQ)) {
        return;
    }

    switch (msg_type) {
        case MSG_TYPE_ACTUATOR_CMD: {
            size_t rsp_len = 0U;
            (void)pb_encode_actuator_response(0U, 0.0f, 0U,
                                               g_frame_buf, &rsp_len);
            (void)tcp_stack_send_command_response(g_frame_buf, rsp_len);
            break;
        }
        case MSG_TYPE_CONFIG_UPDATE: {
            size_t rsp_len = 0U;
            (void)pb_encode_config_response(0U, NULL, g_frame_buf, &rsp_len);
            (void)tcp_stack_send_command_response(g_frame_buf, rsp_len);
            break;
        }
        case MSG_TYPE_STATE_SYNC_REQ: {
            safety_monitor_set_tcp_connected(true);
            break;
        }
        default:
            break;
    }
}

int main(int argc, char *argv[])
{
    (void)argc;
    (void)argv;

    fprintf(stderr, "[QEMU-MCU] Starting Sentinel MCU firmware (QEMU user-mode)\n");

    /* Set up signal handlers for clean shutdown */
    struct sigaction sa;
    sa.sa_handler = signal_handler;
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = 0;
    sigaction(SIGINT, &sa, NULL);
    sigaction(SIGTERM, &sa, NULL);

    /* Phase 1: HAL initialization */
    fprintf(stderr, "[QEMU-MCU] Initializing HAL...\n");
    systick_init();
    gpio_init();
    flash_init();

    /* Phase 2: Load configuration */
    fprintf(stderr, "[QEMU-MCU] Loading configuration...\n");
    sentinel_err_t err = config_store_load(&g_config);
    if (err != SENTINEL_OK) {
        fprintf(stderr, "[QEMU-MCU] Config load failed, using defaults\n");
        config_store_get_defaults(&g_config);
    }

    /* Phase 3: Subsystem initialization */
    fprintf(stderr, "[QEMU-MCU] Initializing subsystems...\n");
    sensor_acq_init(&g_config);
    actuator_init(&g_config);
    safety_monitor_init();
    watchdog_mgr_init();

    /* Initialize TCP stack (uses tcp_stack_qemu.c with POSIX sockets) */
    fprintf(stderr, "[QEMU-MCU] Initializing TCP stack...\n");
    err = tcp_stack_init(&g_config);
    if (err != SENTINEL_OK) {
        fprintf(stderr, "[QEMU-MCU] TCP init failed: %d\n", err);
        return 1;
    }

    /* Register command handler */
    tcp_stack_register_command_callback(on_command_received);

    /* LED on to indicate running */
    gpio_led_set(true);

    fprintf(stderr, "[QEMU-MCU] Entering main loop\n");

    /* ====== SUPER-LOOP ====== */
    while (g_running) {
        uint32_t now_ms = systick_get_ms();

        /* 1. Poll TCP stack (process incoming data, manage connections) */
        tcp_stack_poll();

        /* 2. Update communication status */
        safety_monitor_set_tcp_connected(tcp_stack_is_connected());

        /* 3. Sample sensors if due */
        if (sensor_acq_is_due(now_ms)) {
            uint32_t count = 0U;
            err = sensor_acq_sample(g_readings, &count);
            if ((err == SENTINEL_OK) && (count > 0U)) {
                size_t frame_len = 0U;
                err = pb_encode_sensor_data(g_readings, count,
                                             g_config.sensor_rates_hz[0],
                                             g_frame_buf, &frame_len);
                if (err == SENTINEL_OK) {
                    (void)tcp_stack_send_telemetry(g_frame_buf, frame_len);
                }
            }
        }

        /* 4. Health reporting (every heartbeat interval) */
        if ((now_ms - g_last_health_ms) >= g_config.heartbeat_interval_ms) {
            g_last_health_ms = now_ms;
            health_data_t health;
            if (safety_monitor_get_health(&health) == SENTINEL_OK) {
                size_t frame_len = 0U;
                err = pb_encode_health_status(&health, g_frame_buf, &frame_len);
                if (err == SENTINEL_OK) {
                    (void)tcp_stack_send_telemetry(g_frame_buf, frame_len);
                }
            }
            gpio_led_toggle();
        }

        /* 5. Safety monitor tick */
        safety_monitor_tick(now_ms);

        /* 6. Feed watchdog (simulated in QEMU HAL) */
        watchdog_mgr_feed();

        /* 7. Sleep briefly to avoid spinning CPU at 100%
         * In bare-metal we'd busy-loop, but user-mode should be nice
         * to the host. 1ms sleep maintains reasonable responsiveness. */
        usleep(1000);
    }

    fprintf(stderr, "[QEMU-MCU] Shutdown complete\n");
    return 0;
}
