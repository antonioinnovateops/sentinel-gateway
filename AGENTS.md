# AGENTS.md — Sentinel Gateway Agent Orchestration

## Overview

You are an AI agent tasked with building the Sentinel Gateway embedded system. This document is your entry point. Follow it precisely.

## Your Mission

Build a fully compilable, fully tested, ASPICE-compliant embedded system from the specifications in this repository. Every line of code must trace to a requirement. Every requirement must have a test. Every design decision must have a rationale.

## Execution Order

**You MUST follow this order. Do not skip steps. Do not proceed to the next phase until the current phase is complete and verified.**

### Phase 1: Requirements Engineering (SYS.2 → SWE.1)

1. Read `docs/requirements/system/SRS.md` — the System Requirements Specification
2. Read `docs/requirements/system/stakeholder_requirements.md` — stakeholder needs
3. Generate software requirements by decomposing system requirements
4. Write output to `docs/requirements/software/SWRS.md`
5. Generate traceability matrix: `docs/traceability/sys_to_swe_trace.md`
6. Self-review: verify every SYS requirement maps to ≥1 SWE requirement
7. Self-review: verify every SWE requirement is testable (has acceptance criteria)

**Quality Gate**: All requirements have IDs, types, safety classifications, acceptance criteria, and traceability links.

**Skill Reference**: `.claude/skills/requirements-agent/SKILL.md`

### Phase 2: Architecture Design (SYS.3 → SWE.2)

1. Read the approved SWRS from Phase 1
2. Read `docs/architecture/system/SAD.md` — System Architecture Document
3. Allocate software requirements to components
4. Design software architecture with interfaces
5. Write output to `docs/architecture/software/SWAD.md`
6. Generate Architecture Decision Records in `docs/architecture/software/adr/`
7. Generate interface specifications in `docs/design/interface_specs/`
8. Generate component diagrams (draw.io `.drawio.svg`) in `docs/architecture/diagrams/`
9. Self-review: verify all SWE requirements allocated to components
10. Self-review: verify all interfaces fully specified (types, ranges, units)

**Quality Gate**: Complete component allocation, all interfaces specified, ADRs for every significant decision.

**Skill Reference**: `.claude/skills/architecture-agent/SKILL.md`

### Phase 3: Container Build Environment Setup

1. Read `docs/design/build_environment.md` (BUILD-001) — container pipeline spec
2. Read `docs/design/build_system_spec.md` (BSS-001) — CMake/toolchain spec
3. Read `docs/design/sil_environment.md` (SIL-001) — QEMU SIL spec
4. Create Dockerfiles in `docker/` for all 5 images (build-linux, build-mcu, sil, yocto, analysis)
5. Create `docker-compose.yml` per BUILD-001 Section 4
6. Create CMake toolchain files in `cmake/`
7. Create QEMU launch scripts in `tools/qemu/`
8. Verify: `docker-compose build` succeeds, `docker-compose run --rm sil` passes all tests

**Quality Gate**: All containers build, QEMU dual-VM boots and establishes TCP connectivity.

### Phase 4: Implementation (SWE.3)

1. Read the approved SWAD from Phase 2
2. Read interface specifications
3. For each component:
   a. Write detailed design document in `docs/design/detailed/`
   b. Implement in C (MISRA C:2012 compliant)
   c. Add Doxygen headers with `@implements` tags
   d. Ensure every function traces to a requirement
4. Generate protobuf code from `docs/design/interface_specs/proto/sentinel.proto`
5. Implement Linux gateway in `src/linux/`
6. Implement MCU firmware in `src/mcu/`
7. Write CMakeLists.txt and Dockerfiles per `docs/design/build_system_spec.md` and `docs/design/build_environment.md`
8. Self-review: MISRA compliance check
9. Self-review: verify all `@implements` tags match SWRS requirements

**Quality Gate**: Code compiles for both targets (Linux ARM64, STM32 ARM), zero MISRA Required rule violations, 100% requirement coverage in `@implements` tags.

**Skill Reference**: `.claude/skills/implementation-agent/SKILL.md`

