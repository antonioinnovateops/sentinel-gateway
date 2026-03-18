/**
 * @file stm32u575.h
 * @brief Minimal STM32U575 register definitions for QEMU emulation
 *
 * In real hardware, this would come from the CMSIS device pack.
 * For QEMU netduinoplus2, we define the minimal subset needed.
 */

#ifndef STM32U575_H
#define STM32U575_H

#include <stdint.h>

/* ---- Cortex-M33 System Control Block ---- */
#define SCB_BASE        0xE000ED00UL
#define NVIC_BASE       0xE000E100UL
#define SYSTICK_BASE    0xE000E010UL

/* SysTick registers */
typedef struct {
    volatile uint32_t CTRL;
    volatile uint32_t LOAD;
    volatile uint32_t VAL;
    volatile uint32_t CALIB;
} SysTick_Type;

#define SysTick ((SysTick_Type *)SYSTICK_BASE)

#define SYSTICK_CTRL_ENABLE    (1UL << 0)
#define SYSTICK_CTRL_TICKINT   (1UL << 1)
#define SYSTICK_CTRL_CLKSOURCE (1UL << 2)
#define SYSTICK_CTRL_COUNTFLAG (1UL << 16)

/* ---- IWDG (Independent Watchdog) ---- */
#define IWDG_BASE       0x40003000UL

typedef struct {
    volatile uint32_t KR;
    volatile uint32_t PR;
    volatile uint32_t RLR;
    volatile uint32_t SR;
} IWDG_Type;

#define IWDG ((IWDG_Type *)IWDG_BASE)

#define IWDG_KEY_START   0xCCCCU
#define IWDG_KEY_RELOAD  0xAAAAU
#define IWDG_KEY_ACCESS  0x5555U
#define IWDG_PR_DIV256   6U

/* ---- GPIO ---- */
#define GPIOA_BASE      0x42020000UL
#define GPIOB_BASE      0x42020400UL

typedef struct {
    volatile uint32_t MODER;
    volatile uint32_t OTYPER;
    volatile uint32_t OSPEEDR;
    volatile uint32_t PUPDR;
    volatile uint32_t IDR;
    volatile uint32_t ODR;
    volatile uint32_t BSRR;
    volatile uint32_t LCKR;
    volatile uint32_t AFR[2];
} GPIO_Type;

#define GPIOA ((GPIO_Type *)GPIOA_BASE)
#define GPIOB ((GPIO_Type *)GPIOB_BASE)

/* ---- ADC ---- */
#define ADC1_BASE       0x42028000UL

typedef struct {
    volatile uint32_t ISR;
    volatile uint32_t IER;
    volatile uint32_t CR;
    volatile uint32_t CFGR;
    volatile uint32_t CFGR2;
    volatile uint32_t SMPR1;
    volatile uint32_t SMPR2;
    volatile uint32_t RESERVED1;
    volatile uint32_t TR1;
    volatile uint32_t TR2;
    volatile uint32_t TR3;
    volatile uint32_t RESERVED2;
    volatile uint32_t SQR1;
    volatile uint32_t SQR2;
    volatile uint32_t SQR3;
    volatile uint32_t SQR4;
    volatile uint32_t DR;
    volatile uint32_t RESERVED3[2];
    volatile uint32_t JSQR;
    volatile uint32_t RESERVED4[4];
    volatile uint32_t OFR1;
    volatile uint32_t OFR2;
    volatile uint32_t OFR3;
    volatile uint32_t OFR4;
    volatile uint32_t RESERVED5[4];
    volatile uint32_t JDR1;
    volatile uint32_t JDR2;
    volatile uint32_t JDR3;
    volatile uint32_t JDR4;
    volatile uint32_t RESERVED6[4];
    volatile uint32_t AWD2CR;
    volatile uint32_t AWD3CR;
} ADC_Type;

#define ADC1 ((ADC_Type *)ADC1_BASE)

