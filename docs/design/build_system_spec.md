---
title: "Build System Specification"
document_id: "BSS-001"
project: "Sentinel Gateway"
version: "2.0.0"
date: 2026-03-18
status: "Approved"
aspice_process: "SWE.3 Software Detailed Design"
---

# Build System Specification — Sentinel Gateway

## 1. Overview

This document specifies the CMake-based build system with concrete examples and verified commands.

**Version 2.0 Note**: Updated with lessons learned from implementation:
- ARM toolchain installation via Ubuntu packages (not arm.com downloads)
- Docker UID collision handling
- Explicit CMake options for all build targets
- QEMU user-mode build target

---

## 2. Build Tool Requirements

| Tool | Minimum Version | Installation |
|------|-----------------|--------------|
| CMake | 3.20+ | `apt install cmake` |
| GCC (host) | 11+ | `apt install gcc` |
| ARM GCC (bare-metal) | 13.x | `apt install gcc-arm-none-eabi libnewlib-arm-none-eabi` |
| ARM GCC (Linux cross) | 13.x | `apt install gcc-aarch64-linux-gnu` |
| ARM GCC (32-bit Linux) | 13.x | `apt install gcc-arm-linux-gnueabihf` |

**IMPORTANT**: Do NOT download ARM toolchain from `developer.arm.com` - the URL structure changes frequently. Use Ubuntu packages.

---

## 3. Build Targets

### 3.1 Linux Gateway (`sentinel-gw`)

**CMake Option**: `BUILD_LINUX=ON`

| Property | Value |
|----------|-------|
| Type | ELF executable |
| Toolchain | `aarch64-linux-gnu-gcc` (cross) or native `gcc` |
| C Standard | C11 (no extensions) |
| Output | `build/sentinel-gw` |

**Source Files**:
```
src/linux/main.c
src/linux/gateway_core.c
src/linux/tcp_transport.c
src/linux/sensor_proxy.c
src/linux/actuator_proxy.c
src/linux/health_monitor.c
src/linux/diagnostics.c
src/linux/config_manager.c
src/linux/logger.c
src/common/wire_frame.c
```

**Build Commands**:
```bash
# Native build (for SIL testing)
cmake -B build-linux \
  -DBUILD_LINUX=ON \
  -DBUILD_MCU=OFF \
  -DBUILD_TESTS=OFF

cmake --build build-linux

# Cross-compile for aarch64
cmake -B build-linux-aarch64 \
  -DCMAKE_TOOLCHAIN_FILE=cmake/aarch64-linux.cmake \
  -DBUILD_LINUX=ON \
  -DBUILD_MCU=OFF

cmake --build build-linux-aarch64
```

### 3.2 MCU Firmware — Bare-Metal (`sentinel-mcu`)

**CMake Option**: `BUILD_MCU=ON`

| Property | Value |
|----------|-------|
| Type | ELF + binary (Cortex-M33) |
| Toolchain | `arm-none-eabi-gcc` |
| CPU Flags | `-mcpu=cortex-m33 -mthumb -mfloat-abi=hard -mfpu=fpv5-sp-d16` |
| Linker Script | `src/mcu/stm32u575.ld` |
| Output | `build/sentinel-mcu.elf`, `build/sentinel-mcu.bin` |

**Source Files**:
```
src/mcu/main.c
src/mcu/sensor_acquisition.c
src/mcu/actuator_control.c
src/mcu/protobuf_handler.c
src/mcu/tcp_stack.c                    # lwIP-based (stubbed in v1.0)
src/mcu/watchdog_mgr.c
src/mcu/config_store.c
src/mcu/health_reporter.c
src/mcu/startup_stm32u575.s
src/mcu/hal/adc_driver.c
src/mcu/hal/pwm_driver.c
src/mcu/hal/flash_driver.c
src/mcu/hal/watchdog_driver.c
src/mcu/hal/gpio_driver.c
src/mcu/hal/systick.c
src/common/wire_frame.c
```

**Build Commands**:
```bash
cmake -B build-mcu \
  -DCMAKE_TOOLCHAIN_FILE=cmake/arm-none-eabi.cmake \
  -DBUILD_LINUX=OFF \
  -DBUILD_MCU=ON \
  -DBUILD_TESTS=OFF

cmake --build build-mcu
```

