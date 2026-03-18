/**
 * @file config_store.h
 * @brief FW-06: Flash-backed configuration storage
 */

#ifndef CONFIG_STORE_H
#define CONFIG_STORE_H

#include "../common/sentinel_types.h"

sentinel_err_t config_store_load(system_config_t *config);
sentinel_err_t config_store_save(const system_config_t *config);
sentinel_err_t config_store_validate(const system_config_t *config);
void config_store_get_defaults(system_config_t *config);

#endif /* CONFIG_STORE_H */
