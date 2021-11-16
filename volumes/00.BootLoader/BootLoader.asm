[ORG 0x00]
[BITS 16]

SECTION .text

jmp 0x07C0:START

TOTALSECTORCOUNT: dw 0x02

KERNEL32SECTORCOUNT: dw 0x02
BOOTSTRAPPROCESSOR: db 0x01

START:
    mov ax, 0x07C0
    mov ds, ax
    mov ax, 0xB800
    mov es, ax

    mov ax, 0x0000
    mov ss, ax
    mov sp, 0xFFFE
    mov bp, 0xFFFE

    mov si, 0

.SCREENCLEARLOOP:
    mov byte [ es: si ], 0
    mov byte [ es: si + 1 ], 0x06

    add si, 2

    cmp si, 80 * 25 * 2
    jl .SCREENCLEARLOOP

    push MESSAGE1
    push 0
    push 0
    call PRINTMESSAGE
    add sp, 6

    push IMAGELOADINGMESSAGE
    push 1
    push 0
    call PRINTMESSAGE
    add sp, 6

RESETDISK:
    mov ax, 0
    mov dl, 0
    int 0x13

    jc HANDLEDISKERROR

    mov si, 0x1000
    mov es, si
    mov bx, 0x0000

    mov di, word [ TOTALSECTORCOUNT ]

READDDATA:
    cmp di, 0
    je READEND
    sub di, 0x1

    mov ah, 0x02
    mov al, 0x1
    mov ch, byte [ TRACKNUMBER ]
    mov cl, byte [ SECTORNUMBER ]
    mov dh, byte [ HEADNUMBER ]
    mov dl, 0x00
    int 0x13
    jc HANDLEDISKERROR

    add si, 0x0020

    mov es, si

    mov al, byte [ SECTORNUMBER ]
    add al, 0x01
    mov byte[ SECTORNUMBER ], al
    cmp al, 37
    jl READDDATA

    xor byte [ HEADNUMBER ], 0x01
    mov byte [ SECTORNUMBER ], 0x01

    cmp byte [ HEADNUMBER ], 0x00
    jne READDDATA

    add byte [ TRACKNUMBER ], 0x01
    jmp READDDATA

READEND:
    push LOADINGCOMPLETEMESSAGE
    push 1
    push 20
    call PRINTMESSAGE
    add sp, 6

    jmp 0x1000:0x0000

HANDLEDISKERROR:
    push DISKERRORMESSAGE
    push 1
    push 20
    call PRINTMESSAGE

    jmp $

PRINTMESSAGE:
    push bp
    mov bp, sp

    push es
    push si
    push di
    push ax
    push cx
    push dx

    mov ax, 0xB800
    mov es, ax

    mov ax, word [ bp + 6 ]
    mov si, 160
    mul si
    add di, ax

    mov ax, word [ bp + 4 ]
    mov si, 2
    mul si
    add di, ax

    mov si, word [ bp + 8 ]

.MESSAGELOOP:
    mov cl, byte [ si ]

    cmp cl, 0
    je .MESSAGEEND

    mov byte [ es: di ], cl

    add si, 1
    add di, 2

    jmp .MESSAGELOOP

.MESSAGEEND:
    pop dx
    pop cx
    pop ax
    pop di
    pop si
    pop es
    pop bp
    ret

MESSAGE1:
    db "It's now safe to turn off your computer XD.", 0

DISKERRORMESSAGE:
    db 'DISK ERROR', 0
IMAGELOADINGMESSAGE:
    db 'OS Image Loading...', 0
LOADINGCOMPLETEMESSAGE:
    db 'Complete!', 0

SECTORNUMBER:
    db 0x02
HEADNUMBER:
    db 0x00
TRACKNUMBER:
    db 0x00

times 510 - ( $ - $$ ) db 0x00

db 0x55
db 0xAA