#define ADC_CR_ADEN      (1UL << 0)
#define ADC_CR_ADSTART   (1UL << 2)
#define ADC_ISR_EOC      (1UL << 2)
#define ADC_ISR_EOS      (1UL << 3)

/* ---- TIM2 (PWM) ---- */
#define TIM2_BASE       0x40000000UL

typedef struct {
    volatile uint32_t CR1;
    volatile uint32_t CR2;
    volatile uint32_t SMCR;
    volatile uint32_t DIER;
    volatile uint32_t SR;
    volatile uint32_t EGR;
    volatile uint32_t CCMR1;
    volatile uint32_t CCMR2;
    volatile uint32_t CCER;
    volatile uint32_t CNT;
    volatile uint32_t PSC;
    volatile uint32_t ARR;
    volatile uint32_t RESERVED1;
    volatile uint32_t CCR1;
    volatile uint32_t CCR2;
    volatile uint32_t CCR3;
    volatile uint32_t CCR4;
} TIM_Type;

#define TIM2 ((TIM_Type *)TIM2_BASE)

#define TIM_CR1_CEN     (1UL << 0)
#define TIM_CCER_CC1E   (1UL << 0)
#define TIM_CCER_CC2E   (1UL << 4)

/* ---- FLASH ---- */
#define FLASH_BASE      0x40022000UL

typedef struct {
    volatile uint32_t ACR;
    volatile uint32_t RESERVED1;
    volatile uint32_t KEYR;
    volatile uint32_t OPTKEYR;
    volatile uint32_t SR;
    volatile uint32_t CR;
    volatile uint32_t RESERVED2[2];
    volatile uint32_t OPTR;
} FLASH_Type;

#define FLASH ((FLASH_Type *)FLASH_BASE)

#define FLASH_KEY1       0x45670123UL
#define FLASH_KEY2       0xCDEF89ABUL
#define FLASH_SR_BSY     (1UL << 16)
#define FLASH_CR_PG      (1UL << 0)
#define FLASH_CR_PER     (1UL << 1)
#define FLASH_CR_STRT    (1UL << 16)
#define FLASH_CR_LOCK    (1UL << 31)

/* ---- RCC ---- */
#define RCC_BASE         0x44020C00UL

typedef struct {
    volatile uint32_t CR;
    volatile uint32_t ICSCR;
    volatile uint32_t CFGR;
    volatile uint32_t RESERVED1[2];
    volatile uint32_t AHB1ENR;
    volatile uint32_t AHB2ENR;
    volatile uint32_t AHB3ENR;
    volatile uint32_t RESERVED2;
    volatile uint32_t APB1ENR1;
    volatile uint32_t APB1ENR2;
    volatile uint32_t APB2ENR;
} RCC_Type;

#define RCC ((RCC_Type *)RCC_BASE)

/* Reset cause detection */
#define RCC_CSR          (*(volatile uint32_t *)(RCC_BASE + 0x94UL))
#define RCC_CSR_IWDGRSTF (1UL << 29)

/* ---- Interrupt numbers ---- */
#define SysTick_IRQn    (-1)
#define ADC1_IRQn       18
#define TIM2_IRQn       28

/* ---- Core functions ---- */
static inline void __enable_irq(void)  { __asm volatile ("cpsie i" ::: "memory"); }
static inline void __disable_irq(void) { __asm volatile ("cpsid i" ::: "memory"); }
static inline void __WFI(void)         { __asm volatile ("wfi"); }
static inline void __DSB(void)         { __asm volatile ("dsb 0xF" ::: "memory"); }
static inline void __ISB(void)         { __asm volatile ("isb 0xF" ::: "memory"); }

/* NVIC helpers */
static inline void NVIC_EnableIRQ(int irqn) {
    ((volatile uint32_t *)NVIC_BASE)[irqn / 32] = 1UL << (irqn % 32);
}

/* System clock (default 4 MHz MSI on STM32U575) */
#define SYSTEM_CLOCK_HZ  4000000UL

#endif /* STM32U575_H */
