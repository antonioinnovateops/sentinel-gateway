---
title: "SIL Developer Guide"
document_id: "GUIDE-SIL-001"
project: "Sentinel Gateway"
version: "1.0.0"
date: 2026-03-19
status: "Active"
requirement: "[SYS-081] SIL Developer Accessibility"
---

# SIL Developer Guide — Sentinel Gateway

> Validate your implementation against the Integration Test Plan (ITP-001) with a single command.

## Quick Start

```bash
# 1. Build the cross-compiled binaries (one-time, cached)
docker-compose run --rm build-linux
docker-compose run --rm build-mcu

# 2. Run the full SIL integration test suite
docker-compose run --rm sil
```

That's it. No QEMU setup, no toolchain installs, no manual configuration.

## What Happens When You Run `sil`

```
┌──────────────────────────────────────────────────────────────┐
│                    Docker Container (sil)                     │
│                                                              │
│  1. Compiles MCU simulator from sil_mcu_sim.c (gcc, ~1s)    │
│  2. Uses pre-built sentinel-gw from build-linux step         │
│                                                              │
│  ┌─────────────────┐   TCP:5000    ┌─────────────────────┐  │
│  │   MCU Simulator  │◄────────────►│   Linux Gateway      │  │
│  │   (sil_mcu_sim)  │  cmd channel │   (sentinel-gw)      │  │
│  │                  │              │                      │  │
│  │  Simulates:      │   TCP:5001   │  Listens:            │  │
│  │  • 4 ADC sensors │─────────────►│  • 5001 telemetry    │  │
│  │  • 2 PWM outputs │  telemetry   │  • 5002 diagnostics  │  │
│  │  • Health status │              │  • 5000 commands      │  │
│  │  • Wire framing  │              │                      │  │
│  └─────────────────┘              └──────────┬───────────┘  │
│                                               │              │
│  3. pytest connects to ports, sends commands, │  TCP:5002    │
│     verifies responses against ITP-001        │  diagnostics │
│                                               ▼              │
│  ┌────────────────────────────────────────────────────────┐  │
│  │  pytest test_sil.py                                    │  │
│  │  • IT-01: Sensor data flow (SYS-001..014)              │  │
│  │  • IT-02: Actuator commands  (SYS-016..020)            │  │
│  │  • IT-03: Actuator safety    (SYS-022..025)            │  │
│  │  • IT-04: Heartbeat          (SYS-038..039)            │  │
│  │  • IT-08: Diagnostics        (SYS-046..054)            │  │
│  │  • Stability checks                                    │  │
│  └────────────────────────────────────────────────────────┘  │
└──────────────────────────────────────────────────────────────┘
```

## Test-to-Requirement Traceability

Every test maps directly to an Integration Test Plan scenario and system requirement:

| Test Class | ITP Scenario | Requirements | What It Validates |
|------------|-------------|--------------|-------------------|
| `TestIT08_DiagnosticInterface` | IT-08 | SYS-046..054 | Diagnostic commands: help, version, status, error handling |
| `TestIT01_SensorDataFlow` | IT-01 | SYS-001..014 | Sensor data: MCU → gateway → diagnostic readback |
| `TestIT02_ActuatorCommandFlow` | IT-02 | SYS-016..020 | Actuator commands: diagnostic → gateway → MCU |
| `TestIT04_HeartbeatMonitoring` | IT-04 | SYS-038..039 | MCU health status tracked by gateway |
| `TestStability` | — | — | Both processes survive the full test session |

## Reading Test Output

```
=== SIL Integration Tests ===
Test plan: ITP-001 (docs/test/integration_test_spec/)

test_sil.py::TestIT08_DiagnosticInterface::test_help PASSED              [  7%]
test_sil.py::TestIT08_DiagnosticInterface::test_version PASSED           [ 14%]
test_sil.py::TestIT08_DiagnosticInterface::test_status PASSED            [ 21%]
test_sil.py::TestIT08_DiagnosticInterface::test_unknown_command PASSED   [ 28%]
test_sil.py::TestIT01_SensorDataFlow::test_sensor_read_ch0 PASSED        [ 35%]
...
======================== 14 passed in 7.48s ========================
```

Each line tells you:
- **Test class** → which IT scenario (e.g., `TestIT01` = IT-01)
- **Test method** → what's being verified (e.g., `test_sensor_data_continuous`)
- **PASSED/FAILED** → result

JUnit XML report is saved to `results/sil/sil-results.xml` for CI integration.

## Running Individual Tests

Run a specific IT scenario:

```bash
# Just IT-01 (sensor data)
docker-compose run --rm sil bash -c \
  "cd /workspace/tests/integration && python3 -m pytest test_sil.py::TestIT01_SensorDataFlow -v"

# Just IT-08 (diagnostics)
docker-compose run --rm sil bash -c \
  "cd /workspace/tests/integration && python3 -m pytest test_sil.py::TestIT08_DiagnosticInterface -v"

# A single test
docker-compose run --rm sil bash -c \
  "cd /workspace/tests/integration && python3 -m pytest test_sil.py::TestIT02_ActuatorCommandFlow::test_actuator_command_response -v"
```

