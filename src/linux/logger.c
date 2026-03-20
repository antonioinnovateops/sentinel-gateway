/**
 * @file logger.c
 * @brief JSON Lines file logger
 */

#define _POSIX_C_SOURCE 200809L

#include "logger.h"
#include <stdio.h>
#include <time.h>
#include <string.h>

static FILE *g_sensor_fp = NULL;
static FILE *g_event_fp = NULL;

static void get_iso_timestamp(char *buf, size_t len)
{
    struct timespec ts;
    clock_gettime(CLOCK_REALTIME, &ts);
    struct tm tm;
    gmtime_r(&ts.tv_sec, &tm);
    int n = (int)strftime(buf, len, "%Y-%m-%dT%H:%M:%S", &tm);
    snprintf(buf + n, len - (size_t)n, ".%03ldZ", ts.tv_nsec / 1000000L);
}

/**
 * Escape a string for JSON output.
 * Escapes backslash, double-quote, and control characters.
 * @param src Source string (NULL-terminated)
 * @param dst Destination buffer
 * @param dst_size Size of destination buffer
 */
static void json_escape(const char *src, char *dst, size_t dst_size)
{
    if (dst_size == 0) { return; }
    size_t j = 0;
    for (size_t i = 0; src[i] != '\0' && j < dst_size - 1; i++) {
        char c = src[i];
        if (c == '\\' || c == '"') {
            if (j + 2 >= dst_size) { break; }
            dst[j++] = '\\';
            dst[j++] = c;
        } else if (c == '\n') {
            if (j + 2 >= dst_size) { break; }
            dst[j++] = '\\';
            dst[j++] = 'n';
        } else if (c == '\r') {
            if (j + 2 >= dst_size) { break; }
            dst[j++] = '\\';
            dst[j++] = 'r';
        } else if (c == '\t') {
            if (j + 2 >= dst_size) { break; }
            dst[j++] = '\\';
            dst[j++] = 't';
        } else if ((unsigned char)c < 0x20) {
            /* Skip other control characters */
            continue;
        } else {
            dst[j++] = c;
        }
    }
    dst[j] = '\0';
}

sentinel_err_t logger_init(const char *sensor_path, const char *event_path)
{
    if (sensor_path != NULL) {
        g_sensor_fp = fopen(sensor_path, "a");
        if (g_sensor_fp == NULL) {
            return SENTINEL_ERR_INTERNAL;
        }
    }
    if (event_path != NULL) {
        g_event_fp = fopen(event_path, "a");
        if (g_event_fp == NULL) {
            /* Clean up sensor file on partial failure */
            if (g_sensor_fp != NULL) {
                fclose(g_sensor_fp);
                g_sensor_fp = NULL;
            }
            return SENTINEL_ERR_INTERNAL;
        }
    }
    return SENTINEL_OK;
}

sentinel_err_t logger_write_sensor(const sensor_reading_t *reading)
{
    if ((g_sensor_fp == NULL) || (reading == NULL)) {
        return SENTINEL_ERR_INVALID_ARG;
    }

    char ts[32];
    get_iso_timestamp(ts, sizeof(ts));

    fprintf(g_sensor_fp,
            "{\"ts\":\"%s\",\"ch\":%u,\"raw\":%u,\"cal\":%.3f}\n",
            ts, reading->channel, reading->raw_value,
            (double)reading->calibrated_value);
    fflush(g_sensor_fp);
    return SENTINEL_OK;
}

sentinel_err_t logger_write_event(const char *event_type, const char *message)
{
    if ((g_event_fp == NULL) || (event_type == NULL)) {
        return SENTINEL_ERR_INVALID_ARG;
    }

    char ts[32];
    get_iso_timestamp(ts, sizeof(ts));

    /* Escape strings for safe JSON output */
    char type_escaped[64];
    char msg_escaped[256];
    json_escape(event_type, type_escaped, sizeof(type_escaped));
    json_escape(message ? message : "", msg_escaped, sizeof(msg_escaped));

    fprintf(g_event_fp,
            "{\"ts\":\"%s\",\"type\":\"%s\",\"msg\":\"%s\"}\n",
            ts, type_escaped, msg_escaped);
    fflush(g_event_fp);
    return SENTINEL_OK;
}

void logger_close(void)
{
    if (g_sensor_fp != NULL) { fclose(g_sensor_fp); g_sensor_fp = NULL; }
    if (g_event_fp != NULL) { fclose(g_event_fp); g_event_fp = NULL; }
}
