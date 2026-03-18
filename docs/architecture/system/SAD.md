---
title: "System Architecture Document"
document_id: "SAD-001"
project: "Sentinel Gateway"
system: "Sentinel Gateway Dual-Processor System"
version: "2.0.0"
date: 2026-03-18
status: "Approved"
asil_level: "ASIL-B (process rigor)"
aspice_process: "SYS.3 System Architectural Design"
authors: ["AI Architecture Agent"]
reviewers: ["Antonio Stepien"]
---

# System Architecture Document — Sentinel Gateway

## 1. Introduction

### 1.1 Purpose

This document describes the system architecture of the Sentinel Gateway, defining the system elements, their interfaces, and allocation of system requirements to hardware and software components.

**ASPICE Compliance**:
- SYS.3 BP1: Develop system architectural design (static + dynamic aspects)
- SYS.3 BP2: Allocate system requirements to system elements
- SYS.3 BP3: Define interfaces of system elements
- SYS.3 BP4: Describe dynamic behavior
- SYS.3 BP5: Evaluate alternative architectures
- SYS.3 BP6: Establish traceability

### 1.2 Scope

This architecture covers the complete Sentinel Gateway system operating in a QEMU SIL environment, including:
- Linux gateway application processor (QEMU ARM64)
- STM32U575 microcontroller (QEMU ARM)
- USB CDC-ECM communication link
- Protobuf messaging protocol
- Sensor and actuator interfaces (emulated)

### 1.3 Reference Documents

| Document ID | Title | Version |
|-------------|-------|---------|
| PROD-SPEC-001 | Product Specification | v1.0.0 |
| SRS-001 | System Requirements Specification | v1.0.0 |
| STKH-REQ-001 | Stakeholder Requirements | v1.0.0 |

---

## 2. Architectural Overview

### 2.1 System Context

![System Architecture Overview](../diagrams/system_overview.drawio.svg)

### 2.2 System Decomposition

![System Element Decomposition](../diagrams/system_decomposition.drawio.svg)

---

## 3. System Element Descriptions

### 3.1 SE-01: Linux Gateway Subsystem

**Purpose**: High-level data processing, logging, diagnostics, and system orchestration

**Hardware** (emulated):
- CPU: ARM Cortex-A53 (QEMU `virt` machine, aarch64)
- RAM: 512 MB
- Storage: Virtual disk (ext4 filesystem)
- Interfaces: Virtual USB host controller, virtual Ethernet

**Operating System**: Yocto Linux (Poky, Scarthgap release)
- Kernel: Linux 6.x (LTS)
- Init: systemd
- Custom layer: `meta-sentinel` with gateway application recipe

**Application**: Single-process daemon (`sentinel-gw`) with event-driven architecture
- Main event loop: epoll-based multiplexing of TCP sockets
- Threads: Main thread (event loop) + Log writer thread (async I/O)
- Memory: Dynamic allocation permitted (Linux side), bounded by design

### 3.2 SE-02: MCU Subsystem

**Purpose**: Real-time sensor acquisition, actuator control, and safety-critical I/O management

**Hardware** (emulated):
- MCU: STM32U575 (ARM Cortex-M33, 160 MHz)
- RAM: 786 KB SRAM (static allocation only)
- Flash: 2 MB internal (firmware + NVM configuration)
- Peripherals: 4x ADC channels, 2x PWM timers, USB OTG FS, IWDG

**Firmware**: Bare-metal C (no RTOS)
- Main loop: Super-loop architecture with timer-driven scheduling
- Interrupt handlers: ADC complete, USB, Timer, Watchdog
- Memory model: All static/stack, zero heap usage

### 3.3 SE-03: Communication Link

**Purpose**: Reliable, structured data exchange between Linux and MCU

**Stack**:

![Communication Protocol Stack](../diagrams/comm_stack.drawio.svg)

**Port Allocation**:

| Port | Direction | Protocol | Purpose |
|------|-----------|----------|---------|
| 5000 | Linux → MCU | TCP | Commands (ActuatorCommand, ConfigUpdate, DiagnosticRequest) |
| 5001 | MCU → Linux | TCP | Telemetry (SensorData, HealthStatus) |
| 5002 | External → Linux | TCP | Diagnostic CLI (plain-text commands) |

