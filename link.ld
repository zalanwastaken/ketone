ENTRY(kernel_main)

SECTIONS {
    . = 0x120000;

    .text : {
        *(.start)
        *(.text*)
    }

    .rodata ALIGN(2K) : {
        *(.rodata*)
    }

    .data ALIGN(2K) : {
        *(.data*)
    }

    .bss ALIGN(2K) (NOLOAD) : {
        *(COMMON)
        *(.bss*)
    }

    kernel_end = .;
}
