/*
 * POSIX/BSD functions newlib doesn't provide for Discitix, built on the
 * syscall wrappers in stubs.c.
 *
 * Discitix today has one user (root), no permissions, no symlinks, no
 * device nodes and no *at() syscalls, so:
 *   - permission/ownership/timestamp setters check the file exists and
 *     succeed without doing anything
 *   - symlink/mknod/mkfifo fail with ENOSYS; readlink always says "not a
 *     symlink"
 *   - *at() calls are resolved in userspace against the path each fd was
 *     opened with (see the fd table in stubs.c)
 */
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <sys/utsname.h>
#include <dirent.h>
#include <errno.h>
#include <fcntl.h>
#include <grp.h>
#include <limits.h>
#include <pwd.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>
#include "internal.h"

extern char **environ;

/* ------------------------------------------------------------------------ */
/* stdio                                                                    */
/* ------------------------------------------------------------------------ */

ssize_t getdelim(char **lineptr, size_t *n, int delim, FILE *fp) {
    return __getdelim(lineptr, n, delim, fp);
}

ssize_t getline(char **lineptr, size_t *n, FILE *fp) {
    return __getline(lineptr, n, fp);
}

/* ------------------------------------------------------------------------ */
/* *at() family                                                             */
/* ------------------------------------------------------------------------ */

/* Resolve (dirfd, name) to a single path in out. */
static int at_path(int dirfd, const char *name, char *out, size_t size) {
    if (!name) { errno = EFAULT; return -1; }

    int n;
    if (name[0] == '/' || dirfd == AT_FDCWD) {
        n = snprintf(out, size, "%s", name);
    } else {
        const char *base = __discitix_fd_path(dirfd);
        if (!base) { errno = EBADF; return -1; }
        size_t len = strlen(base);
        n = snprintf(out, size, "%s%s%s", base,
                     (len && base[len - 1] == '/') ? "" : "/", name);
    }
    if (n < 0 || (size_t)n >= size) { errno = ENAMETOOLONG; return -1; }
    return 0;
}

#define AT_PATH(dirfd, name, buf) \
    char buf[PATH_MAX]; \
    if (at_path((dirfd), (name), buf, sizeof buf) < 0) return -1

int openat(int dirfd, const char *name, int flags, ...) {
    mode_t mode = 0;
    if (flags & O_CREAT) {
        va_list ap;
        va_start(ap, flags);
        mode = (mode_t)va_arg(ap, int);
        va_end(ap);
    }
    AT_PATH(dirfd, name, path);
    return open(path, flags, mode);
}

int fstatat(int dirfd, const char *name, struct stat *st, int flags) {
    (void)flags;  /* no symlinks: AT_SYMLINK_NOFOLLOW changes nothing */
    AT_PATH(dirfd, name, path);
    return stat(path, st);
}

int unlinkat(int dirfd, const char *name, int flags) {
    AT_PATH(dirfd, name, path);
    return (flags & AT_REMOVEDIR) ? rmdir(path) : unlink(path);
}

int faccessat(int dirfd, const char *name, int mode, int flags) {
    (void)flags;
    AT_PATH(dirfd, name, path);
    return access(path, mode);
}

int fchmodat(int dirfd, const char *name, mode_t mode, int flags) {
    (void)flags;
    AT_PATH(dirfd, name, path);
    return chmod(path, mode);
}

int fchownat(int dirfd, const char *name, uid_t owner, gid_t group, int flags) {
    (void)flags;
    AT_PATH(dirfd, name, path);
    return chown(path, owner, group);
}

int utimensat(int dirfd, const char *name, const struct timespec times[2], int flags) {
    (void)times; (void)flags;
    AT_PATH(dirfd, name, path);
    struct stat st;
    return stat(path, &st);  /* timestamps can't be set yet */
}

int futimens(int fd, const struct timespec times[2]) {
    (void)times;
    struct stat st;
    return fstat(fd, &st);
}

int mkdirat(int dirfd, const char *name, mode_t mode) {
    AT_PATH(dirfd, name, path);
    return mkdir(path, mode);
}

int symlinkat(const char *target, int dirfd, const char *name) {
    (void)target; (void)dirfd; (void)name;
    errno = ENOSYS;
    return -1;
}

int linkat(int olddirfd, const char *oldname, int newdirfd, const char *newname, int flags) {
    (void)flags;
    AT_PATH(olddirfd, oldname, oldpath);
    AT_PATH(newdirfd, newname, newpath);
    return link(oldpath, newpath);
}

