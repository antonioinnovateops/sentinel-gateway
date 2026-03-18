---
title: "Detailed Design — MCU Sensor Acquisition (FW-01)"
document_id: "DD-FW01"
project: "Sentinel Gateway"
version: "1.1.0"
date: 2026-03-18
status: "Approved"
aspice_process: "SWE.3 Software Detailed Design"
component: "FW-01"
implements: ["SWE-002-1", "SWE-003-1", "SWE-004-1", "SWE-005-1", "SWE-007-1", "SWE-009-1", "SWE-010-1"]
---

# Detailed Design — MCU Sensor Acquisition (FW-01)

## 1. Purpose

Converts raw ADC readings to calibrated engineering units, manages per-channel sample rates, and packages data for transmission to the Linux gateway.

## 2. Implementation Reference

| Item | Value |
|------|-------|
| Source file | `src/mcu/sensor_acquisition.c` |
| Header file | `src/mcu/sensor_acquisition.h` |
| Lines | ~90 |
| CMake target | `sentinel-mcu` or `sentinel-mcu-qemu` |
| Build command | `docker compose run --rm mcu-build` |
| QEMU build | `docker compose run --rm mcu-build-qemu` |

## 3. Module Interface

### 3.1 Function Signatures (Exact)

```c
#include "../common/sentinel_types.h"
#include <stdbool.h>
#include <stddef.h>

sentinel_err_t sensor_acq_init(const system_config_t *config);

bool sensor_acq_is_due(uint32_t now_ms);

sentinel_err_t sensor_acq_sample(sensor_reading_t readings[SENTINEL_MAX_CHANNELS],
                                  uint32_t *count);

void sensor_acq_set_rate(uint8_t channel, uint32_t rate_hz);
```

### 3.2 Error Handling Contract

| Function | Returns | Condition |
|----------|---------|-----------|
| `sensor_acq_init()` | `SENTINEL_OK` | Success |
| `sensor_acq_init()` | `SENTINEL_ERR_INVALID_ARG` | NULL config |
| `sensor_acq_init()` | ADC error | `adc_init()` failed |
| `sensor_acq_is_due()` | `true`/`false` | Any channel due for sampling |
| `sensor_acq_sample()` | `SENTINEL_OK` | Success |
| `sensor_acq_sample()` | `SENTINEL_ERR_INVALID_ARG` | NULL outputs |
| `sensor_acq_sample()` | ADC error | `adc_scan_all()` failed |
| `sensor_acq_set_rate()` | void | Silent ignore if channel invalid |

## 4. Data Flow

![Sensor Acquisition Data Flow](../../architecture/diagrams/sensor_data_flow.drawio.svg)

## 5. Calibration

### 5.1 Calibration Constants

```c
/* Linear calibration: calibrated = raw * scale + offset */
static const float g_cal_scale[SENTINEL_MAX_CHANNELS] = {
    0.0806f,   /* CH0: Temperature °C (0-330°C over 0-4095) */
    0.0244f,   /* CH1: Humidity %RH (0-100 over 0-4095) */
    0.3663f,   /* CH2: Pressure hPa (300-1100 over 0-4095) */
    24.42f     /* CH3: Light lux (0-100000 over 0-4095) */
};

static const float g_cal_offset[SENTINEL_MAX_CHANNELS] = {
    -40.0f,    /* Temperature offset: maps 0 → -40°C */
    0.0f,      /* Humidity offset */
    300.0f,    /* Pressure offset: maps 0 → 300 hPa */
    0.0f       /* Light offset */
};
```

### 5.2 Sensor Channel Mapping

| Channel | Type | Unit | Range | Formula |
|---------|------|------|-------|---------|
| 0 | Temperature | °C | -40 to +290 | `raw * 0.0806 - 40` |
| 1 | Humidity | %RH | 0 to 100 | `raw * 0.0244` |
| 2 | Pressure | hPa | 300 to 1100 | `raw * 0.3663 + 300` |
| 3 | Light | lux | 0 to 100,000 | `raw * 24.42` |

**Note**: Calibration values differ from original design document due to sensor-specific tuning. Current values match ADC full-scale (12-bit, 0-4095) to sensor output range.

## 6. Sample Rate Control

### 6.1 Per-Channel Timing

```c
static uint32_t g_sample_rates_hz[SENTINEL_MAX_CHANNELS];
static uint32_t g_last_sample_ms[SENTINEL_MAX_CHANNELS];
```

Each channel tracks its own last sample time and configured rate.

### 6.2 Due Calculation

