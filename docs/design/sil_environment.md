---
title: "SIL Environment Specification"
document_id: "SIL-001"
project: "Sentinel Gateway"
version: "1.0.0"
date: 2026-03-17
status: "Approved"
aspice_process: "SWE.6 Software Qualification Test"
---

# SIL Environment Specification — Sentinel Gateway

## 1. Purpose

This document specifies the Software-in-the-Loop (SIL) test environment used to verify and validate the Sentinel Gateway system without physical hardware. All verification activities from unit tests through system qualification execute within this containerized SIL environment.

## 2. SIL Architecture

### 2.1 Environment Components

| Component | Implementation | Role |
|-----------|---------------|------|
| Linux Gateway | QEMU `virt` (aarch64) | Runs `sentinel-gw` binary on Yocto Linux |
| MCU Firmware | QEMU `netduinoplus2` (ARM) | Runs `sentinel-mcu.elf` bare-metal |
| Network Bridge | Linux TAP + bridge | Simulates USB CDC-ECM link |
| Test Harness | Python pytest (host) | Orchestrates tests, injects faults |
| QEMU Monitor | QMP protocol (JSON) | Fault injection, VM control |
| Serial Console | QEMU chardev (stdio/socket) | Log capture, boot detection |
| GDB Server | QEMU built-in GDB stub | Debug attach (optional) |

### 2.2 Network Topology

| Endpoint | IP Address | Subnet | Interface |
|----------|-----------|--------|-----------|
| Linux Gateway | 192.168.7.1 | /24 | usb0 (CDC-ECM host) |
| MCU Firmware | 192.168.7.2 | /24 | eth0 (CDC-ECM device) |
| Test Harness | 192.168.7.100 | /24 | br-sentinel (bridge) |

### 2.3 TCP Port Allocation

| Port | Direction | Protocol | Purpose |
|------|-----------|----------|---------|
| 5000 | Linux → MCU | Command | ActuatorCommand, ConfigUpdate |
| 5001 | MCU → Linux | Telemetry | SensorData, HealthStatus |
| 5002 | Linux ← External | Diagnostics | DiagnosticRequest/Response |
| 1234 | Host → QEMU | GDB | Linux VM debug (optional) |
| 1235 | Host → QEMU | GDB | MCU VM debug (optional) |
| 4444 | Host → QEMU | QMP | Linux VM monitor |
| 4445 | Host → QEMU | QMP | MCU VM monitor |

## 3. QEMU Configuration

### 3.1 Linux Gateway VM

**Machine**: `virt` (generic ARM64 virtual platform)
**CPU**: `cortex-a53` (single core sufficient for SIL)
**Memory**: 512 MB
**Storage**: Yocto `core-image-minimal` rootfs (virtio block device)
**Kernel**: Yocto-built `Image` (aarch64 Linux kernel)

**Command-line parameters**:

| Parameter | Value | Rationale |
|-----------|-------|-----------|
| `-machine` | `virt` | Standard ARM64 QEMU machine |
| `-cpu` | `cortex-a53` | Matches target SoC |
| `-m` | `512M` | Matches product spec |
| `-kernel` | `Image` | Direct kernel boot (faster than bootloader) |
| `-drive` | `file=rootfs.ext4,format=raw,if=virtio` | Root filesystem |
| `-append` | `root=/dev/vda rw console=ttyAMA0` | Kernel cmdline |
| `-device usb-ehci` | — | USB host controller |
| `-device usb-net` | `netdev=usb0` | CDC-ECM host side |
| `-netdev tap` | `id=usb0,ifname=tap-gw` | Host TAP interface |
| `-chardev socket` | `id=mon,path=qmp-linux.sock,server=on,wait=off` | QMP monitor |
| `-mon` | `chardev=mon,mode=control` | Enable QMP |
| `-nographic` | — | No GUI (CI-friendly) |

**Boot detection**: Test harness waits for `"sentinel-gw: started"` on serial console (timeout: 30s).

### 3.2 MCU VM

