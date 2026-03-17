---
title: "Build System Specification"
document_id: "BSS-001"
project: "Sentinel Gateway"
version: "1.0.0"
date: 2026-03-17
status: "Approved"
aspice_process: "SWE.3 Software Detailed Design"
---

# Build System Specification — Sentinel Gateway

## 1. Overview

This document specifies the CMake-based build system. It replaces any in-tree build files (CMakeLists.txt, toolchain files, linker scripts) with a pure specification. Actual build files are generated during Phase 3 (Implementation).

## 2. Build Tool

| Property | Value |
|----------|-------|
| Build system | CMake 3.28+ |
| Generator | Unix Makefiles (default) or Ninja |
| C Standard | C11 (no extensions) |
| Languages | C only |

## 3. Build Targets

### 3.1 Linux Gateway (`sentinel-gw`)

| Property | Value |
|----------|-------|
| Type | ELF executable (aarch64) |
| Toolchain | `aarch64-linux-gnu-gcc` 13.x |
| Sysroot | Yocto SDK or Ubuntu cross-compilation packages |
| Link libraries | `protobuf-c`, `pthread` |
| Output | `build/linux/sentinel-gw` |

**Source files** (from SWAD component mapping):

| Component | Source files | Header files |
|-----------|-------------|-------------|
| SW-01 Gateway Core | `gateway_core.c` | `gateway_core.h` |
| SW-02 Protobuf Handler | `protobuf_handler.c` | `protobuf_handler.h` |
| SW-03 Sensor Proxy | `sensor_proxy.c` | `sensor_proxy.h` |
| SW-04 Actuator Proxy | `actuator_proxy.c` | `actuator_proxy.h` |
| SW-05 Health Monitor | `health_monitor.c` | `health_monitor.h` |
| SW-06 Diagnostic Server | `diagnostics.c` | `diagnostics.h` |
| SW-07 Config Manager | `config_manager.c` | `config_manager.h` |
| Shared | `tcp_transport.c`, `wire_frame.c`, `logger.c` | `sentinel_types.h`, `error_codes.h` |
| Generated | `sentinel.pb-c.c` | `sentinel.pb-c.h` |

### 3.2 MCU Firmware (`sentinel-mcu`)

| Property | Value |
|----------|-------|
| Type | ELF + flat binary (ARM Cortex-M33) |
| Toolchain | `arm-none-eabi-gcc` 13.2.Rel1 |
| CPU flags | `-mcpu=cortex-m33 -mthumb -mfloat-abi=hard -mfpu=fpv5-sp-d16` |
| Link flags | `-specs=nosys.specs -specs=nano.specs --gc-sections` |
| Linker script | `stm32u575.ld` |
| Link libraries | `lwip` (static, NO_SYS mode), `nanopb` |
| Output | `build/mcu/sentinel-mcu.elf`, `build/mcu/sentinel-mcu.bin` |
| Post-build | `arm-none-eabi-objcopy -O binary` (ELF → BIN) |
| Size report | `arm-none-eabi-size` output in build log |

**Source files** (from SWAD component mapping):

| Component | Source files | Header files |
|-----------|-------------|-------------|
| FW-01 Sensor Acquisition | `sensor_acquisition.c` | `sensor_acquisition.h` |
| FW-02 Actuator Control | `actuator_control.c` | `actuator_control.h` |
| FW-03 Protobuf Handler | `protobuf_handler.c` | `protobuf_handler.h` |
| FW-04 Communication Mgr | `tcp_stack.c` | `tcp_stack.h` |
| FW-05 Watchdog Manager | `watchdog_mgr.c` | `watchdog_mgr.h` |
| FW-06 Config Store | `config_store.c` | `config_store.h` |
| FW-07 Safety Monitor | `health_reporter.c` | `health_reporter.h` |
| HAL | `adc_driver.c`, `pwm_driver.c`, `gpio_driver.c`, `flash_driver.c`, `usb_cdc.c`, `watchdog_driver.c` | `hal/*.h` |
| Startup | `startup_stm32u575.s`, `system_stm32u575.c` | `stm32u575.h` |
| Generated | `sentinel.pb.c` | `sentinel.pb.h` |
| Main | `main.c` | — |

### 3.3 Unit Tests (`sentinel-tests`)

| Property | Value |
|----------|-------|
| Type | Native executable (host) |
| Toolchain | Host GCC (x86_64) |
| Framework | Unity 2.5+ (single-header C test framework) |
| Mock generator | CMock (optional) or hand-written mocks |
| Coverage | `--coverage` flag, `gcov` + `lcov` |
| Output | `build/tests/sentinel-tests` |

## 4. Compiler Warning Flags

Applied to all targets (MISRA C:2012 friendly):

