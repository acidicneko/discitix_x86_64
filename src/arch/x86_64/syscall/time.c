#include <arch/x86_64/syscall.h>
#include <kernel/sched/scheduler.h>
#include <drivers/pit.h>
#include <drivers/rtc.h>
#include <libk/errno.h>
#include <stdint.h>

// Userland ABI (newlib): struct timespec and clock ids
struct k_timespec {
    int64_t tv_sec;
    int64_t tv_nsec;
};
#define K_CLOCK_REALTIME   1
#define K_CLOCK_MONOTONIC  4

static uint64_t boot_epoch = 0;   // wall-clock seconds at tick 0

int64_t sys_nanosleep(uint64_t req_ptr, uint64_t rem_ptr, uint64_t arg3,
                      uint64_t arg4, uint64_t arg5, uint64_t arg6) {
    (void)arg3; (void)arg4; (void)arg5; (void)arg6;

    const struct k_timespec *req = (const struct k_timespec *)req_ptr;
    struct k_timespec *rem = (struct k_timespec *)rem_ptr;
    if (!req || req->tv_sec < 0 || req->tv_nsec < 0 || req->tv_nsec >= 1000000000)
        return -EINVAL;

    uint64_t hz = pit_get_hz();
    // Round up so we never sleep less than asked
    uint64_t ticks = (uint64_t)req->tv_sec * hz +
                     ((uint64_t)req->tv_nsec * hz + 999999999) / 1000000000;

    task_t *t = get_current_task();
    if (ticks && t) {
        t->wake_tick = get_ticks() + ticks;
        t->sleeping = 1;
        t->state = TASK_BLOCKED;
        // Syscalls run with interrupts off; enable them while halting so
        // the timer can wake us.
        while (t->state == TASK_BLOCKED)
            asm volatile("sti; hlt");
        t->sleeping = 0;
    }

    if (rem) {
        rem->tv_sec = 0;
        rem->tv_nsec = 0;
    }
    return 0;
}

int64_t sys_clock_gettime(uint64_t clock_id, uint64_t ts_ptr, uint64_t arg3,
                          uint64_t arg4, uint64_t arg5, uint64_t arg6) {
    (void)arg3; (void)arg4; (void)arg5; (void)arg6;

    struct k_timespec *ts = (struct k_timespec *)ts_ptr;
    if (!ts) return -EINVAL;

    uint64_t hz = pit_get_hz();
    uint64_t ticks = get_ticks();
    uint64_t up_sec = ticks / hz;
    uint64_t up_nsec = (ticks % hz) * (1000000000 / hz);

    switch (clock_id) {
    case K_CLOCK_MONOTONIC:
        ts->tv_sec = (int64_t)up_sec;
        ts->tv_nsec = (int64_t)up_nsec;
        return 0;
    case K_CLOCK_REALTIME:
        // Read the RTC once and advance it with the PIT afterwards, so
        // wall-clock time has sub-second resolution and never jumps back.
        if (!boot_epoch) boot_epoch = get_unix_epoch() - up_sec;
        ts->tv_sec = (int64_t)(boot_epoch + up_sec);
        ts->tv_nsec = (int64_t)up_nsec;
        return 0;
    default:
        return -EINVAL;
    }
}
