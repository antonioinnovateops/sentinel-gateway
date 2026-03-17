---
title: "Detailed Design — MCU Sensor Acquisition"
document_id: "DD-MCU-003"
project: "Sentinel Gateway"
version: "1.0.0"
date: 2026-03-17
status: "Approved"
aspice_process: "SWE.3 Software Detailed Design"
component: "MCU / sensor_acquisition"
requirements: ["SWE-002-1", "SWE-003-1", "SWE-004-1", "SWE-005-1", "SWE-007-1", "SWE-009-1", "SWE-010-1"]
---

# Detailed Design — MCU Sensor Acquisition

## 1. Purpose

Convert raw ADC readings to calibrated engineering units and package them as protobuf SensorData messages for transmission to the Linux gateway.

## 2. Data Flow

```
Timer ISR (per-channel rate)
    │
    ▼
adc_driver_start_scan()
    │
    ▼ (DMA complete)
adc_driver_scan_complete() → true
    │
    ▼
sensor_acq_process()
    ├── read raw values: adc_driver_get_raw()
    ├── calibrate each channel: calibration_table[ch](raw)
    ├── update latest_readings[] cache
    └── encode: sensor_acq_encode_message()
```

## 3. Calibration Functions

```c
/** Calibration function type — converts raw ADC to engineering units */
typedef float (*calibration_fn_t)(uint16_t raw_value);

/**
 * @brief Temperature calibration: 0V=-40°C, 3.3V=+125°C
 * @implements [SWE-002-1]
 * Formula: T = (raw / 4095.0) * 165.0 - 40.0
 */
static float calibrate_temperature(uint16_t raw)
{
    return (((float)raw / 4095.0f) * 165.0f) - 40.0f;
}

/**
 * @brief Humidity calibration: 0V=0%RH, 3.3V=100%RH
 * @implements [SWE-003-1]
 * Formula: RH = (raw / 4095.0) * 100.0
 */
static float calibrate_humidity(uint16_t raw)
{
    return ((float)raw / 4095.0f) * 100.0f;
}

/**
 * @brief Pressure calibration: 0V=300hPa, 3.3V=1100hPa
 * @implements [SWE-004-1]
 * Formula: P = 300.0 + (raw / 4095.0) * 800.0
 */
static float calibrate_pressure(uint16_t raw)
{
    return 300.0f + (((float)raw / 4095.0f) * 800.0f);
}

/**
 * @brief Light calibration: 0V=0lux, 3.3V=100000lux
 * @implements [SWE-005-1]
 * Formula: L = (raw / 4095.0) * 100000.0
 */
static float calibrate_light(uint16_t raw)
{
    return ((float)raw / 4095.0f) * 100000.0f;
}

/** Calibration lookup table indexed by sensor_channel_t */
static const calibration_fn_t g_calibration[SENSOR_CH_COUNT] = {
    [SENSOR_CH_TEMPERATURE] = calibrate_temperature,
    [SENSOR_CH_HUMIDITY]    = calibrate_humidity,
    [SENSOR_CH_PRESSURE]    = calibrate_pressure,
    [SENSOR_CH_LIGHT]       = calibrate_light,
};

/** Unit strings indexed by sensor_channel_t */
static const char *g_unit_strings[SENSOR_CH_COUNT] = {
    [SENSOR_CH_TEMPERATURE] = "\xC2\xB0""C",  /* °C in UTF-8 */
    [SENSOR_CH_HUMIDITY]    = "%RH",
    [SENSOR_CH_PRESSURE]    = "hPa",
    [SENSOR_CH_LIGHT]       = "lux",
};
```

## 4. Module State

```c
typedef struct {
    sensor_reading_t latest[SENSOR_CH_COUNT];  /** Cached latest readings */
    sensor_config_t  config[SENSOR_CH_COUNT];  /** Per-channel config */
    uint32_t         sequence_number;           /** Global sequence counter */
    uint64_t         boot_time_us;              /** Timestamp offset */
} sensor_acq_context_t;

static sensor_acq_context_t g_sensor_ctx;
```

## 5. Sample Rate Timer Design

Each channel has an independent software downcounter driven by SysTick (1 kHz):

```c
static uint32_t g_sample_counters[SENSOR_CH_COUNT];   /** Downcounters */
static uint32_t g_sample_periods[SENSOR_CH_COUNT];    /** Reload values */

/**
 * @brief Update sample rate for a channel
 * @implements [SWE-009-1]
 */
int32_t sensor_acq_set_rate(sensor_channel_t channel, uint32_t rate_hz)
{
    int32_t result = SENTINEL_ERR_RANGE;

    if ((channel < SENSOR_CH_COUNT) && (rate_hz >= 1U) && (rate_hz <= 100U))
    {
        /* Period in ms = 1000 / rate_hz */
        g_sample_periods[channel] = 1000U / rate_hz;
        g_sample_counters[channel] = g_sample_periods[channel];
        g_sensor_ctx.config[channel].sample_rate_hz = rate_hz;
        result = SENTINEL_OK;
    }

    return result;
}
```

## 6. Message Construction

```c
/**
 * @brief Encode current sensor readings as protobuf SensorData
 * @implements [SWE-007-1]
 *
 * Populates: header (seq, timestamp, version), readings[], sample_rate
 * Uses nanopb for encoding into static buffer
 */
int32_t sensor_acq_encode_message(uint8_t *buffer, size_t *length)
{
    sentinel_SensorData msg = sentinel_SensorData_init_zero;

    /* Header */
    msg.header.sequence_number = g_sensor_ctx.sequence_number++;
    msg.header.timestamp_us = hal_get_time_us() - g_sensor_ctx.boot_time_us;
    msg.header.version.major = 1U;
    msg.header.version.minor = 0U;
    msg.has_header = true;

    /* Readings — one per enabled channel */
    msg.readings_count = 0U;
    for (uint32_t ch = 0U; ch < SENSOR_CH_COUNT; ch++)
    {
        if (g_sensor_ctx.config[ch].enabled)
        {
            sentinel_SensorReading *r = &msg.readings[msg.readings_count];
            r->channel = (sentinel_SensorChannel)ch;
            r->raw_value = g_sensor_ctx.latest[ch].raw_value;
            r->calibrated_value = g_sensor_ctx.latest[ch].calibrated_value;
            strncpy(r->unit, g_unit_strings[ch], sizeof(r->unit) - 1U);
            msg.readings_count++;
        }
    }

    /* Encode */
    pb_ostream_t stream = pb_ostream_from_buffer(buffer, 256U);
    bool ok = pb_encode(&stream, sentinel_SensorData_fields, &msg);

    if (ok)
    {
        *length = stream.bytes_written;
        return SENTINEL_OK;
    }
    return SENTINEL_ERR_INTERNAL;
}
```
