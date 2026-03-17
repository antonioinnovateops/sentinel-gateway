---
title: "Stakeholder Requirements"
document_id: "STKH-REQ-001"
version: "1.0.0"
date: 2026-03-17
status: "Approved"
author: "Antonio Stepien (Product Owner)"
aspice_process: "SYS.1 Stakeholder Requirements Elicitation"
---

# Stakeholder Requirements — Sentinel Gateway

## 1. Introduction

This document captures stakeholder needs and expectations for the Sentinel Gateway product. These requirements are the highest-level inputs to the development process and drive all downstream system and software requirements.

## 2. Stakeholders

| ID | Stakeholder | Role | Concerns |
|----|-------------|------|----------|
| SH-01 | System Integrator | Deploys gateway in field | Reliability, ease of integration, diagnostics |
| SH-02 | Application Developer | Develops custom logic on Linux side | Clean APIs, documentation, extensibility |
| SH-03 | Maintenance Engineer | Services deployed units | Remote diagnostics, firmware updates, logging |
| SH-04 | Safety Assessor | Validates process compliance | Traceability, documentation, safety analysis |
| SH-05 | Sensor/Actuator Vendor | Provides peripheral devices | Interface compatibility, timing guarantees |

## 3. Stakeholder Requirements

### 3.1 Data Acquisition

**[STKH-001] Multi-Sensor Data Collection**
- **Description**: The system shall collect data from multiple environmental sensors simultaneously
- **Rationale**: Industrial monitoring requires concurrent measurement of temperature, humidity, pressure, and ambient light
- **Priority**: Must Have
- **Acceptance**: ≥4 sensor channels, configurable sample rates

**[STKH-002] Real-Time Sensor Sampling**
- **Description**: Sensor data shall be sampled with deterministic timing (jitter ≤ 1ms)
- **Rationale**: Process control applications require predictable measurement intervals
- **Priority**: Must Have
- **Acceptance**: Demonstrated jitter ≤ 1ms at 100 Hz sample rate

**[STKH-003] Sensor Data Logging**
- **Description**: All sensor data shall be logged on the gateway with timestamps for later retrieval
- **Rationale**: Historical data analysis, trend monitoring, and regulatory compliance
- **Priority**: Must Have
- **Acceptance**: Data logged with ≤1ms timestamp resolution

### 3.2 Actuator Control

**[STKH-004] Remote Actuator Command**
- **Description**: The gateway shall accept commands to control physical actuators (fans, valves) connected to the MCU
- **Rationale**: Closed-loop control based on sensor readings
- **Priority**: Must Have
- **Acceptance**: Commands applied within 20ms

**[STKH-005] Actuator Safety Limits**
- **Description**: Actuator outputs shall be constrained to configurable safe operating ranges
- **Rationale**: Prevent equipment damage from erroneous commands
- **Priority**: Must Have
- **Acceptance**: Out-of-range commands rejected with error response

**[STKH-006] Actuator Fail-Safe**
- **Description**: On any system fault, actuators shall transition to a defined safe state
- **Rationale**: Equipment protection in fault conditions
- **Priority**: Must Have
- **Acceptance**: Safe state reached within 2 seconds of fault detection

### 3.3 Communication

**[STKH-007] Structured Inter-Processor Communication**
- **Description**: Data exchange between Linux and MCU shall use a versioned, type-safe protocol
- **Rationale**: Prevent data corruption, enable independent firmware updates
- **Priority**: Must Have
- **Acceptance**: Protocol schema versioned, backward-compatible changes supported

**[STKH-008] Communication Loss Detection**
- **Description**: The system shall detect communication loss between Linux and MCU within 3 seconds
- **Rationale**: Enable automatic recovery before process variables drift
- **Priority**: Must Have
- **Acceptance**: Loss detected and logged within 3 seconds

