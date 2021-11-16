[BITS 64]

SECTION .text

extern Main
extern g_qwAPICIDAddress, g_iWakeUpApplicationProcessorCount

START:
    mov ax, 0x10
    mov ds, ax
    mov es, ax
    mov fs, ax
    mov gs, ax

    mov ss, ax
    mov rsp, 0x6FFFF8
    mov rbp, 0x6FFFF8

    ; Bootstrap Processor이면 Main 함수로 이동
    cmp byte [ 0x7C09 ], 0x01
    je .BOOTSTRAPPROCESSORSTARTPOINT

    ; Application Processor만 실행하는 영역
    mov rax, 0
    mov rbx, qword [ g_qwAPICIDAddress ]
    mov eax, dword [ rbx ]
    shr rax, 24

    mov rbx, 0x10000
    mul rbx

    sub rsp, rax
    sub rbp, rax

    lock inc dword [ g_iWakeUpApplicationProcessorCount ]

.BOOTSTRAPPROCESSORSTARTPOINT:
    call Main

    jmp $