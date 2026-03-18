# Sentinel Gateway — Quickstart Guide

> Reproduce the full ASPICE V-model build, test, and analysis pipeline on your laptop.

## Prerequisites

| Tool | Version | Check |
|------|---------|-------|
| Git | any | `git --version` |
| Docker | 20.10+ | `docker --version` |
| Docker Compose | v2+ | `docker compose version` |

That's it. No cross-compilers, no QEMU, no toolchain installs — Docker handles everything.

---

## Step 1 — Clone and checkout

```bash
git clone https://github.com/antonioinnovateops/sentinel-gateway.git
cd sentinel-gateway
git checkout impl/claude-code
```

## Step 2 — Build all Docker images

```bash
docker compose build
```

This builds 6 images:

| Image | Purpose |
|-------|---------|
| `build-linux` | Cross-compiles the Linux aarch64 gateway binary |
| `build-mcu` | Cross-compiles the STM32U575 Cortex-M33 firmware |
| `build-mcu-qemu` | Cross-compiles MCU firmware for QEMU MPS2-AN505 |
| `sil` | Software-in-the-Loop test environment (Python + pytest) |
| `qemu-sil` | QEMU-based SIL with real ARM emulation |
| `analysis` | Static analysis (cppcheck MISRA checks) |

> ☕ First build takes ~5 minutes (downloads ARM toolchains). Subsequent builds are cached and fast.

## Step 3 — Cross-compile both binaries

Build the Linux gateway:
```bash
docker compose run --rm build-linux
```
Output: `build/linux/sentinel-gw` (aarch64 ELF binary)

Build the MCU firmware:
```bash
docker compose run --rm build-mcu
```
Output: `build/mcu/sentinel-fw.elf` (Cortex-M33, ~4.7KB text, zero heap)

## Step 4 — Run unit tests (31 test cases)

```bash
docker compose run --rm build-linux bash -c "\
  cd /workspace && \
  cmake -B build-test -DBUILD_TESTS=ON && \
  cmake --build build-test && \
  ctest --test-dir build-test --output-on-failure"
```

**Expected output:**
```
31/31 Test cases passed
```

Tests cover:
- Wire frame encode/decode with CRC-16
- Diagnostics command parsing
- Actuator control logic
- Config store read/write
- Error code mapping
- Logger formatting

## Step 5 — Run integration tests (11 end-to-end tests)

```bash
docker compose run --rm sil
```

> **Note:** Steps 3 and 5 have a dependency — the binaries from Step 3 must exist before SIL tests can run. Docker Compose handles this automatically via `depends_on`, but if you skipped Step 3, this step will build them first.

**Expected output:**
```
11/11 PASS
```

These tests simulate the full system:
- MCU ↔ Linux gateway TCP connection
- Protobuf sensor telemetry flow
- Actuator command dispatch
- Health monitoring & watchdog
- Diagnostic text command channel

## Step 6 — Run static analysis

```bash
docker compose run --rm analysis
```

**Expected output:**
```
0 errors, 3 style warnings
```

Runs cppcheck with MISRA C:2012 rules across all source files (`src/common/`, `src/linux/`, `src/mcu/`).

Results saved to: `results/analysis/cppcheck.xml`

---

## Architecture Overview

```
┌─────────────────┐    TCP / Protobuf    ┌─────────────────┐
│   Linux Gateway  │◄───────────────────►│   MCU Firmware   │
│   (aarch64 ELF)  │    Wire frames      │  (Cortex-M33)    │
│                  │                     │                  │
│ • sensor_proxy   │    Diagnostics      │ • safety_monitor │
│ • actuator_proxy │◄───────────────────►│ • watchdog_mgr   │
│ • health_monitor │   Text over TCP     │ • config_store   │
│ • config_manager │                     │ • HAL drivers    │
│ • diagnostics    │                     │ • protobuf_hdlr  │
│ • gateway_core   │                     │ • comm_manager   │
└─────────────────┘                     └─────────────────┘
```

- **Protocol:** Protobuf messages wrapped in wire frames (magic + length + CRC-16)
- **Transport:** TCP sockets with epoll (Linux) / bare-metal super-loop (MCU)
- **Safety:** FMEA-derived failure modes, watchdog timeout, health heartbeats

---

## Run everything in one go

If you just want to build and test everything with a single command:

