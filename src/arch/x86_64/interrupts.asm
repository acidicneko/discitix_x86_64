[bits 64]

extern fault_handler
extern irq_handler

%macro ISR_NOERRCODE 1
  [GLOBAL isr%1]
  isr%1:
    push 0
    push %1
    jmp isr_common_stub
%endmacro

%macro ISR_ERRCODE 1
  [GLOBAL isr%1]
  isr%1:
    push %1
    jmp isr_common_stub
%endmacro

%macro IRQ 2
  global irq%1
  irq%1:
    push 0
    push %2
    jmp irq_common_stub
%endmacro

%macro save_regs 0
      push   rax
      push   rbx
      push   rcx
      push   rdx
      push   rsi
      push   rdi
      push   rbp
      push   r8
      push   r9
      push   r10
      push   r11
      push   r12
      push   r13
      push   r14
      push   r15
%endmacro

%macro restore_regs 0
      pop       r15
      pop       r14
      pop       r13
      pop       r12
      pop       r11
      pop       r10
      pop       r9
      pop       r8
      pop       rbp
      pop       rdi
      pop       rsi
      pop       rdx
      pop       rcx
      pop       rbx
      pop       rax
%endmacro

isr_common_stub:
    cld
    save_regs
    mov rdi, rsp
    call fault_handler
    mov rsp, rax
    restore_regs
    add rsp, 16
    iretq

irq_common_stub:
    cld
    save_regs
    mov rdi, rsp
    call irq_handler
    mov rsp, rax          ; Support context switch - use returned rsp
    restore_regs
    add rsp, 16
    iretq

ISR_NOERRCODE 0
ISR_NOERRCODE 1
ISR_NOERRCODE 2
ISR_NOERRCODE 3
ISR_NOERRCODE 4
ISR_NOERRCODE 5
ISR_NOERRCODE 6
ISR_NOERRCODE 7
ISR_ERRCODE   8
ISR_NOERRCODE 9
ISR_ERRCODE   10
ISR_ERRCODE   11
ISR_ERRCODE   12
ISR_ERRCODE   13
ISR_ERRCODE   14
ISR_NOERRCODE 15
ISR_NOERRCODE 16
ISR_NOERRCODE 17
ISR_NOERRCODE 18
ISR_NOERRCODE 19
ISR_NOERRCODE 20
ISR_NOERRCODE 21
ISR_NOERRCODE 22
ISR_NOERRCODE 23
ISR_NOERRCODE 24
ISR_NOERRCODE 25
ISR_NOERRCODE 26
ISR_NOERRCODE 27
ISR_NOERRCODE 28
ISR_NOERRCODE 29
ISR_NOERRCODE 30
ISR_NOERRCODE 31

IRQ   0,      32
IRQ   1,      33
IRQ   2,      34
IRQ   3,      35
IRQ   4,      36
IRQ   5,      37
IRQ   6,      38
IRQ   7,      39
IRQ   8,      40
IRQ   9,      41
IRQ  10,      42
IRQ  11,      43
IRQ  12,      44
IRQ  13,      45
IRQ  14,      46
IRQ  15,      47

; Syscall handler (int 0x80)
extern syscall_dispatch

global syscall_stub
syscall_stub:
    ; Save all registers (build register_t structure on stack)
    push 0              ; Error code placeholder
    push 0x80           ; Interrupt number (syscall)
    save_regs
    
    ; Call syscall dispatcher with pointer to register_t
    mov rdi, rsp
    call syscall_dispatch
    
    ; Return value is in rax, store it in the saved rax position
    mov [rsp + 14*8], rax   ; Offset to saved rax in register_t
    
    restore_regs
    add rsp, 16         ; Remove int_no and err_code
    iretq

; Helper to jump to Ring 3 user mode
; void jump_to_usermode(uint64_t entry, uint64_t user_stack)
global jump_to_usermode
jump_to_usermode:
    ; rdi = user entry point
    ; rsi = user stack pointer
    
    ; Set up stack frame for iretq
    ; Stack needs: SS, RSP, RFLAGS, CS, RIP
    
    mov rcx, rdi        ; Save entry point
    mov rdx, rsi        ; Save stack pointer
    
    ; Push SS (user data segment with RPL 3)
    mov rax, 0x18 | 3   ; GDT_USER_DATA | RPL 3
    push rax
    
    ; Push RSP (user stack pointer)
    push rdx
    
    ; Push RFLAGS (enable interrupts)
    pushfq
    pop rax
    or rax, 0x200       ; Set IF (interrupt flag)
    push rax
    
    ; Push CS (user code segment with RPL 3)
    mov rax, 0x20 | 3   ; GDT_USER_CODE | RPL 3
    push rax
    
    ; Push RIP (user entry point)
    push rcx
    
    ; Clear registers for clean usermode entry
    xor rax, rax
    xor rbx, rbx
    xor rcx, rcx
    xor rdx, rdx
    xor rsi, rsi
    xor rdi, rdi
    xor rbp, rbp
    xor r8, r8
    xor r9, r9
    xor r10, r10
    xor r11, r11
    xor r12, r12
    xor r13, r13
    xor r14, r14
    xor r15, r15
    
    ; Switch to user mode!
    iretq