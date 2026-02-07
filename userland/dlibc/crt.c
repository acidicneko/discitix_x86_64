#include <syscall.h>

extern int main(int argc, char *argv[]);

void _start(long argc, char *argv[]) {
    int ret = main((int)argc, argv);
    syscall1(SYS_EXIT, ret);  
    for (;;) {
        asm volatile("pause");
    }
}