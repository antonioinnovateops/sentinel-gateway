---
title: "Detailed Design — MCU Configuration Store (FW-06)"
document_id: "DD-FW06"
project: "Sentinel Gateway"
version: "1.1.0"
date: 2026-03-18
status: "Approved"
aspice_process: "SWE.3 Software Detailed Design"
component: "FW-06"
implements: ["SWE-058-1", "SWE-059-1", "SWE-060-1", "SWE-062-1", "SWE-063-1", "SWE-064-1", "SWE-065-1"]
---

# Detailed Design — MCU Configuration Store (FW-06)

## 1. Purpose

Stores and retrieves system configuration in MCU internal flash. CRC-32 protected for data integrity. Provides validated defaults on CRC mismatch or invalid data.

## 2. Implementation Reference

| Item | Value |
|------|-------|
| Source file | `src/mcu/config_store.c` |
| Header file | `src/mcu/config_store.h` |
| Lines | ~130 |
| CMake target | `sentinel-mcu` or `sentinel-mcu-qemu` |
| Build command | `docker compose run --rm mcu-build` |
| QEMU build | `docker compose run --rm mcu-build-qemu` |
| Unit test | `tests/unit/test_config_store.c` |

## 3. Module Interface

### 3.1 Function Signatures (Exact)

```c
#include "../common/sentinel_types.h"

sentinel_err_t config_store_load(system_config_t *config);
sentinel_err_t config_store_save(const system_config_t *config);
sentinel_err_t config_store_validate(const system_config_t *config);
void config_store_get_defaults(system_config_t *config);
```

### 3.2 Error Handling Contract

| Function | Returns | Recovery |
|----------|---------|----------|
| `config_store_load()` | `SENTINEL_OK` | Valid config loaded |
| `config_store_load()` | `SENTINEL_ERR_INVALID_ARG` | NULL pointer, defaults loaded |
| `config_store_load()` | `SENTINEL_ERR_DECODE` | Magic or CRC mismatch, defaults loaded |
| `config_store_load()` | flash error | Flash read failed, defaults loaded |
| `config_store_save()` | `SENTINEL_OK` | Config saved and verified |
| `config_store_save()` | `SENTINEL_ERR_INVALID_ARG` | NULL pointer or validation failed |
| `config_store_save()` | `SENTINEL_ERR_INTERNAL` | Read-back verify failed |
| `config_store_validate()` | `SENTINEL_OK` | All parameters in range |
| `config_store_validate()` | `SENTINEL_ERR_INVALID_ARG` | NULL or invalid actuator limits |
| `config_store_validate()` | `SENTINEL_ERR_OUT_OF_RANGE` | Sensor rate or actuator max out of range |
| `config_store_get_defaults()` | void | NULL-safe (returns early) |

**Key behavior**: On any load failure, `config_store_load()` automatically populates the output with defaults before returning the error code. Caller can use the config immediately.

## 4. Flash Layout

```c
#define CONFIG_MAGIC     0x53454E54UL  /* "SENT" in ASCII */
#define CONFIG_VERSION   1U

typedef struct {
    uint32_t magic;           /* 0x0000: "SENT" */
    uint16_t version;         /* 0x0004: Config version */
    uint16_t reserved;        /* 0x0006: Padding */
    system_config_t config;   /* 0x0008: Actual config data */
    uint32_t crc32;           /* Last 4 bytes: CRC of everything above */
} config_flash_t;
```

| Offset | Size | Content |
|--------|------|---------|
| 0x0000 | 4 | Magic number: `0x53454E54` ("SENT") |
| 0x0004 | 2 | Config version (for migration) |
| 0x0006 | 2 | Reserved |
| 0x0008 | ~40 | `system_config_t` payload |
| Last 4 | 4 | CRC-32 of bytes 0 to (size-4) |

Flash address defined in HAL:
```c
/* From flash_driver.h */
#define FLASH_CONFIG_ADDR  0x08030000UL  /* STM32U575 */
/* QEMU uses RAM-backed simulation */
```

## 5. CRC-32 Implementation

MISRA-compliant byte-by-byte CRC calculation (no lookup table):

```c
static uint32_t crc32_byte(uint32_t crc, uint8_t byte)
{
    crc ^= (uint32_t)byte;
    for (uint8_t bit = 0U; bit < 8U; bit++) {
        if ((crc & 1U) != 0U) {
            crc = (crc >> 1) ^ 0xEDB88320UL;  /* Polynomial */
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
```

## 6. Processing Logic

### 6.1 Load Sequence

```c
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
```

### 6.2 Save Sequence

