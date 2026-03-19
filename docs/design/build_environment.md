---
title: "Build Environment Specification"
document_id: "BUILD-001"
project: "Sentinel Gateway"
version: "1.0.0"
date: 2026-03-17
status: "Approved"
aspice_process: "SWE.3 Software Detailed Design"
---

# Build Environment Specification — Sentinel Gateway

## 1. Overview

All builds and SIL (Software-in-the-Loop) execution run inside containers for full reproducibility. No host toolchain installation required beyond Docker (or Podman).

This document specifies:
- Container images and their contents
- Build pipeline stages
- Cross-compilation toolchains
- SIL runtime environment
- CI/CD integration

## 2. Container Strategy

### 2.1 Image Hierarchy

| Image | Base | Purpose | Approx. Size |
|-------|------|---------|-------------|
| `sentinel-build-linux` | `ubuntu:24.04` | Cross-compile Linux gateway (aarch64) | ~1.2 GB |
| `sentinel-build-mcu` | `ubuntu:24.04` | Cross-compile MCU firmware (arm-none-eabi) | ~900 MB |
| `sentinel-sil` | `ubuntu:24.04` | QEMU runtime + test harness | ~1.5 GB |
| `sentinel-yocto` | `ubuntu:24.04` | Yocto/Poky Scarthgap build | ~3 GB |
| `sentinel-analysis` | `ubuntu:24.04` | Static analysis, MISRA checks, coverage | ~800 MB |

### 2.2 Design Principles

- **Multi-stage builds**: Separate build-time deps from runtime
- **Layer caching**: Toolchain install layers cached; source copied in final stage
- **Reproducible**: All tool versions pinned (no `latest` tags)
- **Rootless**: All containers run as non-root user `builder` (UID 1000)
- **No network at build time**: `--network=none` for firmware builds (air-gapped reproducibility)

## 3. Container Specifications

### 3.1 sentinel-build-linux

**Purpose**: Cross-compile the Linux gateway application for ARM64.

**Toolchain**:
- `aarch64-linux-gnu-gcc` 13.x (Ubuntu 24.04 package)
- CMake 3.28+
- protobuf-c compiler (`protoc-c`) 1.5.x
- protobuf compiler (`protoc`) 25.x

**Build steps**:
```
# Stage 1: Install toolchain
FROM ubuntu:24.04 AS toolchain
RUN apt-get update && apt-get install -y --no-install-recommends \
    gcc-aarch64-linux-gnu \
    g++-aarch64-linux-gnu \
    cmake \
    make \
    protobuf-c-compiler \
    protobuf-compiler \
    && rm -rf /var/lib/apt/lists/*

# Stage 2: Build
FROM toolchain AS build
COPY src/linux/ /workspace/src/linux/
COPY docs/design/interface_specs/proto/ /workspace/src/proto/
COPY CMakeLists.txt /workspace/
WORKDIR /workspace/build
RUN cmake .. -DCMAKE_TOOLCHAIN_FILE=../cmake/aarch64-linux.cmake \
    -DBUILD_LINUX=ON -DBUILD_MCU=OFF \
    && make -j$(nproc)

# Stage 3: Export artifact
FROM scratch AS artifact
COPY --from=build /workspace/build/sentinel-gw /sentinel-gw
```

**CMake toolchain file** (`cmake/aarch64-linux.cmake`):
```cmake
set(CMAKE_SYSTEM_NAME Linux)
set(CMAKE_SYSTEM_PROCESSOR aarch64)
set(CMAKE_C_COMPILER aarch64-linux-gnu-gcc)
set(CMAKE_CXX_COMPILER aarch64-linux-gnu-g++)
set(CMAKE_FIND_ROOT_PATH /usr/aarch64-linux-gnu)
set(CMAKE_FIND_ROOT_PATH_MODE_PROGRAM NEVER)
set(CMAKE_FIND_ROOT_PATH_MODE_LIBRARY ONLY)
set(CMAKE_FIND_ROOT_PATH_MODE_INCLUDE ONLY)
```

**Output**: `sentinel-gw` ELF binary (aarch64)

### 3.2 sentinel-build-mcu

**Purpose**: Cross-compile MCU firmware for ARM Cortex-M33 (STM32U575).

**Toolchain**:
- `arm-none-eabi-gcc` 13.2.rel1 (ARM official release)
- CMake 3.28+
- nanopb 0.4.8 (protobuf code generator for embedded)
- Python 3.12 (for nanopb generator)