**Machine**: `netduinoplus2` (STM32F4-based, closest available to STM32U575)
**CPU**: `cortex-m4` (QEMU maps to Cortex-M33 behavior for our purposes)
**Memory**: 1 MB (QEMU minimum; firmware uses ~192 KB flash + ~256 KB RAM)
**Firmware**: `sentinel-mcu.elf` loaded via `-kernel`

**Known QEMU limitations** (documented per R-001):

| Feature | Real STM32U575 | QEMU netduinoplus2 | Mitigation |
|---------|---------------|-------------------|------------|
| ADC | 14-bit SAR | 12-bit (simulated) | HAL abstraction, mock values |
| Timers | TIM1-TIM17 | TIM2-TIM5 only | Use available timers |
| USB | USB OTG FS | Limited CDC support | TAP bridge workaround |
| Flash | 2 MB dual-bank | File-backed pflash | Functional equivalent |
| Watchdog | IWDG + WWDG | IWDG only | Sufficient for SIL |
| GPIO | 140+ pins | Subset emulated | Map to available |

### 3.3 Peripheral Emulation Strategy

Since QEMU's `netduinoplus2` doesn't perfectly match STM32U575, the following strategies apply:

**ADC (Sensor Input)**:
- QEMU provides basic ADC register emulation
- Test harness writes known values to ADC data registers via QMP `xp` command
- Firmware reads DMA buffer as normal
- Allows deterministic test input without physical sensors

**PWM (Actuator Output)**:
- Timer/PWM registers readable via QMP
- Test harness verifies PWM duty cycle by reading timer compare registers
- Validates actuator commands produce correct register values

**Watchdog**:
- IWDG timer emulated by QEMU
- Unfed watchdog triggers MCU reset (detectable by test harness)
- Tests verify watchdog feed timing

**USB CDC-ECM**:
- Bypassed at hardware level; TAP bridge provides Ethernet connectivity
- Protocol behavior identical from firmware perspective (lwIP TCP/IP stack)
- Trade-off documented: USB enumeration timing not tested in SIL

## 4. Test Fixture Lifecycle

### 4.1 Environment Setup (per test session)

```
1. Create TAP interfaces (tap-gw, tap-mcu)
2. Create bridge (br-sentinel)
3. Assign test harness IP (192.168.7.100/24)
4. Start Linux QEMU (background)
5. Wait for Linux boot (serial: "sentinel-gw: started", timeout 30s)
6. Start MCU QEMU (background)
7. Wait for MCU init (serial: "mcu: init complete", timeout 10s)
8. Verify TCP connectivity (connect 192.168.7.2:5000, timeout 5s)
9. Environment ready → run tests
```

### 4.2 Test Execution

```
For each test:
  1. Reset state (if needed: QEMU monitor system_reset)
  2. Wait for re-init
  3. Execute test steps (send protobuf, verify response)
  4. Capture serial logs + TCP traffic
  5. Assert expected outcomes
  6. Record pass/fail + timing
```

### 4.3 Environment Teardown

```
1. Kill MCU QEMU (SIGTERM, wait 5s, SIGKILL)
2. Kill Linux QEMU (SIGTERM, wait 5s, SIGKILL)
3. Delete bridge and TAP interfaces
4. Collect logs → results/sil/
5. Generate JUnit XML report
```

## 5. Fault Injection Specification

### 5.1 Communication Faults

| ID | Fault | Injection Method | Expected Behavior |
|----|-------|-----------------|-------------------|
| FI-01 | USB disconnect | QMP: `device_del usb-net0` | MCU enters FAILSAFE in ≤3s |
| FI-02 | Network partition | `iptables -A FORWARD -j DROP` on bridge | Both sides detect loss |
| FI-03 | Packet delay (500ms) | `tc qdisc add dev tap-mcu root netem delay 500ms` | Telemetry delayed but no failsafe |
| FI-04 | Packet loss (50%) | `tc qdisc add dev tap-mcu root netem loss 50%` | TCP retransmits, eventual delivery |
| FI-05 | Packet corruption | `tc qdisc add dev tap-mcu root netem corrupt 10%` | TCP checksum catches, retransmit |

