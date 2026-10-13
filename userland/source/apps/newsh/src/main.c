/*
 * shell.c -- a small POSIX-ish shell for newlib targets.
 *
 * Design constraints this was written against:
 *   - linked against newlib
 *   - fcntl.h IS available (open/O_* flags)
 *   - termios.h / sys/ioctl.h are NOT available -> no raw-mode input,
 *     no terminal size queries, no line editing beyond what stdio itself
 *     gives you. Input is read a line at a time with fgets().
 *   - no job control: no &, no SIGCHLD-based reaping, no fg/bg, no
 *     process groups. Every pipeline runs to completion before the
 *     prompt returns.
 *
 * Assumed available from your libc/syscall layer: fork, execve,
 * wait4/waitpid, pipe, dup2, open, close, chdir, getcwd, stat, access.
 * If any of those aren't wired up yet in your syscall table, the
 * corresponding piece here (pipelines need pipe/dup2/fork; redirection
 * needs open/dup2; cd needs chdir) just won't work until they are --
 * everything else degrades gracefully.
 *
 * NOTE on exec: this shell uses execve() exclusively, not execvp().
 * Since execve() does not search $PATH itself, PATH lookup is done
 * manually (see path_resolve()) before every exec, mirroring what
 * execvp() would normally do for us. This keeps us independent of
 * whatever execvp() shim newlib/your libc provides and gives explicit
 * control over the envp passed to the child (see shell_execve()).
 *
 * Supported:
 *   - environment variables: NAME=value assignment, $NAME / ${NAME}
 *     expansion, export/unset/env builtins
 *   - quoting: 'single' (no expansion), "double" (expansion happens),
 *     backslash escapes outside single quotes
 *   - redirection: <, >, >>
 *   - pipelines: cmd1 | cmd2 | cmd3
 *   - sequencing: ; && ||  (short-circuit on exit status)
 *   - builtins: cd, pwd, echo, export, unset, set, exit, test / [ ], .
 *     (source), help
 *   - scripting: if/then/else/fi, while/do/done, for VAR in ...; do/done
 *     both interactively (shell buffers a block until it balances) and
 *     when run as `shell script.sh`
 *
 * Explicitly NOT supported (kept out on purpose -- "keep it simple"):
 *   - command substitution $(...) / `...`
 *   - here-docs, background jobs (&), signal-based job control
 *   - arrays, functions, arithmetic expansion $(( ))
 *   - quoted pipe/redirection characters (i.e. "a|b" as a literal
 *     argument will be misparsed -- don't do that yet)
 *
 * Build:
 *   <your-cross-prefix>-gcc -Wall -O2 -o shell shell.c
 *   (add whatever -specs=/-T/-L flags your newlib target normally needs)
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <errno.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <sys/stat.h>

#define MAX_LINE      1024
#define MAX_TOKENS    128
#define MAX_VARS      256
#define MAX_SEGMENTS  32
#define MAX_PIPESTAGE 16
#define MAX_BLOCKLINES 512

extern char **environ;

/* ------------------------------------------------------------------ */
/* shell-local variables (distinct from the real environment until    */
/* exported)                                                           */
/* ------------------------------------------------------------------ */

typedef struct {
    char name[64];
    char value[448];
    int  exported;
} shvar_t;

static shvar_t g_vars[MAX_VARS];
static int g_nvars = 0;
static int g_last_status = 0;
static const char *g_progname = "shell";

static shvar_t *var_find(const char *name) {
    for (int i = 0; i < g_nvars; i++)
        if (strcmp(g_vars[i].name, name) == 0)
            return &g_vars[i];
    return NULL;
}

static void var_set(const char *name, const char *value, int exported) {
    shvar_t *v = var_find(name);
    if (!v) {
        if (g_nvars >= MAX_VARS) {
            fprintf(stderr, "%s: too many variables\n", g_progname);
            return;
        }
        v = &g_vars[g_nvars++];
        strncpy(v->name, name, sizeof(v->name) - 1);
    }
    strncpy(v->value, value ? value : "", sizeof(v->value) - 1);
    v->value[sizeof(v->value) - 1] = '\0';
    if (exported) v->exported = 1;
    if (v->exported) setenv(v->name, v->value, 1);
}

static const char *var_get(const char *name) {
    shvar_t *v = var_find(name);
    if (v) return v->value;
    const char *e = getenv(name);
    return e ? e : "";
}

static void var_unset(const char *name) {
    for (int i = 0; i < g_nvars; i++) {
        if (strcmp(g_vars[i].name, name) == 0) {
            memmove(&g_vars[i], &g_vars[i + 1],
                    (g_nvars - i - 1) * sizeof(shvar_t));
            g_nvars--;
            break;
        }
    }
    unsetenv(name);
}

/* ------------------------------------------------------------------ */
/* small string helpers                                                */
/* ------------------------------------------------------------------ */

static char *lstrip(char *s) {
    while (*s && isspace((unsigned char)*s)) s++;
    return s;
}

