---
title: "Configuration and Change Management Plan"
document_id: "CCMP-001"
project: "Sentinel Gateway"
version: "1.0.0"
date: 2026-03-18
status: "Approved"
aspice_process: "SUP.8 Configuration Management, SUP.10 Change Request Management"
---

# Configuration and Change Management Plan — Sentinel Gateway

## 1. Purpose

This plan defines configuration management (SUP.8) and change request management (SUP.10) processes, ensuring controlled evolution of all work products.

## 2. Configuration Items

### 2.1 Managed Items

| CI Category | Items | Storage | Naming Convention |
|-------------|-------|---------|------------------|
| Requirements | STKH-REQ-001, SRS-001, SWRS-001 | `docs/requirements/` | `{TYPE}-{ID}` |
| Architecture | SAD-001, SWAD-001, IFS-001 | `docs/architecture/`, `docs/design/` | `{TYPE}-{ID}` |
| Design | Detailed design docs, BSS-001, BUILD-001, SIL-001 | `docs/design/` | `{TYPE}-{ID}` |
| Test | UTP-001, ITP-001, QTP-001, SITP-001, SQTP-001 | `docs/test/` | `{TYPE}-{ID}` |
| Safety | FMEA-001 | `docs/safety/` | `{TYPE}-{ID}` |
| Project | PP-001, RR-001, QAP-001, CCMP-001 | `docs/project/` | `{TYPE}-{ID}` |
| Traceability | TRACE-001 to TRACE-003 | `docs/traceability/` | `TRACE-{ID}` |
| Diagrams | 19 .drawio.svg files | `docs/architecture/diagrams/` | `{name}.drawio.svg` |
| Proto schema | sentinel.proto, sentinel.options | `docs/design/interface_specs/proto/` | — |
| Source code | Linux + MCU (future) | `src/` | per component |
| Test code | Unit + integration (future) | `tests/` | per test type |
| Build config | Dockerfiles, CMake (future) | `docker/`, `cmake/` | — |

### 2.2 Version Control

**Tool**: Git (GitHub: `antonioinnovateops/sentinel-gateway`)
**Branching**: `main` (release), `develop` (integration), feature branches
**Tagging**: `v{major}.{minor}.{patch}` for releases

## 3. Baselines

| Baseline | Contents | Trigger |
|----------|----------|---------|
| REQ-BL | STKH-REQ-001, SRS-001, SWRS-001, traceability | Phase 1 complete |
| ARCH-BL | SAD-001, SWAD-001, IFS-001, diagrams | Phase 2 complete |
| DESIGN-BL | Detailed designs, BSS-001, BUILD-001, SIL-001 | Phase 3 complete |
| CODE-BL | Source code, CMakeLists, Dockerfiles | Phase 4 complete |
| TEST-BL | All test specs + test code | Phase 6 complete |
| RELEASE-BL | All work products | Phase 8 complete |

## 4. Change Request Process

### 4.1 Change Request Categories

| Category | Impact | Approval |
|----------|--------|----------|
| Stakeholder requirement change | Cascades through all V-model levels | Product Owner |
| System requirement change | Cascades through SWE levels | Project Lead |
| Software requirement change | Cascades to design, code, tests | Project Lead |
| Design/code change | Local to component | Developer + Reviewer |
| Test change | Local to test suite | Developer + Reviewer |
| Document correction | No functional impact | Author |

### 4.2 Impact Analysis

For each change request:
1. Identify affected configuration items
2. Trace forward: which downstream items need update?
3. Trace backward: which upstream items justify the change?
4. Estimate effort for all affected items
5. Assess safety impact (ASIL-B requirements affected?)

### 4.3 Change Request Workflow

```
1. SUBMITTED  → Author creates CR with rationale
2. ANALYZED   → Impact analysis completed
3. APPROVED   → Approval per category table
4. IMPLEMENTED → Changes made to all affected CIs
5. VERIFIED   → Changed CIs reviewed and tested
6. CLOSED     → CR closed, baseline updated
```

## 5. Problem Resolution (SUP.9)

### 5.1 Defect Tracking

**Tool**: GitHub Issues with labels
**Labels**: `bug`, `requirement-defect`, `design-defect`, `test-defect`, `safety`

### 5.2 Severity and Priority

| Severity | Definition | Resolution Target |
|----------|-----------|------------------|
| S1 - Critical | Safety impact, data loss, system crash | Immediate |
| S2 - Major | Feature broken, workaround exists | Within current phase |
| S3 - Minor | Cosmetic, documentation error | Before release |

### 5.3 Resolution Workflow

```
1. OPEN       → Defect identified and logged
2. ANALYZED   → Root cause determined
3. FIXING     → Fix in progress
4. REVIEW     → Fix reviewed
5. VERIFIED   → Fix tested
6. CLOSED     → Defect resolved
```

## 6. Measurement (MAN.6)

### 6.1 Project Metrics

| Metric | Source | Collection | Target |
|--------|--------|-----------|--------|
| Requirements count | STKH/SRS/SWRS | Per phase | Tracked |
| Requirements stability | Change requests | Per phase | ≤5% change after baseline |
| Traceability coverage | Matrices | Per phase | 100% |
| Defect density | GitHub Issues | Per phase | Declining trend |
| Test pass rate | Test reports | Per phase | ≥95% |
| MISRA violations | cppcheck reports | Per build | Zero (Required) |
| Code coverage | gcov/lcov | Per build | ≥95% statement |
| Build success rate | CI pipeline | Per commit | ≥99% |
| SIL test duration | CI pipeline | Per run | ≤15 minutes |

### 6.2 Reporting

- Phase gate reports include all metrics for the completed phase
- Trend charts generated for defect density and test pass rate
- Final release includes full measurement summary
