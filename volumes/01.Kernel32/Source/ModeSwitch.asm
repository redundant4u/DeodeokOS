[BITS 32]

global kReadCPUID, kSwitchAndExecute64bitKernel

SECTION .text

kReadCPUID:
    push ebp
    mov ebp, esp
    push eax
    push ebx
    push ecx
    push edx
    push esi

    mov eax, dword [ ebp + 8 ]
    cpuid

    mov esi, dword [ ebp + 12 ]
    mov dword [ esi ], eax

    mov esi, dword [ ebp + 16 ]
    mov dword [ esi ], ebx

    mov esi, dword [ ebp + 20 ]
    mov dword [ esi ], ecx

    mov esi, dword [ ebp + 24 ]
    mov dword [ esi ], edx

    pop esi
    pop edx
    pop ecx
    pop ebx
    pop eax
    pop ebp
    ret

kSwitchAndExecute64bitKernel:
    mov eax, cr4
    or eax, 0x620
    mov cr4, eax

    mov eax, 0x100000 ;c PML4 테이블
    mov cr3, eax

    mov ecx, 0xC0000080 ; IA32_EFER MSR 레지스터
    rdmsr

    or eax, 0x0100

    wrmsr

    mov eax, cr0
    or eax, 0xE000000E
    xor eax, 0x60000004
    mov cr0, eax

    jmp 0x08:0x200000

    jmp $