static void rstrip(char *s) {
    size_t n = strlen(s);
    while (n > 0 && isspace((unsigned char)s[n - 1])) s[--n] = '\0';
}

static char *trim(char *s) {
    s = lstrip(s);
    rstrip(s);
    return s;
}

static char *xstrdup(const char *s) {
    char *d = strdup(s);
    if (!d) { fprintf(stderr, "%s: out of memory\n", g_progname); exit(1); }
    return d;
}

/* first whitespace-delimited word of a trimmed line, into buf */
static void first_word(const char *line, char *buf, size_t bufsz) {
    size_t i = 0;
    while (line[i] && !isspace((unsigned char)line[i]) && i < bufsz - 1) {
        buf[i] = line[i];
        i++;
    }
    buf[i] = '\0';
}

/* ------------------------------------------------------------------ */
/* variable expansion inside a single (already unquoted-for-expansion) */
/* piece of text. $NAME, ${NAME}, $? -> last exit status.              */
/* ------------------------------------------------------------------ */

static char *expand_vars(const char *in) {
    size_t cap = strlen(in) * 2 + 64;
    char *out = malloc(cap);
    size_t oi = 0;
    if (!out) { fprintf(stderr, "%s: out of memory\n", g_progname); exit(1); }

    for (size_t i = 0; in[i]; ) {
        if (in[i] == '$' && in[i + 1] == '?') {
            char buf[16];
            snprintf(buf, sizeof buf, "%d", g_last_status);
            size_t l = strlen(buf);
            if (oi + l + 1 > cap) { cap = (oi + l + 64) * 2; out = realloc(out, cap); }
            memcpy(out + oi, buf, l);
            oi += l;
            i += 2;
        } else if (in[i] == '$' && (isalpha((unsigned char)in[i + 1]) || in[i + 1] == '_' || in[i + 1] == '{')) {
            int braced = (in[i + 1] == '{');
            size_t start = i + (braced ? 2 : 1);
            size_t j = start;
            while (in[j] && (isalnum((unsigned char)in[j]) || in[j] == '_')) j++;
            char name[64];
            size_t nlen = j - start;
            if (nlen >= sizeof name) nlen = sizeof name - 1;
            memcpy(name, in + start, nlen);
            name[nlen] = '\0';
            if (braced && in[j] == '}') j++;
            const char *val = var_get(name);
            size_t l = strlen(val);
            if (oi + l + 1 > cap) { cap = (oi + l + 64) * 2; out = realloc(out, cap); }
            memcpy(out + oi, val, l);
            oi += l;
            i = j;
        } else {
            if (oi + 2 > cap) { cap *= 2; out = realloc(out, cap); }
            out[oi++] = in[i++];
        }
    }
    out[oi] = '\0';
    return out;
}

/* ------------------------------------------------------------------ */
/* tokenizer: splits a line into argv-style tokens, honoring quotes    */
/* and backslash escapes, and expanding variables in unquoted / double */
/* quoted text (not in single quotes).                                 */
/* ------------------------------------------------------------------ */

static int tokenize(const char *line, char **tokens, int max_tokens) {
    int ntok = 0;
    const char *p = line;
    char raw[MAX_LINE];

    while (*p) {
        while (*p && isspace((unsigned char)*p)) p++;
        if (!*p) break;

        size_t ri = 0;
        int need_expand = 0;

        while (*p && !isspace((unsigned char)*p)) {
            if (*p == '\'') {
                p++;
                while (*p && *p != '\'') {
                    if (ri < sizeof(raw) - 1) raw[ri++] = *p;
                    p++;
                }
                if (*p == '\'') p++;
            } else if (*p == '"') {
                p++;
                while (*p && *p != '"') {
                    if (*p == '\\' && (p[1] == '"' || p[1] == '\\' || p[1] == '$')) {
                        p++;
                        if (ri < sizeof(raw) - 1) raw[ri++] = *p;
                        p++;
                    } else {
                        if (ri < sizeof(raw) - 1) raw[ri++] = *p;
                        p++;
                    }
                }
                if (*p == '"') p++;
                need_expand = 1;
            } else if (*p == '\\' && p[1]) {
                p++;
                if (ri < sizeof(raw) - 1) raw[ri++] = *p;
                p++;
            } else if (*p == '$') {
                need_expand = 1;
                if (ri < sizeof(raw) - 1) raw[ri++] = *p;
                p++;
            } else {
                if (ri < sizeof(raw) - 1) raw[ri++] = *p;
                p++;
            }
        }
        raw[ri] = '\0';

        if (ntok >= max_tokens - 1) break;
        tokens[ntok++] = need_expand ? expand_vars(raw) : xstrdup(raw);
    }
    tokens[ntok] = NULL;
    return ntok;
}

static void free_tokens(char **tokens, int n) {
    for (int i = 0; i < n; i++) free(tokens[i]);
}

/* ------------------------------------------------------------------ */
/* a single parsed command stage: argv + optional redirections         */
/* ------------------------------------------------------------------ */

typedef struct {
    char *argv[MAX_TOKENS];
    int   argc;
    char *infile;
    char *outfile;
    int   append; /* 1 if >> */
} cmd_t;

