/**
 * @file logger.h
 * @brief JSON Lines file logger
 */

#ifndef LOGGER_H
#define LOGGER_H

#include "../common/sentinel_types.h"

sentinel_err_t logger_init(const char *sensor_path, const char *event_path);
sentinel_err_t logger_write_sensor(const sensor_reading_t *reading);
sentinel_err_t logger_write_event(const char *event_type, const char *message);
void logger_close(void);

#endif /* LOGGER_H */