### 3.4 SE-04: QEMU SIL Environment

**Purpose**: Provide a fully emulated environment for development and testing without physical hardware

**IMPORTANT: Implementation Note (v2.0)**
The original v1.0 design called for full system emulation (netduinoplus2 + virt machines with TAP bridging). This was abandoned in favor of **QEMU user-mode emulation** because:
1. lwIP TCP stack on bare-metal MCU was too complex to integrate within time constraints
2. QEMU USB CDC-ECM emulation is incomplete
3. TAP/bridge setup requires root privileges, complicating CI/CD

**Implemented Architecture**:
- **Linux Gateway**: Native x86_64 ELF (no QEMU needed for SIL)
- **MCU Firmware**: ARM Linux ELF via `qemu-arm-static` user-mode emulation
- **Network**: Direct host TCP (127.0.0.1) — no USB CDC-ECM emulation
- **TCP Stack**: POSIX sockets (not lwIP)

**Build Targets**:
```bash
# Linux gateway (native)
cmake -DBUILD_LINUX=ON

# MCU for QEMU user-mode (not bare-metal)
cmake -DBUILD_QEMU_MCU=ON
qemu-arm-static ./sentinel-mcu-qemu
```

**Trade-offs**:
- ✓ Same application logic as real MCU (sensor/actuator/health modules)
- ✓ Full TCP/wire frame testing
- ✗ No USB enumeration testing (requires real hardware)
- ✗ No lwIP stack testing (uses POSIX sockets)
- ✗ No watchdog reset testing (QEMU user-mode has no watchdog)

See `docs/design/sil_environment.md` for complete SIL architecture details.

---

## 4. Interface Definitions

### 4.1 IF-01: USB CDC-ECM (SE-01 ↔ SE-02)

| Property | Value |
|----------|-------|
| Physical | USB 2.0 Full Speed (emulated) |
| Protocol | CDC-ECM (Ethernet over USB) |
| Linux side | `usb0` network interface, 192.168.7.1/24 |
| MCU side | USB device mode, 192.168.7.2/24 |
| Bandwidth | Up to 12 Mbps (USB FS), sustained ≥ 1 Mbps |

### 4.2 IF-02: TCP Command Channel (Port 5000)

| Property | Value |
|----------|-------|
| Transport | TCP |
| Port | 5000 (MCU listens) |
| Initiator | Linux gateway connects to MCU |
| Direction | Bidirectional (request/response) |
| Framing | 4-byte LE length + 1-byte msg type + protobuf payload |
| Message types | ActuatorCommand, ActuatorResponse, ConfigUpdate, ConfigResponse, DiagnosticRequest, DiagnosticResponse |
| Timeout | 5 second response timeout |

### 4.3 IF-03: TCP Telemetry Channel (Port 5001)

| Property | Value |
|----------|-------|
| Transport | TCP |
| Port | 5001 (Linux listens) |
| Initiator | MCU connects to Linux |
| Direction | MCU → Linux (streaming) |
| Framing | 4-byte LE length + 1-byte msg type + protobuf payload |
| Message types | SensorData, HealthStatus |
| Rate | Configurable 1-100 Hz (sensor), 1 Hz (health) |

### 4.4 IF-04: Diagnostic CLI (Port 5002)

| Property | Value |
|----------|-------|
| Transport | TCP |
| Port | 5002 (Linux listens) |
| Initiator | External diagnostic client |
| Direction | Bidirectional (command/response) |
| Protocol | Plain-text, line-delimited (CR+LF) |
| Commands | READ_SENSOR, SET_ACTUATOR, GET_STATUS, GET_VERSION, GET_LOG, GET_CONFIG, RESET_MCU |

### 4.5 IF-05: ADC Sensor Interface (SE-02 internal)

