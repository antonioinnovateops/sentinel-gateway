/**
 * @file health_monitor.c
 * @brief SW-05: MCU health monitoring, comm loss detection, recovery
 * @implements SWE-042 through SWE-045
 *
 * Decodes HealthStatus protobuf from MCU, tracks comm state.
 * Protobuf field layout (matches MCU protobuf_handler.c):
 *   Field 1 (LEN): MessageHeader {1:seq, 2:timestamp, 3:version{1:major,2:minor}}
 *   Field 2 (VARINT): state
 *   Field 3 (VARINT): comm_status
 *   Field 4 (VARINT): uptime_s
 *   Field 5 (VARINT): watchdog_resets
 *   Field 7 (VARINT): free_stack
 *   Field 8 (VARINT): fault_code
 *   Field 9 (FIXED32): fan_duty
 *   Field 10 (FIXED32): valve_duty
 */

#define _POSIX_C_SOURCE 200809L

#include "health_monitor.h"
#include "protobuf_lite.h"
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
static uint64_t g_recovery_start_ms = 0U;  /* Timestamp when recovery started */

/* Recovery timing per SYS-042: 5 seconds between attempts */
#define RECOVERY_DELAY_MS 5000U

/* MCU version extracted from health message header */
static uint32_t g_mcu_version_major = 0U;
static uint32_t g_mcu_version_minor = 0U;

static uint64_t get_ms(void)
{
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (uint64_t)ts.tv_sec * 1000ULL + (uint64_t)(ts.tv_nsec / 1000000L);
}

/* Decode version sub-message: {1:major, 2:minor} */
static void decode_version(const uint8_t *buf, size_t len)
{
    size_t pos = 0;
    while (pos < len) {
        uint64_t tag;
        size_t n = pb_dec_varint(buf + pos, len - pos, &tag);
        if (n == 0) { break; }
        pos += n;
        uint32_t field = (uint32_t)(tag >> 3);
        uint32_t wtype = (uint32_t)(tag & 0x07);

        if (wtype == 0) {
            uint64_t v;
            n = pb_dec_varint(buf + pos, len - pos, &v);
            if (n == 0) { break; }
            pos += n;
            if (field == 1) { g_mcu_version_major = (uint32_t)v; }
            else if (field == 2) { g_mcu_version_minor = (uint32_t)v; }
        } else {
            break;
        }
    }
}

/* Decode MessageHeader sub-message: {1:seq, 2:timestamp, 3:version} */
static void decode_header(const uint8_t *buf, size_t len)
{
    size_t pos = 0;
    while (pos < len) {
        uint64_t tag;
        size_t n = pb_dec_varint(buf + pos, len - pos, &tag);
        if (n == 0) { break; }
        pos += n;
        uint32_t field = (uint32_t)(tag >> 3);
        uint32_t wtype = (uint32_t)(tag & 0x07);

        if (wtype == 0) { /* varint — skip seq/timestamp */
            uint64_t v;
            n = pb_dec_varint(buf + pos, len - pos, &v);
            if (n == 0) { break; }
            pos += n;
        } else if (wtype == 2) { /* length-delimited */
            uint64_t slen;
            n = pb_dec_varint(buf + pos, len - pos, &slen);
            if (n == 0) { break; }
            pos += n;
            if (pos + slen > len) { break; }
            if (field == 3) { decode_version(buf + pos, (size_t)slen); }
            pos += (size_t)slen;
        } else {
            break;
        }
    }
}

sentinel_err_t health_monitor_init(void)
{
    (void)memset(&g_health, 0, sizeof(g_health));
    g_hm_state = HM_MONITORING;
    g_recovery_attempts = 0U;
    g_last_health_received_ms = get_ms();
    g_recovery_start_ms = 0U;
    g_mcu_version_major = 0U;
    g_mcu_version_minor = 0U;
    return SENTINEL_OK;
}

sentinel_err_t health_monitor_process_health(const uint8_t *payload, size_t len)
{
    if (payload == NULL || len == 0) {
        return SENTINEL_ERR_INVALID_ARG;
    }

    /* Decode protobuf fields */
    size_t pos = 0;
    while (pos < len) {
        uint64_t tag;
        size_t n = pb_dec_varint(payload + pos, len - pos, &tag);
        if (n == 0) { break; }
        pos += n;
        uint32_t field = (uint32_t)(tag >> 3);
        uint32_t wtype = (uint32_t)(tag & 0x07);

        if (wtype == 0) { /* varint */
            uint64_t v;
            n = pb_dec_varint(payload + pos, len - pos, &v);
            if (n == 0) { break; }
            pos += n;
            switch (field) {
                case 2: g_health.state = (system_state_t)v; break;
                /* case 3: comm_status — managed locally */
                case 4: g_health.uptime_s = (uint32_t)v; break;
                case 5: g_health.watchdog_resets = (uint32_t)v; break;
                default: break;
            }
        } else if (wtype == 5) { /* fixed32 */
            if (pos + 4 > len) { break; }
            /* fields 9, 10: fan_duty, valve_duty — not stored in health state */
            (void)pb_dec_float32(payload + pos);
            pos += 4;
        } else if (wtype == 2) { /* length-delimited */
            uint64_t slen;
            n = pb_dec_varint(payload + pos, len - pos, &slen);
            if (n == 0) { break; }
            pos += n;
            if (pos + slen > len) { break; }
            if (field == 1) { decode_header(payload + pos, (size_t)slen); }
            pos += (size_t)slen;
        } else {
            break;
        }
    }

    g_last_health_received_ms = get_ms();
    g_health.last_health_ms = g_last_health_received_ms;
    g_health.comm_ok = true;

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
                g_recovery_start_ms = now_ms;
                logger_write_event("COMM_LOSS", "MCU health timeout");
            }
            break;

        case HM_COMM_LOSS:
            /* Transition to RECOVERING and start first recovery attempt */
            g_hm_state = HM_RECOVERING;
            g_recovery_attempts = 1U;
            g_recovery_start_ms = now_ms;
            logger_write_event("RECOVERY_START", "Attempting reconnection");
            /* TODO: Implement actual reconnection via transport layer */
            /* transport_reconnect_command(); */
            break;

        case HM_RECOVERING:
            /* Wait RECOVERY_DELAY_MS (5 seconds per SYS-042) before next attempt */
            if ((now_ms - g_recovery_start_ms) >= RECOVERY_DELAY_MS) {
                if (g_recovery_attempts >= 3U) {
                    g_hm_state = HM_FAULT;
                    logger_write_event("FAULT", "Recovery failed after 3 attempts");
                } else {
                    g_recovery_attempts++;
                    g_recovery_start_ms = now_ms;
                    logger_write_event("RECOVERY_RETRY", "Retrying reconnection");
                    /* TODO: Implement actual reconnection via transport layer */
                    /* transport_reconnect_command(); */
                }
            }
            break;

        case HM_FAULT:
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

void health_monitor_get_mcu_version(uint32_t *major, uint32_t *minor)
{
    if (major != NULL) { *major = g_mcu_version_major; }
    if (minor != NULL) { *minor = g_mcu_version_minor; }
}
