/**
 * @file diagnostics.c
 * @brief SW-06: Text-based diagnostic interface
 * @implements SWE-046 through SWE-051
 */

#include "diagnostics.h"
#include "sensor_proxy.h"
#include "actuator_proxy.h"
#include "health_monitor.h"
#include <string.h>
#include <stdio.h>

static const char *state_str(system_state_t s)
{
    switch (s) {
        case STATE_INIT:     return "INIT";
        case STATE_NORMAL:   return "NORMAL";
        case STATE_DEGRADED: return "DEGRADED";
        case STATE_FAILSAFE: return "FAILSAFE";
        case STATE_ERROR:    return "ERROR";
        default:             return "UNKNOWN";
    }
}

sentinel_err_t diagnostics_init(void)
{
    return SENTINEL_OK;
}

sentinel_err_t diagnostics_process_command(const char *cmd, char *response,
                                            size_t response_size)
{
    if ((cmd == NULL) || (response == NULL)) {
        return SENTINEL_ERR_INVALID_ARG;
    }

    if (strncmp(cmd, "status", 6) == 0) {
        linux_health_state_t health;
        health_monitor_get_state(&health);
        snprintf(response, response_size,
                 "state=%s uptime=%us wd_resets=%u comm=%s\n",
                 state_str(health.state),
                 health.uptime_s,
                 health.watchdog_resets,
                 health.comm_ok ? "OK" : "LOST");

    } else if (strncmp(cmd, "sensor read ", 12) == 0) {
        uint8_t ch = (uint8_t)(cmd[12] - '0');
        sensor_reading_t r;
        sentinel_err_t err = sensor_proxy_get_latest(ch, &r);
        if (err == SENTINEL_OK) {
            snprintf(response, response_size,
                     "ch=%u raw=%u cal=%.3f\n",
                     r.channel, r.raw_value, (double)r.calibrated_value);
        } else {
            snprintf(response, response_size, "ERROR: invalid channel\n");
        }

    } else if (strncmp(cmd, "actuator set ", 13) == 0) {
        uint8_t id = 0;
        float val = 0.0f;
        if (sscanf(cmd + 13, "%hhu %f", &id, &val) == 2) {
            sentinel_err_t err = actuator_proxy_send_command(id, val, NULL, NULL);
            snprintf(response, response_size,
                     err == SENTINEL_OK ? "OK\n" : "ERROR\n");
        } else {
            snprintf(response, response_size, "ERROR: usage: actuator set <id> <pct>\n");
        }

    } else if (strncmp(cmd, "version", 7) == 0) {
        snprintf(response, response_size,
                 "linux=%s mcu=pending\n", SENTINEL_VERSION);

    } else if (strncmp(cmd, "help", 4) == 0) {
        snprintf(response, response_size,
                 "Commands: status, sensor read <ch>, actuator set <id> <pct>, "
                 "version, help\n");

    } else {
        snprintf(response, response_size, "ERROR: unknown command\n");
    }

    return SENTINEL_OK;
}
