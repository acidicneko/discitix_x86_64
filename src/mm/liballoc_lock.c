#include <libk/spinlock.h>
#include <mm/liballoc.h>
#include <mm/pmm.h>

spinlock_t liballoc_spinlock;

int liballoc_init() {
  spinlock_init(&liballoc_spinlock);
  return 0; // Indicate success
}

int liballoc_lock() {
  spinlock_acquire(&liballoc_spinlock);
  return 0; // Indicate success
}
int liballoc_unlock() {
  spinlock_release(&liballoc_spinlock);
  return 0; // Indicate success
}

void *liballoc_alloc(size_t size) { return pmalloc(size); }

int liballoc_free(void *adr, size_t size) {
  pmm_free_pages(adr, size);
  return 0;
}
