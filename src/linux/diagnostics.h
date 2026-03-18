/**
 * @file diagnostics.h
 * @brief SW-06: Diagnostic server on TCP port 5002
 */

#ifndef DIAGNOSTICS_H
#define DIAGNOSTICS_H

#include "../common/sentinel_types.h"
#include <stddef.h>

sentinel_err_t diagnostics_init(void);
sentinel_err_t diagnostics_process_command(const char *cmd, char *response,
                                            size_t response_size);

#endif /* DIAGNOSTICS_H */
