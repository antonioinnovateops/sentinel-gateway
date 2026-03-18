/**
 * @file startup_stm32u575.s
 * @brief Minimal Cortex-M33 startup code for QEMU
 *
 * Vector table + Reset_Handler that:
 * 1. Copies .data from flash to RAM
 * 2. Zeros .bss
 * 3. Calls main()
 */

    .syntax unified
    .cpu cortex-m33
    .fpu fpv5-sp-d16
    .thumb

    .section .isr_vector, "a", %progbits
    .type g_pfnVectors, %object
    .size g_pfnVectors, .-g_pfnVectors

g_pfnVectors:
    .word _estack              /* Initial stack pointer */
    .word Reset_Handler        /* Reset */
    .word NMI_Handler          /* NMI */
    .word HardFault_Handler    /* Hard fault */
    .word MemManage_Handler    /* Memory management fault */
    .word BusFault_Handler     /* Bus fault */
    .word UsageFault_Handler   /* Usage fault */
    .word 0                    /* Reserved */
    .word 0                    /* Reserved */
    .word 0                    /* Reserved */
    .word 0                    /* Reserved */
    .word SVC_Handler          /* SVCall */
    .word DebugMon_Handler     /* Debug monitor */
    .word 0                    /* Reserved */
    .word PendSV_Handler       /* PendSV */
    .word SysTick_Handler      /* SysTick */

    /* IRQ handlers (simplified — first 48) */
    .rept 48
    .word Default_Handler
    .endr

/* ============================================================ */

    .section .text.Reset_Handler
    .weak Reset_Handler
    .type Reset_Handler, %function
Reset_Handler:
    /* Copy .data section from flash to RAM */
    ldr r0, =_sdata
    ldr r1, =_edata
    ldr r2, =_sidata
    movs r3, #0
    b .copy_data_check
.copy_data_loop:
    ldr r4, [r2, r3]
    str r4, [r0, r3]
    adds r3, r3, #4
.copy_data_check:
    adds r4, r0, r3
    cmp r4, r1
    blt .copy_data_loop

    /* Zero .bss section */
    ldr r0, =_sbss
    ldr r1, =_ebss
    movs r2, #0
    b .zero_bss_check
.zero_bss_loop:
    str r2, [r0]
    adds r0, r0, #4
.zero_bss_check:
    cmp r0, r1
    blt .zero_bss_loop

    /* Call main */
    bl main

    /* If main returns, loop forever */
    b .

    .size Reset_Handler, .-Reset_Handler

/* ============================================================ */
/* Default handlers (weak, can be overridden) */

    .section .text.Default_Handler, "ax", %progbits
    .weak Default_Handler
    .type Default_Handler, %function
Default_Handler:
    b .
    .size Default_Handler, .-Default_Handler

    /* Weak aliases for exception handlers */
    .weak NMI_Handler
    .set NMI_Handler, Default_Handler
    .weak HardFault_Handler
    .set HardFault_Handler, Default_Handler
    .weak MemManage_Handler
    .set MemManage_Handler, Default_Handler
    .weak BusFault_Handler
    .set BusFault_Handler, Default_Handler
    .weak UsageFault_Handler
    .set UsageFault_Handler, Default_Handler
    .weak SVC_Handler
    .set SVC_Handler, Default_Handler
    .weak DebugMon_Handler
    .set DebugMon_Handler, Default_Handler
    .weak PendSV_Handler
    .set PendSV_Handler, Default_Handler
    /* SysTick_Handler is defined in systick.c — NOT weak here */
