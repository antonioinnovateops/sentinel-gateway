---
title: "SIL Environment Specification"
document_id: "SIL-001"
project: "Sentinel Gateway"
version: "2.0.0"
date: 2026-03-18
status: "Approved"
aspice_process: "SWE.6 Software Qualification Test"
---

# SIL Environment Specification — Sentinel Gateway

## 1. Purpose

This document specifies the Software-in-the-Loop (SIL) test environment. All verification activities from unit tests through system qualification execute within this environment.

**Version 2.0 Note**: This document reflects the **actual implemented architecture** using QEMU **user-mode emulation** (not full system emulation). Key differences from v1.0:
- MCU runs as ARM Linux ELF via `qemu-arm-static` (not bare-metal on netduinoplus2)
- TCP uses POSIX sockets directly (no lwIP, no USB CDC-ECM emulation)
- Both binaries run on host with host networking (no TAP/bridge needed)

---

## 2. SIL Architecture

### 2.1 Key Architecture Decision

**Why User-Mode QEMU (not Full System Emulation)?**

The original v1.0 spec called for:
- QEMU `netduinoplus2` machine for MCU (bare-metal)
- QEMU `virt` machine for Linux gateway
- TAP interfaces and network bridges for CDC-ECM simulation

This was abandoned because:
1. **lwIP TCP stack complexity**: Bare-metal TCP requires lwIP, which adds ~50KB code and significant debugging effort
2. **QEMU USB CDC-ECM limitations**: CDC-ECM emulation is incomplete in QEMU
3. **Network bridge setup**: Requires root privileges, complicates CI/CD

**Implemented Solution**: QEMU User-Mode Emulation
- MCU firmware compiled as ARM Linux ELF (not bare-metal)
- Runs via `qemu-arm-static` with direct host networking
- Uses POSIX sockets for TCP (no lwIP)
- Same application logic (sensor, actuator, health) as real MCU

### 2.2 Environment Components

| Component | Implementation | Role |
|-----------|---------------|------|
| Linux Gateway | Native x86_64 ELF OR `qemu-aarch64-static` | Runs `sentinel-gw` |
| MCU Firmware | ARM Linux ELF via `qemu-arm-static` | Runs `sentinel-mcu-qemu` |
| Network | Host loopback (127.0.0.1) | Direct TCP between processes |
| Test Harness | Python pytest | Test orchestration |
| HAL Shims | `src/mcu/hal/qemu/hal_shim_posix.c` | Software HAL for QEMU |

### 2.3 Network Topology

| Endpoint | IP Address | Notes |
|----------|-----------|-------|
| Linux Gateway | 127.0.0.1 | Host loopback |
| MCU Firmware | 127.0.0.1 | Same host |
| Test Harness | 127.0.0.1 | Same host |

**Port Allocation**:

| Port | Listener | Connector | Purpose |
|------|----------|-----------|---------|
| 5000 | MCU (qemu-arm) | Linux Gateway | Commands |
| 5001 | Linux Gateway | MCU (qemu-arm) | Telemetry |
| 5002 | Linux Gateway | Test Harness / nc | Diagnostics |

**Environment Variables**:
```bash
export SENTINEL_MCU_HOST=127.0.0.1  # Override default 192.168.7.2
```

---

## 3. Build Targets

### 3.1 MCU for QEMU User-Mode

**Target**: `sentinel-mcu-qemu`
**Toolchain**: ARM Linux cross-compiler (`arm-linux-gnueabihf-gcc`)
**Output**: ARM Linux ELF (not bare-metal)

```bash
cmake -B build-qemu \
  -DBUILD_LINUX=OFF \
  -DBUILD_MCU=OFF \
  -DBUILD_QEMU_MCU=ON \
  -DCMAKE_TOOLCHAIN_FILE=cmake/arm-linux-gnueabihf.cmake

cmake --build build-qemu
# Output: build-qemu/sentinel-mcu-qemu
```

**Source Files** (from CMakeLists.txt):
```
src/mcu/hal/qemu/main_qemu.c          # POSIX main (not bare-metal)
src/mcu/sensor_acquisition.c           # Same as real MCU
src/mcu/actuator_control.c             # Same as real MCU
src/mcu/protobuf_handler.c             # Same as real MCU
src/mcu/watchdog_mgr.c                 # Same as real MCU
src/mcu/config_store.c                 # Same as real MCU
src/mcu/health_reporter.c              # Same as real MCU
src/mcu/tcp_stack_qemu.c               # POSIX sockets (not lwIP)
src/mcu/hal/qemu/hal_shim_posix.c      # Software HAL stubs
src/common/wire_frame.c                # Shared
```