**Build steps**:
```
# Stage 1: Install toolchain
FROM ubuntu:24.04 AS toolchain
RUN apt-get update && apt-get install -y --no-install-recommends \
    cmake make python3 python3-pip wget \
    && rm -rf /var/lib/apt/lists/*

# Install ARM toolchain (pinned version)
RUN wget -q https://developer.arm.com/-/media/Files/downloads/gnu/13.2.rel1/binrel/arm-gnu-toolchain-13.2.rel1-x86_64-arm-none-eabi.tar.xz \
    && tar xf arm-gnu-toolchain-*.tar.xz -C /opt/ \
    && rm arm-gnu-toolchain-*.tar.xz
ENV PATH="/opt/arm-gnu-toolchain-13.2.Rel1-x86_64-arm-none-eabi/bin:${PATH}"

# Install nanopb
RUN pip3 install --no-cache-dir nanopb==0.4.8

# Stage 2: Build
FROM toolchain AS build
COPY src/mcu/ /workspace/src/mcu/
COPY docs/design/interface_specs/proto/ /workspace/src/proto/
COPY CMakeLists.txt /workspace/
WORKDIR /workspace/build
RUN cmake .. -DCMAKE_TOOLCHAIN_FILE=../cmake/arm-none-eabi.cmake \
    -DBUILD_LINUX=OFF -DBUILD_MCU=ON \
    && make -j$(nproc)

# Stage 3: Export artifact
FROM scratch AS artifact
COPY --from=build /workspace/build/sentinel-mcu.elf /sentinel-mcu.elf
COPY --from=build /workspace/build/sentinel-mcu.bin /sentinel-mcu.bin
```

**CMake toolchain file** (`cmake/arm-none-eabi.cmake`):
```cmake
set(CMAKE_SYSTEM_NAME Generic)
set(CMAKE_SYSTEM_PROCESSOR arm)
set(CMAKE_C_COMPILER arm-none-eabi-gcc)
set(CMAKE_ASM_COMPILER arm-none-eabi-gcc)
set(CMAKE_OBJCOPY arm-none-eabi-objcopy)
set(CMAKE_SIZE arm-none-eabi-size)

set(CMAKE_C_FLAGS_INIT "-mcpu=cortex-m33 -mthumb -mfloat-abi=hard -mfpu=fpv5-sp-d16")
set(CMAKE_C_FLAGS_INIT "${CMAKE_C_FLAGS_INIT} -ffunction-sections -fdata-sections")
set(CMAKE_EXE_LINKER_FLAGS_INIT "-Wl,--gc-sections -specs=nosys.specs -specs=nano.specs")

set(CMAKE_FIND_ROOT_PATH_MODE_PROGRAM NEVER)
set(CMAKE_FIND_ROOT_PATH_MODE_LIBRARY ONLY)
set(CMAKE_FIND_ROOT_PATH_MODE_INCLUDE ONLY)

# No heap — enforce static allocation
add_compile_definitions(_REENT_SMALL __HEAP_SIZE=0)
```

**Linker script requirements** (`stm32u575.ld`):
- Flash: 0x0800_0000, 2 MB
- RAM: 0x2000_0000, 786 KB
- Stack: 32 KB (top of RAM)
- No heap section (enforce zero dynamic allocation)
- Config sector: 0x0803_0000, 4 KB (separate MEMORY region)

**Output**: `sentinel-mcu.elf` + `sentinel-mcu.bin`

### 3.3 sentinel-sil

**Purpose**: Run both processors in QEMU for integration and system testing.

**Contents**:
- QEMU 8.2+ (system emulation: `qemu-system-aarch64`, `qemu-system-arm`)
- Yocto rootfs image (pre-built `core-image-minimal` for aarch64)
- Test harness (Python 3.12 + pytest)
- Bridge networking scripts
- GDB multiarch (for debugging)

**Build steps**:
```
FROM ubuntu:24.04

RUN apt-get update && apt-get install -y --no-install-recommends \
    qemu-system-arm \
    qemu-system-aarch64 \
    qemu-utils \
    bridge-utils \
    iproute2 \
    python3 python3-pip python3-venv \
    gdb-multiarch \
    socat \
    screen \
    && rm -rf /var/lib/apt/lists/*

RUN pip3 install --no-cache-dir \
    pytest==8.1 \
    pytest-timeout==2.3 \
    protobuf==5.26 \
    pexpect==4.9

# Non-root user
RUN useradd -m -u 1000 builder
USER builder
WORKDIR /workspace
```

