---
title: "Unit Test Plan"
document_id: "UTP-001"
project: "Sentinel Gateway"
version: "1.0.0"
date: 2026-03-17
status: "Approved"
aspice_process: "SWE.4 Software Unit Verification"
---

# Unit Test Plan — Sentinel Gateway

## 1. Test Framework

- **MCU tests**: Unity Test Framework (C-native, no C++ dependency)
- **Linux tests**: Unity Test Framework (same framework for consistency)
- **Mocks**: Hand-written mock functions in `tests/mocks/`
- **Coverage**: gcov + lcov for coverage measurement

## 2. Test Naming Convention

`TC-<SWE_ID>-<sequence>` — e.g., `TC-SWE-002-1-1` = first test for SWE-002-1

## 3. MCU Unit Tests

### 3.1 adc_driver Tests
| Test ID | Description | SWE Req | Input | Expected |
|---------|-------------|---------|-------|----------|
| TC-SWE-001-1-1 | Init configures 4-ch scan mode | SWE-001-1 | Call init() | ADC registers configured |
| TC-SWE-001-1-2 | Init sets 12-bit resolution | SWE-001-1 | Call init() | Resolution register = 12b |
| TC-SWE-001-2-1 | Channel mapping correct | SWE-001-2 | Read all channels | CH0=temp, CH1=hum, CH2=press, CH3=light |
| TC-SWE-001-3-1 | Timer triggers conversion | SWE-001-3 | Set rate 10Hz | ADC triggered at 10Hz |
| TC-SWE-001-3-2 | Jitter within spec | SWE-001-3 | 1000 samples at 100Hz | Jitter ≤ 500µs |
| TC-SWE-011-1-1 | 4-ch scan < 100µs | SWE-011-1 | Full scan | Duration ≤ 100µs |

### 3.2 sensor_acquisition Tests
| Test ID | Description | SWE Req | Input | Expected |
|---------|-------------|---------|-------|----------|
| TC-SWE-002-1-1 | Temp: raw 0 = -40°C | SWE-002-1 | raw=0 | -40.0°C |
| TC-SWE-002-1-2 | Temp: raw 2048 = 42.5°C | SWE-002-1 | raw=2048 | 42.5°C ±0.5 |
| TC-SWE-002-1-3 | Temp: raw 4095 = 125°C | SWE-002-1 | raw=4095 | 125.0°C ±0.5 |
| TC-SWE-003-1-1 | Humidity: raw 0 = 0% | SWE-003-1 | raw=0 | 0.0% |
| TC-SWE-003-1-2 | Humidity: raw 2048 = 50% | SWE-003-1 | raw=2048 | 50.0% ±1.0 |
| TC-SWE-003-1-3 | Humidity: raw 4095 = 100% | SWE-003-1 | raw=4095 | 100.0% |
| TC-SWE-004-1-1 | Pressure: raw 0 = 300 hPa | SWE-004-1 | raw=0 | 300 hPa |
| TC-SWE-004-1-2 | Pressure: raw 2048 = 700 hPa | SWE-004-1 | raw=2048 | 700 ±5 hPa |
| TC-SWE-004-1-3 | Pressure: raw 4095 = 1100 hPa | SWE-004-1 | raw=4095 | 1100 hPa |
| TC-SWE-005-1-1 | Light: raw 0 = 0 lux | SWE-005-1 | raw=0 | 0 lux |
| TC-SWE-005-1-2 | Light: raw 2048 = 50000 lux | SWE-005-1 | raw=2048 | 50000 ±1000 lux |
| TC-SWE-005-1-3 | Light: raw 4095 = 100000 lux | SWE-005-1 | raw=4095 | 100000 lux |
| TC-SWE-009-1-1 | Independent channel rates | SWE-009-1 | CH0=100Hz, CH3=1Hz | Both rates respected |
| TC-SWE-010-1-1 | Jitter at 100Hz ≤ 500µs | SWE-010-1 | 1000 samples | Jitter ≤ 500µs |

