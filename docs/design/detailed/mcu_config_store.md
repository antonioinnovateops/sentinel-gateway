---
title: "Detailed Design — MCU Configuration Store (FW-06)"
document_id: "DD-FW06"
project: "Sentinel Gateway"
version: "1.0.0"
date: 2026-03-18
status: "Approved"
aspice_process: "SWE.3 Software Detailed Design"
component: "FW-06"
implements: ["SWE-058-1", "SWE-059-1", "SWE-060-1", "SWE-062-1", "SWE-063-1", "SWE-064-1", "SWE-065-1"]
---

# Detailed Design — MCU Configuration Store (FW-06)

## 1. Purpose

Stores and retrieves system configuration in MCU internal flash (4 KB config sector at 0x0803_0000). CRC-32 protected for data integrity. Provides defaults on CRC mismatch.

## 2. Module Interface

```c
sentinel_err_t config_store_load(system_config_t *config);
sentinel_err_t config_store_save(const system_config_t *config);
sentinel_err_t config_store_validate(const system_config_t *config);
void config_store_get_defaults(system_config_t *config);
```

## 3. Flash Layout

| Offset | Size | Content |
|--------|------|---------|
| 0x0000 | 4 | Magic number: `0x53454E54` ("SENT") |
| 0x0004 | 2 | Config version (for migration) |
| 0x0006 | 2 | Reserved |
| 0x0008 | 128 | `system_config_t` payload |
| 0x0088 | 4 | CRC-32 of bytes 0x0000–0x0087 |

### 3.1 Data Structure

```c
typedef struct {
    uint32_t sensor_rates_hz[4];         /* Per-channel sample rate */
    float actuator_min_percent[2];       /* Per-actuator minimum */
    float actuator_max_percent[2];       /* Per-actuator maximum */
    uint32_t heartbeat_interval_ms;      /* Health report interval */
    uint32_t comm_timeout_ms;            /* Communication loss timeout */
    uint8_t reserved[72];               /* Future expansion */
} system_config_t;
```

## 4. Processing Logic

### 4.1 Load

1. Read 140 bytes from flash config sector
2. Verify magic number (`0x53454E54`)
3. Compute CRC-32 over bytes 0–135
4. Compare with stored CRC at offset 136
5. If match → copy to output struct
6. If mismatch → log warning, load defaults

### 4.2 Save

1. Validate config (range checks per SWE-063-1)
2. Erase flash config sector (4 KB erase)
3. Write magic + version + config data
4. Compute and write CRC-32
5. Read back and verify (defense against flash corruption)

### 4.3 Defaults

| Parameter | Default |
|-----------|---------|
| Sensor rates | 10, 10, 10, 10 Hz |
| Actuator min | 0%, 0% |
| Actuator max | 95%, 95% |
| Heartbeat interval | 1000 ms |
| Comm timeout | 3000 ms |

## 5. Traceability

| Requirement | Function |
|-------------|----------|
| SWE-058-1 | `config_store_save()` — persist config to flash |
| SWE-059-1 | `config_store_load()` — load config from flash |
| SWE-060-1 | CRC-32 validation on load |
| SWE-062-1 | `config_store_get_defaults()` — factory defaults |
| SWE-063-1 | `config_store_validate()` — range validation |
| SWE-064-1 | Flash read-back verify after write |
| SWE-065-1 | Config version field for future migration |
