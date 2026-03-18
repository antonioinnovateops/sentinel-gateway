/**
 * @file systick.c
 * @brief SysTick millisecond counter
 */

#include "systick.h"
#include "stm32u575.h"

static volatile uint32_t g_tick_ms = 0U;

void systick_init(void)
{
    /* Configure SysTick for 1ms interrupt */
    SysTick->LOAD = (SYSTEM_CLOCK_HZ / 1000U) - 1U;
    SysTick->VAL = 0U;
    SysTick->CTRL = SYSTICK_CTRL_ENABLE | SYSTICK_CTRL_TICKINT | SYSTICK_CTRL_CLKSOURCE;
}

uint32_t systick_get_ms(void)
{
    return g_tick_ms;
}

uint64_t systick_get_us(void)
{
    /* Approximate: ms * 1000 + partial tick */
    uint32_t ms = g_tick_ms;
    uint32_t val = SysTick->VAL;
    uint32_t load = SysTick->LOAD;
    uint32_t us_frac = ((load - val) * 1000U) / (load + 1U);
    return ((uint64_t)ms * 1000ULL) + (uint64_t)us_frac;
}

/* SysTick interrupt handler */
void SysTick_Handler(void)
{
    g_tick_ms++;
}
