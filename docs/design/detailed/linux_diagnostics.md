---
title: "Detailed Design — Linux Diagnostics Server"
document_id: "DD-LINUX-002"
project: "Sentinel Gateway"
version: "2.0.0"
date: 2026-03-18
status: "Approved"
aspice_process: "SWE.3 Software Detailed Design"
component: "LINUX / diagnostics"
requirements: ["SWE-046-2", "SWE-047-1", "SWE-048-1", "SWE-049-1", "SWE-050-1", "SWE-051-1", "SWE-054-1", "SWE-054-2"]
implementation: "src/linux/diagnostics.c"
---

# Detailed Design — Linux Diagnostics Server

## 1. Purpose

Provide a simple text-based diagnostic interface on TCP port 5002 for field troubleshooting, sensor reading, actuator control, and log retrieval.

## 2. Connection Model

**CRITICAL IMPLEMENTATION DETAIL**: The diagnostic server accepts only **ONE client at a time**.

- TCP server on port 5002
- **Single client connection** (epoll single-slot design)
- Line-delimited protocol (newline `\n` terminated)
- No authentication on read commands
- Subsequent connections refused until current client disconnects

```c
// From src/linux/diagnostics.c - epoll setup
static int diag_fd = -1;        // Listening socket
static int client_fd = -1;      // Single connected client (-1 = none)

// Accept logic
if (client_fd >= 0) {
    // Already have a client, reject new connection
    int new_fd = accept(diag_fd, NULL, NULL);
    close(new_fd);  // Immediate close
    return;
}
```

## 3. Command Parser

**CRITICAL**: All commands are **lowercase only** and **case-sensitive**.

```c
typedef struct {
    const char *name;
    int (*handler)(diag_client_t *client, int argc, char **argv);
    const char *help;
} diag_command_t;

static const diag_command_t g_commands[] = {
    { "sensor",   cmd_sensor,       "sensor read <ch:0-3>" },
    { "actuator", cmd_actuator,     "actuator set <id:0-1> <percent:0-100>" },
    { "status",   cmd_status,       "status" },
    { "version",  cmd_version,      "version" },
    { "log",      cmd_log,          "log [count:1-500]" },
    { "config",   cmd_config,       "config get | config set <key> <value>" },
    { "reset",    cmd_reset,        "reset mcu" },
    { "help",     cmd_help,         "help" },
    { NULL, NULL, NULL }
};

// Command lookup - exact lowercase match required
static const diag_command_t* find_command(const char *name) {
    for (int i = 0; g_commands[i].name != NULL; i++) {
        if (strcmp(name, g_commands[i].name) == 0) {  // strcmp, not strcasecmp!
            return &g_commands[i];
        }
    }
    return NULL;  // "ERROR UNKNOWN_COMMAND"
}
```

## 4. Command Implementations

### sensor read

```
Input:  sensor read 0
Output: OK SENSOR 0 23.5 C 1710672000000
Error:  ERROR INVALID_CHANNEL

Input:  SENSOR READ 0           (WRONG - uppercase)
Output: ERROR UNKNOWN_COMMAND
```

Reads from sensor_proxy cache (no MCU round-trip).

### actuator set

```
Input:  actuator set 0 50
Output: OK ACTUATOR 0 SET 50.0%
Error:  ERROR OUT_OF_RANGE
Error:  ERROR RATE_LIMITED

Input:  SET_ACTUATOR 0 50       (WRONG - old format)
Output: ERROR UNKNOWN_COMMAND
```

Sends ActuatorCommand to MCU via actuator_proxy, waits for response.

### status

```
Input:  status
Output (multi-line):
  COMM: CONNECTED
  MCU_STATE: NORMAL
  UPTIME: 3600s
  WDG_RESETS: 0
  SENSOR_0: 23.5 C
  SENSOR_1: 45.2 %RH
  SENSOR_2: 1013.2 hPa
  SENSOR_3: 5000 lux
  ACTUATOR_0: 50.0%
  ACTUATOR_1: 0.0%
  END

Input:  STATUS                  (WRONG - uppercase)
Output: ERROR UNKNOWN_COMMAND
```

