---
title: "Detailed Design — Linux Diagnostics Server"
document_id: "DD-LINUX-002"
project: "Sentinel Gateway"
version: "1.0.0"
date: 2026-03-17
status: "Approved"
aspice_process: "SWE.3 Software Detailed Design"
component: "LINUX / diagnostics"
requirements: ["SWE-046-2", "SWE-047-1", "SWE-048-1", "SWE-049-1", "SWE-050-1", "SWE-051-1", "SWE-054-1"]
---

# Detailed Design — Linux Diagnostics Server

## 1. Purpose

Provide a simple text-based diagnostic interface on TCP port 5002 for field troubleshooting, sensor reading, actuator control, and log retrieval.

## 2. Connection Model

- TCP server on port 5002
- Max 3 concurrent client connections
- Line-delimited protocol (CR+LF)
- No authentication on read commands (GET_*)
- SET/RESET commands could require auth (future)

## 3. Command Parser

```c
typedef struct {
    const char *name;
    int (*handler)(diag_client_t *client, int argc, char **argv);
    const char *help;
} diag_command_t;

static const diag_command_t g_commands[] = {
    { "READ_SENSOR",  cmd_read_sensor,  "READ_SENSOR <ch:0-3>" },
    { "SET_ACTUATOR", cmd_set_actuator, "SET_ACTUATOR <id:0-1> <percent:0-100>" },
    { "GET_STATUS",   cmd_get_status,   "GET_STATUS" },
    { "GET_VERSION",  cmd_get_version,  "GET_VERSION" },
    { "GET_LOG",      cmd_get_log,      "GET_LOG [count:1-500]" },
    { "GET_CONFIG",   cmd_get_config,   "GET_CONFIG" },
    { "RESET_MCU",    cmd_reset_mcu,    "RESET_MCU" },
    { "HELP",         cmd_help,         "HELP" },
    { NULL, NULL, NULL }
};
```

## 4. Command Implementations

### READ_SENSOR
```
Input:  READ_SENSOR 0
Output: OK TEMPERATURE 23.5 °C 1710672000000
Error:  ERROR INVALID_CHANNEL Channel must be 0-3
```
Reads from sensor_proxy cache (no MCU round-trip).

### SET_ACTUATOR
```
Input:  SET_ACTUATOR 0 50
Output: OK ACTUATOR 0 SET 50.0%
Error:  ERROR OUT_OF_RANGE Value must be 0-100
Error:  ERROR RATE_LIMITED Too many commands
```
Sends ActuatorCommand to MCU via actuator_proxy, waits for response.

### GET_STATUS
```
Output (multi-line):
  COMM: CONNECTED
  MCU_STATE: NORMAL
  UPTIME: 3600s
  WDG_RESETS: 0
  SENSOR_0: TEMPERATURE 23.5 °C
  SENSOR_1: HUMIDITY 45.2 %RH
  SENSOR_2: PRESSURE 1013.2 hPa
  SENSOR_3: LIGHT 5000 lux
  ACTUATOR_0: FAN 50.0%
  ACTUATOR_1: VALVE 0.0%
  END
```

### GET_VERSION
```
Output:
  LINUX 1.0.0-abc123
  MCU 1.0.0-def456
  END
```

### GET_LOG
```
Input:  GET_LOG 10
Output: 10 most recent log entries in chronological order, followed by END
```

### GET_CONFIG
```
Output:
  CH0: TEMPERATURE 10Hz enabled
  CH1: HUMIDITY 10Hz enabled
  CH2: PRESSURE 10Hz enabled
  CH3: LIGHT 10Hz enabled
  FAN_LIMITS: 0.0-95.0%
  VALVE_LIMITS: 0.0-100.0%
  END
```

### RESET_MCU
```
Output: OK MCU_RESET_INITIATED
```
Triggers health_monitor recovery sequence.

### HELP
```
Output: Lists all commands with usage, followed by END
```

## 5. Client State Machine

```
CONNECTED → READING_LINE → PARSING → EXECUTING → WRITING_RESPONSE → READING_LINE
                                                                         │
                                                           (client disconnect)
                                                                         ▼
                                                                    DISCONNECTED
```

## 6. Buffer Management

- Input buffer: 256 bytes per client (max command length)
- Output buffer: 4096 bytes per client (max response length)
- Non-blocking I/O with epoll integration