/* ------------------------------------------------------------------------ */
/* Files and directories                                                    */
/* ------------------------------------------------------------------------ */

int creat(const char *path, mode_t mode) {
    return open(path, O_WRONLY | O_CREAT | O_TRUNC, mode);
}

/* Everything is accessible to root; just check the file exists. */
int access(const char *path, int mode) {
    (void)mode;
    struct stat st;
    return stat(path, &st);
}

int chmod(const char *path, mode_t mode) {
    (void)mode;
    struct stat st;
    return stat(path, &st);
}

int fchmod(int fd, mode_t mode) {
    (void)mode;
    struct stat st;
    return fstat(fd, &st);
}

int chown(const char *path, uid_t owner, gid_t group) {
    (void)owner; (void)group;
    struct stat st;
    return stat(path, &st);
}

int lchown(const char *path, uid_t owner, gid_t group) {
    return chown(path, owner, group);
}

int fchown(int fd, uid_t owner, gid_t group) {
    (void)owner; (void)group;
    struct stat st;
    return fstat(fd, &st);
}

/* The kernel's unlink removes directories without checking, so enforce
   rmdir semantics (must be a directory, must be empty) here. */
int rmdir(const char *path) {
    struct stat st;
    if (stat(path, &st) < 0) return -1;
    if (!S_ISDIR(st.st_mode)) { errno = ENOTDIR; return -1; }

    DIR *d = opendir(path);
    if (!d) return -1;
    struct dirent *e;
    while ((e = readdir(d))) {
        if (strcmp(e->d_name, ".") && strcmp(e->d_name, "..")) {
            closedir(d);
            errno = ENOTEMPTY;
            return -1;
        }
    }
    closedir(d);
    return unlink(path);
}

ssize_t readlink(const char *path, char *buf, size_t size) {
    (void)buf; (void)size;
    struct stat st;
    if (stat(path, &st) < 0) return -1;
    errno = EINVAL;  /* exists, but isn't a symlink */
    return -1;
}

int symlink(const char *target, const char *linkpath) {
    (void)target; (void)linkpath;
    errno = ENOSYS;
    return -1;
}

int mknod(const char *path, mode_t mode, dev_t dev) {
    (void)path; (void)mode; (void)dev;
    errno = ENOSYS;
    return -1;
}

int mkfifo(const char *path, mode_t mode) {
    (void)path; (void)mode;
    errno = ENOSYS;
    return -1;
}

void sync(void) {}

int fsync(int fd) {
    (void)fd;
    return 0;
}

int flock(int fd, int operation) {
    (void)fd; (void)operation;
    return 0;  /* single user, nothing to lock against */
}

int chroot(const char *path) {
    (void)path;
    errno = ENOSYS;
    return -1;
}

/* Absolute path with "." and ".." resolved (there are no symlinks). */
char *realpath(const char *path, char *resolved) {
    if (!path) { errno = EINVAL; return NULL; }

    char tmp[PATH_MAX];
    if (path[0] == '/') {
        if (strlen(path) >= sizeof tmp) { errno = ENAMETOOLONG; return NULL; }
        strcpy(tmp, path);
    } else {
        if (!getcwd(tmp, sizeof tmp)) return NULL;
        if (strlen(tmp) + 1 + strlen(path) >= sizeof tmp) { errno = ENAMETOOLONG; return NULL; }
        strcat(tmp, "/");
        strcat(tmp, path);
    }

    char out[PATH_MAX];
    size_t len = 0;
    out[0] = '\0';
    for (char *save, *tok = strtok_r(tmp, "/", &save); tok; tok = strtok_r(NULL, "/", &save)) {
        if (!strcmp(tok, ".")) continue;
        if (!strcmp(tok, "..")) {
            while (len > 0 && out[len - 1] != '/') len--;
            if (len > 0) len--;
            out[len] = '\0';
            continue;
        }
        size_t tl = strlen(tok);
        if (len + 1 + tl >= sizeof out) { errno = ENAMETOOLONG; return NULL; }
        out[len++] = '/';
        memcpy(out + len, tok, tl);
        len += tl;
        out[len] = '\0';
    }
    if (len == 0) strcpy(out, "/");

    struct stat st;
    if (stat(out, &st) < 0) return NULL;

    char *dst = resolved ? resolved : malloc(strlen(out) + 1);
    if (!dst) { errno = ENOMEM; return NULL; }
    strcpy(dst, out);
    return dst;
}

