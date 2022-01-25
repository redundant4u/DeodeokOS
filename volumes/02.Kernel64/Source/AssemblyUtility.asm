[BITS 64]

SECTION .text

global kInPortByte, kOutPortByte, kInPortWord, kOutPortWord
global kLoadGDTR, kLoadTR, kLoadIDTR
global kEnableInterrupt, kDisableInterrupt, kReadRFLAGS
global kReadTSC
global kSwitchContext, kHlt, kTestAndSet, kPause
global kInitializeFPU, kSaveFPUContext, kLoadFPUContext, kSetTS, kClearTS
global kEnableGlobalLocalAPIC

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

.LoadContext:
    mov rsp, rsi

    KLOADCONTEXT
    iretq

kHlt:
    hlt
    hlt
    ret

kTestAndSet:
    mov rax, rsi

    lock cmpxchg byte [ rdi ], dl
    je .SUCCESS

.NOTSAME:
    mov rax, 0x00
    ret

.SUCCESS:
    mov rax, 0x01
    ret

kInitializeFPU:
    finit
    ret

kSaveFPUContext:
    fxsave [ rdi ]
    ret

kLoadFPUContext:
    fxrstor [ rdi ]
    ret

kSetTS:
    push rax

    mov rax, cr0
    or rax, 0x08 ; TS 비트(7)을 1로 설정
    mov cr0, rax

    pop rax
    ret

kClearTS:
    clts
    ret

kInPortWord:
    push rdx

    mov rdx, rdi
    mov rax, 0
    in ax, dx

    pop rdx
    ret

kOutPortWord:
    push rdx
    push rax

    mov rdx, rdi
    mov rax, rsi
    out dx, ax

    pop rax
    pop rdx
    ret

kEnableGlobalLocalAPIC:
    push rax
    push rcx
    push rdx

    ; IA32_APIC_BASE MSR은 레지스터 어드레스 27에 위치
    ; MSR의 상위 32비트와 하위 32비트는 각각 EDX, EAX 레지스터 사용
    mov rcx, 27
    rdmsr

    ; APIC 전역 활성/비활성 필드는 비트 11에 위치
    ; 하위 32비트를 담당하는 EAX 레지스터의 비트 11을 1로 설정 후
    ; MSR 레지스터에 값을 덮어씀
    or rax, 0x0800
    wrmsr

    pop rdx
    pop rcx
    pop rax
    ret

kPause:
    pause
    ret