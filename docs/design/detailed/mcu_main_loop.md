---
title: "Detailed Design — MCU Main Loop (FW-00)"
document_id: "DD-FW00"
project: "Sentinel Gateway"
version: "1.1.0"
date: 2026-03-18
status: "Approved"
aspice_process: "SWE.3 Software Detailed Design"
component: "FW-00"
implements: ["SWE-067-1", "SWE-027-1", "SWE-070-1"]
---

# Detailed Design — MCU Main Loop (FW-00)

## 1. Purpose

The main.c module is the entry point for MCU firmware. It executes the initialization sequence in strict order (safe state before comms) and runs the bare-metal super-loop that drives all application logic.

## 2. Implementation Reference

| Item | Value |
|------|-------|
| Source file | `src/mcu/main.c` |
| QEMU entry | `src/mcu/hal/qemu/main_qemu.c` |
| Lines | ~135 |
| CMake target | `sentinel-mcu` or `sentinel-mcu-qemu` |
| Build command | `docker compose run --rm mcu-build` |
| QEMU build | `docker compose run --rm mcu-build-qemu` |
| QEMU run | `tools/qemu/launch_mcu_fw.sh` |

## 3. Architecture

![MCU Initialization and Super-Loop](../../architecture/diagrams/mcu_init_sequence.drawio.svg)

**Critical ordering**: HAL init and config load MUST complete before TCP init. This ensures actuators are in safe state and watchdog is active before any external communication is possible.

## 4. Initialization Sequence

```c
int main(void)
{
    /* Phase 1: HAL initialization */
    systick_init();
    gpio_init();
    flash_init();

    /* Phase 2: Load configuration */
    sentinel_err_t err = config_store_load(&g_config);
    if (err != SENTINEL_OK) {
        /* Use defaults on CRC failure — config_store_load already populated defaults */
        config_store_get_defaults(&g_config);
    }

    /* Phase 3: Subsystem initialization */
    sensor_acq_init(&g_config);
    actuator_init(&g_config);
    safety_monitor_init();
    watchdog_mgr_init();
    tcp_stack_init(&g_config);

    /* Register command handler */
    tcp_stack_register_command_callback(on_command_received);

    /* LED on to indicate running */
    gpio_led_set(true);

    /* Phase 4: Enter super-loop */
    for (;;) { /* ... */ }

    return 0;  /* Never reached */
}
```

### 4.1 Initialization Order Rationale

| Phase | Modules | Why This Order |
|-------|---------|----------------|
| 1 | systick, gpio, flash | Hardware primitives needed by everything |
| 2 | config_store | Configuration needed by sensor/actuator init |
| 3a | sensor_acq, actuator | Safe state (PWM=0) established |
| 3b | safety_monitor | Can now monitor actuator state |
| 3c | watchdog_mgr | Watchdog starts — must feed within 2s |
| 3d | tcp_stack | Network init — MCU now accepts connections |

## 5. Super-Loop Design

```c
for (;;) {
    uint32_t now_ms = systick_get_ms();

    /* 1. Poll TCP stack (lwIP timers, receive processing) */
    tcp_stack_poll();

    /* 2. Update communication status */
    safety_monitor_set_tcp_connected(tcp_stack_is_connected());

    /* 3. Sample sensors if due */
    if (sensor_acq_is_due(now_ms)) {
        uint32_t count = 0U;
        err = sensor_acq_sample(g_readings, &count);
        if ((err == SENTINEL_OK) && (count > 0U)) {
            /* Encode and send sensor data */
            size_t frame_len = 0U;
            err = pb_encode_sensor_data(g_readings, count,
                                         g_config.sensor_rates_hz[0],
                                         g_frame_buf, &frame_len);
            if (err == SENTINEL_OK) {
                (void)tcp_stack_send_telemetry(g_frame_buf, frame_len);
            }
        }
    }

    /* 4. Health reporting (every heartbeat interval) */
    if ((now_ms - g_last_health_ms) >= g_config.heartbeat_interval_ms) {
        g_last_health_ms = now_ms;
        health_data_t health;
        if (safety_monitor_get_health(&health) == SENTINEL_OK) {
            size_t frame_len = 0U;
            err = pb_encode_health_status(&health, g_frame_buf, &frame_len);
            if (err == SENTINEL_OK) {
                (void)tcp_stack_send_telemetry(g_frame_buf, frame_len);
            }
        }
        /* Toggle LED as heartbeat indicator */
        gpio_led_toggle();
    }

    /* 5. Safety monitor tick */
    safety_monitor_tick(now_ms);

    /* 6. Feed watchdog — MUST be last, proves loop completed */
    watchdog_mgr_feed();
}
```

### 5.1 Loop Step Summary

| Step | Module | Action | Timing |
|------|--------|--------|--------|
| 1 | tcp_stack_poll | Process TCP/IP stack | Every iteration |
| 2 | safety_monitor | Update connection status | Every iteration |
| 3 | sensor_acq | Sample ADC if interval elapsed | Per-channel rate |
| 4 | health_reporter | Send HealthStatus if interval elapsed | Every 1000ms |
| 5 | safety_monitor | Check timeouts, state transitions | Every iteration |
| 6 | watchdog_mgr | Feed watchdog | Every iteration (LAST) |

