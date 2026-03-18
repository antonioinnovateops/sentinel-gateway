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

This builds 4 images:

| Image | Purpose |
|-------|---------|
| `build-linux` | Cross-compiles the Linux aarch64 gateway binary |
| `build-mcu` | Cross-compiles the STM32U575 Cortex-M33 firmware |
| `sil` | Software-in-the-Loop test environment (Python + pytest) |
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

## What's next

- **Phase 9:** QEMU-based SIL with actual aarch64 + Cortex-M33 emulation
- **LLM Benchmarking:** Run different AI models against these same specs to compare output quality
