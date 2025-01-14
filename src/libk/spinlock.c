#include <libk/spinlock.h>

void spinlock_init(spinlock_t *lock) { atomic_store(&lock->lock, 0); }

void spinlock_acquire(spinlock_t *lock) {
  while (atomic_exchange(&lock->lock, 1) == 1) {
    // Busy-wait
    asm volatile("pause");
  }
}

void spinlock_release(spinlock_t *lock) { atomic_store(&lock->lock, 0); }
