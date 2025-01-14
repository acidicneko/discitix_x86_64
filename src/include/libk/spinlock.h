#ifndef SPINLOCK_H
#define SPINLOCK_H

#include <stdatomic.h>

typedef struct {
  atomic_int lock;
} spinlock_t;

void spinlock_init(spinlock_t *lock);
void spinlock_acquire(spinlock_t *lock);
void spinlock_release(spinlock_t *lock);

#endif // SPINLOCK_H
