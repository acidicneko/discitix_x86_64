ENTRY(_start)

/* Define the explicit ELF program headers (segments) */
PHDRS
{
    text_seg  PT_LOAD FLAGS(5); /* 5 = Read (4) + Execute (1) */
    rodata_seg PT_LOAD FLAGS(4); /* 4 = Read Only */
    data_seg  PT_LOAD FLAGS(6); /* 6 = Read (4) + Write (2) */
}

SECTIONS
{
    . = 0x400000;
    
    /* 1. Code Segment (RX) */
    .text : {
        *(.text._start)
        *(.text)
        *(.text.*)
    } :text_seg
    
    . = ALIGN(4096);
    
    /* 2. Read-Only Constants (R) */
    .rodata : {
        *(.rodata)
        *(.rodata.*)
    } :rodata_seg
    
    . = ALIGN(4096);
    
    /* 3. Initialized Data (RW) */
    .data : {
        *(.data)
        *(.data.*)
        *(.tdata)
        *(.tdata.*)
    } :data_seg
    
    /* 4. Uninitialized Data & Dynamic Heap Reserve (RW) */
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
    } :data_seg
    
    /DISCARD/ : {
        *(.comment)
        *(.note.*)
        *(.eh_frame*)
    }
}