/* Pull redirection operators out of a raw token list, building argv
 * and capturing filenames. */
static void build_cmd(char **tokens, int ntok, cmd_t *cmd) {
    memset(cmd, 0, sizeof *cmd);
    int ai = 0;
    for (int i = 0; i < ntok; i++) {
        if (strcmp(tokens[i], "<") == 0 && i + 1 < ntok) {
            cmd->infile = tokens[++i];
        } else if (strcmp(tokens[i], ">") == 0 && i + 1 < ntok) {
            cmd->outfile = tokens[++i];
            cmd->append = 0;
        } else if (strcmp(tokens[i], ">>") == 0 && i + 1 < ntok) {
            cmd->outfile = tokens[++i];
            cmd->append = 1;
        } else {
            cmd->argv[ai++] = tokens[i];
        }
    }
    cmd->argv[ai] = NULL;
    cmd->argc = ai;
}

/* ------------------------------------------------------------------ */
/* PATH resolution + execve wrapper                                    */
/*                                                                      */
/* execve() (unlike execvp()) never searches $PATH on its own, so we    */
/* do it ourselves: if the command contains a '/', treat it as a        */
/* literal path; otherwise walk $PATH (falling back to /bin:/usr/bin    */
/* if unset) looking for the first entry we can access(X_OK).           */
/* ------------------------------------------------------------------ */

/* access(2) isn't available on this target's newlib syscall layer, so
 * we can't ask X_OK directly. stat() (already linked in for the `test`
 * builtin) gets us "does this exist and is it a regular file" instead;
 * that's good enough for PATH search purposes. If the file turns out
 * not to actually be executable, execve() in shell_execve() will fail
 * with a clean errno-based diagnostic rather than silently misfiring. */
static int path_is_regular_file(const char *path) {
    struct stat st;
    return (stat(path, &st) == 0 && S_ISREG(st.st_mode));
}

static char *path_resolve(const char *cmd) {
    if (strchr(cmd, '/')) {
        return path_is_regular_file(cmd) ? xstrdup(cmd) : NULL;
    }

    const char *path = getenv("PATH");
    if (!path || !*path) path = "/bin:/usr/bin";

    char *pathdup = xstrdup(path);
    char *saveptr = NULL;
    char *dir = strtok_r(pathdup, ":", &saveptr);
    char candidate[1024];

    while (dir) {
        if (*dir == '\0') dir = "."; /* empty PATH component means cwd */
        snprintf(candidate, sizeof candidate, "%s/%s", dir, cmd);
        if (path_is_regular_file(candidate)) {
            char *found = xstrdup(candidate);
            free(pathdup);
            return found;
        }
        dir = strtok_r(NULL, ":", &saveptr);
    }
    free(pathdup);
    return NULL;
}

/* Resolves argv[0] against PATH (if needed) and execve()s it with the
 * current process environment. Only returns on failure, and only after
 * printing a diagnostic -- callers should _exit() right after. */
static void shell_execve(char **argv) {
    char *resolved = path_resolve(argv[0]);
    if (!resolved) {
        fprintf(stderr, "%s: %s: command not found\n", g_progname, argv[0]);
        _exit(127);
    }
    execve(resolved, argv, environ);
    /* execve only returns on error */
    fprintf(stderr, "%s: %s: %s\n", g_progname, argv[0], strerror(errno));
    free(resolved);
    _exit(126);
}

/* ------------------------------------------------------------------ */
/* builtins                                                             */
/* ------------------------------------------------------------------ */

static int is_assignment(const char *tok, char *name_out, size_t name_sz, char **value_out);
static int run_command_line(const char *line);

static int builtin_test(int argc, char **argv); /* forward decl, defined below */