/* ------------------------------------------------------------------------ */
/* libgen                                                                   */
/* ------------------------------------------------------------------------ */

char *basename(char *path) {
    static char dot[] = ".";
    if (!path || !*path) return dot;
    size_t len = strlen(path);
    while (len > 1 && path[len - 1] == '/') path[--len] = '\0';
    char *slash = strrchr(path, '/');
    return (slash && slash[1]) ? slash + 1 : path;
}

char *dirname(char *path) {
    static char dot[] = ".";
    if (!path || !*path) return dot;
    size_t len = strlen(path);
    while (len > 1 && path[len - 1] == '/') len--;   /* trailing slashes */
    while (len > 0 && path[len - 1] != '/') len--;   /* last component */
    if (len == 0) return dot;
    while (len > 1 && path[len - 1] == '/') len--;   /* slashes before it */
    path[len] = '\0';
    return path;
}

/* ------------------------------------------------------------------------ */
/* Processes                                                                */
/* ------------------------------------------------------------------------ */

#define MAX_EXECL_ARGS 64

int execv(const char *path, char *const argv[]) {
    return execve(path, argv, environ);
}

int execvp(const char *file, char *const argv[]) {
    if (!file || !*file) { errno = ENOENT; return -1; }
    if (strchr(file, '/')) return execv(file, argv);

    const char *p = getenv("PATH");
    if (!p || !*p) p = "/bin";

    char path[PATH_MAX];
    while (*p) {
        const char *end = strchr(p, ':');
        size_t dlen = end ? (size_t)(end - p) : strlen(p);
        if (dlen == 0) {
            snprintf(path, sizeof path, "%s", file);   /* empty entry = cwd */
        } else {
            snprintf(path, sizeof path, "%.*s/%s", (int)dlen, p, file);
        }
        struct stat st;
        if (stat(path, &st) == 0 && S_ISREG(st.st_mode))
            return execv(path, argv);  /* only returns on failure */
        if (!end) break;
        p = end + 1;
    }
    errno = ENOENT;
    return -1;
}

static int collect_args(const char *arg0, va_list ap, char **argv) {
    int n = 0;
    argv[n++] = (char *)arg0;
    while (arg0) {
        if (n == MAX_EXECL_ARGS) { errno = E2BIG; return -1; }
        argv[n] = va_arg(ap, char *);
        if (!argv[n++]) break;
    }
    return 0;
}

int execl(const char *path, const char *arg0, ...) {
    char *argv[MAX_EXECL_ARGS + 1];
    va_list ap;
    va_start(ap, arg0);
    int r = collect_args(arg0, ap, argv);
    va_end(ap);
    return r < 0 ? -1 : execv(path, argv);
}

int execlp(const char *file, const char *arg0, ...) {
    char *argv[MAX_EXECL_ARGS + 1];
    va_list ap;
    va_start(ap, arg0);
    int r = collect_args(arg0, ap, argv);
    va_end(ap);
    return r < 0 ? -1 : execvp(file, argv);
}

/* popen/pclose via fork + pipe + /bin/dash -c */
static pid_t popen_pids[FOPEN_MAX + 3];

FILE *popen(const char *command, const char *mode) {
    if (!mode || (mode[0] != 'r' && mode[0] != 'w') || mode[1]) {
        errno = EINVAL;
        return NULL;
    }
    int reading = mode[0] == 'r';
    int fds[2];
    if (pipe(fds) < 0) return NULL;

    pid_t pid = fork();
    if (pid < 0) {
        close(fds[0]);
        close(fds[1]);
        return NULL;
    }
    if (pid == 0) {
        /* child: our end becomes its stdout (r) or stdin (w) */
        dup2(reading ? fds[1] : fds[0], reading ? 1 : 0);
        close(fds[0]);
        close(fds[1]);
        execl("/bin/dash", "sh", "-c", command, (char *)NULL);
        _exit(127);
    }

    int mine = reading ? fds[0] : fds[1];
    close(reading ? fds[1] : fds[0]);
    FILE *fp = fdopen(mine, mode);
    if (!fp) {
        close(mine);
        waitpid(pid, NULL, 0);
        return NULL;
    }
    if (mine >= 0 && (size_t)mine < sizeof popen_pids / sizeof popen_pids[0])
        popen_pids[mine] = pid;
    return fp;
}

