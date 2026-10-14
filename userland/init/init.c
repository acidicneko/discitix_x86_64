/*
 * /sbin/init: runs /etc/init/init.lua with a "sys" module of process
 * primitives. If the script can't be loaded or dies, falls back to keeping
 * a shell running on the console so the system stays usable.
 */
#include <lua.h>
#include <lauxlib.h>
#include <lualib.h>

#include <dirent.h>
#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>
#include <sys/stat.h>
#include <sys/wait.h>

#define INIT_SCRIPT "/etc/init/init.lua"
#define FALLBACK_SHELL "/bin/dash"
#define MAX_ARGS 64

extern char **environ;

static int push_error(lua_State *L) {
    int err = errno;
    lua_pushnil(L);
    lua_pushstring(L, strerror(err));
    lua_pushinteger(L, err);
    return 3;
}

// Fills out[] with the strings of the array at idx; NULL-terminated.
static int table_to_strv(lua_State *L, int idx, char **out) {
    luaL_checktype(L, idx, LUA_TTABLE);
    int n = (int)luaL_len(L, idx);
    if (n > MAX_ARGS) return luaL_error(L, "too many arguments (max %d)", MAX_ARGS);
    for (int i = 1; i <= n; i++) {
        lua_rawgeti(L, idx, i);
        out[i - 1] = (char *)luaL_checkstring(L, -1);
        lua_pop(L, 1);  // the table keeps the string alive
    }
    out[n] = NULL;
    return n;
}

static int l_fork(lua_State *L) {
    fflush(NULL);
    pid_t pid = fork();
    if (pid < 0) return push_error(L);
    lua_pushinteger(L, pid);
    return 1;
}

// exec(path, argv_table [, env_table]); only returns on failure
static int l_exec(lua_State *L) {
    const char *path = luaL_checkstring(L, 1);
    char *argv[MAX_ARGS + 1], *envp[MAX_ARGS + 1];
    table_to_strv(L, 2, argv);
    char **env = environ;
    if (!lua_isnoneornil(L, 3)) {
        table_to_strv(L, 3, envp);
        env = envp;
    }
    fflush(NULL);
    execve(path, argv, env);
    return push_error(L);
}

// wait([nohang]) -> pid, "exited"|"signaled", code   |  0 if nohang and none
static int l_wait(lua_State *L) {
    int status;
    pid_t pid = waitpid(-1, &status, lua_toboolean(L, 1) ? WNOHANG : 0);
    if (pid < 0) return push_error(L);
    lua_pushinteger(L, pid);
    if (pid == 0) return 1;
    if (WIFSIGNALED(status)) {
        lua_pushstring(L, "signaled");
        lua_pushinteger(L, WTERMSIG(status));
    } else {
        lua_pushstring(L, "exited");
        lua_pushinteger(L, WEXITSTATUS(status));
    }
    return 3;
}

static int l_open(lua_State *L) {
    const char *path = luaL_checkstring(L, 1);
    int flags = (int)luaL_checkinteger(L, 2);
    int mode = (int)luaL_optinteger(L, 3, 0644);
    int fd = open(path, flags, mode);
    if (fd < 0) return push_error(L);
    lua_pushinteger(L, fd);
    return 1;
}

static int l_dup2(lua_State *L) {
    if (dup2((int)luaL_checkinteger(L, 1), (int)luaL_checkinteger(L, 2)) < 0)
        return push_error(L);
    lua_pushboolean(L, 1);
    return 1;
}

static int l_close(lua_State *L) {
    if (close((int)luaL_checkinteger(L, 1)) < 0) return push_error(L);
    lua_pushboolean(L, 1);
    return 1;
}

// mkdir(path): succeeds if it already exists as a directory
static int l_mkdir(lua_State *L) {
    const char *path = luaL_checkstring(L, 1);
    struct stat st;
    if (stat(path, &st) == 0 && S_ISDIR(st.st_mode)) {
        lua_pushboolean(L, 1);
        return 1;
    }
    if (mkdir(path, 0755) < 0) return push_error(L);
    lua_pushboolean(L, 1);
    return 1;
}

