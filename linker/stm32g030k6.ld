ENTRY(Reset_Handler)

MEMORY
{
    FLASH      (rx)  : ORIGIN = 0x08000000, LENGTH = 32K
    RAM_STATIC (rwx) : ORIGIN = 0x20000000, LENGTH = 7K
    RAM_STACK  (rw)  : ORIGIN = 0x20001C00, LENGTH = 1K
}

_stack_limit = ORIGIN(RAM_STACK);
_stack_top = ORIGIN(RAM_STACK) + LENGTH(RAM_STACK);

SECTIONS
{
    .isr_vector :
    {
        . = ALIGN(4);
        KEEP(*(.isr_vector))
        . = ALIGN(4);
    } > FLASH

    .text :
    {
        . = ALIGN(4);
        *(.text)
        *(.text.*)
        *(.rodata)
        *(.rodata.*)
        . = ALIGN(4);
    } > FLASH

    .ARM.extab :
    {
        *(.ARM.extab* .gnu.linkonce.armextab.*)
    } > FLASH

    .ARM.exidx :
    {
        __exidx_start = .;
        *(.ARM.exidx* .gnu.linkonce.armexidx.*)
        __exidx_end = .;
    } > FLASH

    .preinit_array :
    {
        . = ALIGN(4);
        __preinit_array_start = .;
        KEEP(*(.preinit_array))
        __preinit_array_end = .;
    } > FLASH

    .init_array :
    {
        . = ALIGN(4);
        __init_array_start = .;
        KEEP(*(SORT_BY_INIT_PRIORITY(.init_array.*)))
        KEEP(*(.init_array))
        __init_array_end = .;
    } > FLASH

    .fini_array :
    {
        . = ALIGN(4);
        __fini_array_start = .;
        KEEP(*(SORT_BY_INIT_PRIORITY(.fini_array.*)))
        KEEP(*(.fini_array))
        __fini_array_end = .;
    } > FLASH

    .data :
    {
        . = ALIGN(4);
        _data_start = .;
        *(.data)
        *(.data.*)
        . = ALIGN(4);
        _data_end = .;
    } > RAM_STATIC AT > FLASH

    _data_load = LOADADDR(.data);

    .bss (NOLOAD) :
    {
        . = ALIGN(4);
        _bss_start = .;
        *(.bss)
        *(.bss.*)
        *(COMMON)
        . = ALIGN(4);
        _bss_end = .;
    } > RAM_STATIC

    .noinit (NOLOAD) :
    {
        . = ALIGN(4);
        *(.noinit)
        *(.noinit.*)
        . = ALIGN(4);
        _static_data_end = .;
    } > RAM_STATIC

    .stack (NOLOAD) :
    {
        . = ALIGN(8);
        . = . + LENGTH(RAM_STACK);
        . = ALIGN(8);
    } > RAM_STACK

    .ARM.attributes 0 :
    {
        *(.ARM.attributes)
    }

    /DISCARD/ :
    {
        *(.comment)
        *(.note*)
    }
}

ASSERT(_static_data_end <= ORIGIN(RAM_STATIC) + LENGTH(RAM_STATIC),
       "RAM overflow: static data exceeds its 7 KiB region")
