#include <fcntl.h>
#include <stdio.h>
#include <unistd.h>

#define BUF_SIZE 4096

static int cat_fd(int fd)
{
    char buf[BUF_SIZE];

    for (;;) {
        int n = read(fd, buf, sizeof(buf));

        if (n < 0)
            return -1;

        if (n == 0)
            return 0;

        int written = 0;
        while (written < n) {
            int ret = write(STDOUT_FILENO, buf + written, n - written);
            if (ret < 0)
                return -1;

            written += ret;
        }
    }
}

int main(int argc, char **argv)
{
    if (argc == 1) {
        if (cat_fd(STDIN_FILENO) < 0) {
            fprintf(stderr, "cat: read/write error\n");
            return 1;
        }
        return 0;
    }

    for (int i = 1; i < argc; i++) {
        int fd = open(argv[i], O_RDONLY);

        if (fd < 0) {
            fprintf(stderr, "cat: cannot open %s\n", argv[i]);
            continue;
        }

        if (cat_fd(fd) < 0) {
            fprintf(stderr, "cat: read/write error: %s\n", argv[i]);
            close(fd);
            return 1;
        }

        close(fd);
    }

    return 0;
}
