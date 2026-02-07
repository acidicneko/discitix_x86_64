#include <stdio.h>
#include <fcntl.h>

int main(int argc, char* argv[]) {
    if(argc < 2) {
        print("Usage: dbgln <message>\n");
        return 1;
    }
    int fd = open("/sr0", 1);
    if(fd < 0) {
        print("Failed to open serial device\n");
        return 1;
    }
    for(int i = 1; i < argc; i++) {
        const char* msg = argv[i];
        write(fd, msg, strlen(msg));
        write(fd, " ", 1);
    }
    write(fd, "\n", 1);
    close(fd);
    return 0;
}