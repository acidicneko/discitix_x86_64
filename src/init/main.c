/*MIT License

Copyright (c) 2021 Ayush Yadav

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all
copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
SOFTWARE. */

#include "arch/x86_64/gdt.h"
#include "arch/x86_64/idt.h"
#include "arch/x86_64/isr.h"
#include "arch/x86_64/irq.h"
#include "init/stivale2.h"
#include "drivers/tty/tty.h"
#include "drivers/keyboard.h"
#include "mm/pmm.h"
#include "libk/stdio.h"
#include "libk/string.h"
#include "libk/utils.h"

void kmain(struct stivale2_struct* bootinfo){
    init_tty(bootinfo);
    /*Gruvbox color scheme*/
    init_colors(0x282828, 0xcc241d, 0x98971a, 0xd79921, 0x458588, 0xb16286, 0x689d6a, 0xa89984, 
            0x928374, 0xfb4934, 0xb8bb26, 0xfabd2f, 0x83a598, 0xd8369b, 0x8ec07c, 0xebdbb2);
    gdt_install();
	idt_install();
	init_isr();
	init_irq();
	keyboard_install();
	init_pmm(bootinfo);    
	printf("\nBootloader: %s\nBootloader Version: %s\n", bootinfo->bootloader_brand, bootinfo->bootloader_version);
	printf("Total system memory: %ul KB\n", get_total_memory()/1024);
	sysfetch();
	while(1){
		char c = keyboard_read();
		printf("%c", c);
	}
}