```bash
docker compose build && \
docker compose run --rm build-linux && \
docker compose run --rm build-mcu && \
docker compose run --rm build-linux bash -c "\
  cd /workspace && \
  cmake -B build-test -DBUILD_TESTS=ON && \
  cmake --build build-test && \
  ctest --test-dir build-test --output-on-failure" && \
docker compose run --rm sil && \
docker compose run --rm analysis
```

---

## Inspect the ASPICE specifications

The full spec documentation lives on the `main` branch:

```bash
git checkout main
ls docs/
```

33 work products covering every ASPICE process area:
- **SYS.1–SYS.5:** Stakeholder → System requirements → Architecture → Integration → Qualification
- **SWE.1–SWE.6:** Software requirements → Architecture → Design → Implementation → Unit test → Integration
- **SUP.1/2/8/9/10:** Quality assurance, reviews, config management, change/problem resolution
- **MAN.3/5/6:** Project management, risk, metrics

---

## Troubleshooting

| Problem | Fix |
|---------|-----|
| `docker compose` not found | Install Docker Compose v2: `docker compose` (not `docker-compose`) |
| Build fails on ARM toolchain download | Retry — uses Ubuntu `gcc-arm-none-eabi` package, needs internet |
| Permission denied on `build/` | Run `sudo chown -R $(id -u):$(id -g) build/ results/` |
| SIL tests timeout | Increase Docker memory to 4GB+ in Docker Desktop settings |

---

## Phase 9 — QEMU-based SIL Testing (Optional)

Phase 9 provides higher-fidelity testing by running the actual cross-compiled ARM binaries in QEMU emulation, rather than native x86 simulation.

### Build MCU firmware for QEMU

```bash
docker compose run --rm build-mcu-qemu
```
Output: `build/qemu-mcu/sentinel-mcu-qemu.elf` (Cortex-M33, targets MPS2-AN505)

### Run QEMU SIL tests

```bash
docker compose run --rm qemu-sil
```

**What this does:**
1. Starts MCU firmware in `qemu-system-arm -M mps2-an505` (Cortex-M33 system emulation)
2. Starts Linux gateway in `qemu-aarch64-static` (user-mode emulation)
3. Verifies TCP communication between the two emulated systems
4. Runs the same protocol tests as the x86 SIL suite

**QEMU architecture:**

```
┌────────────────────────────────────────────────────────────┐
│                     Docker Container                        │
│                                                             │
│  ┌─────────────────────┐     TCP      ┌─────────────────┐  │
│  │  qemu-system-arm    │◄────────────►│ qemu-aarch64    │  │
│  │  -M mps2-an505      │  Port 15000  │ (user-mode)     │  │
│  │                     │              │                 │  │
│  │  sentinel-mcu-qemu  │              │ sentinel-gw     │  │
│  │  (Cortex-M33 ELF)   │              │ (aarch64 ELF)   │  │
│  └─────────────────────┘              └─────────────────┘  │
│                                                             │
│  Python test harness connects to gateway ports 5001, 5002  │
└────────────────────────────────────────────────────────────┘
```

### Run QEMU tests locally (without Docker)

If you have QEMU installed on your host system:

```bash
# Build the QEMU MCU firmware
cmake -B build/qemu-mcu -DBUILD_QEMU_MCU=ON \
  -DCMAKE_TOOLCHAIN_FILE=cmake/arm-none-eabi-qemu.cmake
cmake --build build/qemu-mcu

# Launch MCU in QEMU (in background)
./tools/qemu/launch_mcu_fw.sh --background

# Launch Linux gateway
./tools/qemu/launch_linux_gw.sh --background

# Run tests
python3 -m pytest tests/qemu/test_qemu_sil.py -v
```

### MPS2-AN505 vs STM32U575

The MCU firmware has two build targets:

| Target | Platform | Memory Map | HAL |
|--------|----------|------------|-----|
| `sentinel-mcu` | STM32U575 | 0x08000000 Flash | `hal/*.c` |
| `sentinel-mcu-qemu` | MPS2-AN505 | 0x00000000 SRAM | `hal/qemu/*.c` |

The MPS2-AN505 is the closest Cortex-M33 platform that QEMU fully supports.
The QEMU HAL shim (`src/mcu/hal/qemu/`) provides software-simulated peripherals:
- ADC readings (configurable by test harness)
- PWM output (RAM-backed registers)
- GPIO mapped to FPGAIO LEDs
- Watchdog mapped to CMSDK watchdog

---

## What's next

- **LLM Benchmarking:** Run different AI models against these same specs to compare output quality
