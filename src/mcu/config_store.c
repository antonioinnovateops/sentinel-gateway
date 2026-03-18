/**
 * @file config_store.c
 * @brief FW-06: CRC-protected config in flash
 * @implements SWE-058 through SWE-065
 */

#include "config_store.h"
#include "hal/flash_driver.h"
#include <string.h>

#define CONFIG_MAGIC     0x53454E54UL  /* "SENT" */
#define CONFIG_VERSION   1U

/* Flash layout structure */
typedef struct {
    uint32_t magic;
    uint16_t version;
    uint16_t reserved;
    system_config_t config;
    uint32_t crc32;
} config_flash_t;

/* CRC-32 lookup table (MISRA-friendly, no runtime generation) */
static uint32_t crc32_byte(uint32_t crc, uint8_t byte)
{
    crc ^= (uint32_t)byte;
    for (uint8_t bit = 0U; bit < 8U; bit++) {
        if ((crc & 1U) != 0U) {
            crc = (crc >> 1) ^ 0xEDB88320UL;
        } else {
            crc >>= 1;
        }
    }
    return crc;
}

static uint32_t crc32_calc(const uint8_t *data, size_t len)
{
    uint32_t crc = 0xFFFFFFFFUL;
    for (size_t i = 0U; i < len; i++) {
        crc = crc32_byte(crc, data[i]);
    }
    return crc ^ 0xFFFFFFFFUL;
}

void config_store_get_defaults(system_config_t *config)
{
    if (config == NULL) {
        return;
    }
    config->sensor_rates_hz[0] = 10U;
    config->sensor_rates_hz[1] = 10U;
    config->sensor_rates_hz[2] = 10U;
    config->sensor_rates_hz[3] = 10U;
    config->actuator_min_percent[0] = 0.0f;
    config->actuator_min_percent[1] = 0.0f;
    config->actuator_max_percent[0] = 95.0f;
    config->actuator_max_percent[1] = 95.0f;
    config->heartbeat_interval_ms = SENTINEL_HEALTH_INTERVAL_MS;
    config->comm_timeout_ms = SENTINEL_COMM_TIMEOUT_MS;
}

sentinel_err_t config_store_validate(const system_config_t *config)
{
    if (config == NULL) {
        return SENTINEL_ERR_INVALID_ARG;
    }

    for (uint8_t i = 0U; i < SENTINEL_MAX_CHANNELS; i++) {
        if ((config->sensor_rates_hz[i] < 1U) || (config->sensor_rates_hz[i] > 100U)) {
            return SENTINEL_ERR_OUT_OF_RANGE;
        }
    }
    for (uint8_t i = 0U; i < SENTINEL_MAX_ACTUATORS; i++) {
        if (config->actuator_min_percent[i] >= config->actuator_max_percent[i]) {
            return SENTINEL_ERR_INVALID_ARG;
        }
        if (config->actuator_max_percent[i] > 100.0f) {
            return SENTINEL_ERR_OUT_OF_RANGE;
        }
    }
    return SENTINEL_OK;
}

sentinel_err_t config_store_load(system_config_t *config)
{
    if (config == NULL) {
        return SENTINEL_ERR_INVALID_ARG;
    }

    config_flash_t flash_data;
    sentinel_err_t err = flash_read(FLASH_CONFIG_ADDR,
                                     (uint8_t *)&flash_data,
                                     sizeof(flash_data));
    if (err != SENTINEL_OK) {
        config_store_get_defaults(config);
        return err;
    }

    /* Verify magic */
    if (flash_data.magic != CONFIG_MAGIC) {
        config_store_get_defaults(config);
        return SENTINEL_ERR_DECODE;
    }

    /* Verify CRC */
    size_t crc_data_len = sizeof(flash_data) - sizeof(uint32_t);
    uint32_t computed_crc = crc32_calc((const uint8_t *)&flash_data, crc_data_len);
    if (computed_crc != flash_data.crc32) {
        config_store_get_defaults(config);
        return SENTINEL_ERR_DECODE;
    }

    /* Validate loaded config */
    err = config_store_validate(&flash_data.config);
    if (err != SENTINEL_OK) {
        config_store_get_defaults(config);
        return err;
    }

    *config = flash_data.config;
    return SENTINEL_OK;
}

sentinel_err_t config_store_save(const system_config_t *config)
{
    if (config == NULL) {
        return SENTINEL_ERR_INVALID_ARG;
    }

    sentinel_err_t err = config_store_validate(config);
    if (err != SENTINEL_OK) {
        return err;
    }

    config_flash_t flash_data;
    flash_data.magic = CONFIG_MAGIC;
    flash_data.version = CONFIG_VERSION;
    flash_data.reserved = 0U;
    flash_data.config = *config;

    /* Compute CRC over everything except the CRC field itself */
    size_t crc_data_len = sizeof(flash_data) - sizeof(uint32_t);
    flash_data.crc32 = crc32_calc((const uint8_t *)&flash_data, crc_data_len);

    /* Erase config sector */
    err = flash_erase_page(FLASH_CONFIG_ADDR);
    if (err != SENTINEL_OK) {
        return err;
    }

    /* Write */
    err = flash_write(FLASH_CONFIG_ADDR,
                       (const uint8_t *)&flash_data,
                       sizeof(flash_data));
    if (err != SENTINEL_OK) {
        return err;
    }

    /* Read-back verify */
    config_flash_t verify;
    err = flash_read(FLASH_CONFIG_ADDR,
                      (uint8_t *)&verify,
                      sizeof(verify));
    if (err != SENTINEL_OK) {
        return err;
    }

    if (memcmp(&flash_data, &verify, sizeof(flash_data)) != 0) {
        return SENTINEL_ERR_INTERNAL;
    }

    return SENTINEL_OK;
}
