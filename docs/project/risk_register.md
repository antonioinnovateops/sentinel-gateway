---
title: "Risk Register"
document_id: "RR-001"
project: "Sentinel Gateway"
version: "1.0.0"
date: 2026-03-17
status: "Approved"
aspice_process: "MAN.5 Risk Management"
---

# Risk Register — Sentinel Gateway

## Risk Assessment Matrix

| Likelihood | Impact: Low | Impact: Medium | Impact: High |
|------------|:-----------:|:--------------:|:------------:|
| High | Medium | High | Critical |
| Medium | Low | Medium | High |
| Low | Low | Low | Medium |

## Active Risks

### R-001: QEMU MCU Peripheral Mismatch
- **Category**: Technical
- **Likelihood**: High
- **Impact**: Medium
- **Risk Level**: High
- **Description**: QEMU `netduinoplus2` machine emulates STM32F4 peripherals, not STM32U575. ADC, Timer, and USB peripheral register maps may differ.
- **Mitigation**: Implement HAL abstraction layer that isolates hardware access. Write HAL implementations for both QEMU-available peripherals and target STM32U575 registers. Use compile-time selection.
- **Owner**: Implementation Agent
- **Status**: Open

### R-002: lwIP Stack Complexity on Bare-Metal
- **Category**: Technical
- **Likelihood**: Medium
- **Impact**: High
- **Risk Level**: High
- **Description**: lwIP TCP/IP stack on bare-metal MCU requires careful integration (interrupt handling, timer ticks, buffer management) and consumes significant RAM.
- **Mitigation**: Use lwIP's `NO_SYS` mode (no RTOS required). Pre-allocate all buffers statically. Start with minimal configuration (single TCP connection, small window). Reference lwIP bare-metal examples.
- **Owner**: Implementation Agent
- **Status**: Open

### R-003: USB CDC-ECM in QEMU
- **Category**: Technical
- **Likelihood**: Medium
- **Impact**: High
- **Risk Level**: High
- **Description**: QEMU's USB device emulation for CDC-ECM from the MCU side may be limited or require custom configuration.
- **Mitigation**: Alternative approach: use QEMU's virtual network (socket/TAP) to bridge the two VMs directly at the network layer, simulating the USB CDC-ECM link as a virtual Ethernet connection. Document the mapping from emulated to real hardware.
- **Owner**: Architecture Agent
- **Status**: Open

### R-004: Protobuf Code Generation Toolchain
- **Category**: Build
- **Likelihood**: Low
- **Impact**: Medium
- **Risk Level**: Low
- **Description**: nanopb and protobuf-c code generators may have version compatibility issues or generate non-MISRA-compliant code.
- **Mitigation**: Pin specific versions of nanopb and protobuf-c. Run MISRA analysis on generated code and document deviations as "generated code exclusions" per MISRA Compliance plan.
- **Owner**: Implementation Agent
- **Status**: Open

### R-005: AI Model Context Window Limits
- **Category**: Process
- **Likelihood**: Medium
- **Impact**: Medium
- **Risk Level**: Medium
- **Description**: Smaller AI models may not fit entire specification + code in context, leading to inconsistencies between generated code and requirements.
- **Mitigation**: Project structured with modular, self-contained files. Each phase produces clearly referenced outputs. Agent skills include explicit file-reading instructions. AGENTS.md provides sequential execution plan.
- **Owner**: Orchestrator
- **Status**: Open

### R-006: MISRA Compliance of Third-Party Libraries
- **Category**: Quality
- **Likelihood**: High
- **Impact**: Low
- **Risk Level**: Medium
- **Description**: lwIP, nanopb, and protobuf-c are not MISRA-compliant. Static analysis will flag violations in library code.
- **Mitigation**: MISRA Compliance Plan explicitly excludes third-party libraries with documented rationale. Apply MISRA analysis only to project-written code. Document library versions and known issues.
- **Owner**: Review Agent
- **Status**: Open

### R-007: Yocto Build Time
- **Category**: Process
- **Likelihood**: High
- **Impact**: Low
- **Risk Level**: Medium
- **Description**: Initial Yocto build takes 1-3 hours depending on hardware. Iterative rebuilds faster but still significant.
- **Mitigation**: Provide pre-built sstate-cache. Use `core-image-minimal` as base (smallest footprint). Document how to use Docker-based Yocto build for reproducibility.
- **Owner**: Implementation Agent
- **Status**: Open

### R-008: Test Coverage in QEMU
- **Category**: Verification
- **Likelihood**: Medium
- **Impact**: Medium
- **Risk Level**: Medium
- **Description**: Some hardware-specific behavior (ADC noise, USB enumeration timing, watchdog) may not be accurately emulated in QEMU, limiting test validity.
- **Mitigation**: Clearly mark tests as "SIL-verified" vs "requires HIL". Use mock/stub layers for hardware-specific tests. Document QEMU limitations in test reports.
- **Owner**: Verification Agent
- **Status**: Open

## Risk Summary

| Level | Count |
|-------|-------|
| Critical | 0 |
| High | 3 |
| Medium | 4 |
| Low | 1 |
| **Total** | **8** |
