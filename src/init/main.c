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

#include <arch/x86_64/gdt.h>
#include <arch/x86_64/idt.h>
#include <arch/x86_64/irq.h>
#include <arch/x86_64/isr.h>
#include <drivers/keyboard.h>
#include <drivers/pit.h>
#include <drivers/serial.h>
#include <drivers/tty/psf2.h>
#include <drivers/tty/tty.h>
#include <fs/initrd.h>
#include <init/stivale2.h>
#include <libk/shell.h>
#include <libk/stdio.h>
#include <libk/string.h>
#include <libk/utils.h>
#include <mm/pmm.h>

void init_kernel(struct stivale2_struct *bootinfo) {
  initial_psf_setup();
  init_arg_parser(bootinfo);
  if (!arg_exist("noserial")) {
    serial_init(COM1_PORT);
  }

  init_gdt();
  init_idt();
  init_isr();
  init_irq();
  keyboard_install();
  pit_install(100);
  IRQ_START;
  init_tty(bootinfo);
  init_pmm(bootinfo);
  if (arg_exist("gruvbox")) {
    init_colors(0x282828, 0xcc241d, 0x98971a, 0xd79921, 0x458588, 0xb16286,
                0x689d6a, 0xa89984, 0x928374, 0xfb4934, 0xb8bb26, 0xfabd2f,
                0x83a598, 0xd8369b, 0x8ec07c, 0xebdbb2);
  } else {
    init_colors(0x3B4252, 0xBF616A, 0xA3BE8C, 0xEBCB8B, 0x81A1C1, 0xB48EAD,
                0x88C0D0, 0xE5E9F0, 0x4C566A, 0xBF616A, 0xA3BE8C, 0xEBCB8B,
                0x81A1C1, 0xB48EAD, 0x8FBCBB, 0xECEFF4);
  }
  printf("\nBootloader: %s\nBootloader Version: %s\n",
         bootinfo->bootloader_brand, bootinfo->bootloader_version);
  init_initrd(bootinfo);
  dbgln("Kernel initialised successfully!\n\r");
  print_font_details();
}

void kmain(struct stivale2_struct *bootinfo) {
  init_kernel(bootinfo);
  sysfetch();
  init_shell();
  for (;;) {
    asm("hlt");
  }
}
