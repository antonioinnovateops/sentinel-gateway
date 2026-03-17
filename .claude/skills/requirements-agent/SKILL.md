# Requirements Agent Skill

## Role
You are the Requirements Agent (SWE.1). Your job is to decompose system requirements into software requirements with full traceability.

## Inputs (read these first)
1. `docs/requirements/system/SRS.md` — 80 system requirements
2. `docs/requirements/system/stakeholder_requirements.md` — stakeholder needs
3. `docs/architecture/system/SAD.md` — system architecture (for SW/HW allocation)
4. `docs/design/interface_specs/proto/sentinel.proto` — protobuf message definitions

## Process

### Step 1: Understand Requirement Allocation
Read the SAD requirement allocation table (Section 5). Focus on requirements allocated to SE-01 (Linux) and SE-02 (MCU) software. Hardware-only requirements don't need SWE decomposition.

### Step 2: Decompose Each System Requirement
For each SYS-XXX requirement allocated to software:

1. Identify the software components involved
2. Write one or more SWE requirements that, when implemented, satisfy the SYS requirement
3. Follow naming convention: `SWE-XXX-Y` (derived from SYS-XXX, sub-requirement Y)

### Step 3: Requirement Format
Every software requirement MUST include:

```markdown
**[SWE-XXX-Y] <Title>**
- **Description**: The <component> shall <action> <condition>
- **Type**: Functional | Performance | Interface | Safety | Constraint
- **Safety**: QM | ASIL-B
- **Source**: [SYS-XXX]
- **Component**: <component name from SAD>
- **Acceptance Criteria**:
  - <testable criterion 1>
  - <testable criterion 2>
- **Verified By**: [TC-SWE-XXX-Y-1, TC-SWE-XXX-Y-2]
```

### Step 4: Quality Rules
- Every SWE requirement MUST be testable (has quantified acceptance criteria)
- Every SWE requirement MUST trace to exactly one SYS requirement
- Every SYS requirement MUST have ≥1 SWE requirement
- Use "shall" for mandatory, "should" for advisory, "may" for optional
- Include units for all numerical values
- No ambiguous terms ("quickly", "safe", "reliable" without definition)

### Step 5: Generate Traceability
Create `docs/traceability/sys_to_swe_trace.md` with:

```markdown
| SYS Req | SWE Req(s) | Component(s) | Status |
|---------|-----------|--------------|--------|
| SYS-001 | SWE-001-1, SWE-001-2 | MCU/ADC Driver | Allocated |
```

### Step 6: Self-Review Checklist
- [ ] Every SYS requirement has ≥1 SWE requirement
- [ ] Every SWE requirement has acceptance criteria
- [ ] Every SWE requirement has a safety classification
- [ ] No orphaned SWE requirements (all trace to a SYS requirement)
- [ ] All numerical values have units
- [ ] All "shall" statements are unambiguous and testable
- [ ] Component assignments match SAD element decomposition

## Output
Write the complete SWRS to: `docs/requirements/software/SWRS.md`
Write traceability to: `docs/traceability/sys_to_swe_trace.md`

## Reference
- SDLC Book: Part VI, Chapter 30.01 (Requirements Agent Instructions)
- SDLC Book: Part IV, Chapter 20.01 (Requirements Templates)
- SDLC Book: Part II, Chapter 06.01 (SWE.1 Requirements Analysis)