**Runtime volumes**:
| Host mount | Container path | Purpose |
|-----------|---------------|---------|
| `./build/linux/` | `/workspace/artifacts/linux/` | Gateway binary |
| `./build/mcu/` | `/workspace/artifacts/mcu/` | Firmware ELF/BIN |
| `./tests/` | `/workspace/tests/` | Test scripts (pytest) |
| `./src/` | `/workspace/src/` | Source code (for MCU sim build) |
| `./CMakeLists.txt` | `/workspace/CMakeLists.txt` | Build config (for native gateway build) |
| `./results/sil/` | `/workspace/results/` | JUnit XML test reports |

### 3.4 sentinel-yocto

**Purpose**: Build the Yocto Poky Linux image for the ARM64 gateway.

**Contents**:
- Yocto Poky Scarthgap (4.0)
- `meta-sentinel` custom layer
- `core-image-minimal` + sentinel-gw recipe

**Layer structure** (specification):

![Yocto meta-sentinel Layer Structure](../architecture/diagrams/yocto_layer_structure.drawio.svg)

**Key recipe features**:
- `sentinel-gw` starts via systemd on boot
- USB CDC-ECM gadget configured at boot (static IP 192.168.7.1)
- Read-only rootfs with tmpfs overlay for logs
- Watchdog daemon (`systemd-watchdog`) as backup

**Build steps**:
```
FROM ubuntu:24.04

# Yocto build dependencies
RUN apt-get update && apt-get install -y --no-install-recommends \
    gawk wget git diffstat unzip texinfo gcc build-essential \
    chrpath socat cpio python3 python3-pip python3-pexpect \
    xz-utils debianutils iputils-ping python3-git python3-jinja2 \
    python3-subunit zstd liblz4-tool file locales libacl1 \
    && rm -rf /var/lib/apt/lists/*

RUN locale-gen en_US.UTF-8
ENV LANG=en_US.UTF-8

RUN useradd -m -u 1000 builder
USER builder
WORKDIR /home/builder

# Clone Poky Scarthgap
RUN git clone -b scarthgap git://git.yoctoproject.org/poky.git
```

**Output**: `core-image-minimal-sentinel.wic.gz` (bootable disk image)

**sstate-cache**: Mounted as Docker volume for incremental builds. First build ~2 hours; subsequent ~10 minutes.

### 3.5 sentinel-analysis

**Purpose**: Static analysis, MISRA checking, coverage reporting.

**Contents**:
- cppcheck 2.13+ (MISRA addon)
- clang-tidy 17
- gcov / lcov (coverage)
- Cppcheck MISRA C:2012 rules text (user-provided, not redistributable)
- Python report generators

**Build steps**:
```
FROM ubuntu:24.04

RUN apt-get update && apt-get install -y --no-install-recommends \
    cppcheck \
    clang-tidy-17 \
    lcov \
    python3 python3-pip \
    && rm -rf /var/lib/apt/lists/*

RUN pip3 install --no-cache-dir \
    junit-xml==1.9 \
    jinja2==3.1
```

## 4. Docker Compose Orchestration

### 4.1 Full Pipeline

```yaml
# docker-compose.yml
version: "3.8"

services:
  build-linux:
    build:
      context: .
      dockerfile: docker/Dockerfile.build-linux
    volumes:
      - ./build/linux:/workspace/output
      - build-cache-linux:/workspace/build
    network_mode: none

  build-mcu:
    build:
      context: .
      dockerfile: docker/Dockerfile.build-mcu
    volumes:
      - ./build/mcu:/workspace/output
      - build-cache-mcu:/workspace/build
    network_mode: none

  analysis:
    build:
      context: .
      dockerfile: docker/Dockerfile.analysis
    volumes:
      - .:/workspace/src:ro
      - ./results/analysis:/workspace/output
    depends_on:
      build-linux:
        condition: service_completed_successfully
      build-mcu:
        condition: service_completed_successfully

  sil:
    build:
      context: .
      dockerfile: docker/Dockerfile.sil
    volumes:
      - ./build/linux:/workspace/artifacts/linux:ro
      - ./build/mcu:/workspace/artifacts/mcu:ro
      - ./tests:/workspace/tests:ro
      - ./src:/workspace/src:ro
      - ./CMakeLists.txt:/workspace/CMakeLists.txt:ro
      - ./config:/workspace/config:ro
      - ./cmake:/workspace/cmake:ro
      - ./results/sil:/workspace/results
    user: root
    environment:
      - PROJECT_ROOT=/workspace
    depends_on:
      build-linux:
        condition: service_completed_successfully
      build-mcu:
        condition: service_completed_successfully

  yocto:
    build:
      context: .
      dockerfile: docker/Dockerfile.yocto
    volumes:
      - ./build/linux:/workspace/artifacts/linux:ro
      - ./build/yocto:/workspace/output
      - yocto-sstate:/home/builder/poky/build/sstate-cache
      - yocto-downloads:/home/builder/poky/build/downloads
    profiles:
      - full    # Only run with: docker compose --profile full up

volumes:
  build-cache-linux:
  build-cache-mcu:
  yocto-sstate:
  yocto-downloads:
```

