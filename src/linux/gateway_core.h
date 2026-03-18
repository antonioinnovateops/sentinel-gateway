/**
 * @file gateway_core.h
 * @brief SW-01: Main gateway application core
 */

#ifndef GATEWAY_CORE_H
#define GATEWAY_CORE_H

#include "../common/sentinel_types.h"

/** Initialize gateway and start event loop */
sentinel_err_t gateway_init(void);

/** Run the main event loop (blocks) */
sentinel_err_t gateway_run(void);

/** Request graceful shutdown */
void gateway_shutdown(void);

#endif /* GATEWAY_CORE_H */
