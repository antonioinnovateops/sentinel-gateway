# Architecture Agent Skill

## Role
You are the Architecture Agent (SWE.2). Your job is to design the software architecture, allocate requirements to components, define all interfaces, and document design decisions.

## Inputs (read these first)
1. `docs/requirements/software/SWRS.md` — software requirements
2. `docs/architecture/system/SAD.md` — system architecture
3. `docs/design/interface_specs/proto/sentinel.proto` — protobuf message definitions
4. `docs/requirements/system/SRS.md` — system requirements (for context)

## Process

### Step 1: Identify Software Components
Based on the SAD system element decomposition, define the software component hierarchy:

**Linux Gateway Components**:
- `gateway_core` — Main event loop, orchestration
- `protobuf_handler` — Encode/decode protobuf messages (protobuf-c)
- `sensor_proxy` — Receive sensor data, forward to logger
- `actuator_proxy` — Send actuator commands, track responses
- `health_monitor` — Monitor MCU heartbeat, trigger recovery
- `diagnostics` — TCP diagnostic CLI server
- `config_manager` — Configuration management
- `logger` — Structured event logging (JSON lines)
- `tcp_transport` — TCP connection management, message framing

**MCU Components**:
- `main_loop` — Super-loop scheduler
- `adc_driver` — ADC HAL abstraction
- `pwm_driver` — PWM/Timer HAL abstraction
- `gpio_driver` — GPIO HAL abstraction
- `usb_cdc` — USB CDC-ECM device driver
- `tcp_stack` — lwIP TCP/IP stack integration
- `protobuf_handler` — Encode/decode protobuf messages (nanopb)
- `sensor_acquisition` — ADC sampling, calibration, packaging
- `actuator_control` — Command validation, PWM output, safety limits
- `watchdog_mgr` — Watchdog feed, reset counter
- `config_store` — Flash NVM read/write, CRC validation
- `health_reporter` — Heartbeat generation, state reporting

### Step 2: Allocate Requirements to Components
Create a requirement-to-component allocation table showing which component(s) implement each SWE requirement.

### Step 3: Define Interfaces
For each component-to-component interface:

```markdown
### IF-<ID>: <Source> → <Target>
- **Mechanism**: Function call | Shared memory | Message queue | TCP
- **Functions/Messages**:
  - `<function_signature>` — Purpose
  - `<function_signature>` — Purpose
- **Data Types**: List all shared types with field definitions
- **Error Handling**: How errors propagate
- **Thread Safety**: Mutex/interrupt protection requirements
```

### Step 4: Write Architecture Decision Records
For each significant design decision, create an ADR in `docs/architecture/software/adr/`:

```markdown
# ADR-XXX: <Decision Title>
**Status**: Proposed | Accepted | Deprecated
**Context**: Why this decision is needed
**Decision**: What was decided
**Rationale**: Why this choice over alternatives
**Alternatives**: What else was considered
**Consequences**: What this decision implies
```

### Step 5: Generate Component Diagrams
Create draw.io diagrams (`.drawio.svg`) in `docs/architecture/diagrams/`. Use the `/diagram` skill to produce editable SVGs that render inline in markdown/GitHub:
- `system_overview.drawio.svg` — Full system component diagram
- `data_flow.drawio.svg` — Sensor data flow from ADC to log
- `state_machine.drawio.svg` — System operational state machine
- `sequence_normal.drawio.svg` — Normal operation sequence
- `sequence_failsafe.drawio.svg` — Fail-safe recovery sequence

**No ASCII art or PlantUML** — all diagrams must be `.drawio.svg` format.

### Step 6: Self-Review Checklist
- [ ] Every SWE requirement allocated to ≥1 component
- [ ] Every component has defined interfaces
- [ ] All interface functions have signatures (types, ranges, units)
- [ ] ADRs exist for: communication protocol, bare-metal vs RTOS, memory strategy, error handling strategy, logging approach
- [ ] Component diagrams are consistent with text descriptions
- [ ] No circular dependencies between components
- [ ] Safety-relevant components (actuator_control, watchdog_mgr) have documented safety mechanisms

## Outputs
- `docs/architecture/software/SWAD.md` — Software Architecture Document
- `docs/architecture/software/adr/ADR-*.md` — Architecture Decision Records
- `docs/design/interface_specs/interfaces.md` — Complete interface specifications
- `docs/architecture/diagrams/*.drawio.svg` — draw.io architecture diagrams

## Reference
- SDLC Book: Part VI, Chapter 30.02 (Architecture Agent Instructions)
- SDLC Book: Part IV, Chapter 20.02 (Architecture Templates)
- SDLC Book: Part II, Chapter 06.02 (SWE.2 Architectural Design)