**Key Defines**:
```c
SENTINEL_PLATFORM_MCU=1
SENTINEL_PLATFORM_QEMU=1
SENTINEL_QEMU_USERMODE=1
_POSIX_C_SOURCE=200809L
```

### 3.2 Linux Gateway (Native)

For SIL testing, build Linux gateway natively (not cross-compiled):

```bash
cmake -B build-linux \
  -DBUILD_LINUX=ON \
  -DBUILD_MCU=OFF \
  -DBUILD_TESTS=OFF

cmake --build build-linux
# Output: build-linux/sentinel-gw
```

### 3.3 Unit Tests

```bash
cmake -B build-tests \
  -DBUILD_LINUX=OFF \
  -DBUILD_MCU=OFF \
  -DBUILD_TESTS=ON \
  -DENABLE_COVERAGE=ON

cmake --build build-tests
ctest --test-dir build-tests --output-on-failure
```

---

## 4. Docker Container

### 4.1 Dockerfile.qemu-sil

```dockerfile
FROM ubuntu:24.04

# QEMU user-mode and dependencies
RUN apt-get update && apt-get install -y --no-install-recommends \
    qemu-user-static \
    binfmt-support \
    libc6-arm64-cross \
    libc6-armhf-cross \
    python3 python3-pip python3-venv \
    gcc cmake make \
    net-tools procps strace file \
    && rm -rf /var/lib/apt/lists/*

# Python test deps
RUN pip3 install --break-system-packages \
    pytest==8.1.1 \
    pytest-timeout==2.3.1

# Non-root user (with UID collision handling)
RUN useradd -m builder || true
USER builder
WORKDIR /workspace

# Volumes:
# ./build-linux/     -> /workspace/artifacts/linux/
# ./build-qemu/      -> /workspace/artifacts/mcu-qemu/
# ./tests/           -> /workspace/tests/
# ./results/         -> /workspace/results/

ENV SENTINEL_MCU_HOST=127.0.0.1
CMD ["python3", "-m", "pytest", "/workspace/tests/qemu/", "-v", "--timeout=120"]
```

### 4.2 Running SIL Tests

```bash
# Build all targets
docker build -f docker/Dockerfile.build-linux -t sentinel-linux .
docker build -f docker/Dockerfile.build-mcu-qemu -t sentinel-mcu-qemu .

# Extract artifacts
docker run --rm sentinel-linux cat /workspace/output/sentinel-gw > build/sentinel-gw
docker run --rm sentinel-mcu-qemu cat /workspace/output/sentinel-mcu-qemu > build/sentinel-mcu-qemu
chmod +x build/sentinel-gw build/sentinel-mcu-qemu

# Run SIL tests
docker run --rm \
  -v $(pwd)/build:/workspace/artifacts \
  -v $(pwd)/tests:/workspace/tests \
  -v $(pwd)/results:/workspace/results \
  sentinel-qemu-sil
```

---

## 5. HAL Shim Architecture

### 5.1 QEMU HAL Shims

File: `src/mcu/hal/qemu/hal_shim_posix.c`

These shims replace hardware drivers with pure software implementations:

| Driver | Real Hardware | QEMU Shim |
|--------|--------------|-----------|
| ADC | STM32 ADC registers | Simulated values with drift |
| PWM | TIM2 compare registers | In-memory tracking |
| Flash | Flash memory @ 0x08030000 | RAM array (4KB) |
| GPIO | Port registers | Boolean state |
| Watchdog | IWDG peripheral | No-op |
| SysTick | Hardware timer | `usleep()` approximation |

### 5.2 ADC Simulation

```c
/* hal_shim_posix.c */
static uint32_t g_adc_base_values[ADC_MAX_CHANNELS] = {2048, 1024, 3000, 500};
static uint32_t g_adc_read_count = 0;

sentinel_err_t adc_read_channel(uint8_t channel, uint32_t *value) {
    /* Simulate drift: ±5 counts based on read count */
    int32_t drift = ((int32_t)(g_adc_read_count % 11) - 5);
    int32_t val = (int32_t)g_adc_base_values[channel] + drift;
    if (val < 0) val = 0;
    if (val > 4095) val = 4095;
    *value = (uint32_t)val;
    g_adc_read_count++;
    return SENTINEL_OK;
}
```

