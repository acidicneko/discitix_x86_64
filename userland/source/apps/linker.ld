ENTRY(_start)

SECTIONS
{
    . = 0x400000;
    
    /* 1. Code Segment */
    .text : {
        *(.text._start)
        *(.text)
        *(.text.*)
    }
    
    /* 2. Read-Only Constants */
    .rodata : {
        *(.rodata)
        *(.rodata.*)
    }
    
    /* 3. Initialized Data */
    .data : {
        *(.data)
        *(.data.*)
        *(.tdata)
        *(.tdata.*)
    }
    
    /* 4. Uninitialized Data & Dynamic Heap Reserve */
    .bss : {
        *(.bss)
        *(.bss.*)
        *(.tbss)
        *(.tbss.*)
        *(COMMON)
        
        . = ALIGN(4096);
        
        /* Anchor the sbrk() tracking pointers here */
        _end = .;
        __end__ = .;
        
        . = . + 0x20000;
    }
    
    /DISCARD/ : {
        *(.comment)
        *(.note.*)
        *(.eh_frame*)
    }
}