static int l_exists(lua_State *L) {
    struct stat st;
    lua_pushboolean(L, stat(luaL_checkstring(L, 1), &st) == 0);
    return 1;
}

static int l_sleep(lua_State *L) {
    lua_Integer ms = luaL_checkinteger(L, 1);
    struct timespec ts = { .tv_sec = ms / 1000, .tv_nsec = (ms % 1000) * 1000000 };
    nanosleep(&ts, NULL);
    return 0;
}

// Milliseconds since boot
static int l_now(lua_State *L) {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    lua_pushinteger(L, (lua_Integer)ts.tv_sec * 1000 + ts.tv_nsec / 1000000);
    return 1;
}

static int l_listdir(lua_State *L) {
    DIR *d = opendir(luaL_checkstring(L, 1));
    if (!d) return push_error(L);
    lua_newtable(L);
    int i = 1;
    struct dirent *e;
    while ((e = readdir(d))) {
        if (!strcmp(e->d_name, ".") || !strcmp(e->d_name, "..")) continue;
        lua_pushstring(L, e->d_name);
        lua_rawseti(L, -2, i++);
    }
    closedir(d);
    return 1;
}

static int l_exit(lua_State *L) {
    fflush(NULL);
    _exit((int)luaL_optinteger(L, 1, 0));
    return 0;
}

static int l_getpid(lua_State *L) {
    lua_pushinteger(L, getpid());
    return 1;
}

static const luaL_Reg sys_funcs[] = {
    { "fork", l_fork },     { "exec", l_exec },   { "wait", l_wait },
    { "open", l_open },     { "dup2", l_dup2 },   { "close", l_close },
    { "mkdir", l_mkdir },   { "exists", l_exists }, { "sleep", l_sleep },
    { "now", l_now },       { "listdir", l_listdir }, { "exit", l_exit },
    { "getpid", l_getpid }, { NULL, NULL },
};

static int luaopen_sys(lua_State *L) {
    luaL_newlib(L, sys_funcs);
    static const struct { const char *name; int value; } consts[] = {
        { "O_RDONLY", O_RDONLY }, { "O_WRONLY", O_WRONLY }, { "O_RDWR", O_RDWR },
        { "O_CREAT", O_CREAT },   { "O_APPEND", O_APPEND }, { "O_TRUNC", O_TRUNC },
    };
    for (size_t i = 0; i < sizeof consts / sizeof consts[0]; i++) {
        lua_pushinteger(L, consts[i].value);
        lua_setfield(L, -2, consts[i].name);
    }
    return 1;
}

static void fallback_shell(void) {
    fprintf(stderr, "init: falling back to %s on the console\n", FALLBACK_SHELL);
    for (;;) {
        pid_t pid = fork();
        if (pid == 0) {
            execl(FALLBACK_SHELL, "dash", (char *)NULL);
            _exit(127);
        }
        if (pid > 0) {
            int status;
            while (waitpid(pid, &status, 0) < 0 && errno == EINTR) {}
        }
        struct timespec ts = { .tv_sec = 1, .tv_nsec = 0 };
        nanosleep(&ts, NULL);
    }
}

int main(void) {
    lua_State *L = luaL_newstate();
    if (!L) {
        fprintf(stderr, "init: cannot create Lua state\n");
        fallback_shell();
    }
    luaL_openlibs(L);
    luaL_requiref(L, "sys", luaopen_sys, 1);
    lua_pop(L, 1);

    if (luaL_dofile(L, INIT_SCRIPT) != LUA_OK)
        fprintf(stderr, "init: %s\n", lua_tostring(L, -1));
    else
        fprintf(stderr, "init: %s returned\n", INIT_SCRIPT);

    lua_close(L);
    fallback_shell();
    return 0;
}
