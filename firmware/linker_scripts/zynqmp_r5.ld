
/* Default linker script for Cortex-R5 firmware running from TCM on ZynqMP */
MEMORY
{
    TCM (rwx) : ORIGIN = 0x00000000, LENGTH = 256K
}

ENTRY(_start)

_estack = ORIGIN(TCM) + LENGTH(TCM);

SECTIONS
{
    .vectors :
    {
        KEEP(*(.vectors))
        KEEP(*(.vectors.*))
    } > TCM

    .text :
    {
        *(.text*)
        *(.rodata*)
        *(.glue*)
        *(.gnu.linkonce.t*)
        *(.gnu.linkonce.r*)
        *(.ARM.extab*)
    } > TCM

    .ARM.exidx :
    {
        *(.ARM.exidx*)
    } > TCM

    .data :
    {
        __data_start__ = .;
        *(.data*)
        __data_end__ = .;
    } > TCM

    .bss (NOLOAD) :
    {
        __bss_start__ = .;
        *(.bss*)
        *(COMMON)
        __bss_end__ = .;
    } > TCM

    .heap (NOLOAD) :
    {
        . = ALIGN(8);
        __heap_start__ = .;
        . += 0x2000;
        __heap_end__ = .;
    } > TCM

    .stack (NOLOAD) :
    {
        . = ALIGN(8);
        __stack_bottom__ = .;
        . += 0x2000;
        __stack_top__ = .;
    } > TCM

    __end__ = .;
}

PROVIDE(_stack = __stack_top__);
PROVIDE(__StackTop = __stack_top__);
PROVIDE(__StackLimit = __stack_bottom__);
PROVIDE(_end = __end__);
PROVIDE(__end__ = __end__);
PROVIDE(__heap_start = __heap_start__);
PROVIDE(__heap_end = __heap_end__);
