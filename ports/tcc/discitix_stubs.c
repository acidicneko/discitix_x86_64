/* Functions tcc needs that the Discitix libc doesn't provide (yet). */
#include <string.h>
#include <stdlib.h>

/* realpath — used by tcc for include path resolution */
char *realpath(const char *path, char *resolved) {
    char *out = resolved ? resolved : malloc(4096);
    if (out) strncpy(out, path, 4096);
    return out;
}

/* execvp — tcc uses this to invoke sub-processes */
int execvp(const char *file, char *const argv[]) {
    (void)file; (void)argv;
    return -1;
}
