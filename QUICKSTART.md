# Sentinel Gateway — Quickstart Guide

> Reproduce the full ASPICE V-model build, test, and analysis pipeline on your laptop.
> Everything runs in Docker — no cross-compilers, no QEMU, no toolchain installs.

## Prerequisites

| Tool | Version | Check |
|------|---------|-------|
| Git | any | `git --version` |
| Docker | 20.10+ | `docker --version` |
| docker-compose | v2+ | `docker-compose version` |

---

## Step 1 — Clone and checkout

```bash
git clone https://github.com/antonioinnovateops/sentinel-gateway.git
cd sentinel-gateway
git checkout impl/claude-code
```

## Step 2 — Build all Docker images

```bash
docker-compose build
```

This builds 8 images:

| Image | Purpose |
|-------|---------|
| `build-linux` | Cross-compiles the Linux aarch64 gateway binary |
| `build-mcu` | Cross-compiles the STM32U575 Cortex-M33 firmware |
| `build-mcu-qemu` | Cross-compiles MCU firmware for QEMU user-mode |
| `test` | Unit tests (native x86 build + ctest) |
| `sil` | SIL integration tests — ITP-001 scenarios (pytest) |
| `qemu-sil` | QEMU-based SIL with real ARM emulation |
| `analysis` | Static analysis (cppcheck MISRA checks) |

> First build takes ~5 minutes (downloads ARM toolchains). Subsequent builds are cached.

## Step 3 — Cross-compile both binaries

```bash
docker-compose run --rm build-linux
docker-compose run --rm build-mcu
```

Output:
- `build/linux/sentinel-gw` — aarch64 ELF binary
- `build/mcu/sentinel-mcu.elf` — Cortex-M33, ~4.7 KB text, zero heap

## Step 4 — Run unit tests

```bash
docker-compose run --rm test
```

**Expected:** `4/4 tests passed`

Tests cover: wire frame encode/decode, error codes, config store, actuator control.

## Step 5 — Run SIL integration tests

```bash
docker-compose run --rm sil
```

**Expected:** `14 passed`

This starts a MCU simulator + Linux gateway inside the container and runs the Integration Test Plan (ITP-001) scenarios via the diagnostic interface:

| Test | ITP Scenario | What It Validates |
|------|-------------|-------------------|
| `TestIT08` | IT-08 | Diagnostic commands: help, version, status, error handling |
| `TestIT01` | IT-01 | Sensor data flow: MCU → gateway → diagnostic readback |
| `TestIT02` | IT-02 | Actuator commands: diagnostic → gateway → MCU |
| `TestIT04` | IT-04 | Heartbeat: MCU health status tracked by gateway |
| `TestStability` | — | Both processes survive the full test session |

JUnit XML report: `results/sil/sil-results.xml`

> For the full SIL developer guide (running individual tests, interactive diagnostics, adding new tests), see `docs/guides/sil_developer_guide.md`.

## Step 6 — Run QEMU SIL (optional, higher fidelity)

```bash
docker-compose run --rm build-mcu-qemu
docker-compose run --rm qemu-sil
```

Runs the actual cross-compiled ARM binaries under QEMU user-mode emulation instead of the x86 MCU simulator. Same protocol tests, real instruction set.

## Step 7 — Run static analysis

```bash
docker-compose run --rm analysis
```

Runs cppcheck with MISRA C:2012 rules across `src/common/`, `src/linux/`, `src/mcu/`.

Results: `results/analysis/cppcheck.xml`

---

## Run everything in one go

```bash
docker-compose build && \
docker-compose run --rm build-linux && \
docker-compose run --rm build-mcu && \
docker-compose run --rm test && \
docker-compose run --rm sil && \
docker-compose run --rm analysis
```

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

- **Protocol:** Protobuf messages wrapped in wire frames (length + type + CRC-16)
- **Transport:** TCP sockets with epoll (Linux) / bare-metal super-loop (MCU)
- **Safety:** FMEA-derived failure modes, watchdog timeout, health heartbeats

---

## Inspect the ASPICE specifications

```bash
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
| `docker-compose` not found | Install docker-compose v2 |
| Build fails on ARM toolchain download | Retry — needs internet for Ubuntu packages |
| Permission denied on `build/` or `results/` | `sudo chown -R $(id -u):$(id -g) build/ results/` |
| SIL tests timeout | Increase Docker memory to 4GB+ |
