#ifndef DISCITIX_LIBC_INTERNAL_H
#define DISCITIX_LIBC_INTERNAL_H

/* Shared between stubs.c and compat.c; not installed. */

/* Absolute path an fd was opened with, or NULL if unknown. */
const char *__discitix_fd_path(int fd);

#endif