### 3.3 actuator_control Tests
| Test ID | Description | SWE Req | Input | Expected |
|---------|-------------|---------|-------|----------|
| TC-SWE-016-1-1 | Fan PWM init at 0% | SWE-016-1 | Init | 0% duty, 25kHz |
| TC-SWE-016-1-2 | Fan PWM set 50% | SWE-016-1 | set_duty(50) | PWM = 50% |
| TC-SWE-016-2-1 | PWM update within 40µs | SWE-016-2 | set_duty(75) | Register updated < 40µs |
| TC-SWE-017-1-1 | Valve PWM init at 0% | SWE-017-1 | Init | 0% duty, 25kHz |
| TC-SWE-022-1-1 | Reject value 101% | SWE-022-1 | cmd=101% | ERR_OUT_OF_RANGE |
| TC-SWE-022-1-2 | Reject value -1% | SWE-022-1 | cmd=-1% | ERR_OUT_OF_RANGE |
| TC-SWE-022-1-3 | Accept value at max | SWE-022-1 | cmd=95% (max) | OK |
| TC-SWE-022-1-4 | Accept value at min | SWE-022-1 | cmd=0% (min) | OK |
| TC-SWE-022-2-1 | Readback matches written | SWE-022-2 | set+read | Values match |
| TC-SWE-022-2-2 | Readback mismatch → fail-safe | SWE-022-2 | Corrupt register | Fail-safe |
| TC-SWE-023-1-1 | Fan clamped to max 95% | SWE-023-1 | cmd=100% | Applied=95% |
| TC-SWE-023-1-2 | Fan at 95% accepted | SWE-023-1 | cmd=95% | Applied=95% |
| TC-SWE-024-1-1 | Valve limits enforced | SWE-024-1 | cmd at boundaries | Clamped correctly |
| TC-SWE-021-1-1 | 50 cmds/sec accepted | SWE-021-1 | 50 cmds in 1s | All OK |
| TC-SWE-021-1-2 | 51st cmd rejected | SWE-021-1 | 51 cmds in 1s | RATE_LIMITED |
| TC-SWE-019-1-1 | Response for valid cmd | SWE-019-1 | Valid cmd | Response with OK |
| TC-SWE-019-1-2 | Response for invalid cmd | SWE-019-1 | Invalid cmd | Response with error |
| TC-SWE-026-1-1 | Comm loss → fail-safe at 3s | SWE-026-1 | No msgs for 3s | Actuators at 0% |
| TC-SWE-026-1-2 | Timer resets on activity | SWE-026-1 | Msg received | Timer reset |
| TC-SWE-027-1-1 | Boot → safe state before comms | SWE-027-1 | Boot sequence | PWM=0% before TCP |
| TC-SWE-028-1-1 | Corrupt protobuf → fail-safe | SWE-028-1 | Corrupted bytes | Fail-safe within 100ms |
| TC-SWE-028-1-2 | State changes to FAILSAFE | SWE-028-1 | Decode error | state=FAILSAFE |

### 3.4 config_store Tests
| Test ID | Description | SWE Req | Input | Expected |
|---------|-------------|---------|-------|----------|
| TC-SWE-058-1-1 | Valid config applied | SWE-058-1 | ConfigUpdate msg | Rates changed |
| TC-SWE-059-1-1 | Valid auth → limits updated | SWE-059-1 | Valid token | Limits changed |
| TC-SWE-059-1-2 | Invalid auth → rejected | SWE-059-1 | Bad token | AUTH_FAILED |
| TC-SWE-060-1-1 | Config survives power cycle | SWE-060-1 | Save → reload | Values match |
| TC-SWE-060-1-2 | CRC validates after reload | SWE-060-1 | Save → read | CRC OK |
| TC-SWE-062-1-1 | Defaults on blank flash | SWE-062-1 | Blank flash | Default config |
| TC-SWE-062-1-2 | Defaults on CRC failure | SWE-062-1 | Corrupt flash | Default config |
| TC-SWE-063-1-1 | Rate 0 Hz rejected | SWE-063-1 | rate=0 | RANGE error |
| TC-SWE-063-1-2 | Rate 101 Hz rejected | SWE-063-1 | rate=101 | RANGE error |
| TC-SWE-064-1-1 | Limit -1% rejected | SWE-064-1 | min=-1 | RANGE error |
| TC-SWE-064-1-2 | Limit 101% rejected | SWE-064-1 | max=101 | RANGE error |
| TC-SWE-064-1-3 | Min > max rejected | SWE-064-1 | min=80, max=50 | RANGE error |
| TC-SWE-065-1-1 | Valid CRC → config loaded | SWE-065-1 | Valid flash | Config loaded |
| TC-SWE-065-1-2 | Invalid CRC → defaults | SWE-065-1 | Bad CRC | Defaults loaded |

### 3.5 watchdog_mgr Tests
| Test ID | Description | SWE Req | Input | Expected |
|---------|-------------|---------|-------|----------|
| TC-SWE-069-1-1 | IWDG init with 2s timeout | SWE-069-1 | Init | IWDG running |
| TC-SWE-070-1-1 | Feed every ≤500ms | SWE-070-1 | Main loop | Feed interval OK |
| TC-SWE-071-1-1 | Reset counter increments | SWE-071-1 | WDG reset | Counter + 1 |