### 5.3 PWM Simulation

```c
static float g_pwm_duty[PWM_MAX_CHANNELS] = {0};

sentinel_err_t pwm_set_duty(uint8_t channel, float percent) {
    g_pwm_duty[channel] = percent;
    return SENTINEL_OK;
}

sentinel_err_t pwm_get_duty(uint8_t channel, float *percent) {
    *percent = g_pwm_duty[channel];
    return SENTINEL_OK;
}
```

### 5.4 Flash Simulation

```c
static uint8_t g_simulated_flash[FLASH_CONFIG_SIZE] __attribute__((aligned(4)));

/* Initialized to 0xFF (erased state) */
sentinel_err_t flash_init(void) {
    memset(g_simulated_flash, 0xFF, sizeof(g_simulated_flash));
    return SENTINEL_OK;
}

/* Read/write to RAM array */
sentinel_err_t flash_read(uint32_t addr, uint8_t *data, size_t len) {
    uint32_t offset = addr - FLASH_CONFIG_ADDR;
    memcpy(data, &g_simulated_flash[offset], len);
    return SENTINEL_OK;
}
```

**Note**: Flash contents are **not persistent** across process restarts in QEMU user-mode.

---

## 6. TCP Stack for QEMU

### 6.1 Architecture

File: `src/mcu/tcp_stack_qemu.c`

Unlike bare-metal MCU (which would use lwIP), the QEMU version uses standard POSIX sockets:

```c
#include <sys/socket.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <arpa/inet.h>
#include <fcntl.h>
#include <sys/select.h>
```

### 6.2 Connection Model

**Command Port (5000)**:
- MCU creates listening socket on 0.0.0.0:5000
- Linux gateway connects
- Single client (new connection replaces old)
- Non-blocking with `select()` for multiplexing

**Telemetry Port (5001)**:
- MCU connects to `127.0.0.1:5001` (or `SENTINEL_MCU_HOST`)
- Linux gateway listens
- Reconnect on failure with exponential backoff

### 6.3 Known QEMU Limitations

| Issue | Description | Workaround |
|-------|-------------|------------|
| epoll_wait | QEMU aarch64 user-mode has epoll issues | Use `select()` instead |
| Timing precision | `usleep()` not precise | Acceptable for SIL |
| USB | No USB emulation | Use POSIX sockets |
| Watchdog | No hardware watchdog | Watchdog is no-op |

---

## 7. Test Harness

### 7.1 Test File Structure

```
tests/
├── qemu/
│   ├── __init__.py
│   ├── conftest.py          # Pytest fixtures
│   └── test_qemu_sil.py     # SIL integration tests
├── unit/
│   ├── stubs/               # HAL stubs for host testing
│   ├── vendor/unity.{c,h}   # Unity test framework
│   ├── test_wire_frame.c
│   ├── test_diagnostics.c
│   ├── test_config_store.c
│   └── ...
└── integration/
    └── test_e2e.py          # End-to-end tests
```

### 7.2 QEMU Test Fixture

```python
# tests/qemu/conftest.py
import pytest
import subprocess
import socket
import time

@pytest.fixture(scope="session")
def mcu_process():
    """Start MCU firmware in QEMU user-mode."""
    proc = subprocess.Popen(
        ["qemu-arm-static", "/workspace/artifacts/mcu-qemu/sentinel-mcu-qemu"],
        stdout=subprocess.PIPE,
        stderr=subprocess.PIPE
    )
    # Wait for TCP port 5000 to be ready
    for _ in range(50):
        try:
            s = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
            s.connect(("127.0.0.1", 5000))
            s.close()
            break
        except:
            time.sleep(0.1)
    yield proc
    proc.terminate()
    proc.wait()
```

### 7.3 Sample SIL Test

```python
# tests/qemu/test_qemu_sil.py
import socket
import struct

def test_wire_frame_round_trip(mcu_process):
    """Test sending a command and receiving response."""
    sock = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
    sock.connect(("127.0.0.1", 5000))
    sock.settimeout(5.0)

    # Send ActuatorCommand (type=0x10)
    payload = struct.pack("<Bf", 0, 50.0)  # actuator_id=0, percent=50.0
    header = struct.pack("<IB", len(payload), 0x10)
    sock.sendall(header + payload)

    # Receive ActuatorResponse (type=0x11)
    hdr = sock.recv(5)
    length, msg_type = struct.unpack("<IB", hdr)
    assert msg_type == 0x11
    payload = sock.recv(length)
    # Verify response...
    sock.close()
```