### 3.3 MCU Firmware — QEMU User-Mode (`sentinel-mcu-qemu`)

**CMake Option**: `BUILD_QEMU_MCU=ON`

This target builds the MCU application logic as an ARM Linux ELF for QEMU user-mode emulation.

| Property | Value |
|----------|-------|
| Type | ARM Linux ELF |
| Toolchain | `arm-linux-gnueabihf-gcc` |
| TCP Stack | POSIX sockets (`tcp_stack_qemu.c`) |
| HAL | Software shims (`hal_shim_posix.c`) |
| Output | `build/sentinel-mcu-qemu` |

**Source Files**:
```
src/mcu/hal/qemu/main_qemu.c           # POSIX main entry
src/mcu/sensor_acquisition.c            # Same as bare-metal
src/mcu/actuator_control.c
src/mcu/protobuf_handler.c
src/mcu/watchdog_mgr.c
src/mcu/config_store.c
src/mcu/health_reporter.c
src/mcu/tcp_stack_qemu.c               # POSIX sockets
src/mcu/hal/qemu/hal_shim_posix.c      # Software HAL
src/common/wire_frame.c
```

**Build Commands**:
```bash
cmake -B build-qemu \
  -DCMAKE_TOOLCHAIN_FILE=cmake/arm-linux-gnueabihf.cmake \
  -DBUILD_LINUX=OFF \
  -DBUILD_MCU=OFF \
  -DBUILD_QEMU_MCU=ON

cmake --build build-qemu

# Run with QEMU
qemu-arm-static build-qemu/sentinel-mcu-qemu
```

### 3.4 Unit Tests (`sentinel-tests`)

**CMake Option**: `BUILD_TESTS=ON`

| Property | Value |
|----------|-------|
| Type | Native executable (host) |
| Framework | Unity 2.5+ |
| Coverage | gcov + lcov |
| Output | `build/tests/test_*` |

**Test Files**:
```
tests/unit/test_wire_frame.c
tests/unit/test_diagnostics.c
tests/unit/test_config_store.c
tests/unit/test_actuator_control.c
tests/unit/test_logger.c
tests/unit/test_error_codes.c
tests/unit/stubs/pwm_driver_stub.c
tests/unit/stubs/sensor_proxy_stub.c
tests/unit/vendor/unity.c
```

**Build Commands**:
```bash
cmake -B build-tests \
  -DBUILD_LINUX=OFF \
  -DBUILD_MCU=OFF \
  -DBUILD_TESTS=ON \
  -DENABLE_COVERAGE=ON

cmake --build build-tests
ctest --test-dir build-tests --output-on-failure

# Generate coverage report
cd build-tests && lcov --capture --directory . --output-file coverage.info
genhtml coverage.info --output-directory coverage_html
```

---

## 4. CMake Options

| Option | Default | Description |
|--------|---------|-------------|
| `BUILD_LINUX` | ON | Build Linux gateway |
| `BUILD_MCU` | OFF | Build bare-metal MCU firmware |
| `BUILD_QEMU_MCU` | OFF | Build MCU for QEMU user-mode |
| `BUILD_TESTS` | OFF | Build unit tests |
| `ENABLE_COVERAGE` | OFF | Add `--coverage` flags |
| `CMAKE_BUILD_TYPE` | RelWithDebInfo | Optimization level |

**Mutual Exclusivity**: `BUILD_MCU` and `BUILD_QEMU_MCU` should not both be ON.

---

## 5. Compiler Flags

### 5.1 Warning Flags (All Targets)

Applied to all targets for MISRA C:2012 compatibility:

```cmake
add_compile_options(
    -Wall -Wextra -Wpedantic
    -Wshadow -Wconversion -Wsign-conversion
    -Wdouble-promotion -Wformat=2 -Wundef
    -Wstrict-prototypes -Wmissing-prototypes
    -Wredundant-decls -Wnested-externs
    -Wno-unused-parameter
)
```

### 5.2 MCU-Specific Flags

