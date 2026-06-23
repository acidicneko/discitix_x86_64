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
        
        /* Snap the heap start to a perfectly clean 4K Page Boundary */
        . = ALIGN(4096);
        
        /* Anchor the sbrk() tracking pointers here */
        _end = .;
        __end__ = .;
        
        /* * CRITICAL FIX: Add 128KB of empty space!
         * This expands the ELF memory size, tricking the kernel ELF loader
         * into actively mapping 32 extra physical pages for Newlib's malloc 
         * to use without triggering a Page Fault!
         */
        . = . + 0x20000;
    }
    
    /DISCARD/ : {
        *(.comment)
        *(.note.*)
        *(.eh_frame*)
    }
}
