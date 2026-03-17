---
title: "SWE to Architecture Component Traceability"
document_id: "TRACE-003"
project: "Sentinel Gateway"
version: "1.0.0"
date: 2026-03-17
status: "Approved"
aspice_process: "SUP.8 Configuration Management"
---

# Software Requirements to Architecture Traceability

This matrix maps each software requirement (SWRS-001) to its implementing architecture component (SWAD-001).

## MCU Firmware Components

| SWE Requirement | Component | Module | SWAD Section |
|----------------|-----------|--------|-------------|
| SWE-001-1 | FW-01 | sensor_acquisition | 2.2.1 |
| SWE-001-2 | FW-01 | sensor_acquisition | 2.2.1 |
| SWE-001-3 | FW-01 | sensor_acquisition | 2.2.1 |
| SWE-002-1 | FW-01 | sensor_acquisition | 2.2.1 |
| SWE-003-1 | FW-01 | sensor_acquisition | 2.2.1 |
| SWE-004-1 | FW-01 | sensor_acquisition | 2.2.1 |
| SWE-005-1 | FW-01 | sensor_acquisition | 2.2.1 |
| SWE-007-1 | FW-01 | sensor_acquisition | 2.2.1 |
| SWE-007-2 | FW-01 | sensor_acquisition | 2.2.1 |
| SWE-008-1 | FW-01 | sensor_acquisition | 2.2.1 |
| SWE-009-1 | FW-01 | sensor_acquisition | 2.2.1 |
| SWE-010-1 | FW-01 | sensor_acquisition | 2.2.1 |
| SWE-011-1 | FW-01 | sensor_acquisition (HAL: adc_driver) | 2.2.1, 2.3 |
| SWE-016-1 | FW-02 | actuator_control (HAL: pwm_driver) | 2.2.2, 2.3 |
| SWE-016-2 | FW-02 | actuator_control | 2.2.2 |
| SWE-017-1 | FW-02 | actuator_control (HAL: pwm_driver) | 2.2.2, 2.3 |
| SWE-018-1 | FW-02 | actuator_control | 2.2.2 |
| SWE-019-1 | FW-02 | actuator_control + FW-03 protobuf_handler | 2.2.2, 2.2.3 |
| SWE-021-1 | FW-02 | actuator_control | 2.2.2 |
| SWE-022-1 | FW-02 | actuator_control | 2.2.2 |
| SWE-022-2 | FW-02 | actuator_control | 2.2.2 |
| SWE-023-1 | FW-02 | actuator_control | 2.2.2 |
| SWE-024-1 | FW-02 | actuator_control | 2.2.2 |
| SWE-026-1 | FW-07 | safety_monitor + FW-02 actuator_control | 2.2.7, 2.2.2 |
| SWE-027-1 | FW-02 | actuator_control | 2.2.2 |
| SWE-028-1 | FW-07 | safety_monitor | 2.2.7 |
| SWE-029-1 | FW-07 | safety_monitor | 2.2.7 |
| SWE-030-1 | FW-04 | communication_manager (HAL: usb_cdc) | 2.2.4, 2.3 |
| SWE-031-1 | FW-04 | communication_manager | 2.2.4 |
| SWE-032-1 | FW-04 | communication_manager | 2.2.4 |
| SWE-033-1 | FW-04 | communication_manager | 2.2.4 |
| SWE-035-1 | FW-03 | protobuf_handler | 2.2.3 |
| SWE-035-2 | FW-03 | protobuf_handler | 2.2.3 |
| SWE-036-1 | FW-03 | protobuf_handler | 2.2.3 |
| SWE-037-1 | FW-03 | protobuf_handler | 2.2.3 |
| SWE-038-1 | FW-07 | safety_monitor | 2.2.7 |
| SWE-039-1 | FW-07 | safety_monitor | 2.2.7 |
| SWE-040-1 | FW-07 | safety_monitor | 2.2.7 |
| SWE-041-1 | FW-07 | safety_monitor | 2.2.7 |
| SWE-043-1 | FW-04 | communication_manager | 2.2.4 |
| SWE-058-1 | FW-06 | configuration_store (HAL: flash_driver) | 2.2.6, 2.3 |
| SWE-059-1 | FW-06 | configuration_store | 2.2.6 |
| SWE-060-1 | FW-06 | configuration_store | 2.2.6 |
| SWE-062-1 | FW-06 | configuration_store | 2.2.6 |
| SWE-063-1 | FW-06 | configuration_store | 2.2.6 |
| SWE-064-1 | FW-06 | configuration_store | 2.2.6 |
| SWE-065-1 | FW-06 | configuration_store | 2.2.6 |
| SWE-067-1 | FW-05 + all | main_loop (init sequence) | 2.2.5 |
| SWE-069-1 | FW-05 | watchdog_manager (HAL: watchdog_driver) | 2.2.5, 2.3 |
| SWE-070-1 | FW-05 | watchdog_manager | 2.2.5 |
| SWE-071-1 | FW-05 | watchdog_manager | 2.2.5 |
| SWE-072-1 | All FW | All modules (compile-time constraint) | 2.1 |
| SWE-073-1 | All FW | All modules (linker script constraint) | 2.1 |