```cmake
# CPU architecture
set(CMAKE_C_FLAGS_INIT "-mcpu=cortex-m33 -mthumb -mfloat-abi=hard -mfpu=fpv5-sp-d16")

# Size optimization
set(CMAKE_C_FLAGS_INIT "${CMAKE_C_FLAGS_INIT} -ffunction-sections -fdata-sections")

# Linker flags
set(CMAKE_EXE_LINKER_FLAGS_INIT "-Wl,--gc-sections -specs=nosys.specs -specs=nano.specs")
```

---

## 6. Preprocessor Defines

### 6.1 Linux Build

| Define | Value | Purpose |
|--------|-------|---------|
| `SENTINEL_PLATFORM_LINUX` | 1 | Platform selection |
| `SENTINEL_VERSION` | `"1.0.0"` | From CMake `project(VERSION)` |
| `_POSIX_C_SOURCE` | `200809L` | POSIX API level |

### 6.2 MCU Build (Bare-Metal)

| Define | Value | Purpose |
|--------|-------|---------|
| `SENTINEL_PLATFORM_MCU` | 1 | Platform selection |
| `STM32U575xx` | 1 | Device selection |
| `__HEAP_SIZE` | `0` | Enforce zero heap |
| `_REENT_SMALL` | 1 | Minimal newlib |
| `PB_NO_ERRMSG` | 1 | nanopb: no error strings |
| `PB_BUFFER_ONLY` | 1 | nanopb: buffer mode |

### 6.3 QEMU MCU Build

| Define | Value | Purpose |
|--------|-------|---------|
| `SENTINEL_PLATFORM_MCU` | 1 | Platform selection |
| `SENTINEL_PLATFORM_QEMU` | 1 | QEMU detection |
| `SENTINEL_QEMU_USERMODE` | 1 | User-mode QEMU |
| `_POSIX_C_SOURCE` | `200809L` | POSIX API |
| `PB_NO_ERRMSG` | 1 | nanopb config |
| `PB_BUFFER_ONLY` | 1 | nanopb config |

---

## 7. Toolchain Files

### 7.1 ARM Bare-Metal (`cmake/arm-none-eabi.cmake`)

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
set(CMAKE_TRY_COMPILE_TARGET_TYPE STATIC_LIBRARY)

# No heap
add_compile_definitions(_REENT_SMALL __HEAP_SIZE=0)
```

### 7.2 AArch64 Linux (`cmake/aarch64-linux.cmake`)

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

### 7.3 ARM 32-bit Linux (`cmake/arm-linux-gnueabihf.cmake`)

```cmake
set(CMAKE_SYSTEM_NAME Linux)
set(CMAKE_SYSTEM_PROCESSOR arm)
set(CMAKE_C_COMPILER arm-linux-gnueabihf-gcc)
set(CMAKE_CXX_COMPILER arm-linux-gnueabihf-g++)
set(CMAKE_FIND_ROOT_PATH /usr/arm-linux-gnueabihf)
set(CMAKE_FIND_ROOT_PATH_MODE_PROGRAM NEVER)
set(CMAKE_FIND_ROOT_PATH_MODE_LIBRARY ONLY)
set(CMAKE_FIND_ROOT_PATH_MODE_INCLUDE ONLY)
```

---

## 8. Linker Script (MCU)

File: `src/mcu/stm32u575.ld`

### 8.1 Memory Regions

| Region | Start | Size | Attributes |
|--------|-------|------|-----------|
| FLASH | 0x08000000 | 192K | rx |
| CONFIG | 0x08030000 | 4K | r |
| RAM | 0x20000000 | 256K | rwx |

### 8.2 Sections

| Section | Region | Contents |
|---------|--------|----------|
| `.isr_vector` | FLASH | Interrupt vector table (first) |
| `.text` | FLASH | Code |
| `.rodata` | FLASH | Constants |
| `.data` | RAM (from FLASH) | Initialized variables |
| `.bss` | RAM | Zero-initialized variables |
| `._stack` | RAM (top) | Stack (32 KB) |

**No `.heap` section** — enforces static allocation.

### 8.3 Key Assertions

```ld
ASSERT(. <= ORIGIN(FLASH) + LENGTH(FLASH), "Firmware exceeds flash")
ASSERT(_stack_size >= 0x8000, "Stack too small (need 32KB)")
```

---

## 9. Version Header Generation

### 9.1 Template File

`config/version.h.in`:
```c
#ifndef SENTINEL_VERSION_H
#define SENTINEL_VERSION_H

