---
title: "Detailed Design — MCU Actuator Control"
document_id: "DD-MCU-002"
project: "Sentinel Gateway"
version: "1.0.0"
date: 2026-03-17
status: "Approved"
aspice_process: "SWE.3 Software Detailed Design"
component: "MCU / actuator_control"
requirements: ["SWE-016-1", "SWE-016-2", "SWE-017-1", "SWE-018-1", "SWE-019-1", "SWE-021-1", "SWE-022-1", "SWE-022-2", "SWE-023-1", "SWE-024-1", "SWE-026-1", "SWE-027-1", "SWE-028-1", "SWE-029-1"]
safety_class: "ASIL-B"
---

# Detailed Design — MCU Actuator Control

## 1. Purpose

The actuator_control module is the most safety-critical component. It processes actuator commands with multi-layer validation and manages fail-safe transitions.

## 2. Safety Architecture

```
Command In ──► [1] Decode ──► [2] Auth Check ──► [3] Rate Limit
                                                        │
                                                        ▼
              [6] Readback ◄── [5] PWM Write ◄── [4] Range+Limit
                   │                                    Validate
                   ▼
              [7] Response ──► Response Out

Any step failure ──► enter_safe_state()
```

### Validation Layers (Defense in Depth)

1. **Protobuf decode**: Invalid data → fail-safe (SWE-028-1)
2. **Authentication**: Safety limit changes require auth token (SWE-059-1)
3. **Rate limiting**: Max 50 cmd/sec/actuator (SWE-021-1)
4. **Range validation**: Reject values outside 0-100% (SWE-022-1)
5. **Limit clamping**: Clamp to configured min/max (SWE-023-1, SWE-024-1)
6. **Output readback**: Verify PWM register matches intent (SWE-022-2)

## 3. State Machine

```
         ┌──────────┐
         │  INIT    │ (PWM = 0%, no commands accepted)
         └────┬─────┘
              │ Init complete
         ┌────▼─────┐
    ┌────│  ACTIVE  │◄──── Recovery from FAILSAFE
    │    └────┬─────┘      (only via full state sync)
    │         │
    │    Any safety       
    │    violation         
    │         │
    │    ┌────▼─────┐
    └───►│ FAILSAFE │ (PWM = 0%, reject all commands)
         └──────────┘
```

## 4. Data Structures

```c
/** Per-actuator runtime state */
typedef struct {
    actuator_id_t id;
    float current_duty;         /** Current PWM duty cycle (0-100%) */
    float target_duty;          /** Last commanded target */
    actuator_limits_t limits;   /** Configured safety limits */
    uint32_t cmd_count;         /** Commands in current 1-second window */
    uint32_t window_start_ms;   /** Start of rate-limit window */
    bool active;                /** True if accepting commands */
} actuator_state_internal_t;

/** Module state */
typedef struct {
    actuator_state_internal_t actuators[ACTUATOR_COUNT];
    mcu_state_t state;          /** INIT, ACTIVE, or FAILSAFE */
    failsafe_cause_t cause;     /** Why failsafe was triggered */
    uint32_t comm_last_activity_ms;  /** Last message from gateway */
    uint32_t comm_timeout_ms;   /** Timeout threshold (default 3000) */
} actuator_ctrl_context_t;

static actuator_ctrl_context_t g_ctx;  /** Module-private state */
```

## 5. Key Algorithms

### 5.1 Range Validation + Limit Clamping