static int try_builtin(cmd_t *cmd, int *status) {
    if (cmd->argc == 0) { *status = 0; return 1; }
    const char *cmd0 = cmd->argv[0];

    if (strcmp(cmd0, "exit") == 0) {
        int code = cmd->argc > 1 ? atoi(cmd->argv[1]) : g_last_status;
        exit(code);
    }
    if (strcmp(cmd0, "cd") == 0) {
        const char *dir = cmd->argc > 1 ? cmd->argv[1] : var_get("HOME");
        if (!dir || !*dir) dir = "/";
        if (chdir(dir) != 0) {
            fprintf(stderr, "cd: %s: no such directory\n", dir);
            *status = 1;
        } else {
            *status = 0;
        }
        return 1;
    }
    if (strcmp(cmd0, "pwd") == 0) {
        char buf[512];
        if (getcwd(buf, sizeof buf)) printf("%s\n", buf);
        *status = 0;
        return 1;
    }
    if (strcmp(cmd0, "echo") == 0) {
        for (int i = 1; i < cmd->argc; i++) {
            fputs(cmd->argv[i], stdout);
            if (i + 1 < cmd->argc) fputc(' ', stdout);
        }
        fputc('\n', stdout);
        *status = 0;
        return 1;
    }
    if (strcmp(cmd0, "export") == 0) {
        for (int i = 1; i < cmd->argc; i++) {
            char name[64]; char *val = NULL;
            if (is_assignment(cmd->argv[i], name, sizeof name, &val)) {
                var_set(name, val, 1);
                free(val);
            } else {
                shvar_t *v = var_find(cmd->argv[i]);
                if (v) { v->exported = 1; setenv(v->name, v->value, 1); }
                else   setenv(cmd->argv[i], "", 1);
            }
        }
        *status = 0;
        return 1;
    }
    if (strcmp(cmd0, "unset") == 0) {
        for (int i = 1; i < cmd->argc; i++) var_unset(cmd->argv[i]);
        *status = 0;
        return 1;
    }
    if (strcmp(cmd0, "set") == 0) {
        for (int i = 0; i < g_nvars; i++)
            printf("%s=%s%s\n", g_vars[i].name, g_vars[i].value,
                   g_vars[i].exported ? " (exported)" : "");
        *status = 0;
        return 1;
    }
    if (strcmp(cmd0, "env") == 0) {
        for (char **e = environ; *e; e++) printf("%s\n", *e);
        *status = 0;
        return 1;
    }
    if (strcmp(cmd0, "test") == 0) {
        *status = builtin_test(cmd->argc, cmd->argv);
        return 1;
    }
    if (strcmp(cmd0, "[") == 0) {
        /* expect trailing "]"; the real test args are argv[1 .. argc-2] */
        if (cmd->argc < 2 || strcmp(cmd->argv[cmd->argc - 1], "]") != 0) {
            fprintf(stderr, "[: missing closing ]\n");
            *status = 2;
            return 1;
        }
        cmd->argv[cmd->argc - 1] = NULL;
        *status = builtin_test(cmd->argc - 2, cmd->argv + 1);
        return 1;
    }
    if (strcmp(cmd0, "help") == 0) {
        printf("builtins: cd pwd echo export unset set env exit test [ . help\n");
        printf("scripting: if/then/else/fi  while/do/done  for VAR in ...; do/done\n");
        *status = 0;
        return 1;
    }
    if (strcmp(cmd0, ".") == 0 || strcmp(cmd0, "source") == 0) {
        if (cmd->argc < 2) { fprintf(stderr, "%s: filename required\n", cmd0); *status = 1; return 1; }
        FILE *f = fopen(cmd->argv[1], "r");
        if (!f) { fprintf(stderr, "%s: %s: no such file\n", cmd0, cmd->argv[1]); *status = 1; return 1; }
        char linebuf[MAX_LINE];
        while (fgets(linebuf, sizeof linebuf, f))
            run_command_line(linebuf);
        fclose(f);
        *status = g_last_status;
        return 1;
    }
    return 0; /* not a builtin */
}

/* minimal `test` / `[ ]`: supports -z -n -f -d -e, and binary
 * string/int comparisons: = != -eq -ne -lt -le -gt -ge */
static int builtin_test(int argc, char **argv) {
    if (argc == 2) {
        if (strcmp(argv[0], "-z") == 0) return strlen(argv[1]) == 0 ? 0 : 1;
        if (strcmp(argv[0], "-n") == 0) return strlen(argv[1]) != 0 ? 0 : 1;
        struct stat st;
        if (strcmp(argv[0], "-f") == 0) return (stat(argv[1], &st) == 0 && S_ISREG(st.st_mode)) ? 0 : 1;
        if (strcmp(argv[0], "-d") == 0) return (stat(argv[1], &st) == 0 && S_ISDIR(st.st_mode)) ? 0 : 1;
        if (strcmp(argv[0], "-e") == 0) return (stat(argv[1], &st) == 0) ? 0 : 1;
        /* single bare string: true if non-empty */
        return strlen(argv[0]) ? 0 : 1;
    }
    if (argc == 3) {
        const char *a = argv[0], *op = argv[1], *b = argv[2];
        if (strcmp(op, "=") == 0)  return strcmp(a, b) == 0 ? 0 : 1;
        if (strcmp(op, "!=") == 0) return strcmp(a, b) != 0 ? 0 : 1;
        long ai = atol(a), bi = atol(b);
        if (strcmp(op, "-eq") == 0) return ai == bi ? 0 : 1;
        if (strcmp(op, "-ne") == 0) return ai != bi ? 0 : 1;
        if (strcmp(op, "-lt") == 0) return ai <  bi ? 0 : 1;
        if (strcmp(op, "-le") == 0) return ai <= bi ? 0 : 1;
        if (strcmp(op, "-gt") == 0) return ai >  bi ? 0 : 1;
        if (strcmp(op, "-ge") == 0) return ai >= bi ? 0 : 1;
    }
    fprintf(stderr, "test: unsupported expression\n");
    return 2;
}

/* ------------------------------------------------------------------ */
/* execution: one external command, with redirection applied           */
/* ------------------------------------------------------------------ */