### 4.2 Quick Commands

| Command | Purpose |
|---------|---------|
| `docker-compose run --rm build-linux` | Cross-compile Linux gateway (aarch64) |
| `docker-compose run --rm build-mcu` | Cross-compile MCU firmware (Cortex-M33) |
| `docker-compose run --rm test` | Run unit tests (native x86 build + ctest) |
| `docker-compose run --rm sil` | Run SIL integration tests (ITP-001 via pytest) |
| `docker-compose run --rm qemu-sil` | Run QEMU SIL with real ARM binaries |
| `docker-compose run --rm analysis` | Run MISRA + static analysis |
| `docker-compose run --rm sil bash` | Interactive SIL shell for debugging |

## 5. SIL Environment Specification

### 5.1 QEMU Launch Configuration

**Linux Gateway VM** (`qemu-system-aarch64`):
```bash
qemu-system-aarch64 \
  -machine virt \
  -cpu cortex-a53 \
  -m 512M \
  -kernel Image \
  -drive file=rootfs.ext4,format=raw,if=virtio \
  -append "root=/dev/vda rw console=ttyAMA0" \
  -device usb-ehci \
  -device usb-net,netdev=usb0 \
  -netdev tap,id=usb0,ifname=tap-gw,script=no,downscript=no \
  -serial mon:stdio \
  -nographic \
  -S -gdb tcp::1234   # Optional: GDB attach
```

**MCU VM** (`qemu-system-arm`):
```bash
qemu-system-arm \
  -machine netduinoplus2 \
  -cpu cortex-m4 \
  -m 1M \
  -kernel sentinel-mcu.elf \
  -device usb-net,netdev=usb0 \
  -netdev tap,id=usb0,ifname=tap-mcu,script=no,downscript=no \
  -serial mon:stdio \
  -nographic \
  -S -gdb tcp::1235   # Optional: GDB attach
```

### 5.2 Virtual Network Bridge

```bash
# Create bridge between the two QEMU TAP interfaces
ip link add br-sentinel type bridge
ip link set tap-gw master br-sentinel
ip link set tap-mcu master br-sentinel
ip link set br-sentinel up
ip link set tap-gw up
ip link set tap-mcu up
```

This simulates the USB CDC-ECM link as a virtual Ethernet bridge. Both VMs see each other at Layer 2 — same behavior as real USB Ethernet.

### 5.3 SIL Test Harness

**Test runner**: Python pytest with custom fixtures.

**Architecture**:

![SIL Test Harness Architecture](../architecture/diagrams/sil_test_harness.drawio.svg)

**Fixtures**:
- `qemu_linux` — starts Linux QEMU, waits for boot, yields, kills on teardown
- `qemu_mcu` — starts MCU QEMU, yields, kills on teardown
- `sil_env` — starts both VMs + bridge, waits for TCP connectivity
- `tcp_client` — connects to specified port, sends/receives protobuf messages
- `fault_injector` — QEMU monitor commands (disconnect USB, freeze CPU, corrupt memory)

**Test timeout**: 60 seconds per test (SIL startup ~10s, test execution ~5s, margin ~45s).

### 5.4 Fault Injection via QEMU Monitor

| Fault | QEMU Command | Tests |
|-------|-------------|-------|
| USB disconnect | `device_del usb-net0` | Failsafe, recovery |
| MCU freeze | `stop` (on MCU VM) | Linux timeout detection |
| MCU reset | `system_reset` (on MCU VM) | State sync, reconnect |
| Network delay | `tc qdisc add dev tap-mcu root netem delay 500ms` | Timeout handling |
| Memory corruption | `xp /x <addr>` + `set` | Safety monitor, CRC check |

