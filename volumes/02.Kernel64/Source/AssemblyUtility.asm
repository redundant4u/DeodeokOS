[BITS 64]

global kInPortByte, kOutPortByte, kLoadGDTR, kLoadTR, kLoadIDTR

SECTION .text

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