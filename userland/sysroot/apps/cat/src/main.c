#include <fcntl.h>
#include <stdio.h>
#include <unistd.h>

#define BUF_SIZE 4096

int main(int argc, char **argv)
{
    if (argc < 2) {
        fprintf(stderr, "usage: cat <file> [file ...]\n");
        return 1;
    }

    char buf[BUF_SIZE];

    for (int i = 1; i < argc; i++) {

        int fd = open(argv[i], O_RDONLY);

        if (fd < 0) {
            fprintf(stderr, "cat: cannot open %s\n", argv[i]);
            continue;
        }

        for (;;) {

            int n = read(fd, buf, sizeof(buf));

            if (n < 0) {
                fprintf(stderr, "cat: read error: %s\n", argv[i]);
                close(fd);
                return 1;
            }

            if (n == 0)
                break;

            int written = 0;

            while (written < n) {

                int ret = write(
                    STDOUT_FILENO,
                    buf + written,
                    n - written
                );

                if (ret < 0) {
                    fprintf(stderr, "cat: write error\n");
                    close(fd);
                    return 1;
                }

                written += ret;
            }
        }

        close(fd);
    }

    return 0;
}
