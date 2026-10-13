#include <kernel/ipc/pipe.h>
#include <kernel/vfs/vfs.h>
#include <kernel/sched/scheduler.h>
#include <libk/string.h>
#include <mm/liballoc.h>
#include <libk/utils.h>

#define PIPE_READ_END  0
#define PIPE_WRITE_END 1

file_operations_t pipe_read_fops  = { .read = pipe_read,  .write = NULL,       .open = NULL, .close = pipe_close };
file_operations_t pipe_write_fops = { .read = NULL,       .write = pipe_write, .open = NULL, .close = pipe_close };

static inline uint64_t irqsave(void) {
    uint64_t f; asm volatile("pushfq; pop %0; cli" : "=r"(f) :: "memory"); return f;
}
static inline void irqrestore(uint64_t f) {
    asm volatile("push %0; popfq" :: "r"(f) : "memory", "cc");
}


static file_t *pipe_make_end(pipe_t *p, int end) {
    inode_t *inode = (inode_t *)kmalloc(sizeof(inode_t));
    memset(inode, 0, sizeof(inode_t));
    inode->type = FT_FIFO;
    inode->f_ops = (end == PIPE_READ_END) ? &pipe_read_fops : &pipe_write_fops;
    inode->private = p;              // both ends share the same pipe_t
    inode->is_pipe_write_end = end;  // add this bool/int field to inode_t

    file_t *file = (file_t *)kmalloc(sizeof(file_t));
    memset(file, 0, sizeof(file_t));
    file->inode = inode;
    file->offset = 0;
    file->flags = 0;

    return file;
}

int64_t sys_pipe(uint64_t fds_ptr, uint64_t arg2, uint64_t arg3,
             uint64_t arg4, uint64_t arg5, uint64_t arg6) {
    (void)arg2; (void)arg3; (void)arg4; (void)arg5; (void)arg6;

    int *fds = (int*)fds_ptr;
    if (!fds) return -1;

    task_t *current = get_current_task();
    if (!current) return -1;

    // find two free fd slots
    int fd_r = -1, fd_w = -1;
    for (int i = 0; i < MAX_FDS; i++) {
        if (current->fd_table[i] == NULL) {
            if (fd_r == -1) fd_r = i;
            else if (fd_w == -1) { fd_w = i; break; }
        }
    }
    if (fd_r == -1 || fd_w == -1) return -1; // out of fds

    pipe_t *p = (pipe_t *)kmalloc(sizeof(pipe_t));
    if (!p) return -1;
    memset(p, 0, sizeof(pipe_t));
    spinlock_init(&p->lock);
    p->readers = 1;
    p->writers = 1;

    file_t *rfile = pipe_make_end(p, PIPE_READ_END);
    file_t *wfile = pipe_make_end(p, PIPE_WRITE_END);
    if (!rfile || !wfile) {
        rfile ? kfree(rfile) : kfree(wfile);
        return -1;
    }

    current->fd_table[fd_r] = rfile;
    current->fd_table[fd_w] = wfile;

    fds[0] = fd_r;
    fds[1] = fd_w;

    log("PIPE", INFO, "created pipe: read fd=%d write fd=%d\n\r", fd_r, fd_w);
    return 0;
}


long pipe_read(file_t *file, void *buf, size_t len, uint64_t offset) {
    (void)offset;
    pipe_t *p = (pipe_t *)file->inode->private;
    uint8_t *dest = (uint8_t*)buf;
    size_t total = 0;

    while (total == 0) {
        uint64_t flags = irqsave();
        spinlock_acquire(&p->lock);

        if (p->count > 0) {
            size_t to_copy = (len < p->count) ? len : p->count;
            for (size_t i = 0; i < to_copy; i++) {
                dest[i] = p->buf[p->read_pos];
                p->read_pos = (p->read_pos + 1) % PIPE_BUF_SIZE;
            }
            p->count -= to_copy;
            total = to_copy;

            task_t *w = p->blocked_writer;
            if (w) {
                p->blocked_writer = NULL;
                w->state = TASK_RUNNABLE;
            }

            spinlock_release(&p->lock);
            irqrestore(flags);
            break;
        }

        if (p->writers == 0) {
            spinlock_release(&p->lock);
            irqrestore(flags);
            return 0; // EOF
        }

        task_t *current = get_current_task();
        p->blocked_reader = current;
        current->state = TASK_BLOCKED;
        current->wake_tick = 0xFFFFFFFFFFFFFFFF;

        spinlock_release(&p->lock);
        irqrestore(flags);

        while (current->state == TASK_BLOCKED) {
            asm volatile("sti; hlt");
        }
    }

    return (long)total;
}

long pipe_write(file_t *file, const void *buf, size_t len, uint64_t offset) {
    (void)offset;
    pipe_t *p = (pipe_t *)file->inode->private;
    const uint8_t *src = (const uint8_t*)buf;
    size_t written = 0;

    while (written < len) {
        uint64_t flags = irqsave();
        spinlock_acquire(&p->lock);

        if (p->readers == 0) {
            spinlock_release(&p->lock);
            irqrestore(flags);
            return written > 0 ? (long)written : -1;
        }

        size_t space = PIPE_BUF_SIZE - p->count;
        if (space > 0) {
            size_t to_copy = len - written;
            if (to_copy > space) to_copy = space;
            for (size_t i = 0; i < to_copy; i++) {
                p->buf[p->write_pos] = src[written + i];
                p->write_pos = (p->write_pos + 1) % PIPE_BUF_SIZE;
            }
            p->count += to_copy;
            written += to_copy;

            task_t *r = p->blocked_reader;
            if (r) {
                p->blocked_reader = NULL;
                r->state = TASK_RUNNABLE;
            }

            spinlock_release(&p->lock);
            irqrestore(flags);
            continue;
        }

        task_t *current = get_current_task();
        p->blocked_writer = current;
        current->state = TASK_BLOCKED;
        current->wake_tick = 0xFFFFFFFFFFFFFFFF;

        spinlock_release(&p->lock);
        irqrestore(flags);

        while (current->state == TASK_BLOCKED) {
            asm volatile("sti; hlt");
        }
    }

    return (long)written;
}

int pipe_close(inode_t* inode) {
    pipe_t *p = (pipe_t *)inode->private;
    uint64_t flags = irqsave();
    spinlock_acquire(&p->lock);

    if (inode->is_pipe_write_end) {
        if (p->writers > 0) p->writers--;
    } else {
        if (p->readers > 0) p->readers--;
    }

    int readers = p->readers, writers = p->writers;

    if (writers == 0 && p->blocked_reader) {
        task_t *r = p->blocked_reader;
        p->blocked_reader = NULL;
        r->state = TASK_RUNNABLE;
    }

    if (readers == 0 && p->blocked_writer) {
        task_t *w = p->blocked_writer;
        p->blocked_writer = NULL;
        w->state = TASK_RUNNABLE;
    }

    spinlock_release(&p->lock);
    irqrestore(flags);

    if (readers == 0 && writers == 0) {
        kfree(p);
    }

    kfree(inode);
    return 0;
}
