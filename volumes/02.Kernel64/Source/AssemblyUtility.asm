[BITS 64]

SECTION .text

global kInPortByte, kOutPortByte, kLoadGDTR, kLoadTR, kLoadIDTR
global kEnableInterrupt, kDisableInterrupt, kReadRFLAGS
global kReadTSC
global kSwitchContext, kHlt

kInPortByte:
    push rdx

    mov rdx, rdi
    mov rax, 0
    in al, dx

    pop rdx
    ret

kOutPortByte:
    push rdx
    push rax

    mov rdx, rdi
    mov rax, rsi
    out dx, al

    pop rax
    pop rdx
    ret

kLoadGDTR:
    lgdt [ rdi ]
    ret

kLoadTR:
    ltr di
    ret

kLoadIDTR:
    lidt [ rdi ]
    ret

kEnableInterrupt:
    sti
    ret

kDisableInterrupt:
    cli
    ret

kReadRFLAGS:
    pushfq
    pop rax

    ret

kReadTSC:
    push rdx

    rdtsc

    shl rdx, 32
    or rax, rdx

    pop rdx
    ret

%macro KSAVECONTEXT 0
    push rbp
    push rax
    push rbx
    push rcx
    push rdx
    push rdi
    push rsi
    push r8
    push r9
    push r10
    push r11
    push r12
    push r13
    push r14
    push r15
    

    ; DS와 ES는 스택에 직접 삽입할 수 없으므로 RAX에 저장한 후 스택에 삽입
    mov ax, ds
    push rax
    push fs
    push gs
%endmacro

%macro KLOADCONTEXT 0
    pop gs
    pop fs
    pop rax
    mov es, ax
    pop rax
    mov ds, ax

    pop r15
    pop r14
    pop r13
    pop r12
    pop r11
    pop r10
    pop r9
    pop r8
    pop rsi
    pop rdi
    pop rdx
    pop rcx
    pop rbx
    pop rax
    pop rbp
%endmacro

kSwitchContext:
    push rbp
    mov rbp, rsp

    pushfq
    cmp rdi, 0
    je .LoadContext
    popfq

    ; 현재 태스크의 콘텍스트를 저장
    push rax

    ; SS, RSP, RFLAGS, CS, RIP 레지스터 순서대로 삽입
    mov ax, ss
    mov qword[rdi + (23 * 8)], rax

    mov rax, rbp
    add rax, 16
    mov qword[rdi + (22 * 8)], rax

    pushfq
    pop rax
    mov qword[rdi + (21 * 8)], rax

    mov ax, cs
    mov qword[rdi + (20 * 8)], rax

    mov rax, qword[rbp + 8]
    mov qword[rdi + (19 * 8)], rax

    ; 저장한 레지스터를 복구한 후 인터럽트가 발생했을 때처럼 나머지 콘텍스트를 모두 저장
    pop rax
    pop rbp

    add rdi, (19 * 8)
    mov rsp, rdi
    sub rdi, (19 * 8)

    KSAVECONTEXT

.LoadContext
    mov rsp, rsi

    KLOADCONTEXT
    iretq

kHlt:
    hlt
    hlt
    ret