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

#include "kernel/vfs/vfs.h"
#include <arch/x86_64/gdt.h>
#include <arch/x86_64/idt.h>
#include <arch/x86_64/irq.h>
#include <arch/x86_64/isr.h>
#include <arch/x86_64/syscall.h>
#include <drivers/keyboard.h>
#include <drivers/pit.h>
#include <drivers/serial.h>
#include <drivers/tty/psf2.h>
#include <drivers/tty/tty.h>
#include <fs/stripFS.h>
#include <init/stivale2.h>
#include <kernel/elf.h>
#include <kernel/sched/scheduler.h>
#include <libk/shell.h>
#include <libk/stdio.h>
#include <libk/string.h>
#include <libk/utils.h>
#include <mm/liballoc.h>
#include <mm/pmm.h>
#include <mm/vmm.h>

void init_kernel() {
  initial_psf_setup();
  init_arg_parser();
  if (!arg_exist("noserial")) {
    serial_init(COM1_PORT);
  }
  init_gdt();
  init_idt();
  init_isr();
  init_irq();
  keyboard_install();
  init_scheduler();
  pit_install(100);
  IRQ_START;
  init_pmm();
  liballoc_init();
  init_vmm();
  init_initrd_stripFS();
  vfs_mkdir("/dev");
  init_serial_device();
  init_syscalls();

  init_tty();
  if (arg_exist("gruvbox")) {
    init_colors(0x282828, 0xcc241d, 0x98971a, 0xd79921, 0x458588, 0xb16286,
                0x689d6a, 0xa89984, 0x928374, 0xfb4934, 0xb8bb26, 0xfabd2f,
                0x83a598, 0xd8369b, 0x8ec07c, 0xebdbb2);
  } else {
    init_colors(0x2E3440, 0x3B4252, 0x434C5E, 0x4C566A, 0xD8DEE9, 0xE5E9F0,
                0xECEFF4, 0x8FBCBB, 0x88C0D0, 0x81A1C1, 0x5E81AC, 0xBF616A,
                0xD08770, 0xEBCB8B, 0xA3BE8C, 0xB48EAD);
  }
  dbgln("Kernel initialised successfully!\n\r");
  print_font_details();
}

task_t* run_elf_from_initrd(const char* filename) {
    inode_t* inode = NULL;
    file_t* file = NULL;
    
    char path[256];
    path[0] = '/';
    strncpy(path + 1, filename, sizeof(path) - 2);
    
    if (vfs_lookup_path(path, &inode) != 0 || !inode) {
        dbgln("ELF: File not found: %s\n\r", path);
        return NULL;
    }
    
    if (vfs_open(&file, inode, 0) != 0 || !file) {
        dbgln("ELF: Failed to open: %s\n\r", path);
        return NULL;
    }
    
    size_t file_size = inode->size;
    void* buffer = pmalloc((file_size + 4095) / 4096);
    if (!buffer) {
        dbgln("ELF: Failed to allocate buffer for %d bytes\n\r", (int)file_size);
        vfs_close(file);
        return NULL;
    }
  
    size_t bytes_read = vfs_read(file, buffer, file_size);
    vfs_close(file);
    
    if (bytes_read != file_size) {
        dbgln("ELF: Read failed, got %d of %d bytes\n\r", (int)bytes_read, (int)file_size);
        pmm_free_pages(buffer, (file_size + 4095) / 4096);
        return NULL;
    }
    
    dbgln("ELF: Loaded %s (%d bytes)\n\r", filename, (int)file_size);
    
    task_t* task = create_elf_task(buffer, file_size, 2);
    
    pmm_free_pages(buffer, (file_size + 4095) / 4096);
    
    return task;
}

void kmain() {
  init_kernel();
  sysfetch();

  task_t* elf_task = run_elf_from_initrd("sh");
  if (elf_task) {
      dbgln("Created ELF task id=%d from initrd\n\r", elf_task->id);
  } else {
      dbgln("No ELF program in initrd (or load failed)\n\r");
  }

  for (;;) {
    asm("sti; hlt");
  }
}
