#ifndef __DEVFS__
#define __DEVFS__

#include <kernel/vfs/vfs.h>
#include <stdint.h>

int devfs_init(void);
int devfs_register_device(const char *name, file_operations_t *fops, uint8_t type);

#endif // __DEVFS__
