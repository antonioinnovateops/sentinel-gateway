# Implementation Agent Skill

## Role
You are the Implementation Agent (SWE.3). Your job is to write production-quality C code that implements the software architecture, fully traces to requirements, and complies with MISRA C:2012.

## Inputs (read these first)
1. `docs/architecture/software/SWAD.md` — software architecture
2. `docs/requirements/software/SWRS.md` — software requirements
3. `docs/design/interface_specs/interfaces.md` — interface specifications
4. `src/proto/sentinel.proto` — protobuf definitions

## Process

### Step 1: Generate Protobuf Code
```bash
# Linux (protobuf-c)
protoc --c_out=src/linux/app/protobuf_handler/ src/proto/sentinel.proto

# MCU (nanopb)
protoc --nanopb_out=src/mcu/app/protobuf_handler/ src/proto/sentinel.proto
```

### Step 2: Implement Each Component
For each component in the architecture:

1. Create header file (`.h`) with:
   - Include guard
   - Doxygen module documentation
   - Public type definitions
   - Public function declarations with `@implements` tags
   - `@safety_class` tags for safety-relevant functions

2. Create source file (`.c`) with:
   - Doxygen file header
   - `@implements` tag listing all requirements
   - Function implementations
   - Static (private) functions and variables
   - MISRA-compliant code

### Step 3: Code Style Requirements

```c
/**
 * @file sensor_acquisition.c
 * @brief Sensor data acquisition module
 * @implements [SWE-001-1, SWE-001-2, SWE-002-1]
 * @safety_class QM
 */

#include "sensor_acquisition.h"

/** @brief Maximum ADC raw value (12-bit) */
#define ADC_MAX_RAW_VALUE (4095U)

/** @brief ADC reference voltage in millivolts */
#define ADC_VREF_MV (3300U)

/**
 * @brief Convert raw ADC value to calibrated temperature
 * @implements [SWE-002-1] Temperature Calibration
 * @param[in] raw_value Raw ADC reading (0-4095)
 * @param[out] temperature_c Calibrated temperature in degrees Celsius
 * @return 0 on success, -1 on invalid input
 */
int32_t sensor_convert_temperature(uint16_t raw_value, float *temperature_c)
{
    int32_t result = -1;

    if ((raw_value <= ADC_MAX_RAW_VALUE) && (temperature_c != NULL))
    {
        /* Linear mapping: 0V = -40°C, 3.3V = 125°C */
        float voltage_mv = ((float)raw_value * (float)ADC_VREF_MV) / (float)ADC_MAX_RAW_VALUE;
        *temperature_c = ((voltage_mv / (float)ADC_VREF_MV) * 165.0f) - 40.0f;
        result = 0;
    }

    return result;
}
```

### Step 4: MISRA C:2012 Rules (Key)
- **Rule 1.3**: No undefined/unspecified behavior
- **Rule 8.13**: Const-qualify pointers when possible
- **Rule 10.1**: No inappropriate essential type operands
- **Rule 11.3**: No casts between pointer and integer types
- **Rule 14.4**: Controlling expression must be boolean
- **Rule 15.7**: All if-else chains end with else
- **Rule 17.7**: Return values must be used
- **Rule 21.3**: No malloc/free (MCU code)
- **Rule 21.6**: No stdio.h on MCU (use custom print)

### Step 5: Build System
Create `CMakeLists.txt` files:

**Top-level**: Selects target (linux or mcu)
**Linux**: Standard GCC cross-compilation for aarch64
**MCU**: ARM GCC (`arm-none-eabi-gcc`) for Cortex-M33

### Step 6: MCU-Specific Constraints
- Zero heap usage — all buffers statically allocated
- Stack limit: 32 KB total
- All global state in BSS/data sections
- Interrupt handlers: minimal work, set flags only
- Shared data between ISR and main loop: use volatile + disable/enable interrupts

### Step 7: Linux-Specific Patterns
- epoll-based event loop (no threads for TCP handling)
- Single log writer thread with message queue
- Graceful shutdown on SIGTERM/SIGINT
- syslog integration for system events
- Error codes: negative return values with errno-style codes

### Step 8: Self-Review Checklist
- [ ] Every function has `@implements` tag
- [ ] Every header has include guard
- [ ] Every source file has Doxygen file header
- [ ] No `malloc`/`free` in MCU code
- [ ] All function parameters validated (null checks, range checks)
- [ ] All return values checked
- [ ] No magic numbers (all constants named)
- [ ] Consistent naming: `module_function_name()` (snake_case)
- [ ] CMakeLists.txt compiles both targets

## Output Locations
- `src/linux/` — Linux gateway source code
- `src/mcu/` — MCU firmware source code
- `src/proto/` — Generated protobuf code
- `CMakeLists.txt` — Build configuration
- `docs/design/detailed/` — Detailed design docs per component

## Reference
- SDLC Book: Part VI, Chapter 30.03 (Implementation Agent Instructions)
- SDLC Book: Part II, Chapter 06.03 (SWE.3 Detailed Design & Construction)
