# Sentinel Gateway

**An ASPICE-compliant embedded Linux gateway with MCU peripheral communication — built entirely by AI agents.**

## Purpose

This project serves as a **proof-of-concept** demonstrating that AI agents, guided by a comprehensive SDLC methodology (from the book *"SDLC with AI"* by Antonio Stepien), can autonomously build a safety-compliant embedded system from a minimal product specification.

## The Product

Sentinel Gateway is an embedded system comprising:

- **Linux Gateway (Main CPU)**: A Yocto-based embedded Linux system running on QEMU ARM64, responsible for high-level logic, data aggregation, cloud connectivity, and diagnostics
- **MCU Peripheral (STM32U575)**: A Cortex-M33 microcontroller handling real-time sensor acquisition and actuator control, emulated via QEMU ARM
- **Communication**: Protocol Buffers (protobuf) over TCP/IP via USB Ethernet (CDC-ECM), providing structured, versioned, and type-safe messaging between the two processors

## The Goal

Prove that with a sufficiently detailed specification and the right agent skill-set:

1. **Any capable AI model** can build this project from scratch
2. The result is **fully compilable and deployable** in a Software-in-the-Loop (SIL) environment
3. Every artifact is **traceable** — requirement ↔ architecture ↔ code ↔ test
4. The process follows **ASPICE CL2** rigor with **ASIL-B** safety practices
5. Different AI models can be **benchmarked** against identical specifications

## Architecture Overview

![System Architecture Overview](docs/architecture/diagrams/system_overview.drawio.svg)

### Data Flow

![Sensor and Actuator Data Flow](docs/architecture/diagrams/data_flow.drawio.svg)

### State Machine

![System Operational State Machine](docs/architecture/diagrams/state_machine.drawio.svg)

## Repository Structure

> **Note**: This is a **specification-only repository**. No implementation code exists yet — only Markdown specs, diagrams, protobuf schemas, and empty directory placeholders. Implementation is triggered in Phase 3+ (see Project Plan).

![Repository Structure](docs/architecture/diagrams/repo_structure.drawio.svg)

## ASPICE Process Mapping

| ASPICE Process | Agent Role | Key Deliverables |
|----------------|------------|------------------|
| SYS.2 | Requirements Agent | System Requirements Specification |
| SYS.3 | Architecture Agent | System Architecture Document |
| SWE.1 | Requirements Agent | Software Requirements Specification |
| SWE.2 | Architecture Agent | Software Architecture Document |
| SWE.3 | Implementation Agent | Source code, detailed design |
| SWE.4 | Verification Agent | Unit tests, coverage reports |
| SWE.5 | Verification Agent | Integration tests |
| SWE.6 | Verification Agent | Qualification tests |
| SUP.2 | Review Agent | Code review, MISRA compliance |
| SUP.8 | All Agents | Configuration management (Git) |
| MAN.3 | Orchestrator | Project plan, risk register |

## How to Build (Containerized)

Everything runs in Docker containers — no host toolchain installation required.

```bash
# Prerequisites: Docker 24+ and Docker Compose v2

# Build both targets (Linux gateway + MCU firmware)
docker compose up build-linux build-mcu

# Run MISRA + static analysis
docker compose up analysis

# Run SIL integration tests (QEMU dual-VM)
docker compose up sil

# Build Yocto image (first time ~2 hours)
docker compose --profile full up yocto

# Full pipeline (build → analysis → SIL)
docker compose up

# Interactive SIL debugging shell
docker compose run sil bash
```

See [Build Environment Spec](docs/design/build_environment.md) and [SIL Environment Spec](docs/design/sil_environment.md) for full details.

## Benchmarking AI Models

This project is designed to be built from scratch by AI agents. To benchmark:

1. Clone this repo (specification only, no implementation)
2. Point your AI agent at `AGENTS.md`
3. The agent reads skills from `.claude/skills/`
4. Measure: time to completion, test pass rate, MISRA compliance, code quality

## License

MIT License — © 2026 InnovateOps Limited

## References

- *SDLC with AI* — Antonio Stepien (https://github.com/antonioinnovateops/sdlc_with_ai)
- ASPICE PAM v4.0
- ISO 26262:2018
- Protocol Buffers v3 (https://protobuf.dev)
- Yocto Project (https://www.yoctoproject.org)
