/**
 * @file main.c
 * @brief MCU firmware entry point — bare-metal super-loop
 * @implements SWE.3 Main Loop Design (mcu_main_loop.md)
 *
 * Super-loop architecture:
 * 1. Initialize all subsystems
 * 2. Loop forever:
 *    a. Poll TCP stack
 *    b. Sample sensors (if due)
 *    c. Encode and transmit sensor data
 *    d. Encode and transmit health status (every 1s)
 *    e. Safety monitor tick
 *    f. Feed watchdog
 */

#include "../common/sentinel_types.h"
#include "../common/wire_frame.h"
#include "hal/systick.h"
#include "hal/gpio_driver.h"
#include "hal/flash_driver.h"
#include "sensor_acquisition.h"
#include "actuator_control.h"
#include "protobuf_handler.h"
#include "tcp_stack.h"
#include "watchdog_mgr.h"
#include "config_store.h"
#include "health_reporter.h"

/* Static buffers — no heap allocation */
static system_config_t g_config;
static sensor_reading_t g_readings[SENTINEL_MAX_CHANNELS];
static uint8_t g_frame_buf[WIRE_FRAME_MAX_SIZE];
static uint32_t g_last_health_ms = 0U;

/* Command dispatch callback */
static void on_command_received(const uint8_t *frame, size_t len);

int main(void)
{
    /* Phase 1: HAL initialization */
    systick_init();
    gpio_init();
    flash_init();

    /* Phase 2: Load configuration */
    sentinel_err_t err = config_store_load(&g_config);
    if (err != SENTINEL_OK) {
        /* Use defaults on CRC failure */
        config_store_get_defaults(&g_config);
    }

    /* Phase 3: Subsystem initialization */
    sensor_acq_init(&g_config);
    actuator_init(&g_config);
    safety_monitor_init();
    watchdog_mgr_init();
    tcp_stack_init(&g_config);

    /* Register command handler */
    tcp_stack_register_command_callback(on_command_received);

    /* LED on to indicate running */
    gpio_led_set(true);

    /* ====== SUPER-LOOP ====== */
    for (;;) {
        uint32_t now_ms = systick_get_ms();

        /* 1. Poll TCP stack (lwIP timers, receive processing) */
        tcp_stack_poll();

        /* 2. Update communication status */
        safety_monitor_set_tcp_connected(tcp_stack_is_connected());

        /* 3. Sample sensors if due */
        if (sensor_acq_is_due(now_ms)) {
            uint32_t count = 0U;
            err = sensor_acq_sample(g_readings, &count);
            if ((err == SENTINEL_OK) && (count > 0U)) {
                /* Encode and send sensor data */
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
            /* Toggle LED as heartbeat indicator */
            gpio_led_toggle();
        }

        /* 5. Safety monitor tick */
        safety_monitor_tick(now_ms);

        /* 6. Feed watchdog — MUST be last, proves loop completed */
        watchdog_mgr_feed();
    }

    /* Should never reach here */
    return 0;
}

/**
 * Command dispatch callback — called from tcp_stack when data arrives
 * on port 5000. Decodes wire frame, dispatches to appropriate handler.
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

    /* Reject commands in failsafe */
    if (safety_monitor_is_failsafe() && (msg_type != MSG_TYPE_STATE_SYNC_REQ)) {
        /* Only state sync allowed in failsafe */
        return;
    }

    switch (msg_type) {
        case MSG_TYPE_ACTUATOR_CMD: {
            /* Simplified: extract actuator_id and target_percent from payload */
            /* Full nanopb decode will replace this */
            /* For now: acknowledge with STATUS_OK */
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
            /* Recovery: transition from failsafe to normal */
            safety_monitor_set_tcp_connected(true);
            break;
        }
        default:
            /* Unknown message type — ignore */
            break;
    }
}
