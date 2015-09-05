#include "errno_defs.h"

        .global safe_syscall_base
        .global safe_syscall_start
        .global safe_syscall_end
        .type   safe_syscall_base, @function

safe_syscall_base:
        push    %rbp
        mov     %rsi, %rax // Move syscall number
        mov     %rdi, %rbp // Move signal_pending pointer

        mov     %rdx, %rdi // Move syscall arguments
        mov     %rcx, %rsi
        mov     %r8,  %rdx
        mov     %r9,  %r10
        mov     16(%rsp), %r8
        mov     24(%rsp), %r9

safe_syscall_start:
        testl   $1, (%rbp) // Check signal_pending
        jnz     return_ERESTARTSYS
        syscall
safe_syscall_end:
        pop     %rbp
        mov     %rax, %rdi
        jmp     convert_syscall_return_value

return_ERESTARTSYS:
        mov     $-TARGET_ERESTARTSYS, %rax
        pop     %rbp
        ret

        .size   safe_syscall_base, .-safe_syscall_base
