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
#include <drivers/rtc.h>
#include <drivers/serial.h>
#include <drivers/tty/psf2.h>
#include <drivers/tty/tty.h>
#include <fs/stripFS.h>
#include <fs/procfs.h>
#include <init/stivale2.h>
#include <kernel/elf.h>
#include <kernel/sched/scheduler.h>
#include <libk/stdio.h>
#include <libk/string.h>
#include <libk/utils.h>
#include <mm/liballoc.h>
#include <mm/pmm.h>
#include <mm/vmm.h>

void enable_avx(void) {
    uint64_t cr4;
    
    // 1. Enable XSAVE and Extended States in CR4
    asm volatile("mov %%cr4, %0" : "=r"(cr4));
    cr4 |= (1ULL << 18); // Set CR4.OSXSAVE
    asm volatile("mov %0, %%cr4" :: "r"(cr4));

    // 2. Set XCR0 (Extended Control Register 0) to allow AVX + SSE
    uint32_t eax, edx;
    asm volatile("xgetbv" : "=a"(eax), "=d"(edx) : "c"(0));
    
    eax |= (1ULL << 0); // x87 state
    eax |= (1ULL << 1); // SSE state (XMM)
    eax |= (1ULL << 2); // AVX state (YMM)
    
    asm volatile("xsetbv" :: "a"(eax), "d"(edx), "c"(0));
}

void enable_sse(void) {
    uint64_t cr0;
    uint64_t cr4;

    // Read CR0
    asm volatile("mov %%cr0, %0" : "=r"(cr0));
    cr0 &= ~(1ULL << 2); // Clear CR0.EM (Emulation)
    cr0 |= (1ULL << 1);  // Set CR0.MP (Monitor Coprocessor)
    asm volatile("mov %0, %%cr0" :: "r"(cr0));

    // Read CR4
    asm volatile("mov %%cr4, %0" : "=r"(cr4));
    cr4 |= (1ULL << 9);  // Set CR4.OSFXSR (Operating System FXSAVE/FXRSTOR Support)
    cr4 |= (1ULL << 10); // Set CR4.OSXMMEXCPT (Operating System Unmasked Exception Support)
    asm volatile("mov %0, %%cr4" :: "r"(cr4));
}

void init_kernel() {
  enable_sse();
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
  rtc_init();
  IRQ_START;
  init_pmm();
  liballoc_init();
  init_vmm();
  init_initrd_stripFS();
  init_procfs();
  vfs_mkdir("/dev");
  init_serial_device();
  init_syscalls();

  init_tty();
  if (arg_exist("gruvbox")) {
    init_colors(
    0x000000,  // black
    0xaa0000,  // red
    0x00aa00,  // green
    0xaa5500,  // yellow (brown)
    0x0000aa,  // blue
    0xaa00aa,  // magenta
    0x00aaaa,  // cyan
    0xaaaaaa,  // light gray

    0x555555,  // dark gray
    0xff5555,  // bright red
    0x55ff55,  // bright green
    0xffff55,  // bright yellow
    0x5555ff,  // bright blue
    0xff55ff,  // bright magenta
    0x55ffff,  // bright cyan
    0xffffff   // white
);

  } else {
    init_colors(0x2E3440, 0x3B4252, 0x434C5E, 0x4C566A, 0xD8DEE9, 0xE5E9F0,
                0xECEFF4, 0x8FBCBB, 0x88C0D0, 0x81A1C1, 0x5E81AC, 0xBF616A,
                0xD08770, 0xEBCB8B, 0xA3BE8C, 0xB48EAD);
  }
  printf("UNIX Epoch: %ul\n\r", get_unix_epoch());
  dbgln("Kernel initialised successfully!\n\r");
  print_font_details();
}

task_t* run_elf_from_initrd(const char* filename, int argc, char *argv[]) {
    inode_t* inode = NULL;
    file_t* file = NULL;
    
    // Absolute path resolution formatting safely bounded
    char path[256];
    path[0] = '/';
    strncpy(path + 1, filename, sizeof(path) - 2);
    path[sizeof(path) - 1] = '\0'; // Guarantee null-termination
    
    if (vfs_lookup_path(path, &inode) != 0 || !inode) {
        dbgln("ELF: File not found: %s\n\r", path);
        return NULL;
    }
    
    if (vfs_open(&file, inode, 0) != 0 || !file) {
        dbgln("ELF: Failed to open file descriptor: %s\n\r", path);
        return NULL;
    }
    
    size_t file_size = inode->size;
    size_t pages_needed = (file_size + 4095) / 4096;
    
    void* buffer = pmalloc(pages_needed);
    if (!buffer) {
        dbgln("ELF: OOM allocating temporary buffer for %s (%d bytes)\n\r", path, (int)file_size);
        vfs_close(file);
        return NULL;
    }
  
    size_t bytes_read = vfs_read(file, buffer, file_size);
    vfs_close(file); // Close the file stream early now that data is in-memory
    
    if (bytes_read != file_size) {
        dbgln("ELF: Read size mismatch on %s, got %d of %d bytes\n\r", path, (int)bytes_read, (int)file_size);
        pmm_free_pages(buffer, pages_needed);
        return NULL;
    }
    
    dbgln("ELF: Loaded %s successfully to buffer. Routing to context generator...\n\r", filename);
    
    // Call our newly revamped task builder with full ABI compatibility!
    // We pass 4 pages for the kernel execution stack to avoid overflow during nested interrupts.
    task_t* task = create_elf_task_args(buffer, file_size, 4, argc, argv);
    
    // Clean up temporary copy buffer now that the loader has mapped the binary into child CR3 space
    pmm_free_pages(buffer, pages_needed);
    
    if (!task) {
        dbgln("ELF: Failed context generation structure for executable %s\n\r", filename);
        return NULL;
    }
    
    return task;
}

void kmain() {
  init_kernel();
  sysfetch();

  char* argv[] = {"/sh", NULL};
  task_t* elf_task = run_elf_from_initrd("sh", 1, argv);
  
  if (elf_task) {
      dbgln("Created ELF task id=%d from initrd\n\r", elf_task->id);
  } else {
      dbgln("No ELF program in initrd (or load failed)\n\r");
  }

  for (;;) {
    asm("sti; hlt");
  }
}
