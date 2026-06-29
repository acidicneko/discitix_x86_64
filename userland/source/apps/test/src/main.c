#include <fcntl.h>
#include <unistd.h>
#include <stdio.h>
#include <string.h>

int main() {
    printf("--- Starting lseek test ---\n");

    // 1. Create and open the file
    int fd = open("seektest.txt", O_CREAT | O_TRUNC | O_RDWR, 0666);
    if (fd < 0) {
        printf("[FAIL] Could not open/create seektest.txt\n");
        return 1;
    }

    // 2. Test initial write
    int written = write(fd, "HELLO WORLD", 11);
    if (written != 11) {
        printf("[FAIL] write() returned %d, expected 11\n", written);
    }

    // 3. Test SEEK_SET (Jump to beginning)
    off_t pos = lseek(fd, 0, SEEK_SET);
    if (pos != 0) {
        printf("[FAIL] lseek(SEEK_SET) returned %d, expected 0\n", (int)pos);
    }

    // 4. Test Read
    char buf[6] = {0};
    int bytes_read = read(fd, buf, 5);
    if (strcmp(buf, "HELLO") != 0) {
        printf("[FAIL] Read wrong data. Expected 'HELLO', got '%s'\n", buf);
    }

    // 5. Test SEEK_CUR (Skip the space character)
    pos = lseek(fd, 1, SEEK_CUR); 
    if (pos != 6) {
        printf("[FAIL] lseek(SEEK_CUR) returned %d, expected 6\n", (int)pos);
    }

    // 6. Test second read
    memset(buf, 0, 6);
    read(fd, buf, 5);
    if (strcmp(buf, "WORLD") != 0) {
        printf("[FAIL] Read wrong data. Expected 'WORLD', got '%s'\n", buf);
    }

    // 7. Test Overwrite (This is what TCC does to fix ELF headers!)
    lseek(fd, 0, SEEK_SET);
    write(fd, "J", 1); // Changes file to "JELLO WORLD"

    // 8. Test SEEK_END
    pos = lseek(fd, 0, SEEK_END);
    if (pos != 11) {
        printf("[FAIL] lseek(SEEK_END) returned %d, expected 11\n", (int)pos);
    }

    close(fd);
    printf("--- Test Complete. Check for [FAIL] messages above. ---\n");
    return 0;
}