| Property | Value |
|----------|-------|
| Peripheral | STM32U575 ADC1 |
| Channels | CH0 (temp), CH1 (humidity), CH2 (pressure), CH3 (light) |
| Resolution | 12-bit (0-4095) |
| Reference | 3.3V VDDA |
| Mode | Scan mode, DMA transfer |
| Conversion | ≤ 100 µs for all 4 channels |

### 4.6 IF-06: PWM Actuator Interface (SE-02 internal)

| Property | Value |
|----------|-------|
| Peripheral | STM32U575 TIM2 (CH1: fan, CH2: valve) |
| Frequency | 25 kHz (inaudible for fan) |
| Resolution | 16-bit counter (65536 steps) |
| Duty cycle | 0-100% (0-65535 counter value) |
| Update rate | Immediate on command receipt |

---

## 5. Requirement Allocation

### 5.1 Allocation to System Elements

| Requirement | SE-01 (Linux) | SE-02 (MCU) | SE-03 (Comm) | Notes |
|-------------|:---:|:---:|:---:|-------|
| SYS-001 (ADC channels) | | ✓ | | MCU ADC hardware |
| SYS-002 (Temp sensor) | | ✓ | | ADC CH0 |
| SYS-003 (Humidity sensor) | | ✓ | | ADC CH1 |
| SYS-004 (Pressure sensor) | | ✓ | | ADC CH2 |
| SYS-005 (Light sensor) | | ✓ | | ADC CH3 |
| SYS-006 (ADC resolution) | | ✓ | | 12-bit ADC config |
| SYS-007 (Data packaging) | | ✓ | | nanopb encoding |
| SYS-008 (Data transmission) | | ✓ | ✓ | TCP port 5001 |
| SYS-009 (Sample rate config) | ✓ | ✓ | ✓ | Config via protobuf |
| SYS-010 (Timing jitter) | | ✓ | | Timer interrupt |
| SYS-011 (ADC conversion time) | | ✓ | | DMA scan mode |
| SYS-012 (Per-channel rate) | | ✓ | | Independent timers |
| SYS-013 (Data logging) | ✓ | | | JSON lines log |
| SYS-014 (Log timestamp) | ✓ | | | Linux clock |
| SYS-015 (Log rotation) | ✓ | | | Logrotate |
| SYS-016 (Fan control) | | ✓ | | PWM TIM2 CH1 |
| SYS-017 (Valve control) | | ✓ | | PWM TIM2 CH2 |
| SYS-018 (Actuator cmd msg) | ✓ | | ✓ | protobuf-c encode |
| SYS-019 (Actuator ack) | | ✓ | ✓ | nanopb encode |
| SYS-020 (Actuator latency) | ✓ | ✓ | ✓ | End-to-end |
| SYS-021 (Rate limiting) | | ✓ | | MCU-side enforcement |
| SYS-022 (Range validation) | | ✓ | | MCU-side check |
| SYS-023 (Fan safety limits) | | ✓ | | MCU config |
| SYS-024 (Valve safety limits) | | ✓ | | MCU config |
| SYS-025 (Limit config protection) | ✓ | ✓ | ✓ | Auth token flow |
| SYS-026 (Comm loss fail-safe) | | ✓ | | MCU timer check |
| SYS-027 (Watchdog fail-safe) | | ✓ | | Startup init |
| SYS-028 (Error fail-safe) | | ✓ | | Decode error handler |
| SYS-029 (Fail-safe reporting) | | ✓ | ✓ | HealthStatus msg |
| SYS-030 (USB CDC-ECM) | ✓ | ✓ | ✓ | Both sides |
| SYS-031 (Static IP) | ✓ | ✓ | | Config |
| SYS-032 (TCP cmd port) | ✓ | ✓ | ✓ | Port 5000 |
| SYS-033 (TCP telemetry port) | ✓ | ✓ | ✓ | Port 5001 |
| SYS-034 (Protobuf format) | ✓ | ✓ | ✓ | Shared .proto |
| SYS-035 (Message framing) | ✓ | ✓ | ✓ | Length + type prefix |
| SYS-036 (Sequence numbers) | ✓ | ✓ | | Both track |
| SYS-037 (Protocol version) | ✓ | ✓ | | Version field |
| SYS-038 (MCU heartbeat) | | ✓ | ✓ | 1 Hz health msg |
| SYS-039 (Linux HB monitor) | ✓ | | | Timeout detection |
| SYS-040 (MCU GW monitor) | | ✓ | | Timeout detection |
| SYS-041 (Comm status) | ✓ | ✓ | | State machine |
| SYS-042 (USB power cycle) | ✓ | | | QEMU monitor cmd |
| SYS-043 (TCP reconnection) | ✓ | ✓ | ✓ | Both sides |
| SYS-044 (State resync) | ✓ | ✓ | ✓ | Sync protocol |
| SYS-045 (Recovery limit) | ✓ | | | Linux counter |
| SYS-046 (Diag TCP interface) | ✓ | | | Port 5002 |
| SYS-047 (Read sensor cmd) | ✓ | | | Proxy to MCU |
| SYS-048 (Set actuator cmd) | ✓ | | | Proxy to MCU |
| SYS-049 (Get status cmd) | ✓ | | | Aggregated |
| SYS-050 (Get version cmd) | ✓ | ✓ | ✓ | Both versions |
| SYS-051 (Reset MCU cmd) | ✓ | | | USB power cycle |
| SYS-052 (Event log) | ✓ | | | Linux logging |
| SYS-053 (Log persistence) | ✓ | | | Persistent FS |
| SYS-054 (Log retrieval) | ✓ | | | Diag interface |
| SYS-055 (Log severity) | ✓ | | | Log levels |
| SYS-056 (Linux version) | ✓ | | | Build metadata |
| SYS-057 (MCU version) | | ✓ | ✓ | Diag response |
| SYS-058 (Sensor rate config) | ✓ | ✓ | ✓ | ConfigUpdate msg |
| SYS-059 (Actuator limit config) | ✓ | ✓ | ✓ | ConfigUpdate msg |
| SYS-060 (Config persistence) | | ✓ | | Flash NVM |
| SYS-061 (Config read-back) | ✓ | ✓ | ✓ | DiagRequest msg |
| SYS-062 (Default config) | | ✓ | | Flash defaults |
| SYS-063 (Rate validation) | | ✓ | | MCU validation |
| SYS-064 (Limit validation) | | ✓ | | MCU validation |
| SYS-065 (Config CRC) | | ✓ | | Flash CRC check |
| SYS-066 (Linux boot time) | ✓ | | | Yocto optimized |
| SYS-067 (MCU boot time) | | ✓ | | Init sequence |
| SYS-068 (TCP throughput) | ✓ | ✓ | ✓ | End-to-end |
| SYS-069 (HW watchdog) | | ✓ | | IWDG config |
| SYS-070 (WDG feed freq) | | ✓ | | Main loop |
| SYS-071 (WDG reset counter) | | ✓ | | Flash counter |
| SYS-072 (Static memory) | | ✓ | | No malloc |
| SYS-073 (Stack usage) | | ✓ | | ≤ 32 KB |
| SYS-074 to SYS-080 | ✓ | ✓ | | Process reqs |

