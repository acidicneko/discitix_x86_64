#include <arch/ports.h>
#include <arch/x86_64/irq.h>
#include <drivers/pit.h>
#include <drivers/tty/tty.h>
#include <libk/utils.h>

volatile uint64_t pit_ticks = 0;
uint16_t hz = 0;

void pit_handler(register_t *regs) {
  (void)regs;
  pit_ticks++;

  if (tty_initialized) {
    if (pit_ticks % (hz / 2) == 0) {
      tty_toggle_cursor_visibility();
    }
  }
}

void set_frequency(uint16_t h) {
  hz = h;
  uint16_t divisor = 1193180 / h;
  outb(0x43, 0x36);
  outb(0x40, divisor & 0xFF);
  outb(0x40, (divisor >> 8) & 0xFF);
}

void pit_install(uint16_t hertz) {
  irq_install_handler(0, pit_handler);
  set_frequency(hertz);
  // dbgln("PIT initialised at %d Hertz\n\r", hertz);
}

void pit_wait(int ticks) {
  uint64_t eticks;
  eticks = pit_ticks + ticks;
  while (pit_ticks < eticks) {
    asm volatile("hlt");
  }
}

uint64_t get_ticks() { return pit_ticks; }
