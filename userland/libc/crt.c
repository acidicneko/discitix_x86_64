extern char **environ;
extern int main(int argc, char **argv, char **envp);

__attribute__((naked, section(".text._start")))
void _start(void) {
    asm volatile (
        "xor %rbp, %rbp\n\t"
        "and $-16, %rsp\n\t"
        "mov %rdx, environ(%rip)\n\t"
        "call main\n\t"
        "mov %eax, %edi\n\t"
        "mov $0, %rax\n\t"
        "int $0x80\n\t"
        "1:\n\t"
        "pause\n\t"
        "jmp 1b\n\t"
        "nop\n\t"
        "ud2\n\t"
    );
}
