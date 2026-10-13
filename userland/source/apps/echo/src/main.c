#include <unistd.h>
#include <string.h>

int main(int argc, char *argv[])
{
    for (int i = 1; i < argc; i++) {
        write(STDOUT_FILENO, argv[i], strlen(argv[i]));

        if (i != argc - 1)
            write(STDOUT_FILENO, " ", 1);
    }

    write(STDOUT_FILENO, "\n", 1);
    return 0;
}
