#include <drivers/tty/tty.h>
#include <drivers/tty/psf2.h>
#include <libk/errno.h>
#include <stddef.h>
#include <stdint.h>

long tty_write(file_t* file, const void* data, size_t len, uint64_t off){
  (void)off;
  if (!data) return -EFAULT;
  tty_t *tty = (tty_t *)file->inode->private;
  if (!tty) return -ENXIO;

  tty_write_chars(tty, (const char *)data, len);
  return (long)len;
}

void tty_switch(int id){
  if (id < 0 || id >= TTY_NUM) return;
  tty_t *tty = &ttys[id];
  if (tty == current_tty) return;

  uint64_t flags;
  asm volatile("pushfq; pop %0; cli" : "=r"(flags) :: "memory");
  current_tty = tty;
  tty_repaint(tty);
  asm volatile("push %0; popfq" :: "r"(flags) : "memory", "cc");
}