## Using the Diagnostic Port Interactively

You can manually test the diagnostic interface — useful when developing new features:

```bash
# Start the SIL environment without running tests
docker-compose run --rm sil bash -c "
  cd /workspace/tests/integration &&
  python3 -c \"
from conftest import SILEnvironment
import os, time
env = SILEnvironment(os.environ['PROJECT_ROOT'])
env.build()
env.start()
print('SIL is running. Diagnostic port: localhost:5002')
print('Press Ctrl+C to stop.')
try:
    while True: time.sleep(1)
except KeyboardInterrupt:
    env.stop()
\"
"
```

Then from another terminal:

```bash
# Connect to diagnostic port inside the container
docker exec -it <container_id> socat - TCP:127.0.0.1:5002

# Available commands:
HELP
GET_VERSION
GET_STATUS
READ_SENSOR 0
READ_SENSOR 1
SET_ACTUATOR 0 50
GET_LOG 10
```

## How the MCU Simulator Works

The MCU simulator (`tests/integration/sil_mcu_sim.c`) replaces real hardware:

| Real MCU | Simulator |
|----------|-----------|
| STM32U575 ADC peripheral | Software-simulated ADC with ±5 LSB drift |
| Hardware PWM timers | RAM-backed float values |
| lwIP TCP/IP stack | POSIX TCP sockets |
| Bare-metal super-loop | `usleep(10ms)` tick loop |
| Flash config store | In-memory struct |
| Wire frame + protobuf | Same `wire_frame.c` as production code |

The simulator runs the same state machine, message encoding, and protocol logic as the real firmware — only the HAL layer is replaced.

## Adding a New Test

To add a test for a new feature:

1. **Identify the IT scenario** — check `docs/test/integration_test_spec/integration_test_plan.md`

2. **Add a test class** in `tests/integration/test_sil.py`:

```python
class TestIT07_ConfigurationUpdate:
    """IT-07: Configuration Update Flow

    Verifies: SYS-058 to SYS-065
    """

    def test_config_update_accepted(self, sil_env):
        """Config update command produces ACK response (SYS-058)."""
        s = connect_tcp(CMD_PORT, timeout=3)
        try:
            # Build config update protobuf payload
            payload = build_config_update(channel=0, rate_hz=50)
            send_frame(s, MSG_CONFIG_UPDATE, payload)
            msg_type, _ = recv_frame(s, timeout=3)
            assert msg_type == MSG_CONFIG_RSP
        finally:
            s.close()
```

3. **Update the MCU simulator** if your feature needs new behavior in `sil_mcu_sim.c`

4. **Run**: `docker-compose run --rm sil`

## Evaluating Your Implementation

Use this checklist to validate your work:

| Step | Command | What to Check |
|------|---------|---------------|
| 1. Build | `docker-compose run --rm build-linux` | Compiles without errors |
| 2. Unit tests | `docker-compose run --rm test` | All unit tests pass |
| 3. SIL tests | `docker-compose run --rm sil` | All integration tests pass |
| 4. QEMU SIL | `docker-compose run --rm qemu-sil` | Real ARM binaries pass (optional) |
| 5. Static analysis | `docker-compose run --rm analysis` | No MISRA violations |

Or run everything:

```bash
docker-compose build && \
docker-compose run --rm build-linux && \
docker-compose run --rm build-mcu && \
docker-compose run --rm test && \
docker-compose run --rm sil && \
docker-compose run --rm analysis
```

## Troubleshooting

| Problem | Fix |
|---------|-----|
| `Port 5000 did not become ready` | Previous container still running. Run `docker-compose down` |
| `MCU sim build failed` | Missing `gcc` or `libc6-dev` in container. Rebuild: `docker-compose build sil` |
| `No pre-built gateway found` | Run `docker-compose run --rm build-linux` first |
| `test_sensor_data_continuous FAILED` | Gateway may be slow to start. Increase timeout in test or retry |
| `Permission denied` on results | Run `sudo chown -R $(id -u):$(id -g) results/` |

## File Map

| File | Purpose |
|------|---------|
| `tests/integration/test_sil.py` | Pytest test cases (run these) |
| `tests/integration/conftest.py` | SIL environment fixtures (build + lifecycle) |
| `tests/integration/sil_mcu_sim.c` | MCU simulator (compiled at test time) |
| `tests/integration/test_e2e.py` | Legacy standalone test script |
| `docker/Dockerfile.sil` | Container image with gcc, cmake, pytest |
| `docs/test/integration_test_spec/integration_test_plan.md` | Formal test plan (ITP-001) |
| `docs/design/sil_environment.md` | SIL architecture specification (SIL-001) |
| `results/sil/sil-results.xml` | JUnit XML output (generated) |