```c
bool sensor_acq_is_due(uint32_t now_ms)
{
    for (uint8_t ch = 0U; ch < SENTINEL_MAX_CHANNELS; ch++) {
        if (g_sample_rates_hz[ch] == 0U) {
            continue;  /* Channel disabled */
        }
        uint32_t interval = 1000U / g_sample_rates_hz[ch];
        if ((now_ms - g_last_sample_ms[ch]) >= interval) {
            return true;
        }
    }
    return false;
}
```

### 6.3 Rate Update

```c
void sensor_acq_set_rate(uint8_t channel, uint32_t rate_hz)
{
    if (channel < SENTINEL_MAX_CHANNELS) {
        g_sample_rates_hz[channel] = rate_hz;
    }
}
```

Valid rates: 1-100 Hz. Rate of 0 disables the channel.

## 7. Sampling Logic

```c
sentinel_err_t sensor_acq_sample(sensor_reading_t readings[SENTINEL_MAX_CHANNELS],
                                  uint32_t *count)
{
    if ((readings == NULL) || (count == NULL)) {
        return SENTINEL_ERR_INVALID_ARG;
    }

    uint32_t now_ms = systick_get_ms();
    uint32_t raw_values[SENTINEL_MAX_CHANNELS];
    *count = 0U;

    /* Scan all ADC channels */
    sentinel_err_t err = adc_scan_all(raw_values);
    if (err != SENTINEL_OK) {
        return err;
    }

    for (uint8_t ch = 0U; ch < SENTINEL_MAX_CHANNELS; ch++) {
        if (g_sample_rates_hz[ch] == 0U) {
            continue;
        }

        uint32_t interval = 1000U / g_sample_rates_hz[ch];
        if ((now_ms - g_last_sample_ms[ch]) >= interval) {
            readings[*count].channel = ch;
            readings[*count].raw_value = raw_values[ch];
            readings[*count].calibrated_value =
                ((float)raw_values[ch] * g_cal_scale[ch]) + g_cal_offset[ch];
            readings[*count].timestamp_ms = (uint64_t)now_ms;
            g_last_sample_ms[ch] = now_ms;
            (*count)++;
        }
    }
    return SENTINEL_OK;
}
```

### 7.1 Output Format

```c
/* From sentinel_types.h */
typedef struct {
    uint8_t  channel;
    uint32_t raw_value;
    float    calibrated_value;
    uint64_t timestamp_ms;
} sensor_reading_t;
```

## 8. Dependencies

| Dependency | Include | Purpose |
|------------|---------|---------|
| sentinel_types.h | `#include "../common/sentinel_types.h"` | Types |
| adc_driver.h | `#include "hal/adc_driver.h"` | ADC hardware |
| systick.h | `#include "hal/systick.h"` | Timestamps |
| stddef.h | `#include <stddef.h>` | NULL definition |

## 9. HAL Abstraction

### 9.1 ADC Driver Interface

```c
/* From adc_driver.h */
sentinel_err_t adc_init(void);
sentinel_err_t adc_scan_all(uint32_t values[SENTINEL_MAX_CHANNELS]);
```

### 9.2 QEMU Implementation

QEMU variant (`src/mcu/hal/qemu/adc_driver_qemu.c`) returns simulated values:
- Temperature: varies with simulated time
- Other channels: fixed test values

## 10. Implementation Lessons

1. **Required includes**: Added `<stddef.h>` for NULL definition during build integration.

2. **Calibration differs from spec**: Original design used different formulas. Current implementation uses linear scale+offset matching actual sensor datasheets.

3. **All channels scanned together**: `adc_scan_all()` reads all 4 channels in one call, even if only some are due. This simplifies ADC sequencing.

4. **Output array size**: Caller must provide array of `SENTINEL_MAX_CHANNELS` (4) elements, even though `count` may be less.

5. **Timestamp in milliseconds**: Uses `systick_get_ms()` cast to uint64_t for protobuf compatibility.

## 11. Traceability

| Requirement | Implementation | Status |
|-------------|----------------|--------|
| SWE-002-1 | Temperature calibration (scale 0.0806, offset -40) | Complete |
| SWE-003-1 | Humidity calibration (scale 0.0244, offset 0) | Complete |
| SWE-004-1 | Pressure calibration (scale 0.3663, offset 300) | Complete |
| SWE-005-1 | Light calibration (scale 24.42, offset 0) | Complete |
| SWE-007-1 | `sensor_acq_sample()` — produce sensor_reading_t | Complete |
| SWE-009-1 | `sensor_acq_set_rate()` — per-channel rate control | Complete |
| SWE-010-1 | Rate range validation (1-100 Hz) | In config_store |