## Linux Gateway Components

| SWE Requirement | Component | Module | SWAD Section |
|----------------|-----------|--------|-------------|
| SWE-013-1 | SW-03 | sensor_proxy | 3.2.3 |
| SWE-013-2 | SW-03 | sensor_proxy | 3.2.3 |
| SWE-015-1 | SW-03 | sensor_proxy | 3.2.3 |
| SWE-018-2 | SW-04 | actuator_proxy | 3.2.4 |
| SWE-019-2 | SW-04 | actuator_proxy | 3.2.4 |
| SWE-035-3 | SW-02 | protobuf_handler (tcp_transport) | 3.2.2 |
| SWE-035-4 | SW-02 | protobuf_handler (tcp_transport) | 3.2.2 |
| SWE-036-2 | SW-02 | protobuf_handler | 3.2.2 |
| SWE-042-1 | SW-05 | health_monitor | 3.2.5 |
| SWE-043-2 | SW-05 | health_monitor (tcp_transport) | 3.2.5 |
| SWE-044-1 | SW-05 | health_monitor | 3.2.5 |
| SWE-045-1 | SW-05 | health_monitor | 3.2.5 |
| SWE-046-1 | SW-01 | gateway_core | 3.2.1 |
| SWE-046-2 | SW-06 | diagnostic_server | 3.2.6 |
| SWE-047-1 | SW-06 | diagnostic_server | 3.2.6 |
| SWE-048-1 | SW-06 | diagnostic_server | 3.2.6 |
| SWE-049-1 | SW-06 | diagnostic_server | 3.2.6 |
| SWE-050-1 | SW-06 | diagnostic_server | 3.2.6 |
| SWE-051-1 | SW-06 | diagnostic_server | 3.2.6 |
| SWE-052-1 | SW-01 | logger | 3.2.1 |
| SWE-053-1 | SW-01 | logger | 3.2.1 |
| SWE-054-1 | SW-01 | logger | 3.2.1 |
| SWE-055-1 | SW-01 | logger | 3.2.1 |
| SWE-057-1 | SW-06 | diagnostic_server | 3.2.6 |
| SWE-058-2 | SW-07 | config_manager | 3.2.7 |
| SWE-061-1 | SW-07 | config_manager | 3.2.7 |
| SWE-066-1 | SW-01 | gateway_core | 3.2.1 |

## Coverage Summary

| Target | Total Requirements | Allocated to Component | Coverage |
|--------|-------------------|----------------------|----------|
| MCU Firmware | 53 | 53 | 100% |
| Linux Gateway | 27 | 27 | 100% |
| **Total** | **80** | **80** | **100%** |

All 80 software requirements are allocated to at least one architecture component. No orphan requirements exist.