```
-Wall -Wextra -Wpedantic
-Wshadow -Wconversion -Wsign-conversion
-Wdouble-promotion -Wformat=2 -Wundef
-Wstrict-prototypes -Wmissing-prototypes
-Wredundant-decls -Wnested-externs
-Wno-unused-parameter
-Werror  (in CI; optional locally)
```

## 5. Preprocessor Defines

### 5.1 Linux Build

| Define | Value | Purpose |
|--------|-------|---------|
| `SENTINEL_PLATFORM_LINUX` | 1 | Platform selection |
| `SENTINEL_VERSION` | `"1.0.0"` | From `config/version.h.in` |
| `_POSIX_C_SOURCE` | `200809L` | POSIX API level |
| `LOG_LEVEL` | `LOG_INFO` (default) | Runtime log verbosity |

### 5.2 MCU Build

| Define | Value | Purpose |
|--------|-------|---------|
| `SENTINEL_PLATFORM_MCU` | 1 | Platform selection |
| `STM32U575xx` | 1 | CMSIS device selection |
| `__HEAP_SIZE` | `0` | Enforce zero heap |
| `_REENT_SMALL` | 1 | Minimal newlib reentrant struct |
| `LWIP_NO_SYS` | `1` | lwIP without OS |
| `MEM_LIBC_MALLOC` | `0` | lwIP uses static pool |
| `MEMP_MEM_MALLOC` | `0` | lwIP uses static pool |
| `PB_NO_ERRMSG` | 1 | nanopb: save flash (no error strings) |
| `PB_BUFFER_ONLY` | 1 | nanopb: buffer mode only (no callbacks) |

## 6. Protobuf Code Generation

### 6.1 Linux (protobuf-c)

```bash
protoc --c_out=src/linux/generated/ \
       --proto_path=docs/design/interface_specs/proto/ \
       sentinel.proto
```

Output: `sentinel.pb-c.c`, `sentinel.pb-c.h`

### 6.2 MCU (nanopb)

```bash
nanopb_generator \
  --options-path=docs/design/interface_specs/proto/ \
  -D src/mcu/generated/ \
  docs/design/interface_specs/proto/sentinel.proto
```

Output: `sentinel.pb.c`, `sentinel.pb.h`

## 7. Version Header Generation

`config/version.h.in` template (specification):

```c
#ifndef SENTINEL_VERSION_H
#define SENTINEL_VERSION_H

#define SENTINEL_VERSION_MAJOR  @PROJECT_VERSION_MAJOR@
#define SENTINEL_VERSION_MINOR  @PROJECT_VERSION_MINOR@
#define SENTINEL_VERSION_PATCH  @PROJECT_VERSION_PATCH@
#define SENTINEL_VERSION_STRING "@PROJECT_VERSION@"

#endif /* SENTINEL_VERSION_H */
```

CMake `configure_file()` populates at build time from `project(VERSION x.y.z)`.

## 8. Linker Script Specification (MCU)

File: `stm32u575.ld`

**Memory regions**:

| Region | Start | Size | Attributes |
|--------|-------|------|-----------|
| FLASH | 0x08000000 | 192K | rx |
| CONFIG | 0x08030000 | 4K | r |
| RAM | 0x20000000 | 786K | rwx |

**Sections**:

| Section | Region | Contents |
|---------|--------|----------|
| .isr_vector | FLASH | Interrupt vector table (must be first) |
| .text | FLASH | Code |
| .rodata | FLASH | Constants, nanopb descriptors |
| .data | RAM (load from FLASH) | Initialized static variables |
| .bss | RAM | Zero-initialized static variables |
| .lwip_pool | RAM | lwIP memory pool (128 KB, aligned) |
| ._stack | RAM (top) | Stack (32 KB, grows down) |
| No .heap | — | Not defined (enforces zero malloc) |

**Assertions**:
- `ASSERT(. <= ORIGIN(FLASH) + LENGTH(FLASH), "Firmware exceeds flash")`
- `ASSERT(_stack_size >= 0x8000, "Stack too small")`

## 9. Build Options

| CMake Option | Default | Description |
|-------------|---------|-------------|
| `BUILD_LINUX` | ON | Build Linux gateway |
| `BUILD_MCU` | OFF | Build MCU firmware |
| `BUILD_TESTS` | ON | Build test suite |
| `ENABLE_MISRA` | OFF | Run cppcheck MISRA addon |
| `ENABLE_COVERAGE` | OFF | Add `--coverage` flags |
| `CMAKE_BUILD_TYPE` | `RelWithDebInfo` | Optimization level |

## 10. Traceability

| Requirement | Coverage |
|-------------|----------|
| SYS-051 | Reproducible build (containerized, pinned versions) |
| SYS-056 | Version header generation (Section 7) |
| SYS-072 | Static memory enforcement (Section 5.2, 8) |
| STKH-017 | Build system specification (full document) |
| SWE-069-1 | Watchdog driver in MCU build |
| SWE-045-1 | MISRA compliance checking (Section 4, option) |