## 6. CI/CD Integration

### 6.1 GitHub Actions Pipeline

```yaml
# .github/workflows/build-and-test.yml
name: Build and Test

on:
  push:
    branches: [main, develop]
  pull_request:
    branches: [main]

jobs:
  build:
    runs-on: ubuntu-latest
    steps:
      - uses: actions/checkout@v4

      - name: Build Linux Gateway
        run: docker compose up --build build-linux

      - name: Build MCU Firmware
        run: docker compose up --build build-mcu

      - name: Upload artifacts
        uses: actions/upload-artifact@v4
        with:
          name: build-artifacts
          path: build/

  analysis:
    needs: build
    runs-on: ubuntu-latest
    steps:
      - uses: actions/checkout@v4
      - uses: actions/download-artifact@v4
        with:
          name: build-artifacts
          path: build/

      - name: MISRA + Static Analysis
        run: docker compose up --build analysis

      - name: Upload reports
        uses: actions/upload-artifact@v4
        with:
          name: analysis-reports
          path: results/analysis/

  sil-test:
    needs: build
    runs-on: ubuntu-latest
    steps:
      - uses: actions/checkout@v4
      - uses: actions/download-artifact@v4
        with:
          name: build-artifacts
          path: build/

      - name: Run SIL Tests
        run: docker compose up --build sil

      - name: Upload test results
        uses: actions/upload-artifact@v4
        with:
          name: sil-results
          path: results/sil/
        if: always()

  yocto:
    if: github.ref == 'refs/heads/main'
    needs: [analysis, sil-test]
    runs-on: ubuntu-latest
    steps:
      - uses: actions/checkout@v4
      - name: Build Yocto Image
        run: docker compose --profile full up --build yocto
        timeout-minutes: 180
```

### 6.2 Pipeline Stages

![CI/CD Pipeline Stages](../architecture/diagrams/ci_pipeline.drawio.svg)

## 7. Local Development

### 7.1 Prerequisites

- Docker 24+ or Podman 4+
- Docker Compose v2
- ~10 GB disk space (images + caches)
- 4+ GB RAM (for QEMU dual-VM)

### 7.2 First-Time Setup

```bash
git clone https://github.com/antonioinnovateops/sentinel-gateway.git
cd sentinel-gateway

# Build all containers (first time ~10 min for toolchain download)
docker compose build

# Run the full pipeline
docker compose up

# Interactive development: get a shell in the SIL container
docker compose run sil bash
```

### 7.3 Development Workflow

```
1. Edit source in src/linux/ or src/mcu/
2. docker compose up build-linux build-mcu   (rebuild ~30s)
3. docker compose up sil                      (test ~1 min)
4. docker compose up analysis                 (MISRA check ~3 min)
5. git commit && git push                     (CI runs full pipeline)
```

## 8. Version Pinning

All tool versions are pinned for reproducibility:

| Tool | Version | Source |
|------|---------|--------|
| Ubuntu base | 24.04 LTS | Docker Hub |
| GCC (aarch64) | 13.2.0 | Ubuntu package |
| GCC (arm-none-eabi) | 13.2.Rel1 | ARM official |
| CMake | 3.28.x | Ubuntu package |
| QEMU | 8.2.x | Ubuntu package |
| nanopb | 0.4.8 | PyPI |
| protobuf-c | 1.5.x | Ubuntu package |
| Python | 3.12.x | Ubuntu package |
| Yocto Poky | Scarthgap (4.0) | git.yoctoproject.org |
| cppcheck | 2.13.x | Ubuntu package |
| clang-tidy | 17.x | Ubuntu package |
| pytest | 8.1.x | PyPI |

## 9. Traceability

| Requirement | Coverage |
|-------------|----------|
| SYS-050 (QEMU SIL) | Sections 5.1–5.4 |
| SYS-051 (Reproducible builds) | Section 2, 8 |
| SYS-055 (CI/CD pipeline) | Section 6 |
| SYS-072 (Static memory) | Section 3.2 toolchain flags |
| SYS-080 (Test environment) | Section 5 |
| STKH-017 (Build system) | Sections 3–4 |
| STKH-018 (Automation) | Section 6 |