static int run_external(cmd_t *cmd) {
    pid_t pid = fork();
    if (pid < 0) {
        perror("fork");
        return 1;
    }
    if (pid == 0) {
        if (cmd->infile) {
            int fd = open(cmd->infile, O_RDONLY);
            if (fd < 0) { fprintf(stderr, "%s: %s: cannot open\n", g_progname, cmd->infile); _exit(1); }
            dup2(fd, STDIN_FILENO);
            close(fd);
        }
        if (cmd->outfile) {
            int flags = O_WRONLY | O_CREAT | (cmd->append ? O_APPEND : O_TRUNC);
            int fd = open(cmd->outfile, flags, 0644);
            if (fd < 0) { fprintf(stderr, "%s: %s: cannot open\n", g_progname, cmd->outfile); _exit(1); }
            dup2(fd, STDOUT_FILENO);
            close(fd);
        }
        shell_execve(cmd->argv);
        /* shell_execve never returns */
    }
    int wstatus;
    waitpid(pid, &wstatus, 0);
    if (WIFEXITED(wstatus)) return WEXITSTATUS(wstatus);
    return 1;
}

/* Run a pipeline of '|'-separated commands (already split into raw
 * per-stage strings). Builtins inside a pipeline are also forked so
 * that pipe fd juggling stays uniform and simple. */
static int run_pipeline(char **stage_lines, int nstages) {
    if (nstages == 1) {
        char *tokens[MAX_TOKENS];
        int ntok = tokenize(stage_lines[0], tokens, MAX_TOKENS);
        if (ntok == 0) { free_tokens(tokens, ntok); return 0; }
        cmd_t cmd;
        build_cmd(tokens, ntok, &cmd);
        int status;
        if (cmd.argc > 0 && try_builtin(&cmd, &status)) {
            free_tokens(tokens, ntok);
            return status;
        }
        if (cmd.argc == 0) { free_tokens(tokens, ntok); return 0; }
        status = run_external(&cmd);
        free_tokens(tokens, ntok);
        return status;
    }

    int pipefd[MAX_PIPESTAGE][2];
    for (int i = 0; i < nstages - 1; i++) {
        if (pipe(pipefd[i]) != 0) { perror("pipe"); return 1; }
    }

    pid_t pids[MAX_PIPESTAGE];
    for (int i = 0; i < nstages; i++) {
        char *tokens[MAX_TOKENS];
        int ntok = tokenize(stage_lines[i], tokens, MAX_TOKENS);
        cmd_t cmd;
        build_cmd(tokens, ntok, &cmd);

        pid_t pid = fork();
        if (pid < 0) { perror("fork"); return 1; }
        if (pid == 0) {
            if (i > 0) dup2(pipefd[i - 1][0], STDIN_FILENO);
            if (i < nstages - 1) dup2(pipefd[i][1], STDOUT_FILENO);
            for (int j = 0; j < nstages - 1; j++) {
                close(pipefd[j][0]);
                close(pipefd[j][1]);
            }
            if (cmd.infile) {
                int fd = open(cmd.infile, O_RDONLY);
                if (fd >= 0) { dup2(fd, STDIN_FILENO); close(fd); }
            }
            if (cmd.outfile) {
                int flags = O_WRONLY | O_CREAT | (cmd.append ? O_APPEND : O_TRUNC);
                int fd = open(cmd.outfile, flags, 0644);
                if (fd >= 0) { dup2(fd, STDOUT_FILENO); close(fd); }
            }
            if (cmd.argc == 0) _exit(0);
            int bstatus;
            if (try_builtin(&cmd, &bstatus)) _exit(bstatus);
            shell_execve(cmd.argv);
            /* shell_execve never returns */
        }
        pids[i] = pid;
        free_tokens(tokens, ntok);
    }

    for (int j = 0; j < nstages - 1; j++) {
        close(pipefd[j][0]);
        close(pipefd[j][1]);
    }

    int last_status = 0;
    for (int i = 0; i < nstages; i++) {
        int wstatus;
        waitpid(pids[i], &wstatus, 0);
        if (i == nstages - 1 && WIFEXITED(wstatus)) last_status = WEXITSTATUS(wstatus);
    }
    return last_status;
}

/* ------------------------------------------------------------------ */
/* assignment detection: NAME=value with no spaces, NAME a valid       */
/* identifier                                                           */
/* ------------------------------------------------------------------ */

static int is_assignment(const char *tok, char *name_out, size_t name_sz, char **value_out) {
    const char *eq = strchr(tok, '=');
    if (!eq || eq == tok) return 0;
    size_t nlen = (size_t)(eq - tok);
    if (nlen >= name_sz) return 0;
    if (!(isalpha((unsigned char)tok[0]) || tok[0] == '_')) return 0;
    for (size_t i = 0; i < nlen; i++)
        if (!(isalnum((unsigned char)tok[i]) || tok[i] == '_')) return 0;
    memcpy(name_out, tok, nlen);
    name_out[nlen] = '\0';
    char *expanded = expand_vars(eq + 1);
    *value_out = expanded;
    return 1;
}

/* ------------------------------------------------------------------ */
/* split a line into '|' pipeline stages, respecting quotes             */
/* ------------------------------------------------------------------ */

