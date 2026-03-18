/**
 * @file systick.h
 * @brief SysTick millisecond timer
 */

#ifndef SYSTICK_H
#define SYSTICK_H

#include <stdint.h>

void systick_init(void);
uint32_t systick_get_ms(void);
uint64_t systick_get_us(void);

#endif /* SYSTICK_H */