## 6. Command Dispatch

```c
static void on_command_received(const uint8_t *frame, size_t len)
{
    if ((frame == NULL) || (len < WIRE_FRAME_HEADER_SIZE)) { return; }

    uint8_t msg_type = 0U;
    uint32_t payload_len = 0U;

    sentinel_err_t err = wire_frame_decode_header(frame, len,
                                                    &msg_type, &payload_len);
    if (err != SENTINEL_OK) { return; }

    /* Mark communication as alive */
    safety_monitor_set_comm_ok(true);

    /* Reject commands in failsafe (except state sync) */
    if (safety_monitor_is_failsafe() && (msg_type != MSG_TYPE_STATE_SYNC_REQ)) {
        return;
    }

    switch (msg_type) {
        case MSG_TYPE_ACTUATOR_CMD:   /* 0x10 */
            /* Decode, execute, send response */
            break;
        case MSG_TYPE_CONFIG_UPDATE:  /* 0x20 */
            /* Decode, apply, send response */
            break;
        case MSG_TYPE_STATE_SYNC_REQ: /* 0x40 */
            /* Recovery: transition from failsafe to normal */
            safety_monitor_set_tcp_connected(true);
            break;
        default:
            break;
    }
}
```

### 6.1 Failsafe Command Handling

| Message Type | In Normal State | In Failsafe State |
|--------------|-----------------|-------------------|
| ACTUATOR_CMD | Process | **Reject** |
| CONFIG_UPDATE | Process | **Reject** |
| STATE_SYNC_REQ | Process | Process (recovery path) |

## 7. Static Buffers

No heap allocation. All buffers are static:

```c
static system_config_t g_config;
static sensor_reading_t g_readings[SENTINEL_MAX_CHANNELS];
static uint8_t g_frame_buf[WIRE_FRAME_MAX_SIZE];
static uint32_t g_last_health_ms = 0U;
```

## 8. Timing Analysis

| Step | Worst-Case Time | Notes |
|------|-----------------|-------|
| TCP poll | 2 ms | lwIP processing |
| Sensor sample | 0.5 ms | 4-channel ADC scan |
| Sensor encode | 0.3 ms | Protobuf encoding |
| Health encode | 0.3 ms | Protobuf encoding |
| Safety tick | 0.01 ms | State checks |
| Watchdog feed | 0.01 ms | Register write |
| **Total worst-case** | **~3 ms** | Well within 2000ms watchdog |

## 9. Platform Differences

### 9.1 SysTick Clock Source

| Platform | Clock | SysTick Load |
|----------|-------|--------------|
| STM32U575 | 4 MHz (MSI default) | 3,999 for 1ms |
| MPS2-AN505 (QEMU) | 25 MHz | 24,999 for 1ms |

```c
/* From stm32u575.h */
#define SYSTEM_CLOCK_HZ  4000000UL  /* Default 4 MHz MSI */

/* From mps2_an505.h */
#define SYSTEM_CLOCK_HZ  25000000UL  /* 25 MHz on MPS2 */
```

### 9.2 Startup Assembly

| Platform | Startup File | Vector Table |
|----------|--------------|--------------|
| STM32U575 | `src/mcu/startup_stm32u575.s` | Flash at 0x08000000 |
| MPS2-AN505 | `src/mcu/startup_mps2_an505.s` | SRAM at 0x00000000 |

## 10. Shared Data Protection

| Data | Writers | Readers | Protection |
|------|---------|---------|------------|
| ADC DMA buffer | DMA ISR | main_loop | volatile + scan_complete flag |
| Sample timer flag | Timer ISR | main_loop | volatile boolean |
| SysTick counter | SysTick ISR | lwIP timers | volatile uint32_t |
| TCP rx buffer | USB ISR | main_loop | Ring buffer with head/tail |

All ISR-to-main-loop communication uses volatile flags. No critical sections needed (single reader, single writer pattern).

## 11. Dependencies

| Dependency | Include | Purpose |
|------------|---------|---------|
| sentinel_types.h | `#include "../common/sentinel_types.h"` | Types, constants |
| wire_frame.h | `#include "../common/wire_frame.h"` | Frame encoding |
| All subsystem headers | Various | Module APIs |

## 12. Implementation Lessons

1. **Config load fallback**: `config_store_load()` populates defaults on failure. Extra `config_store_get_defaults()` call is belt-and-suspenders.

2. **LED heartbeat**: LED toggle on health report provides visual indication without dedicated timer.

3. **Watchdog MUST be last**: Feeding watchdog only after all processing ensures a hung step causes reset.

4. **QEMU differences**: QEMU uses RAM-backed peripherals. Some HAL drivers have `_qemu.c` variants.

5. **No interrupts in main loop**: Bare-metal super-loop with polled I/O. Only SysTick ISR runs.

## 13. Traceability

| Requirement | Implementation | Status |
|-------------|----------------|--------|
| SWE-067-1 | Initialization sequence in `main()` | Complete |
| SWE-027-1 | Safe state (PWM=0) via `actuator_init()` | Complete |
| SWE-070-1 | `watchdog_mgr_feed()` at end of loop | Complete |
