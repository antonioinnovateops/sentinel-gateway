---
title: "System → Software Requirements Traceability"
document_id: "TRACE-002"
project: "Sentinel Gateway"
version: "1.0.0"
date: 2026-03-17
status: "Approved"
aspice_process: "SWE.1 BP5 — Establish traceability"
---

# System → Software Requirements Traceability

| SYS Req | SWE Req(s) | Component(s) | Coverage |
|---------|-----------|--------------|----------|
| SYS-001 | SWE-001-1, SWE-001-2 | MCU/adc_driver | ✅ |
| SYS-002 | SWE-001-2, SWE-002-1 | MCU/adc_driver, MCU/sensor_acquisition | ✅ |
| SYS-003 | SWE-001-2, SWE-003-1 | MCU/adc_driver, MCU/sensor_acquisition | ✅ |
| SYS-004 | SWE-001-2, SWE-004-1 | MCU/adc_driver, MCU/sensor_acquisition | ✅ |
| SYS-005 | SWE-001-2, SWE-005-1 | MCU/adc_driver, MCU/sensor_acquisition | ✅ |
| SYS-006 | SWE-001-1 | MCU/adc_driver | ✅ |
| SYS-007 | SWE-007-1, SWE-007-2 | MCU/sensor_acquisition, MCU/protobuf_handler | ✅ |
| SYS-008 | SWE-008-1 | MCU/tcp_stack | ✅ |
| SYS-009 | SWE-009-1, SWE-001-3 | MCU/sensor_acquisition, MCU/adc_driver | ✅ |
| SYS-010 | SWE-010-1, SWE-001-3 | MCU/sensor_acquisition | ✅ |
| SYS-011 | SWE-011-1 | MCU/adc_driver | ✅ |
| SYS-012 | SWE-009-1 | MCU/sensor_acquisition | ✅ |
| SYS-013 | SWE-013-1, SWE-013-2 | LINUX/sensor_proxy, LINUX/logger | ✅ |
| SYS-014 | SWE-013-2 | LINUX/logger | ✅ |
| SYS-015 | SWE-015-1 | LINUX/logger | ✅ |
| SYS-016 | SWE-016-1, SWE-016-2 | MCU/pwm_driver, MCU/actuator_control | ✅ |
| SYS-017 | SWE-017-1 | MCU/pwm_driver | ✅ |
| SYS-018 | SWE-018-1, SWE-018-2 | MCU/protobuf_handler, LINUX/actuator_proxy | ✅ |
| SYS-019 | SWE-019-1, SWE-019-2 | MCU/actuator_control, LINUX/actuator_proxy | ✅ |
| SYS-020 | SWE-016-2 | MCU/actuator_control, MCU/pwm_driver | ✅ |
| SYS-021 | SWE-021-1 | MCU/actuator_control | ✅ |
| SYS-022 | SWE-022-1, SWE-022-2 | MCU/actuator_control | ✅ |
| SYS-023 | SWE-023-1 | MCU/actuator_control | ✅ |
| SYS-024 | SWE-024-1 | MCU/actuator_control | ✅ |
| SYS-025 | SWE-059-1 | MCU/config_store | ✅ |
| SYS-026 | SWE-026-1 | MCU/actuator_control | ✅ |
| SYS-027 | SWE-027-1, SWE-016-1, SWE-017-1, SWE-067-1 | MCU/main_loop, MCU/pwm_driver | ✅ |
| SYS-028 | SWE-028-1 | MCU/protobuf_handler, MCU/actuator_control | ✅ |
| SYS-029 | SWE-029-1 | MCU/health_reporter | ✅ |
| SYS-030 | SWE-030-1 | MCU/usb_cdc | ✅ |
| SYS-031 | SWE-031-1 | MCU/tcp_stack | ✅ |
| SYS-032 | SWE-032-1, SWE-046-1 | MCU/tcp_stack, LINUX/gateway_core | ✅ |
| SYS-033 | SWE-033-1, SWE-046-1 | MCU/tcp_stack, LINUX/gateway_core | ✅ |
| SYS-034 | SWE-007-2, SWE-018-1, SWE-018-2 | MCU/protobuf_handler, LINUX/actuator_proxy | ✅ |
| SYS-035 | SWE-035-1, SWE-035-2, SWE-035-3, SWE-035-4 | MCU/tcp_stack, LINUX/tcp_transport | ✅ |
| SYS-036 | SWE-036-1, SWE-036-2 | MCU/protobuf_handler, LINUX/sensor_proxy | ✅ |
| SYS-037 | SWE-037-1 | MCU/protobuf_handler | ✅ |
| SYS-038 | SWE-038-1 | MCU/health_reporter | ✅ |
| SYS-039 | SWE-039-1 | LINUX/health_monitor | ✅ |
| SYS-040 | SWE-040-1 | MCU/health_reporter | ✅ |
| SYS-041 | SWE-041-1 | MCU/health_reporter | ✅ |
| SYS-042 | SWE-042-1 | LINUX/health_monitor | ✅ |
| SYS-043 | SWE-043-1, SWE-043-2 | LINUX/gateway_core, MCU/tcp_stack | ✅ |
| SYS-044 | SWE-044-1 | LINUX/gateway_core | ✅ |
| SYS-045 | SWE-045-1 | LINUX/health_monitor | ✅ |
| SYS-046 | SWE-046-1, SWE-046-2 | LINUX/gateway_core, LINUX/diagnostics | ✅ |
| SYS-047 | SWE-047-1 | LINUX/diagnostics | ✅ |
| SYS-048 | SWE-048-1 | LINUX/diagnostics | ✅ |
| SYS-049 | SWE-049-1 | LINUX/diagnostics | ✅ |
| SYS-050 | SWE-050-1 | LINUX/diagnostics | ✅ |
| SYS-051 | SWE-051-1 | LINUX/diagnostics, LINUX/health_monitor | ✅ |
| SYS-052 | SWE-052-1 | LINUX/logger | ✅ |
| SYS-053 | SWE-053-1 | LINUX/logger | ✅ |
| SYS-054 | SWE-054-1 | LINUX/diagnostics | ✅ |
| SYS-055 | SWE-055-1 | LINUX/logger | ✅ |
| SYS-056 | SWE-050-1 | LINUX/diagnostics | ✅ |
| SYS-057 | SWE-057-1 | LINUX/gateway_core | ✅ |
| SYS-058 | SWE-058-1, SWE-058-2 | MCU/config_store, LINUX/config_manager | ✅ |
| SYS-059 | SWE-059-1 | MCU/config_store | ✅ |
| SYS-060 | SWE-060-1 | MCU/config_store | ✅ |
| SYS-061 | SWE-061-1 | LINUX/config_manager | ✅ |
| SYS-062 | SWE-062-1 | MCU/config_store | ✅ |
| SYS-063 | SWE-063-1 | MCU/config_store | ✅ |
| SYS-064 | SWE-064-1 | MCU/config_store | ✅ |
| SYS-065 | SWE-065-1, SWE-060-1 | MCU/config_store | ✅ |
| SYS-066 | SWE-066-1 | LINUX/gateway_core | ✅ |
| SYS-067 | SWE-067-1 | MCU/main_loop | ✅ |
| SYS-068 | SWE-008-1, SWE-033-1 | MCU/tcp_stack | ✅ |
| SYS-069 | SWE-069-1 | MCU/watchdog_mgr | ✅ |
| SYS-070 | SWE-070-1 | MCU/main_loop, MCU/watchdog_mgr | ✅ |
| SYS-071 | SWE-071-1 | MCU/watchdog_mgr | ✅ |
| SYS-072 | SWE-072-1 | MCU/all modules | ✅ |
| SYS-073 | SWE-073-1 | MCU/all modules | ✅ |
| SYS-074 | (Process req — not software) | — | N/A |
| SYS-075 | (Process req — not software) | — | N/A |
| SYS-076 | (Process req — not software) | — | N/A |
| SYS-077 | (Process req — not software) | — | N/A |
| SYS-078 | (Process req — not software) | — | N/A |
| SYS-079 | (Process req — not software) | — | N/A |
| SYS-080 | (Process req — not software) | — | N/A |

## Summary

- **Total system requirements**: 80
- **Software-relevant requirements**: 73 (SYS-001 to SYS-073)
- **Process-only requirements**: 7 (SYS-074 to SYS-080)
- **Total SWE requirements generated**: 85
- **Orphaned SYS requirements**: 0
- **Coverage**: 100%