#define SENTINEL_VERSION_MAJOR  @PROJECT_VERSION_MAJOR@
#define SENTINEL_VERSION_MINOR  @PROJECT_VERSION_MINOR@
#define SENTINEL_VERSION_PATCH  @PROJECT_VERSION_PATCH@
#define SENTINEL_VERSION        "@PROJECT_VERSION@"

#endif /* SENTINEL_VERSION_H */
```

### 9.2 CMake Configuration

```cmake
project(sentinel-gateway VERSION 1.0.0 LANGUAGES C ASM)

configure_file(
    ${CMAKE_SOURCE_DIR}/config/version.h.in
    ${CMAKE_BINARY_DIR}/generated/sentinel_version.h
    @ONLY
)
include_directories(${CMAKE_BINARY_DIR}/generated)
```

---

## 10. Docker Build

### 10.1 Dockerfile.build-linux

```dockerfile
FROM ubuntu:24.04 AS toolchain

RUN apt-get update && apt-get install -y --no-install-recommends \
    gcc-aarch64-linux-gnu \
    g++-aarch64-linux-gnu \
    cmake make \
    && rm -rf /var/lib/apt/lists/*

# Handle UID collision (Ubuntu 24.04 has uid 1000 user)
RUN useradd -m builder || true
USER builder
WORKDIR /workspace

FROM toolchain AS build
COPY --chown=builder:builder . /workspace/

RUN cmake -B build \
      -DCMAKE_TOOLCHAIN_FILE=cmake/aarch64-linux.cmake \
      -DBUILD_LINUX=ON -DBUILD_MCU=OFF -DBUILD_TESTS=OFF && \
    cmake --build build

# Extract artifact
FROM scratch AS artifact
COPY --from=build /workspace/build/sentinel-gw /
```

### 10.2 Dockerfile.build-mcu

```dockerfile
FROM ubuntu:24.04 AS toolchain

RUN apt-get update && apt-get install -y --no-install-recommends \
    cmake make \
    gcc-arm-none-eabi \
    libnewlib-arm-none-eabi \
    && rm -rf /var/lib/apt/lists/*

RUN useradd -m builder || true
USER builder
WORKDIR /workspace

FROM toolchain AS build
COPY --chown=builder:builder . /workspace/

RUN cmake -B build \
      -DCMAKE_TOOLCHAIN_FILE=cmake/arm-none-eabi.cmake \
      -DBUILD_LINUX=OFF -DBUILD_MCU=ON && \
    cmake --build build

FROM scratch AS artifact
COPY --from=build /workspace/build/sentinel-mcu.elf /
COPY --from=build /workspace/build/sentinel-mcu.bin /
```

### 10.3 Docker Build Commands

```bash
# Build Linux gateway
docker build -f docker/Dockerfile.build-linux -t sentinel-linux .
docker run --rm sentinel-linux cat /sentinel-gw > build/sentinel-gw
chmod +x build/sentinel-gw

# Build MCU firmware
docker build -f docker/Dockerfile.build-mcu -t sentinel-mcu .
docker run --rm sentinel-mcu cat /sentinel-mcu.elf > build/sentinel-mcu.elf

# Build QEMU MCU
docker build -f docker/Dockerfile.build-mcu-qemu -t sentinel-mcu-qemu .
docker run --rm sentinel-mcu-qemu cat /sentinel-mcu-qemu > build/sentinel-mcu-qemu
chmod +x build/sentinel-mcu-qemu
```

---

## 11. Required C11 Headers

MCU bare-metal code requires explicit includes (not automatically available in freestanding environment):

```c
#include <stdint.h>   /* uint8_t, uint32_t, etc. */
#include <stddef.h>   /* size_t, NULL */
#include <stdbool.h>  /* bool, true, false */
#include <string.h>   /* memcpy, memset, strlen */
```

**Do NOT assume these are implicitly included.**

---

## 12. Traceability

| Requirement | Coverage |
|-------------|----------|
| SYS-072 | Static memory enforcement (Section 6.2, 8) |
| SYS-056, SYS-057 | Version header generation (Section 9) |
| STKH-017 | Build system specification (full document) |
| SYS-079, SYS-080 | MISRA flags (Section 5.1) |
