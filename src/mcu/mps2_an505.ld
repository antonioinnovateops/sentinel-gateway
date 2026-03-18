/**
 * MPS2-AN505 Linker Script — Sentinel Gateway MCU Firmware (QEMU)
 *
 * Memory map for MPS2+ AN505 Cortex-M33 FPGA image in QEMU:
 *   0x00000000 - 0x003FFFFF : Code SRAM (4 MB)
 *   0x20000000 - 0x203FFFFF : Data SRAM (4 MB)
 *
 * No heap section (zero dynamic allocation enforced)
 *
 * @see QEMU hw/arm/mps2-tz.c for memory map details
 */

MEMORY
{
    FLASH  (rx)  : ORIGIN = 0x00000000, LENGTH = 512K
    RAM    (rwx) : ORIGIN = 0x20000000, LENGTH = 512K
}

/* Stack at top of RAM */
_estack = ORIGIN(RAM) + LENGTH(RAM);
_stack_size = 0x8000; /* 32 KB */

SECTIONS
{
    /* Interrupt vector table — must be first */
    .isr_vector :
    {
        . = ALIGN(4);
        KEEP(*(.isr_vector))
        . = ALIGN(4);
    } > FLASH

    /* Code */
    .text :
    {
        . = ALIGN(4);
        *(.text)
        *(.text*)
        *(.glue_7)
        *(.glue_7t)

        KEEP(*(.init))
        KEEP(*(.fini))
        . = ALIGN(4);
        _etext = .;
    } > FLASH

    /* Read-only data (constants, nanopb descriptors) */
    .rodata :
    {
        . = ALIGN(4);
        *(.rodata)
        *(.rodata*)
        . = ALIGN(4);
    } > FLASH

    /* ARM exception unwind tables */
    .ARM.extab :
    {
        *(.ARM.extab* .gnu.linkonce.armextab.*)
    } > FLASH

    .ARM :
    {
        __exidx_start = .;
        *(.ARM.exidx*)
        __exidx_end = .;
    } > FLASH

    /* Used by startup to initialize .data */
    _sidata = LOADADDR(.data);

    /* Initialized data (copied from flash to RAM by startup) */
    .data :
    {
        . = ALIGN(4);
        _sdata = .;
        *(.data)
        *(.data*)
        . = ALIGN(4);
        _edata = .;
    } > RAM AT> FLASH

    /* Uninitialized data (zeroed by startup) */
    .bss :
    {
        . = ALIGN(4);
        _sbss = .;
        __bss_start__ = _sbss;
        *(.bss)
        *(.bss*)
        *(COMMON)
        . = ALIGN(4);
        _ebss = .;
        __bss_end__ = _ebss;
    } > RAM

    /* No-init section (survives soft reset) */
    .noinit (NOLOAD) :
    {
        . = ALIGN(4);
        *(.noinit)
        . = ALIGN(4);
    } > RAM

    /* lwIP memory pool (128 KB, aligned) */
    .lwip_pool (NOLOAD) :
    {
        . = ALIGN(4);
        _lwip_pool_start = .;
        . = . + 131072; /* 128 KB */
        _lwip_pool_end = .;
    } > RAM

    /* Stack (32 KB at top of RAM, grows downward) */
    ._stack (NOLOAD) :
    {
        . = ALIGN(8);
        _sstack = .;
        . = . + _stack_size;
        . = ALIGN(8);
    } > RAM

    /* NO .heap section — enforce zero dynamic allocation */

    /* Assertions */
    ASSERT(. <= ORIGIN(RAM) + LENGTH(RAM), "RAM overflow")
    ASSERT(_stack_size >= 0x8000, "Stack too small (minimum 32 KB)")
}