static int split_pipe(char *line, char **stages, int max_stages) {
    int n = 0;
    char *start = line;
    int in_single = 0, in_double = 0;
    char *p = line;
    while (*p) {
        if (*p == '\'' && !in_double) in_single = !in_single;
        else if (*p == '"' && !in_single) in_double = !in_double;
        else if (*p == '|' && !in_single && !in_double) {
            *p = '\0';
            if (n < max_stages) stages[n++] = start;
            start = p + 1;
        }
        p++;
    }
    if (n < max_stages) stages[n++] = start;
    return n;
}

/* ------------------------------------------------------------------ */
/* split a line into ; && || segments, respecting quotes. Each segment */
/* is paired with the operator that PRECEDES it ("" for the first).     */
/* ------------------------------------------------------------------ */

typedef struct {
    char op[3];   /* "", ";", "&&", "||" */
    char *text;
} segment_t;

static int split_logical(char *line, segment_t *segs, int max_segs) {
    int n = 0;
    char *start = line;
    int in_single = 0, in_double = 0;
    char *p = line;
    char pending_op[3] = "";

    while (*p) {
        if (*p == '\'' && !in_double) { in_single = !in_single; p++; continue; }
        if (*p == '"' && !in_single)  { in_double = !in_double; p++; continue; }
        if (!in_single && !in_double) {
            if (p[0] == '&' && p[1] == '&') {
                *p = '\0';
                if (n < max_segs) { strcpy(segs[n].op, pending_op); segs[n].text = start; n++; }
                strcpy(pending_op, "&&");
                p += 2; start = p; continue;
            }
            if (p[0] == '|' && p[1] == '|') {
                *p = '\0';
                if (n < max_segs) { strcpy(segs[n].op, pending_op); segs[n].text = start; n++; }
                strcpy(pending_op, "||");
                p += 2; start = p; continue;
            }
            if (p[0] == ';') {
                *p = '\0';
                if (n < max_segs) { strcpy(segs[n].op, pending_op); segs[n].text = start; n++; }
                strcpy(pending_op, ";");
                p += 1; start = p; continue;
            }
        }
        p++;
    }
    if (n < max_segs) { strcpy(segs[n].op, pending_op); segs[n].text = start; n++; }
    return n;
}

/* ------------------------------------------------------------------ */
/* run_command_line: one logical line (no control-flow keywords),      */
/* handles ; && || and, per-segment, pipelines and assignment.          */
/* ------------------------------------------------------------------ */

static int run_command_line(const char *line_in) {
    char linebuf[MAX_LINE];
    strncpy(linebuf, line_in, sizeof linebuf - 1);
    linebuf[sizeof linebuf - 1] = '\0';

    char *hash = NULL; /* strip comments, but not inside quotes */
    {
        int in_s = 0, in_d = 0;
        for (char *q = linebuf; *q; q++) {
            if (*q == '\'' && !in_d) in_s = !in_s;
            else if (*q == '"' && !in_s) in_d = !in_d;
            else if (*q == '#' && !in_s && !in_d) { hash = q; break; }
        }
    }
    if (hash) *hash = '\0';

    char *trimmed = trim(linebuf);
    if (*trimmed == '\0') return g_last_status;

    segment_t segs[MAX_SEGMENTS];
    int nseg = split_logical(trimmed, segs, MAX_SEGMENTS);

    int status = g_last_status;
    for (int i = 0; i < nseg; i++) {
        if (i > 0) {
            if (strcmp(segs[i].op, "&&") == 0 && status != 0) continue;
            if (strcmp(segs[i].op, "||") == 0 && status == 0) continue;
        }
        char *seg = trim(segs[i].text);
        if (*seg == '\0') continue;

        /* bare variable assignment as the whole segment, e.g. FOO=bar */
        char name[64]; char *val = NULL;
        char firstword[128];
        first_word(seg, firstword, sizeof firstword);
        if (strchr(firstword, '=') && is_assignment(firstword, name, sizeof name, &val)
            && strlen(firstword) == strlen(seg)) {
            var_set(name, val, 0);
            free(val);
            status = 0;
            continue;
        }

        char *stages[MAX_PIPESTAGE];
        int nstages = split_pipe(seg, stages, MAX_PIPESTAGE);
        status = run_pipeline(stages, nstages);
    }
    g_last_status = status;
    return status;
}

/* ------------------------------------------------------------------ */
/* control-flow interpreter over an array of raw lines. Recognizes     */
/* if/then/else/fi, while/do/done, for/do/done. Nesting is tracked by  */
/* counting opener keywords vs "fi"/"done" as first words.              */
/* ------------------------------------------------------------------ */

static int interp_lines(char **lines, int n);

/* find index (within [start,n)) of first line whose first word is
 * exactly `kw`, at nesting depth 0 relative to start. Any of
 * if/while/for increases depth; fi/done decreases it. Returns -1 if
 * not found. */