---

## 8. Fault Injection (SIL)

### 8.1 Available Fault Injection Methods

| Fault | Method | Expected Behavior |
|-------|--------|-------------------|
| Communication loss | Kill MCU process | Linux detects timeout, enters degraded state |
| Invalid message | Send malformed wire frame | MCU rejects, sends error response |
| Out-of-range command | Send actuator value > 100% | MCU clamps to limit |
| Protocol error | Send unknown message type | MCU ignores or logs error |
| TCP disconnect | Close socket mid-message | Both sides handle partial frame |

### 8.2 CRC Corruption Test

**Important**: The wire frame does NOT have CRC. To test corruption handling:

1. Corrupt payload data (not header) at offset 5+
2. MCU will fail to decode protobuf → enter fail-safe
3. Original test corrupted offset 10 which was header → moved to offset 8 (payload area)

```python
def test_corrupted_payload(mcu_process):
    sock = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
    sock.connect(("127.0.0.1", 5000))

    # Valid-looking header but corrupted payload
    payload = b"\xFF\xFF\xFF\xFF\xFF"  # Invalid protobuf
    header = struct.pack("<IB", len(payload), 0x10)
    sock.sendall(header + payload)

    # MCU should respond with error or enter fail-safe
    # ...
```

---

## 9. Output Artifacts

### 9.1 Test Results

| File | Format | Description |
|------|--------|-------------|
| `results/qemu-sil/junit.xml` | JUnit XML | Test results for CI |
| `results/qemu-sil/pytest.log` | Text | Full pytest output |
| `results/unit/test_results.xml` | JUnit XML | Unit test results |
| `results/coverage/lcov.info` | LCOV | Code coverage data |

### 9.2 Build Artifacts

| File | Description |
|------|-------------|
| `build/linux/sentinel-gw` | Linux gateway (native x86_64) |
| `build/qemu-mcu/sentinel-mcu-qemu` | MCU firmware (ARM Linux ELF) |
| `build/tests/test_*` | Unit test executables |

---

## 10. CI/CD Integration

### 10.1 GitHub Actions Example

```yaml
name: SIL Tests

on: [push, pull_request]

jobs:
  sil-tests:
    runs-on: ubuntu-24.04
    steps:
      - uses: actions/checkout@v4

      - name: Install QEMU
        run: |
          sudo apt-get update
          sudo apt-get install -y qemu-user-static binfmt-support

      - name: Build MCU for QEMU
        run: |
          cmake -B build-qemu -DBUILD_QEMU_MCU=ON
          cmake --build build-qemu

      - name: Build Linux Gateway
        run: |
          cmake -B build-linux -DBUILD_LINUX=ON
          cmake --build build-linux

      - name: Run SIL Tests
        run: |
          export SENTINEL_MCU_HOST=127.0.0.1
          python3 -m pytest tests/qemu/ -v --junit-xml=results/junit.xml

      - name: Upload Results
        uses: actions/upload-artifact@v4
        with:
          name: sil-results
          path: results/
```

---

## 11. Differences from Real Hardware

| Aspect | Real Hardware | QEMU SIL |
|--------|--------------|----------|
| TCP Stack | lwIP NO_SYS mode | POSIX sockets |
| USB | CDC-ECM enumeration | Not emulated |
| Watchdog | Hardware IWDG | No-op |
| Flash | Persistent NOR flash | RAM (not persistent) |
| ADC | Real sensor readings | Simulated values |
| PWM | Hardware timer outputs | Software tracking |
| Timing | Hardware SysTick | `usleep()` approximation |
| Network | USB CDC-ECM (192.168.7.x) | Host loopback (127.0.0.1) |

**Testing Implications**:
- USB enumeration timing NOT tested in SIL
- Watchdog reset recovery NOT tested in SIL
- Flash persistence NOT tested in SIL (separate test needed)
- ADC noise/drift NOT realistic

---

## 12. Traceability

| Requirement | Coverage |
|-------------|----------|
| SYS-066, SYS-067 | Boot time verification (Section 7) |
| SYS-026, SYS-028 | Fault injection (Section 8) |
| SYS-034, SYS-035 | Wire frame testing (Section 7.3) |
| STKH-016 | Testability via SIL (full document) |
| SYS-072 | Static allocation verified in unit tests |
