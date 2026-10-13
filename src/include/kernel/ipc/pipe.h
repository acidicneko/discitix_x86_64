// kernel/ipc/pipe.h

#ifndef __PIPE_H__
#define __PIPE_H__

#include <stdint.h>
#include <stddef.h>
#include <kernel/vfs/vfs.h>
#include <libk/spinlock.h>
#include <kernel/sched/scheduler.h>

#define PIPE_BUF_SIZE 4096
typedef struct pipe {
    uint8_t  buf[PIPE_BUF_SIZE];
    size_t   read_pos;
    size_t   write_pos;
    size_t   count;
    int      readers;
    int      writers;
    spinlock_t lock;
} pipe_t;


int64_t sys_pipe(uint64_t fds_ptr, uint64_t arg2, uint64_t arg3,
             uint64_t arg4, uint64_t arg5, uint64_t arg6);


long pipe_read(file_t *file, void *buf, size_t len, uint64_t offset);
long pipe_write(file_t *file, const void *buf, size_t len, uint64_t offset);
int  pipe_close(inode_t* inode);

#endif // __PIPE_H__
