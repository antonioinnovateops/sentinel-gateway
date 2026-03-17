---
title: "MISRA C:2012 Compliance Plan"
document_id: "MISRA-PLAN-001"
project: "Sentinel Gateway"
version: "1.0.0"
date: 2026-03-17
status: "Approved"
---

# MISRA C:2012 Compliance Plan

## 1. Scope

This plan defines MISRA C:2012 compliance scope, tooling, and deviation handling for the Sentinel Gateway project.

## 2. Applicable Code

| Directory | In Scope | Rationale |
|-----------|:--------:|-----------|
| `src/linux/app/` | ✅ | Project application code |
| `src/linux/bsp/` | ✅ | Project BSP code |
| `src/linux/common/` | ✅ | Project shared code |
| `src/mcu/app/` | ✅ | Project application code |
| `src/mcu/hal/` | ✅ | Project HAL code |
| `src/mcu/common/` | ✅ | Project shared code |
| `docs/design/interface_specs/proto/` (generated) | ❌ | Auto-generated protobuf code |
| lwIP library | ❌ | Third-party, not modifiable |
| nanopb library | ❌ | Third-party, not modifiable |
| protobuf-c library | ❌ | Third-party, not modifiable |
| Unity test framework | ❌ | Test-only code |
| `tests/` | ❌ | Test code, not production |

## 3. Compliance Targets

| Rule Category | Target | Handling |
|---------------|--------|----------|
| Mandatory rules | 100% compliance | No deviations permitted |
| Required rules | 100% compliance | Deviations require documented rationale |
| Advisory rules | Best effort | Deviations documented, no blocking |

## 4. Static Analysis Tool

**Primary**: cppcheck with MISRA addon
**Secondary**: GCC `-Wall -Wextra -Wpedantic -Wconversion`

## 5. Known Deviations

### DEV-001: Rule 21.6 (Required) — stdio.h usage on Linux
- **Scope**: Linux application code only
- **Rationale**: Linux application legitimately uses stdio for logging and file I/O
- **Risk**: None (Linux is not safety-critical target)

### DEV-002: Advisory rules in generated protobuf code
- **Scope**: Auto-generated protobuf files (`*.pb-c.c`, `*.pb-c.h`, `*.pb.c`, `*.pb.h`) in `src/linux/generated/` and `src/mcu/generated/`
- **Rationale**: Auto-generated code; not modifiable without breaking regeneration
- **Risk**: Low (nanopb is widely used in safety-critical projects)

## 6. Review Process

1. Static analysis runs on every commit (CI pipeline)
2. MISRA report reviewed before each phase gate
3. New Required rule violations block merge
4. Advisory violations logged for future improvement