int pclose(FILE *fp) {
    int fd = fileno(fp);
    if (fd < 0 || (size_t)fd >= sizeof popen_pids / sizeof popen_pids[0] || !popen_pids[fd]) {
        errno = ECHILD;
        return -1;
    }
    pid_t pid = popen_pids[fd];
    popen_pids[fd] = 0;
    fclose(fp);

    int status;
    if (waitpid(pid, &status, 0) < 0) return -1;
    return status;
}

/* newlib's system() is a stub returning ENOSYS on this target (its
   system.o is removed from libc.a by the Makefile). */
int system(const char *command) {
    if (!command) return 1;  /* a shell is available */

    pid_t pid = fork();
    if (pid < 0) return -1;
    if (pid == 0) {
        execl("/bin/dash", "sh", "-c", command, (char *)NULL);
        _exit(127);
    }
    int status;
    if (waitpid(pid, &status, 0) < 0) return -1;
    return status;
}

pid_t setsid(void) {
    return getpid();  /* no sessions; every process acts as a leader */
}

/* ------------------------------------------------------------------------ */
/* Time                                                                     */
/* ------------------------------------------------------------------------ */

unsigned int sleep(unsigned int seconds) {
    struct timespec ts = { .tv_sec = seconds, .tv_nsec = 0 };
    nanosleep(&ts, NULL);
    return 0;
}

int usleep(useconds_t usec) {
    struct timespec ts = { .tv_sec = usec / 1000000, .tv_nsec = (usec % 1000000) * 1000 };
    return nanosleep(&ts, NULL);
}

int clock_settime(clockid_t clock_id, const struct timespec *tp) {
    (void)clock_id; (void)tp;
    errno = EPERM;
    return -1;
}

/* ------------------------------------------------------------------------ */
/* Users, groups, system identity                                           */
/* ------------------------------------------------------------------------ */

static struct passwd root_pw = {
    .pw_name = "root", .pw_passwd = "", .pw_uid = 0, .pw_gid = 0,
    .pw_gecos = "root", .pw_dir = "/", .pw_shell = "/bin/dash",
};

static char *root_members[] = { "root", NULL };
static struct group root_gr = {
    .gr_name = "root", .gr_passwd = "", .gr_gid = 0, .gr_mem = root_members,
};

struct passwd *getpwuid(uid_t uid) { return uid == 0 ? &root_pw : NULL; }
struct passwd *getpwnam(const char *name) { return name && !strcmp(name, "root") ? &root_pw : NULL; }
struct group *getgrgid(gid_t gid) { return gid == 0 ? &root_gr : NULL; }
struct group *getgrnam(const char *name) { return name && !strcmp(name, "root") ? &root_gr : NULL; }

char *getlogin(void) { return "root"; }

int gethostname(char *name, size_t len) {
    const char host[] = "discitix";
    if (len < sizeof host) { errno = ENAMETOOLONG; return -1; }
    memcpy(name, host, sizeof host);
    return 0;
}

int sethostname(const char *name, size_t len) {
    (void)name; (void)len;
    errno = EPERM;
    return -1;
}

int uname(struct utsname *u) {
    if (!u) { errno = EFAULT; return -1; }
    strcpy(u->sysname, "Discitix");
    strcpy(u->nodename, "discitix");
    strcpy(u->release, "0.1");
    strcpy(u->version, "0.1");
    strcpy(u->machine, "x86_64");
    return 0;
}

char *ttyname(int fd) {
    static char buf[PATH_MAX];
    if (!isatty(fd)) { errno = ENOTTY; return NULL; }
    const char *p = __discitix_fd_path(fd);
    snprintf(buf, sizeof buf, "%s", (p && !strncmp(p, "/dev/", 5)) ? p : "/dev/tty0");
    return buf;
}

long sysconf(int name) {
    switch (name) {
    /* Bytes of argv+envp strings. The kernel copies them onto an 8 KiB
       stack and caps each list at 64 entries. */
    case _SC_ARG_MAX:          return 4096;
    case _SC_OPEN_MAX:         return 16;    /* kernel MAX_FDS */
    case _SC_PAGESIZE:         return 4096;
    case _SC_CLK_TCK:          return 100;
    case _SC_CHILD_MAX:        return 64;
    case _SC_LINE_MAX:         return 2048;
    case _SC_HOST_NAME_MAX:    return 255;
    case _SC_LOGIN_NAME_MAX:   return 32;
    case _SC_NPROCESSORS_CONF:
    case _SC_NPROCESSORS_ONLN: return 1;
    default:
        errno = EINVAL;
        return -1;
    }
}