### 3.6 Memory Constraint Tests
| Test ID | Description | SWE Req | Input | Expected |
|---------|-------------|---------|-------|----------|
| TC-SWE-072-1-1 | No malloc in MCU code | SWE-072-1 | Static analysis | Zero heap refs |
| TC-SWE-073-1-1 | Stack ≤ 32 KB | SWE-073-1 | Stack analysis | ≤ 32768 bytes |

## 4. Linux Unit Tests

### 4.1 sensor_proxy Tests
| Test ID | Description | SWE Req | Input | Expected |
|---------|-------------|---------|-------|----------|
| TC-SWE-013-1-1 | SensorData decoded correctly | SWE-013-1 | Valid protobuf | All fields extracted |
| TC-SWE-013-1-2 | Sequence gap logged | SWE-013-1 | Gap in seq nums | Warning logged |
| TC-SWE-013-2-1 | JSON lines written | SWE-013-2 | Sensor data | Valid JSON line |
| TC-SWE-013-2-2 | Timestamp ≤ 1ms resolution | SWE-013-2 | Log entry | ms precision |

### 4.2 health_monitor Tests
| Test ID | Description | SWE Req | Input | Expected |
|---------|-------------|---------|-------|----------|
| TC-SWE-039-1-1 | 3 missed HB → COMM_LOSS | SWE-039-1 | No HB for 3s | Event raised |
| TC-SWE-039-1-2 | Event within 3.5s | SWE-039-1 | Timer check | ≤ 3.5s |
| TC-SWE-042-1-1 | USB power cycle triggered | SWE-042-1 | Comm loss | QEMU cmd sent |
| TC-SWE-043-1-1 | Reconnect with backoff | SWE-043-1 | TCP drop | Backoff delays correct |
| TC-SWE-044-1-1 | State sync after reconnect | SWE-044-1 | Reconnect | SyncRequest sent |
| TC-SWE-045-1-1 | 3 failures → FAULT | SWE-045-1 | 3 recovery fails | FAULT state |

### 4.3 diagnostics Tests
| Test ID | Description | SWE Req | Input | Expected |
|---------|-------------|---------|-------|----------|
| TC-SWE-046-2-1 | Accept connection on 5002 | SWE-046-2 | TCP connect | Connected |
| TC-SWE-047-1-1 | READ_SENSOR valid channel | SWE-047-1 | READ_SENSOR 0 | OK + reading |
| TC-SWE-047-1-2 | READ_SENSOR invalid channel | SWE-047-1 | READ_SENSOR 5 | ERROR |
| TC-SWE-048-1-1 | SET_ACTUATOR valid | SWE-048-1 | SET_ACTUATOR 0 50 | OK |
| TC-SWE-048-1-2 | SET_ACTUATOR error | SWE-048-1 | SET_ACTUATOR 0 200 | ERROR |
| TC-SWE-049-1-1 | GET_STATUS complete | SWE-049-1 | GET_STATUS | All fields present |
| TC-SWE-050-1-1 | GET_VERSION both | SWE-050-1 | GET_VERSION | Linux + MCU versions |
| TC-SWE-051-1-1 | RESET_MCU initiated | SWE-051-1 | RESET_MCU | OK MCU_RESET_INITIATED |
| TC-SWE-054-1-1 | GET_LOG returns entries | SWE-054-1 | GET_LOG 10 | 10 entries |

### 4.4 logger Tests
| Test ID | Description | SWE Req | Input | Expected |
|---------|-------------|---------|-------|----------|
| TC-SWE-015-1-1 | Rotation at 100MB | SWE-015-1 | 100MB data | File rotated |
| TC-SWE-052-1-1 | All event types logged | SWE-052-1 | Various events | Valid JSON |
| TC-SWE-053-1-1 | Logs persist after restart | SWE-053-1 | Write → restart | Logs present |
| TC-SWE-055-1-1 | Severity filtering works | SWE-055-1 | Level=WARNING | DEBUG/INFO suppressed |

## 5. Coverage Targets

| Metric | Target | Rationale |
|--------|--------|-----------|
| Statement coverage | ≥ 95% | ASPICE SWE.4 best practice |
| Branch coverage | ≥ 90% | High confidence in path coverage |
| MC/DC (ASIL-B functions) | 100% | ISO 26262 ASIL-B requirement |

## 6. Total Test Count

| Category | Count |
|----------|-------|
| MCU ADC driver | 6 |
| MCU sensor calibration | 12 |
| MCU sample rate | 2 |
| MCU actuator control | 22 |
| MCU config store | 14 |
| MCU watchdog | 3 |
| MCU memory | 2 |
| Linux sensor proxy | 4 |
| Linux health monitor | 6 |
| Linux diagnostics | 9 |
| Linux logger | 4 |
| **TOTAL** | **84** |
