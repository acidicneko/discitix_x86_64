ENTRY(_start)

KERNEL_VIRTUAL_BASE = 0xFFFF800000000000;
KERNEL_PHYSICAL_BASE = 0x100000;

SECTIONS
{
    . = KERNEL_PHYSICAL_BASE;
    
    .stivale2hdr ALIGN(4K) : AT(ADDR(.stivale2hdr))
    {
        KEEP(*(.stivale2hdr))
    }

    .text ALIGN(4K) : AT(ADDR(.text))
    {
        *(.text .text.*)
    }

    .rodata ALIGN(4K) : AT(ADDR(.rodata))
    {
        *(.rodata .rodata.*)
    }

    .data ALIGN(4K) : AT(ADDR(.data))
    {
        *(.data .data.*)
    }

    .bss ALIGN(4K) : AT(ADDR(.bss))
    {
        *(COMMON)
        *(.bss .bss.*)
    }

    kernel_start = KERNEL_PHYSICAL_BASE;
    kernel_end = .;
    kernel_size = kernel_end - kernel_start;

    /DISCARD/ :
    {
        *(.eh_frame)
        *(.note .note.*)
    }
}