### 5.2 Allocation Summary

| System Element | Allocated Requirements | Safety-Relevant |
|----------------|----------------------|-----------------|
| SE-01: Linux Gateway | 45 | 5 |
| SE-02: MCU | 55 | 20 |
| SE-03: Communication | 25 | 3 |

*Note: Requirements may be allocated to multiple elements.*

---

## 6. Dynamic Behavior

### 6.1 System Operational Modes

![System Operational State Machine](../diagrams/state_machine.drawio.svg)

**Mode Descriptions**:

| Mode | Linux State | MCU State | Actuators |
|------|-------------|-----------|-----------|
| INIT | Booting, waiting for USB | Booting, USB init | Safe state (0%) |
| CONNECT | USB enumerated, TCP connecting | CDC-ECM up, TCP connecting | Safe state (0%) |
| NORMAL | Full operation | Full operation | Active control |
| DEGRADED | Packet loss detected, monitoring | Increased heartbeat rate | Active (cautious) |
| RECOVERY | USB power cycling, reconnecting | Rebooting | Safe state (0%) |
| FAULT | Permanent fault, awaiting human | N/A (unreachable) | Safe state (0%) |
| DIAG | Diagnostic session active | Normal + diagnostic responder | Active control |

### 6.2 Normal Operation Sequence