### 5.2 Processor Faults

| ID | Fault | Injection Method | Expected Behavior |
|----|-------|-----------------|-------------------|
| FI-06 | MCU freeze | QMP: `stop` (MCU VM) | Linux detects timeout, attempts recovery |
| FI-07 | MCU reset | QMP: `system_reset` (MCU VM) | MCU reboots to safe state, state sync |
| FI-08 | Linux crash | QMP: `quit` (Linux VM) | MCU enters FAILSAFE (no heartbeats) |
| FI-09 | MCU watchdog timeout | Don't feed IWDG for >2s | MCU auto-resets |

### 5.3 Data Faults

| ID | Fault | Injection Method | Expected Behavior |
|----|-------|-----------------|-------------------|
| FI-10 | Invalid protobuf | Send malformed data on TCP:5000 | MCU rejects, sends error response |
| FI-11 | Out-of-range value | Send ActuatorCommand with value=150% | MCU clamps to max, logs warning |
| FI-12 | Config CRC mismatch | QMP: write bad CRC to flash sector | MCU loads defaults on next boot |
| FI-13 | Oversized message | Send 64 KB protobuf (exceeds 512B buffer) | MCU rejects, no buffer overflow |

## 6. Test Data Management

### 6.1 Golden Test Vectors

Pre-computed protobuf messages stored in `tests/fixtures/`:

| File | Contents | Used By |
|------|----------|---------|
| `sensor_data_normal.bin` | Valid SensorData (4 channels, mid-range) | Telemetry tests |
| `sensor_data_limits.bin` | SensorData at min/max boundaries | Boundary tests |
| `actuator_cmd_valid.bin` | Valid ActuatorCommand (50%) | Actuator tests |
| `actuator_cmd_invalid.bin` | ActuatorCommand (150%, out-of-range) | Validation tests |
| `config_update.bin` | ConfigUpdate (50 Hz sample rate) | Config tests |
| `health_status_normal.bin` | HealthStatus (state=NORMAL) | Health tests |
| `health_status_failsafe.bin` | HealthStatus (state=FAILSAFE) | Failsafe tests |
| `malformed_protobuf.bin` | Truncated/corrupted protobuf | Robustness tests |

### 6.2 Expected Results

Each test vector has a corresponding expected response in `tests/expected/`:
- Byte-exact protobuf response messages
- Expected serial console output patterns (regex)
- Expected timing bounds (min/max response time)

## 7. Reporting

### 7.1 Output Artifacts

| File | Format | Contents |
|------|--------|----------|
| `results/sil/junit.xml` | JUnit XML | Test pass/fail for CI integration |
| `results/sil/timing.csv` | CSV | Per-test timing (setup, execute, teardown) |
| `results/sil/serial_linux.log` | Text | Full Linux QEMU serial output |
| `results/sil/serial_mcu.log` | Text | Full MCU QEMU serial output |
| `results/sil/coverage.json` | JSON | Code coverage (if instrumented build) |
| `results/sil/summary.md` | Markdown | Human-readable test summary |

### 7.2 CI Badge Criteria

| Badge | Condition |
|-------|-----------|
| ✅ SIL Pass | All tests pass, no timeouts |
| ⚠️ SIL Partial | ≥90% pass, failures in non-safety tests |
| ❌ SIL Fail | Any safety-critical test fails OR <90% pass rate |

## 8. Traceability

| Requirement | Coverage |
|-------------|----------|
| SYS-050 | QEMU SIL environment (Sections 2–3) |
| SYS-074 | Fault injection (Section 5) |
| SYS-075 | Test automation (Section 4) |
| SYS-076 | Test reporting (Section 7) |
| SYS-080 | Test environment reproducibility (Section 3) |
| STKH-016 | Testability (full document) |
| STKH-019 | CI/CD integration (Section 7) |
| ITP-001 | All 12 integration scenarios executable in this environment |
