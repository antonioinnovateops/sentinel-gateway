---
title: "Detailed Design — MCU Main Loop"
document_id: "DD-MCU-001"
project: "Sentinel Gateway"
version: "1.0.0"
date: 2026-03-17
status: "Approved"
aspice_process: "SWE.3 Software Detailed Design"
component: "MCU / main_loop"
requirements: ["SWE-067-1", "SWE-027-1", "SWE-070-1"]
---

# Detailed Design — MCU Main Loop

## 1. Purpose

The main_loop module is the entry point for MCU firmware. It executes the initialization sequence in a strict order (safe state before comms) and runs the super-loop that drives all application logic.

## 2. Initialization Sequence

![MCU Initialization and Super-Loop](../../architecture/diagrams/mcu_init_sequence.drawio.svg)

**Critical ordering**: Steps 2-3 MUST complete before step 6 (USB init). This ensures actuators are in safe state and watchdog is active before any external communication is possible.

## 3. Super-Loop Design

```c
static void super_loop(void)
{
    while (true)  /* MISRA: infinite loop is intentional */
    {
        /* [A] Poll TCP stack — process incoming/outgoing TCP data */
        tcp_stack_poll();

        /* [B] Process received commands (if any) */
        if (tcp_stack_has_command())
        {
            uint8_t cmd_buf[512];
            size_t cmd_len;
            uint8_t resp_buf[512];
            size_t resp_len;

            tcp_stack_recv_command(cmd_buf, &cmd_len);
            actuator_ctrl_process_command(cmd_buf, cmd_len, resp_buf, &resp_len);
            tcp_stack_send_response(resp_buf, resp_len);

            health_reporter_gateway_activity();  /* Reset comm timeout */
        }

        /* [C] Process sensor data (if ADC scan complete) */
        if (adc_driver_scan_complete())
        {
            sensor_acq_process();

            /* Encode and send SensorData */
            uint8_t tx_buf[256];
            size_t tx_len;
            sensor_acq_encode_message(tx_buf, &tx_len);
            tcp_stack_send_telemetry(MSG_TYPE_SENSOR_DATA, tx_buf, tx_len);
        }

        /* [D] Send heartbeat (if 1s timer elapsed) */
        if (health_reporter_heartbeat_due())
        {
            health_reporter_send_heartbeat();
        }

        /* [E] Check communication timeout */
        if (health_reporter_is_comm_timeout())
        {
            actuator_ctrl_enter_safe_state(FAILSAFE_COMM_LOSS);
            health_reporter_set_state(MCU_STATE_FAILSAFE);
        }

        /* [F] Feed watchdog — MUST be last, proves loop completed */
        watchdog_mgr_feed();  /* ← SWE-070-1 */
    }
}
```

## 4. Timing Analysis

| Step | Worst-Case Time | Notes |
|------|-----------------|-------|
| A: TCP poll | 2 ms | lwIP processing |
| B: Command process | 1 ms | Decode + validate + PWM write |
| C: Sensor process | 0.5 ms | Calibrate + encode 4 channels |
| D: Heartbeat | 0.5 ms | Encode + send HealthStatus |
| E: Comm check | 0.01 ms | Timer comparison |
| F: Watchdog feed | 0.01 ms | Register write |
| **Total worst-case** | **~4 ms** | Well within 500ms WDG limit |

## 5. Shared Data Protection

| Data | Writers | Readers | Protection |
|------|---------|---------|------------|
| ADC DMA buffer | DMA ISR | main_loop | volatile + scan_complete flag |
| Sample timer flag | Timer ISR | main_loop | volatile boolean |
| SysTick counter | SysTick ISR | lwIP timers | volatile uint32_t |
| TCP rx buffer | USB ISR | main_loop | Ring buffer with head/tail |

All ISR-to-main-loop communication uses volatile flags. No critical sections needed (single reader, single writer pattern).