![Normal Operation Sequence](../diagrams/sequence_normal.drawio.svg)

### 6.3 Fail-Safe Sequence

![Fail-Safe and Recovery Sequence](../diagrams/sequence_failsafe.drawio.svg)

### 6.4 Configuration Update Sequence

![Configuration Update Sequence](../diagrams/sequence_config.drawio.svg)

---

## 7. Architecture Decision Records

### ADR-001: Protobuf over TCP vs Raw Binary Protocol

**Context**: Need structured IPC between Linux and MCU with schema evolution support.

**Decision**: Use Protocol Buffers v3 with nanopb (MCU) and protobuf-c (Linux).

**Rationale**:
- Schema-defined, type-safe messages prevent data corruption
- Backward/forward compatibility via protobuf field numbering
- nanopb generates static C code with zero heap allocation (fits MCU constraint)
- protobuf-c is lightweight for Linux embedded use
- Shared `.proto` file ensures both sides agree on message format
- Binary encoding is compact (important for USB FS bandwidth)

**Alternatives Considered**:
- Raw struct serialization: No versioning, endianness issues, fragile
- JSON: Too heavyweight for MCU, parsing complexity, bandwidth waste
- FlatBuffers: Good performance but less ecosystem support in embedded C
- CBOR: Decent but lacks schema enforcement

**Consequences**: Need protobuf compiler in build toolchain, nanopb adds ~5 KB to MCU firmware.

### ADR-002: USB CDC-ECM vs UART for IPC

**Context**: Need reliable communication between Linux SoC and MCU peripheral.

**Decision**: USB CDC-ECM (Ethernet over USB) with TCP/IP.

**Rationale**:
- TCP provides reliable, ordered delivery (vs raw USB bulk or UART)
- Ethernet/TCP stack available on both sides (Linux native, lwIP on MCU)
- Multiple TCP connections for different data streams (cmd, telemetry, diag)
- QEMU has good USB emulation support
- CDC-ECM is a standard USB class — driver-free on Linux
- Bandwidth: 12 Mbps (USB FS) far exceeds UART typical rates

**Alternatives Considered**:
- UART: Simple but no flow control, single channel, harder to multiplex
- SPI: Fast but master/slave model doesn't fit bidirectional streaming
- CAN: Automotive standard but limited bandwidth (1 Mbps), complex setup in QEMU
- Virtual serial (virtio): QEMU-specific, not representative of real hardware

**Consequences**: Need lwIP or similar TCP stack on MCU, USB device driver implementation.

### ADR-003: Bare-Metal vs RTOS on MCU

**Context**: MCU needs to handle real-time sensor sampling, USB, TCP, and actuator control.

**Decision**: Bare-metal super-loop with interrupt-driven scheduling.

**Rationale**:
- Simpler to analyze for timing guarantees (no scheduler overhead)
- Easier to prove MISRA compliance (no RTOS internals to certify)
- Sufficient for 4 ADC channels + 2 PWM + USB (not high task count)
- Deterministic worst-case execution time (WCET) analysis
- Smaller memory footprint (~20 KB flash for application)
- RTOS available as stretch goal (FreeRTOS) if complexity grows

**Alternatives Considered**:
- FreeRTOS: Mature, but adds ~10 KB flash, task management complexity, MISRA gaps
- Zephyr: Feature-rich but heavy for this application, certification overhead
- ThreadX: Good safety credentials but licensing considerations

**Consequences**: Must carefully design interrupt priorities, shared data protection via disable/enable interrupts, cooperative scheduling in main loop.

### ADR-004: QEMU Machine Types

**Context**: Need QEMU machines that accurately represent target hardware.