static int find_at_depth0(char **lines, int start, int n, const char *kw) {
    int depth = 0;
    for (int i = start; i < n; i++) {
        char fw[128];
        first_word(trim(lines[i]), fw, sizeof fw);
        if (strcmp(fw, "if") == 0 || strcmp(fw, "while") == 0 || strcmp(fw, "for") == 0) {
            depth++;
            continue;
        }
        if (strcmp(fw, "fi") == 0 || strcmp(fw, "done") == 0) {
            if (depth == 0) {
                if (strcmp(fw, kw) == 0) return i;
                /* mismatched closer at depth 0: stop searching */
                return -1;
            }
            depth--;
            continue;
        }
        if (depth == 0 && strcmp(fw, kw) == 0) return i;
    }
    return -1;
}

/* Strip a leading keyword and an optional trailing "; then" / "; do"
 * suffix from a line, returning the condition/list text and whether
 * the suffix was found on this same line. */
static char *strip_keyword_and_suffix(const char *line, const char *keyword, const char *suffix, int *found_suffix) {
    const char *p = line + strlen(keyword);
    char *rest = xstrdup(p);
    char *t = trim(rest);
    size_t tl = strlen(t);
    size_t sl = strlen(suffix);
    *found_suffix = 0;
    if (tl >= sl && strcmp(t + tl - sl, suffix) == 0) {
        t[tl - sl] = '\0';
        rstrip(t);
        *found_suffix = 1;
    }
    char *result = xstrdup(t);
    free(rest);
    return result;
}

static int interp_lines(char **lines, int n) {
    int i = 0;
    int status = g_last_status;

    while (i < n) {
        char *raw = trim(lines[i]);
        if (*raw == '\0' || *raw == '#') { i++; continue; }

        char fw[128];
        first_word(raw, fw, sizeof fw);

        if (strcmp(fw, "if") == 0) {
            int found;
            char *cond = strip_keyword_and_suffix(raw, "if", "then", &found);
            int cond_line_idx = i;
            if (!found) {
                i++;
                while (i < n && *trim(lines[i]) == '\0') i++;
                char fw2[16]; first_word(trim(lines[i]), fw2, sizeof fw2);
                if (strcmp(fw2, "then") != 0) {
                    fprintf(stderr, "%s: line %d: expected 'then' after if\n", g_progname, i + 1);
                    free(cond);
                    return 1;
                }
            }
            int fi_idx = find_at_depth0(lines, i + 1, n, "fi");
            if (fi_idx < 0) {
                fprintf(stderr, "%s: line %d: 'if' without matching 'fi'\n", g_progname, cond_line_idx + 1);
                free(cond);
                return 1;
            }
            int else_idx = -1;
            {
                int depth = 0;
                for (int k = i + 1; k < fi_idx; k++) {
                    char fwk[16]; first_word(trim(lines[k]), fwk, sizeof fwk);
                    if (strcmp(fwk, "if") == 0 || strcmp(fwk, "while") == 0 || strcmp(fwk, "for") == 0) depth++;
                    else if (strcmp(fwk, "fi") == 0 || strcmp(fwk, "done") == 0) depth--;
                    else if (depth == 0 && strcmp(fwk, "else") == 0) { else_idx = k; break; }
                }
            }
            status = run_command_line(cond);
            int then_start = i + 1;
            int then_end = (else_idx >= 0) ? else_idx : fi_idx;
            if (status == 0) {
                status = interp_lines(lines + then_start, then_end - then_start);
            } else if (else_idx >= 0) {
                status = interp_lines(lines + else_idx + 1, fi_idx - else_idx - 1);
            }
            free(cond);
            i = fi_idx + 1;
            g_last_status = status;
            continue;
        }

        if (strcmp(fw, "while") == 0) {
            int found;
            char *cond = strip_keyword_and_suffix(raw, "while", "do", &found);
            int cond_line_idx = i;
            if (!found) {
                i++;
                while (i < n && *trim(lines[i]) == '\0') i++;
                char fw2[16]; first_word(trim(lines[i]), fw2, sizeof fw2);
                if (strcmp(fw2, "do") != 0) {
                    fprintf(stderr, "%s: line %d: expected 'do' after while\n", g_progname, i + 1);
                    free(cond);
                    return 1;
                }
            }
            int done_idx = find_at_depth0(lines, i + 1, n, "done");
            if (done_idx < 0) {
                fprintf(stderr, "%s: line %d: 'while' without matching 'done'\n", g_progname, cond_line_idx + 1);
                free(cond);
                return 1;
            }
            int body_start = i + 1, body_len = done_idx - body_start;
            long iterations = 0;
            const long MAX_ITER = 1000000; /* guard against runaway scripts */
            while (run_command_line(cond) == 0) {
                status = interp_lines(lines + body_start, body_len);
                if (++iterations > MAX_ITER) {
                    fprintf(stderr, "%s: while loop exceeded %ld iterations, aborting\n", g_progname, MAX_ITER);
                    break;
                }
            }
            free(cond);
            i = done_idx + 1;
            g_last_status = status;
            continue;
        }

        if (strcmp(fw, "for") == 0) {
            /* syntax: for VAR in v1 v2 v3 [; do]   (do may be next line) */
            int found;
            char *rest = strip_keyword_and_suffix(raw, "for", "do", &found);
            int cond_line_idx = i;
            if (!found) {
                i++;
                while (i < n && *trim(lines[i]) == '\0') i++;
                char fw2[16]; first_word(trim(lines[i]), fw2, sizeof fw2);
                if (strcmp(fw2, "do") != 0) {
                    fprintf(stderr, "%s: line %d: expected 'do' after for\n", g_progname, i + 1);
                    free(rest);
                    return 1;
                }
            }
            int done_idx = find_at_depth0(lines, i + 1, n, "done");
            if (done_idx < 0) {
                fprintf(stderr, "%s: line %d: 'for' without matching 'done'\n", g_progname, cond_line_idx + 1);
                free(rest);
                return 1;
            }
            char varname[64]; char listtext[MAX_LINE];
            {
                char *r = rest;
                char *var_tok = strtok(r, " \t");
                char *in_tok  = strtok(NULL, " \t");
                if (!var_tok || !in_tok || strcmp(in_tok, "in") != 0) {
                    fprintf(stderr, "%s: line %d: expected 'for VAR in LIST'\n", g_progname, cond_line_idx + 1);
                    free(rest);
                    return 1;
                }
                strncpy(varname, var_tok, sizeof varname - 1);
                varname[sizeof varname - 1] = '\0';
                char *remainder = strtok(NULL, "");
                strncpy(listtext, remainder ? remainder : "", sizeof listtext - 1);
                listtext[sizeof listtext - 1] = '\0';
            }
            int body_start = i + 1, body_len = done_idx - body_start;

            char *expanded_list = expand_vars(listtext);
            char *save = NULL;
            char *item = strtok_r(expanded_list, " \t", &save);
            while (item) {
                var_set(varname, item, 0);
                status = interp_lines(lines + body_start, body_len);
                item = strtok_r(NULL, " \t", &save);
            }
            free(expanded_list);
            free(rest);
            i = done_idx + 1;
            g_last_status = status;
            continue;
        }

        /* plain command line */
        status = run_command_line(raw);
        i++;
    }
    return status;
}