```c
/**
 * @brief Validate and clamp actuator command value
 * @implements [SWE-022-1] Range Validation
 * @implements [SWE-023-1] Fan Safety Limits
 * @implements [SWE-024-1] Valve Safety Limits
 * @safety_class ASIL-B
 *
 * @param[in]  id              Actuator identifier
 * @param[in]  target_percent  Commanded value (0.0-100.0)
 * @param[out] applied_percent Actual value after clamping
 * @return SENTINEL_OK or error code
 */
static int32_t validate_and_clamp(actuator_id_t id,
                                   float target_percent,
                                   float *applied_percent)
{
    int32_t result = SENTINEL_ERR_RANGE;

    if ((id < ACTUATOR_COUNT) && (applied_percent != NULL))
    {
        const actuator_limits_t *limits = &g_ctx.actuators[id].limits;

        /* Hard range check: 0-100% absolute */
        if ((target_percent >= 0.0f) && (target_percent <= 100.0f))
        {
            /* Clamp to configured limits */
            float clamped = target_percent;
            if (clamped > limits->max_percent)
            {
                clamped = limits->max_percent;
            }
            if (clamped < limits->min_percent)
            {
                clamped = limits->min_percent;
            }

            *applied_percent = clamped;
            result = SENTINEL_OK;
        }
        /* else: out of range, return error */
    }

    return result;
}
```

### 5.2 Rate Limiting

```c
/**
 * @brief Check rate limit for an actuator
 * @implements [SWE-021-1] Rate Limiting
 * @safety_class ASIL-B
 *
 * @param[in] id Actuator identifier
 * @return true if command allowed, false if rate-limited
 */
static bool check_rate_limit(actuator_id_t id)
{
    bool allowed = false;
    uint32_t now_ms = hal_get_tick_ms();
    actuator_state_internal_t *act = &g_ctx.actuators[id];

    /* Reset window if 1 second has passed */
    if ((now_ms - act->window_start_ms) >= 1000U)
    {
        act->cmd_count = 0U;
        act->window_start_ms = now_ms;
    }

    /* Check against limit */
    if (act->cmd_count < 50U)
    {
        act->cmd_count++;
        allowed = true;
    }

    return allowed;
}
```

### 5.3 Output Readback Verification

```c
/**
 * @brief Verify PWM output matches commanded value
 * @implements [SWE-022-2] Output Readback
 * @safety_class ASIL-B
 *
 * @param[in] id      Actuator identifier
 * @param[in] expected Expected duty cycle (%)
 * @return true if readback matches, false triggers fail-safe
 */
static bool verify_output(actuator_id_t id, float expected)
{
    float actual = 0.0f;
    bool match = false;

    if (pwm_driver_get_duty((uint8_t)id, &actual) == SENTINEL_OK)
    {
        /* Allow ±0.01% tolerance (1 PWM step at 16-bit resolution) */
        float diff = actual - expected;
        if (diff < 0.0f) { diff = -diff; }  /* abs */

        if (diff <= 0.01f)
        {
            match = true;
        }
    }

    if (!match)
    {
        actuator_ctrl_enter_safe_state(FAILSAFE_INTERNAL_ERROR);
    }

    return match;
}
```

## 6. Communication Timeout

```c
/**
 * @brief Check if gateway communication has timed out
 * @implements [SWE-026-1] Communication Loss Timer
 * @safety_class ASIL-B
 */
bool actuator_ctrl_check_comm_timeout(void)
{
    uint32_t now_ms = hal_get_tick_ms();
    uint32_t elapsed = now_ms - g_ctx.comm_last_activity_ms;

    return (elapsed >= g_ctx.comm_timeout_ms);
}
```

## 7. Fail-Safe Entry

```c
/**
 * @brief Transition all actuators to safe state
 * @implements [SWE-026-1], [SWE-027-1], [SWE-028-1]
 * @safety_class ASIL-B
 */
void actuator_ctrl_enter_safe_state(failsafe_cause_t cause)
{
    /* Set all PWM to 0% */
    for (uint8_t i = 0U; i < (uint8_t)ACTUATOR_COUNT; i++)
    {
        (void)pwm_driver_set_duty(i, 0.0f);
        g_ctx.actuators[i].current_duty = 0.0f;
        g_ctx.actuators[i].active = false;
    }

    g_ctx.state = MCU_STATE_FAILSAFE;
    g_ctx.cause = cause;

    /* Immediately report via health reporter */
    health_reporter_set_state(MCU_STATE_FAILSAFE);
}
```
