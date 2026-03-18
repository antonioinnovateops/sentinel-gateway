/**
 * @file mps2_an505.h
 * @brief MPS2+ AN505 (Cortex-M33) register definitions for QEMU emulation
 *
 * The MPS2+ AN505 FPGA image is the closest Cortex-M33 platform that QEMU
 * fully supports. This file maps the MPS2 peripherals for use with the
 * Sentinel Gateway MCU firmware.
 *
 * Memory map (AN505 in QEMU):
 *   0x00000000 - 0x003FFFFF : Code SRAM (4 MB)
 *   0x20000000 - 0x203FFFFF : Data SRAM (4 MB)
 *   0x40000000 - 0x4FFFFFFF : APB Peripherals
 *
 * @see QEMU hw/arm/mps2-tz.c for implementation details
 */

#ifndef MPS2_AN505_H
#define MPS2_AN505_H

#include <stdint.h>

/* ---- Cortex-M33 System Control Block ---- */
#define SCB_BASE        0xE000ED00UL
#define NVIC_BASE       0xE000E100UL
#define SYSTICK_BASE    0xE000E010UL

/* SysTick registers (same as Cortex-M standard) */
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

/* ---- MPS2 FPGA GPIO (simulates LEDs/buttons) ---- */
#define FPGAIO_BASE     0x40302000UL

typedef struct {
    volatile uint32_t LED;       /* 0x00: LED outputs */
    volatile uint32_t RESERVED1[1];
    volatile uint32_t BUTTON;    /* 0x08: Button inputs */
    volatile uint32_t RESERVED2[1];
    volatile uint32_t CLK1HZ;    /* 0x10: 1Hz counter */
    volatile uint32_t CLK100HZ;  /* 0x14: 100Hz counter */
    volatile uint32_t COUNTER;   /* 0x18: Cycle counter */
    volatile uint32_t RESERVED3[1];
    volatile uint32_t MISC;      /* 0x20: Misc control */
} FPGAIO_Type;

#define FPGAIO ((FPGAIO_Type *)FPGAIO_BASE)

/* ---- MPS2 UART (CMSDK UART) ---- */
#define UART0_BASE      0x40200000UL
#define UART1_BASE      0x40201000UL

typedef struct {
    volatile uint32_t DATA;      /* 0x00: Data register */
    volatile uint32_t STATE;     /* 0x04: Status register */
    volatile uint32_t CTRL;      /* 0x08: Control register */
    volatile uint32_t INTSTATUS; /* 0x0C: Interrupt status */
    volatile uint32_t BAUDDIV;   /* 0x10: Baud rate divider */
} CMSDK_UART_Type;

#define UART0 ((CMSDK_UART_Type *)UART0_BASE)
#define UART1 ((CMSDK_UART_Type *)UART1_BASE)

#define CMSDK_UART_STATE_TXFULL  (1UL << 0)
#define CMSDK_UART_STATE_RXFULL  (1UL << 1)
#define CMSDK_UART_CTRL_TXEN     (1UL << 0)
#define CMSDK_UART_CTRL_RXEN     (1UL << 1)

/* ---- MPS2 Timer (CMSDK Timer) ---- */
#define TIMER0_BASE     0x40000000UL
#define TIMER1_BASE     0x40001000UL

typedef struct {
    volatile uint32_t CTRL;      /* 0x00: Control register */
    volatile uint32_t VALUE;     /* 0x04: Current value */
    volatile uint32_t RELOAD;    /* 0x08: Reload value */
    volatile uint32_t INTSTATUS; /* 0x0C: Interrupt status/clear */
} CMSDK_TIMER_Type;

#define TIMER0 ((CMSDK_TIMER_Type *)TIMER0_BASE)
#define TIMER1 ((CMSDK_TIMER_Type *)TIMER1_BASE)

#define CMSDK_TIMER_CTRL_EN      (1UL << 0)
#define CMSDK_TIMER_CTRL_IRQEN   (1UL << 3)

/* ---- MPS2 Watchdog (CMSDK Watchdog) ---- */
#define WDOG_BASE       0x40081000UL