**[STKH-009] Automatic Recovery**
- **Description**: The system shall automatically attempt recovery from communication failures
- **Rationale**: Minimize downtime in unattended deployments
- **Priority**: Must Have
- **Acceptance**: Recovery attempted within 5 seconds, success rate ≥90% for transient faults

### 3.4 Diagnostics

**[STKH-010] Remote Diagnostic Access**
- **Description**: A maintenance engineer shall be able to query system status, read sensors, and control actuators remotely
- **Rationale**: Field troubleshooting without physical access
- **Priority**: Must Have
- **Acceptance**: Command-line diagnostic interface accessible via network

**[STKH-011] Comprehensive Logging**
- **Description**: All system events (sensor readings, commands, errors, state changes) shall be logged with timestamps
- **Rationale**: Post-incident analysis and compliance auditing
- **Priority**: Must Have
- **Acceptance**: Logs persist across gateway reboots, retrievable via diagnostic interface

**[STKH-012] Firmware Version Reporting**
- **Description**: Both Linux and MCU firmware versions shall be queryable remotely
- **Rationale**: Fleet management, update tracking
- **Priority**: Should Have
- **Acceptance**: Version strings include build date, commit hash

### 3.5 Configuration

**[STKH-013] Runtime Configuration**
- **Description**: Sensor sample rates, actuator limits, and communication parameters shall be configurable without firmware rebuild
- **Rationale**: Adapt to different deployment scenarios
- **Priority**: Must Have
- **Acceptance**: Configuration applied within 1 second, persists across resets

**[STKH-014] Configuration Validation**
- **Description**: Invalid configuration values shall be rejected with clear error messages
- **Rationale**: Prevent misconfiguration in field
- **Priority**: Must Have
- **Acceptance**: Range validation for all configurable parameters

### 3.6 Reliability & Safety

**[STKH-015] Continuous Operation**
- **Description**: The system shall operate continuously for ≥1 year without human intervention
- **Rationale**: Remote/unattended deployment sites
- **Priority**: Must Have
- **Acceptance**: MTBF ≥ 8,760 hours demonstrated

**[STKH-016] Watchdog Protection**
- **Description**: MCU firmware shall be protected against hangs by hardware watchdog
- **Rationale**: Recovery from firmware faults
- **Priority**: Must Have
- **Acceptance**: Watchdog triggers within 2 seconds of firmware hang

**[STKH-017] No Dynamic Memory on MCU**
- **Description**: MCU firmware shall not use heap allocation (malloc/free)
- **Rationale**: Eliminate memory fragmentation and leak risks in long-running systems
- **Priority**: Must Have
- **Acceptance**: Static analysis confirms zero heap usage

### 3.7 Development Process

**[STKH-018] ASPICE CL2 Compliance**
- **Description**: The development process shall follow ASPICE Capability Level 2 practices
- **Rationale**: Industry compliance, process maturity demonstration
- **Priority**: Must Have
- **Acceptance**: All SYS, SWE, and SUP process work products complete

**[STKH-019] Full Traceability**
- **Description**: Every requirement shall be traceable to architecture, code, and tests
- **Rationale**: ASPICE assessment readiness, impact analysis capability
- **Priority**: Must Have
- **Acceptance**: Zero orphaned requirements in traceability matrix

**[STKH-020] MISRA C Compliance**
- **Description**: All C code shall comply with MISRA C:2012
- **Rationale**: Code quality, safety standard alignment
- **Priority**: Must Have
- **Acceptance**: Zero Required rule violations, documented Advisory rule deviations

## 4. Priority Summary

| Priority | Count | IDs |
|----------|-------|-----|
| Must Have | 19 | STKH-001 through STKH-020 (excl. STKH-012) |
| Should Have | 1 | STKH-012 |

## 5. Traceability

These stakeholder requirements flow into the System Requirements Specification (SRS, document SRS-001) via the SYS.2 process. Traceability is maintained in `docs/traceability/stkh_to_sys_trace.md`.
