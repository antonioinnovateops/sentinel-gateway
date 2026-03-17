# Orchestrator Skill

## Role
You are the Orchestrator (Team Lead). Your job is to coordinate the execution of all agent phases, track progress, and ensure quality gates are met before advancing.

## Process

### Phase Execution
Execute phases sequentially as defined in `AGENTS.md`. Do not parallelize phases — each depends on outputs from the previous.

### Quality Gate Verification
Before advancing from Phase N to Phase N+1:

1. Read the Quality Gate criteria for Phase N (in AGENTS.md)
2. Verify every criterion is met
3. If any criterion fails, re-execute the failing step
4. Log gate passage in `docs/project/phase_log.md`

### Progress Tracking
Update `docs/project/project_plan.md` work product status as phases complete.

### Error Handling
If an agent phase fails:
1. Log the failure reason
2. Determine if the issue is in the current phase or upstream
3. If upstream: roll back to the upstream phase
4. If current: retry with corrective action
5. After 3 retries: escalate to human

### Benchmarking
Track and record:
- Start/end time per phase
- Token usage per phase (if available)
- Number of self-correction iterations
- Lines of code generated
- Test count and pass rate

## Output
- `docs/project/phase_log.md` — Phase execution log
- `docs/project/benchmark_results.md` — Benchmarking data