typedef struct {
    volatile uint32_t LOAD;      /* 0x000: Load register */
    volatile uint32_t VALUE;     /* 0x004: Current value */
    volatile uint32_t CTRL;      /* 0x008: Control register */
    volatile uint32_t INTCLR;    /* 0x00C: Interrupt clear */
    volatile uint32_t RAWINTSTAT;/* 0x010: Raw interrupt status */
    volatile uint32_t MASKINTSTAT;/* 0x014: Masked interrupt status */
    volatile uint32_t RESERVED[762];
    volatile uint32_t LOCK;      /* 0xC00: Lock register */
} CMSDK_WDOG_Type;

#define WDOG ((CMSDK_WDOG_Type *)WDOG_BASE)

#define CMSDK_WDOG_CTRL_INTEN    (1UL << 0)
#define CMSDK_WDOG_CTRL_RESEN    (1UL << 1)
#define CMSDK_WDOG_UNLOCK_KEY    0x1ACCE551UL

/* ---- Simulated GPIO for STM32 compatibility ---- */
/* We'll map GPIO operations to FPGAIO->LED */
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

/* Use RAM-backed simulated GPIO registers */
extern GPIO_Type g_qemu_gpioa;
extern GPIO_Type g_qemu_gpiob;

#define GPIOA (&g_qemu_gpioa)
#define GPIOB (&g_qemu_gpiob)

/* ---- Simulated ADC ---- */
/* MPS2 doesn't have ADC - simulate via software */
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
} ADC_Type;

extern ADC_Type g_qemu_adc1;
#define ADC1 (&g_qemu_adc1)

#define ADC_CR_ADEN      (1UL << 0)
#define ADC_CR_ADSTART   (1UL << 2)
#define ADC_ISR_EOC      (1UL << 2)
#define ADC_ISR_EOS      (1UL << 3)

/* ---- Simulated TIM2 for PWM ---- */
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

extern TIM_Type g_qemu_tim2;
#define TIM2 (&g_qemu_tim2)

#define TIM_CR1_CEN     (1UL << 0)
#define TIM_CCER_CC1E   (1UL << 0)
#define TIM_CCER_CC2E   (1UL << 4)

/* ---- Simulated Flash ---- */
typedef struct {
    volatile uint32_t ACR;
    volatile uint32_t RESERVED1;
    volatile uint32_t KEYR;
    volatile uint32_t OPTKEYR;
    volatile uint32_t SR;
    volatile uint32_t CR;
} FLASH_Type;

extern FLASH_Type g_qemu_flash;
#define FLASH (&g_qemu_flash)

#define FLASH_KEY1       0x45670123UL
#define FLASH_KEY2       0xCDEF89ABUL
#define FLASH_SR_BSY     (1UL << 16)
#define FLASH_CR_PG      (1UL << 0)
#define FLASH_CR_PER     (1UL << 1)
#define FLASH_CR_STRT    (1UL << 16)
#define FLASH_CR_LOCK    (1UL << 31)

/* ---- Simulated IWDG ---- */
/* Map to CMSDK Watchdog */
typedef struct {
    volatile uint32_t KR;
    volatile uint32_t PR;
    volatile uint32_t RLR;
    volatile uint32_t SR;
} IWDG_Type;

extern IWDG_Type g_qemu_iwdg;
#define IWDG (&g_qemu_iwdg)

#define IWDG_KEY_START   0xCCCCU
#define IWDG_KEY_RELOAD  0xAAAAU
#define IWDG_KEY_ACCESS  0x5555U
#define IWDG_PR_DIV256   6U

/* ---- Simulated RCC ---- */
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

extern RCC_Type g_qemu_rcc;
extern volatile uint32_t g_qemu_rcc_csr;

#define RCC (&g_qemu_rcc)
#define RCC_CSR          g_qemu_rcc_csr
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

/* System clock (25 MHz on MPS2 AN505) */
#define SYSTEM_CLOCK_HZ  25000000UL

#endif /* MPS2_AN505_H */