**Decision**: 
- Linux: `qemu-system-aarch64 -machine virt` (generic ARM64)
- MCU: `qemu-system-arm -machine netduinoplus2` (STM32F4-based, closest available)

**Rationale**:
- `virt` machine supports virtio devices, PCI, USB — good Linux support
- `netduinoplus2` provides STM32-like peripherals (ADC, Timer, GPIO, USB)
- Both are well-maintained in upstream QEMU
- Custom QEMU machine definition possible as refinement

**Alternatives Considered**:
- `raspi3b` for Linux: Specific to BCM2837, unnecessary constraints
- `stm32vldiscovery` for MCU: Too limited (STM32F100, no USB)
- Custom QEMU machines: Maximum accuracy but high development effort

**Consequences**: Some peripheral differences between QEMU `netduinoplus2` (STM32F4) and real STM32U575 — HAL abstraction layer handles this.

### ADR-005: Yocto vs Buildroot for Linux

**Context**: Need a minimal embedded Linux build for the gateway.

**Decision**: Yocto Project (Poky, Scarthgap release).

**Rationale**:
- Industry standard for embedded Linux (automotive, industrial)
- Reproducible builds with manifest/lockfile
- Custom layer (`meta-sentinel`) for gateway application
- Extensive BSP support for ARM64
- Demonstrates real-world build system for compliance demos

**Alternatives Considered**:
- Buildroot: Simpler but less flexible, less industry adoption
- Debian/Ubuntu minimal: Not reproducible, not embedded-focused
- Alpine Linux: Lightweight but musl libc complications

**Consequences**: Yocto build is slow (first build ~2h), but reproducible. Layer structure well-documented.

---

## 8. Safety Architecture

### 8.1 Safety-Relevant Functions

| Function | ASIL | System Element | Safe State |
|----------|------|----------------|------------|
| Actuator control (fan) | ASIL-B | SE-02 (MCU) | 0% duty cycle |
| Actuator control (valve) | ASIL-B | SE-02 (MCU) | 0% duty cycle |
| Actuator range validation | ASIL-B | SE-02 (MCU) | Reject command |
| Communication loss detection | ASIL-B | SE-02 (MCU) | Fail-safe transition |
| Watchdog monitoring | ASIL-B | SE-02 (MCU) | MCU reset → safe state |
| Heartbeat monitoring | ASIL-B | SE-01 (Linux) | Trigger recovery |

### 8.2 Safety Mechanisms

1. **Hardware Watchdog (IWDG)**: Independent clock, cannot be disabled after start, resets MCU on firmware hang
2. **Communication Timeout**: MCU-side 5-second timer triggers fail-safe on Linux silence
3. **Range Validation**: Double-check on actuator values (command validation + PWM output verification)
4. **Sequence Numbers**: Detect message loss/duplication, prevent stale commands
5. **CRC-32 on Config**: Detect flash corruption, revert to safe defaults
6. **Safe State on Boot**: All actuators initialized to 0% before any communication

---

## 9. Resource Budgets

### 9.1 MCU Memory Budget

| Component | Flash (KB) | RAM (KB) |
|-----------|-----------|----------|
| HAL drivers | 30 | 2 |
| lwIP TCP/IP stack | 80 | 40 |
| USB CDC-ECM driver | 20 | 8 |
| nanopb (protobuf) | 5 | 2 |
| Application logic | 40 | 10 |
| Configuration store | 4 | 1 |
| Stack | — | 32 |
| **Total** | **179** | **95** |
| **Available** | **2048** | **786** |
| **Margin** | **91%** | **88%** |

### 9.2 Linux Memory Budget

| Component | RAM (MB) |
|-----------|----------|
| Kernel + modules | 30 |
| systemd + services | 20 |
| Gateway application | 10 |
| Protobuf library | 2 |
| Log buffers | 10 |
| Network stack | 5 |
| **Total** | **77** |
| **Available** | **512** |
| **Margin** | **85%** |

---

## 10. Traceability

This document traces to SRS-001 (System Requirements Specification) via the requirement allocation table in Section 5. Forward traceability to software requirements is maintained in `docs/traceability/sys_to_swe_trace.md`.