### Phase 5: Unit Verification (SWE.4)

1. Read source code from Phase 3
2. For each function:
   a. Generate unit tests (typical, boundary, invalid, error cases)
   b. Create mocks for external dependencies
   c. Write test specification in `docs/test/unit_test_spec/`
3. Place test code in `tests/unit/`
4. Achieve ≥95% statement coverage, ≥90% branch coverage
5. Self-review: verify every requirement has ≥1 test case

**Quality Gate**: All tests pass, coverage targets met, traceability complete.

**Skill Reference**: `.claude/skills/verification-agent/SKILL.md`

### Phase 6: Integration Testing (SWE.5)

1. Read `docs/test/integration_test_spec/integration_test_plan.md` (ITP-001) — 12 test scenarios
2. Read `docs/design/sil_environment.md` (SIL-001) — fault injection methods
3. Implement integration tests in `tests/integration/` (Python pytest)
4. Write `tests/integration/conftest.py` (QEMU fixtures per SIL-001 Section 4)
5. Test protobuf communication between Linux ↔ MCU in `sentinel-sil` container
6. Test fault injection scenarios (USB disconnect, MCU freeze, etc.)
7. Run: `docker compose up sil`

**Quality Gate**: All 12 integration scenarios pass in containerized QEMU SIL environment.

**Skill Reference**: `.claude/skills/verification-agent/SKILL.md`

### Phase 7: System Qualification (SWE.6)

1. Design qualification tests against system requirements
2. Test complete system behavior in QEMU SIL
3. Write test specifications in `docs/test/qualification_test_spec/`
4. Place test code in `tests/system/`
5. Generate final test report in `docs/test/results/`

**Quality Gate**: All system requirements verified, test report complete.

### Phase 8: Review & Compliance (SUP.2)

1. Run MISRA C:2012 static analysis on all source files
2. Verify traceability: SYS → SWE → Architecture → Code → Tests
3. Check all documentation for completeness
4. Generate final traceability matrix in `docs/traceability/`
5. Generate compliance summary

**Quality Gate**: Zero critical findings, complete traceability, all documents approved.

## Agent Skills

Each agent role has a dedicated skill file in `.claude/skills/`:

| Skill | Agent Role | ASPICE Process |
|-------|------------|----------------|
| `requirements-agent/` | Requirements Agent | SYS.2, SWE.1 |
| `architecture-agent/` | Architecture Agent | SYS.3, SWE.2 |
| `implementation-agent/` | Implementation Agent | SWE.3 |
| `verification-agent/` | Verification Agent | SWE.4, SWE.5, SWE.6 |
| `review-agent/` | Review Agent | SUP.2 |
| `orchestrator/` | Team Lead | MAN.3 |

## Rules

1. **Never skip a phase** — each phase depends on the previous
2. **Never skip self-review** — catch errors before they propagate
3. **Every artifact needs metadata** — YAML front matter with document_id, version, status
4. **Traceability is mandatory** — no orphaned requirements, no untested code
5. **MISRA C:2012 compliance** — zero Required rule violations
6. **Git discipline** — commit after each phase, meaningful commit messages
7. **When in doubt, escalate** — flag safety decisions and ambiguities for human review

## Reference Material

The SDLC book (`sdlc_with_ai/`) provides the methodology. Key chapters:

- Part II (Ch 5-12): ASPICE process definitions
- Part III (Ch 13-18): Tool integration patterns
- Part IV (Ch 19-24): Practical implementation templates
- Part V (Ch 25-28): Industry case studies
- Part VI (Ch 29-32): Agent execution instructions
- Part VII (Ch 33-36): Engineering tutorials

## Success Criteria

The project is complete when:

- [ ] All system requirements have traceable software requirements
- [ ] Software architecture covers all requirements with defined interfaces
- [ ] All code compiles for both targets without errors
- [ ] Zero MISRA C:2012 Required rule violations
- [ ] ≥95% unit test statement coverage
- [ ] All integration tests pass in QEMU SIL
- [ ] All system qualification tests pass
- [ ] Complete bidirectional traceability matrix
- [ ] All documentation complete with proper metadata