### version

```
Input:  version
Output:
  Linux: 1.0.0-abc123
  MCU: 1.0.0-def456
  END
```

### log

```
Input:  log 10
Output: 10 most recent log entries in chronological order, followed by END

Input:  log                     (default: 50 entries)
```

### config get / config set

```
Input:  config get
Output:
  channel0_rate: 10
  channel1_rate: 10
  channel2_rate: 10
  channel3_rate: 10
  fan_max: 95
  valve_max: 100
  END

Input:  config set channel0_rate 50
Output: OK CONFIG channel0_rate SET 50

Input:  config set channel0_rate 999
Output: ERROR OUT_OF_RANGE
```

### reset mcu

```
Input:  reset mcu
Output: OK MCU_RESET_INITIATED
```

Triggers health_monitor recovery sequence.

### help

```
Input:  help
Output:
  Available commands:
    sensor read <ch>          - Read sensor channel (0-3)
    actuator set <id> <val>   - Set actuator (0-1) to percent (0-100)
    status                    - Show system status
    version                   - Show firmware versions
    log [count]               - Show recent log entries
    config get                - Show current configuration
    config set <key> <val>    - Set configuration value
    reset mcu                 - Trigger MCU reset
    help                      - Show this help

  NOTE: All commands are lowercase only!
  END
```

## 5. Error Responses

All errors follow the format: `ERROR <CODE> [message]\n`

| Error Code | Meaning |
|------------|---------|
| `UNKNOWN_COMMAND` | Command not recognized (including wrong case) |
| `INVALID_CHANNEL` | Sensor channel not 0-3 |
| `INVALID_ACTUATOR` | Actuator ID not 0-1 |
| `OUT_OF_RANGE` | Value outside allowed range |
| `RATE_LIMITED` | Too many commands per second |
| `COMM_ERROR` | MCU communication failed |
| `TIMEOUT` | MCU response timeout |

## 6. Client State Machine

![Diagnostics Client State Machine](../../architecture/diagrams/diag_client_state.drawio.svg)

```
DISCONNECTED
     │
     v
ACCEPTING ──(second client)──► REJECT (close immediately)
     │
     v
CONNECTED ──(recv command)──► PROCESSING
     │                              │
     │                              v
     │                        SENDING_RESPONSE
     │                              │
     └──────────────────────────────┘
     │
     v (client disconnect or error)
DISCONNECTED
```

## 7. Buffer Management

- Input buffer: 256 bytes per client (max command length)
- Output buffer: 4096 bytes per client (max response length)
- Non-blocking I/O with epoll integration

```c
typedef struct {
    int fd;
    char input_buf[256];
    size_t input_len;
    char output_buf[4096];
    size_t output_len;
    size_t output_sent;
} diag_client_t;
```

## 8. Test Commands

```bash
# Connect to diagnostic port
nc localhost 5002

# Valid commands (lowercase!)
status
sensor read 0
actuator set 0 50
version
help

# Invalid commands (will fail!)
STATUS              # uppercase
SENSOR READ 0       # uppercase
GET_STATUS          # old format
```

## 9. Traceability

| Requirement | Implementation |
|-------------|---------------|
| SWE-046-2 | `diagnostics_init()` — TCP server on port 5002, single client |
| SWE-047-1 | `cmd_sensor()` — sensor read command |
| SWE-048-1 | `cmd_actuator()` — actuator set command |
| SWE-049-1 | `cmd_status()` — status command |
| SWE-050-1 | `cmd_version()` — version command |
| SWE-051-1 | `cmd_reset()` — reset mcu command |
| SWE-054-1 | `cmd_log()` — log command |
| SWE-054-2 | `cmd_help()` — help command |