/* ------------------------------------------------------------------ */
/* script-file mode                                                    */
/* ------------------------------------------------------------------ */

static int run_script_file(const char *path) {
    FILE *f = fopen(path, "r");
    if (!f) {
        fprintf(stderr, "%s: %s: no such file\n", g_progname, path);
        return 1;
    }
    char *lines[MAX_BLOCKLINES];
    int n = 0;
    char buf[MAX_LINE];
    while (n < MAX_BLOCKLINES && fgets(buf, sizeof buf, f)) {
        rstrip(buf);
        lines[n++] = xstrdup(buf);
    }
    fclose(f);
    int status = interp_lines(lines, n);
    for (int i = 0; i < n; i++) free(lines[i]);
    return status;
}

/* ------------------------------------------------------------------ */
/* interactive mode: reads one logical unit at a time. A "unit" is a   */
/* single line, unless it opens an if/while/for block, in which case   */
/* the shell keeps reading (with a continuation prompt) until the      */
/* block balances (depth returns to 0), then interprets the whole      */
/* buffered block at once.                                              */
/* ------------------------------------------------------------------ */

static void print_prompt(void) {
    char cwd[512];
    if (getcwd(cwd, sizeof cwd))
        printf("%s:%s$ ", g_progname, cwd);
    else
        printf("%s$ ", g_progname);
    fflush(stdout);
}

static void run_interactive(void) {
    char *block[MAX_BLOCKLINES];
    for (;;) {
        int n = 0;
        int depth = 0;
        int have_any = 0;

        print_prompt();
        char buf[MAX_LINE];
        if (!fgets(buf, sizeof buf, stdin)) {
            printf("\n");
            break;
        }
        rstrip(buf);
        block[n++] = xstrdup(buf);
        have_any = 1;

        char fw[128];
        first_word(trim(buf), fw, sizeof fw);
        if (strcmp(fw, "if") == 0 || strcmp(fw, "while") == 0 || strcmp(fw, "for") == 0) depth++;

        while (depth > 0 && n < MAX_BLOCKLINES) {
            printf("> ");
            fflush(stdout);
            if (!fgets(buf, sizeof buf, stdin)) break;
            rstrip(buf);
            block[n++] = xstrdup(buf);
            first_word(trim(buf), fw, sizeof fw);
            if (strcmp(fw, "if") == 0 || strcmp(fw, "while") == 0 || strcmp(fw, "for") == 0) depth++;
            else if (strcmp(fw, "fi") == 0 || strcmp(fw, "done") == 0) depth--;
        }

        if (have_any) interp_lines(block, n);
        for (int i = 0; i < n; i++) free(block[i]);
    }
}

/* ------------------------------------------------------------------ */

int main(int argc, char **argv) {
    var_set("PS1", "$ ", 0);
    if (!getenv("HOME")) setenv("HOME", "/", 1);

    if (argc > 1) {
        return run_script_file(argv[1]);
    }
    run_interactive();
    return g_last_status;
}
