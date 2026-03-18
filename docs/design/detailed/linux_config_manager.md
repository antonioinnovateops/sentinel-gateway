---
title: "Detailed Design — Linux Config Manager (SW-07)"
document_id: "DD-SW07"
project: "Sentinel Gateway"
version: "1.0.0"
date: 2026-03-18
status: "Approved"
aspice_process: "SWE.3 Software Detailed Design"
component: "SW-07"
implements: ["SWE-058-2", "SWE-061-1"]
---

# Detailed Design — Linux Config Manager (SW-07)

## 1. Purpose

Manages system configuration on the Linux side. Sends ConfigUpdate messages to MCU, processes ConfigResponse, provides configuration query interface for diagnostics.

## 2. Module Interface

```c
sentinel_err_t config_manager_init(const char *config_file_path);
sentinel_err_t config_manager_update_sensor_rate(uint8_t channel, uint32_t rate_hz);
sentinel_err_t config_manager_update_actuator_limits(uint8_t actuator, float min, float max);
sentinel_err_t config_manager_get_current(system_config_t *out);
sentinel_err_t config_manager_process_response(const ConfigResponse *msg);
```

## 3. Processing Logic

1. Load initial config from `/etc/sentinel/config.json` (or defaults)
2. On update request: validate parameters → encode ConfigUpdate protobuf → send to MCU
3. Wait for ConfigResponse (timeout 2s)
4. On success: update local config cache
5. On failure: log error, return error to caller

## 4. Validation Rules

| Parameter | Valid Range | Default |
|-----------|-----------|---------|
| Sensor rate | 1–100 Hz | 10 Hz |
| Actuator min | 0–94% | 0% |
| Actuator max | 1–95% | 95% |
| Actuator max must be > min | — | — |

## 5. Traceability

| Requirement | Function |
|-------------|----------|
| SWE-058-2 | `config_manager_update_*()` — send config updates to MCU |
| SWE-061-1 | `config_manager_get_current()` — read-back current config |