```c
sentinel_err_t config_store_save(const system_config_t *config)
{
    /* 1. Validate config */
    err = config_store_validate(config);
    if (err != SENTINEL_OK) { return err; }

    /* 2. Prepare flash structure */
    config_flash_t flash_data;
    flash_data.magic = CONFIG_MAGIC;
    flash_data.version = CONFIG_VERSION;
    flash_data.reserved = 0U;
    flash_data.config = *config;

    /* 3. Compute CRC */
    size_t crc_data_len = sizeof(flash_data) - sizeof(uint32_t);
    flash_data.crc32 = crc32_calc((const uint8_t *)&flash_data, crc_data_len);

    /* 4. Erase config sector */
    err = flash_erase_page(FLASH_CONFIG_ADDR);
    if (err != SENTINEL_OK) { return err; }

    /* 5. Write */
    err = flash_write(FLASH_CONFIG_ADDR,
                       (const uint8_t *)&flash_data,
                       sizeof(flash_data));
    if (err != SENTINEL_OK) { return err; }

    /* 6. Read-back verify */
    config_flash_t verify;
    err = flash_read(FLASH_CONFIG_ADDR, (uint8_t *)&verify, sizeof(verify));
    if (err != SENTINEL_OK) { return err; }

    if (memcmp(&flash_data, &verify, sizeof(flash_data)) != 0) {
        return SENTINEL_ERR_INTERNAL;
    }

    return SENTINEL_OK;
}
```

### 6.3 Validation Rules

```c
sentinel_err_t config_store_validate(const system_config_t *config)
{
    if (config == NULL) {
        return SENTINEL_ERR_INVALID_ARG;
    }

    /* Sensor rates: 1-100 Hz */
    for (uint8_t i = 0U; i < SENTINEL_MAX_CHANNELS; i++) {
        if ((config->sensor_rates_hz[i] < 1U) ||
            (config->sensor_rates_hz[i] > 100U)) {
            return SENTINEL_ERR_OUT_OF_RANGE;
        }
    }

    /* Actuator limits: min < max, max <= 100% */
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
```

### 6.4 Default Values

```c
void config_store_get_defaults(system_config_t *config)
{
    if (config == NULL) { return; }

    config->sensor_rates_hz[0] = 10U;
    config->sensor_rates_hz[1] = 10U;
    config->sensor_rates_hz[2] = 10U;
    config->sensor_rates_hz[3] = 10U;
    config->actuator_min_percent[0] = 0.0f;
    config->actuator_min_percent[1] = 0.0f;
    config->actuator_max_percent[0] = 95.0f;
    config->actuator_max_percent[1] = 95.0f;
    config->heartbeat_interval_ms = SENTINEL_HEALTH_INTERVAL_MS;  /* 1000 */
    config->comm_timeout_ms = SENTINEL_COMM_TIMEOUT_MS;           /* 3000 */
}
```

| Parameter | Default | Valid Range |
|-----------|---------|-------------|
| Sensor rates | 10 Hz (all) | 1–100 Hz |
| Actuator min | 0% (both) | 0–99% |
| Actuator max | 95% (both) | 1–100% |
| Heartbeat interval | 1000 ms | Compile-time |
| Comm timeout | 3000 ms | Compile-time |

## 7. Dependencies

| Dependency | Include | Purpose |
|------------|---------|---------|
| sentinel_types.h | `#include "../common/sentinel_types.h"` | Types, constants |
| flash_driver.h | `#include "hal/flash_driver.h"` | Flash read/write/erase |
| string.h | `#include <string.h>` | memcmp |

## 8. Implementation Lessons

1. **Default fallback on any failure**: Load always populates output with valid defaults before returning error. This ensures safe operation even with corrupted flash.

2. **No MISRA lookup table**: CRC uses byte-by-byte computation to avoid static const array issues with some MISRA checkers.

3. **Read-back verify**: After writing, config is read back and compared byte-for-byte to detect flash failures.

4. **QEMU flash simulation**: Flash driver has a QEMU variant (`src/mcu/hal/qemu/flash_driver_qemu.c`) that uses RAM-backed storage.

5. **Config version field**: Reserved for future migration. Currently version 1.

## 9. Traceability

| Requirement | Implementation | Status |
|-------------|----------------|--------|
| SWE-058-1 | `config_store_save()` — persist config to flash | Complete |
| SWE-059-1 | `config_store_load()` — load config from flash | Complete |
| SWE-060-1 | CRC-32 validation on load | Complete |
| SWE-062-1 | `config_store_get_defaults()` — factory defaults | Complete |
| SWE-063-1 | `config_store_validate()` — range validation | Complete |
| SWE-064-1 | Flash read-back verify after write | Complete |
| SWE-065-1 | Config version field for future migration | Complete |
