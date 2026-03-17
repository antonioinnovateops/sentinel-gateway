---
title: "Project Plan"
document_id: "PP-001"
project: "Sentinel Gateway"
version: "1.0.0"
date: 2026-03-17
status: "Approved"
aspice_process: "MAN.3 Project Management"
---

# Project Plan — Sentinel Gateway

## 1. Project Overview

**Objective**: Build a fully functional, ASPICE-compliant, dual-processor embedded system entirely through AI agent execution, using the SDLC with AI book as the methodology guide.

**Success Criteria**: Fully compilable firmware for both targets, complete test suite passing in QEMU SIL, zero MISRA Required violations, complete traceability matrix.

## 2. Phase Breakdown

### Phase 1: Requirements Engineering
**Duration**: ~2 hours (AI agent time)
**Inputs**: Product Specification, Stakeholder Requirements, SRS
**Outputs**: Software Requirements Specification (SWRS), traceability matrices
**Quality Gate**: All 80 system requirements decomposed, all SWE requirements testable

### Phase 2: Architecture Design
**Duration**: ~3 hours
**Inputs**: SWRS, SRS, System Architecture
**Outputs**: Software Architecture Document (SWAD), ADRs, interface specs, diagrams
**Quality Gate**: All requirements allocated, all interfaces specified

### Phase 3: Container Build Environment Setup
**Duration**: ~2 hours
**Inputs**: Build System Spec (BSS-001), SIL Environment Spec (SIL-001), Build Environment Spec (BUILD-001)
**Outputs**: Dockerfiles (5 images), docker-compose.yml, CMake toolchain files, QEMU launch scripts
**Quality Gate**: `docker compose build` succeeds, all containers healthy, QEMU boots both VMs

### Phase 4: Implementation
**Duration**: ~8 hours
**Inputs**: SWAD, SWRS, interface specs, container build environment
**Outputs**: C source code (Linux + MCU), CMakeLists.txt, linker scripts, Doxygen headers
**Quality Gate**: Compiles in containers for both targets, MISRA clean

### Phase 5: Unit Verification
**Duration**: ~4 hours
**Inputs**: Source code, SWRS
**Outputs**: Unit tests, mock implementations, coverage reports
**Quality Gate**: ≥95% statement coverage, all tests pass in container

### Phase 6: Integration Testing (SIL)
**Duration**: ~3 hours
**Inputs**: All source code, containerized QEMU SIL environment
**Outputs**: Integration test suite, SIL test scripts, fault injection tests
**Quality Gate**: All 12 integration scenarios pass in QEMU SIL container

### Phase 7: System Qualification
**Duration**: ~2 hours
**Inputs**: SRS, complete system in SIL
**Outputs**: Qualification test suite, final test report
**Quality Gate**: All system requirements verified in SIL

### Phase 8: Review & Compliance
**Duration**: ~2 hours
**Inputs**: All artifacts
**Outputs**: MISRA report, traceability matrix, compliance summary
**Quality Gate**: Zero critical findings

**Total Estimated AI Agent Time**: ~26 hours

## 3. Work Product Checklist

| ID | Work Product | ASPICE Process | Status |
|----|-------------|----------------|--------|
| WP-01 | Stakeholder Requirements | SYS.1 | ✅ Complete |
| WP-02 | System Requirements Specification | SYS.2 | ✅ Complete |
| WP-03 | System Architecture Document | SYS.3 | ✅ Complete |
| WP-04 | Software Requirements Specification | SWE.1 | ✅ Complete (80 SWE reqs) |
| WP-05 | Software Architecture Document | SWE.2 | ✅ Complete |
| WP-06 | Architecture Decision Records | SWE.2 | ✅ Complete (5 ADRs in SAD) |
| WP-07 | Interface Specifications | SWE.2 | ✅ Complete (IFS-001 + proto) |
| WP-08 | Detailed Design Documents | SWE.3 | ✅ Complete (5 component docs) |
| WP-09 | Source Code (Linux) | SWE.3 | 🔲 Phase 4 |
| WP-10 | Source Code (MCU) | SWE.3 | 🔲 Phase 4 |
| WP-11 | Unit Test Specifications | SWE.4 | ✅ Complete (84 test cases) |
| WP-12 | Unit Test Code | SWE.4 | 🔲 Phase 5 |
| WP-13 | Integration Test Specifications | SWE.5 | ✅ Complete (12 scenarios) |
| WP-14 | Integration Test Code | SWE.5 | 🔲 Phase 6 |
| WP-15 | Qualification Test Specifications | SWE.6 | ✅ Complete (QTP-001) |
| WP-16 | Qualification Test Code | SWE.6 | 🔲 Phase 7 |
| WP-17 | MISRA Compliance Plan | SUP.2 | ✅ Complete (plan only; report Phase 8) |
| WP-18 | Traceability Matrices | SUP.8 | ✅ Complete (STKH→SYS, SYS→SWE) |
| WP-19 | Project Plan | MAN.3 | ✅ Complete |
| WP-20 | Risk Register | MAN.5 | ✅ Complete (8 risks) |
| WP-21 | Safety Analysis (FMEA) | Safety | ✅ Complete (18 failure modes) |
| WP-22 | Protobuf Schema | SWE.2 | ✅ Complete |
| WP-23 | Build Environment Spec | SWE.3 | ✅ Complete (BUILD-001) |
| WP-24 | SIL Environment Spec | SWE.6 | ✅ Complete (SIL-001) |
| WP-25 | Build System Spec | SWE.3 | ✅ Complete (BSS-001) |
| WP-26 | Architecture Diagrams | SWE.2 | ✅ Complete (19 .drawio.svg) |
| WP-27 | Container Dockerfiles | SWE.3 | 🔲 Phase 3 |
| WP-28 | QEMU Launch Scripts | SWE.6 | 🔲 Phase 3 |

## 4. Benchmarking Protocol

When executing with an AI model:

1. Record model name, version, and parameters
2. Start timer at Phase 1 begin
3. Record time per phase
4. Record token usage per phase
5. Record number of self-correction iterations
6. Record final metrics:
   - Lines of code generated
   - Test count and pass rate
   - MISRA violations
   - Traceability coverage
   - Total wall-clock time
   - Total token consumption
