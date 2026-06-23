#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <sys/wait.h>
#include <stdlib.h>

#define MAX_LINE 256
#define MAX_ARGS 16

static int parse_args(char *line, char *argv[])
{
    int argc = 0;

    while (*line && argc < MAX_ARGS - 1) {
        while (*line == ' ' || *line == '\t')
            line++;

        if (*line == '\0' || *line == '\n')
            break;

        argv[argc++] = line;

        while (*line &&
               *line != ' ' &&
               *line != '\t' &&
               *line != '\n')
            line++;

        if (*line == '\0')
            break;

        *line++ = '\0';
    }

    argv[argc] = NULL;
    return argc;
}

static void print_prompt(void)
{
    char cwd[128];

    printf("\033[1;32m");

    if (getcwd(cwd, sizeof(cwd)))
        printf("%s", cwd);
    else
        printf("?");

    printf(" $ \033[0m");
    fflush(stdout);
}

int main(void)
{
    char line[MAX_LINE];
    char *argv[MAX_ARGS];

    puts("Simple Shell");
    puts("Type 'help' for commands");

    while (1) {
        print_prompt();

        if (!fgets(line, sizeof(line), stdin))
            break;

        line[strcspn(line, "\n")] = '\0';

        if (line[0] == '\0')
            continue;

        int argc = parse_args(line, argv);

        if (argc == 0)
            continue;

        /* builtins */

        if (!strcmp(argv[0], "exit")) {
            break;
        }

        if (!strcmp(argv[0], "clear")) {
            printf("\033[2J\033[H");
            continue;
        }

        if (!strcmp(argv[0], "pwd")) {
            char cwd[128];

            if (getcwd(cwd, sizeof(cwd)))
                puts(cwd);
            else
                puts("pwd: failed");

            continue;
        }

        if (!strcmp(argv[0], "cd")) {
            const char *target =
                (argc > 1) ? argv[1] : "/";

            if (chdir(target) != 0)
                printf("cd: %s: no such directory\n",
                       target);

            continue;
        }

        if (!strcmp(argv[0], "help")) {
            puts("Builtins:");
            puts("  cd [dir]");
            puts("  pwd");
            puts("  clear");
            puts("  exit");
            continue;
        }

        /* external command */

        char path[128];
        snprintf(path, sizeof(path), "/%s", argv[0]);

        pid_t pid = fork();

        if (pid < 0) {
            puts("fork failed");
            continue;
        }

        if (pid == 0) {
            execve(path, argv, NULL);

            fprintf(stderr,
                    "command not found: %s\n",
                    argv[0]);

            _exit(127);
        }

        int status;
        waitpid(pid, &status, 0);
    }

    puts("bye");
    return 0;
}
