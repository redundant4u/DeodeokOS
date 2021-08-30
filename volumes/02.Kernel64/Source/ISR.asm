[BITS 64]

SECTION .text

extern kCommonExceptionHandler, kCommonInterruptHandler, kKeyboardHandler
extern kTimerHandler, kDeviceNotAvailableHandler, kHDDHandler

; 예외 처리를 위한 ISR
global kISRDivideError, kISRDebug, kISRNMI, kISRBreakPoint, kISROverflow
global kISRBoundRangeExceeded, kISRInvalidOpcode, kISRDeviceNotAvailable, kISRDoubleFault
global kISRCoprocessorSegmentOverrun, kISRInvalidTSS, kISRSegmentNotPresent
global kISRStackSegmentFault, kISRGeneralProtection, kISRPageFault, kISR15
global kISRFPUError, kISRAlignmentCheck, kISRMachineCheck, kISRSIMDError, kISRETCException

; 인터럽트 처리를 위한 ISR
global kISRTimer, kISRKeyboard, kISRSlavePIC, kISRSerial2, kISRSerial1, kISRParallel2
global kISRFloppy, kISRParallel1, kISRRTC, kISRReserved, kISRNotUsed1, kISRNotUsed2
global kISRMouse, kISRCoprocessor, kISRHDD1, kISRHDD2, kISRETCInterrupt

%macro KSAVECONTEXT 0
    push rbp
    mov rbp, rsp
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

    mov ax, ds
    push rax
    mov ax, es
    push rax
    push fs
    push gs

    mov ax, 0x10
    mov ds, ax
    mov es, ax
    mov gs, ax
    mov fs, ax
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

; 예외 핸들러
kISRDivideError:
    KSAVECONTEXT

    mov rdi, 0
    call kCommonExceptionHandler

    KLOADCONTEXT
    iretq

kISRDebug:
    KSAVECONTEXT

    mov rdi, 1
    call kCommonExceptionHandler

    KLOADCONTEXT
    iretq

kISRNMI:
    KSAVECONTEXT

    mov rdi, 2
    call kCommonExceptionHandler

    KLOADCONTEXT
    iretq

kISRBreakPoint:
    KSAVECONTEXT

    mov rdi, 3
    call kCommonExceptionHandler

    KLOADCONTEXT
    iretq

kISROverflow:
    KSAVECONTEXT

    mov rdi, 4
    call kCommonExceptionHandler

    KLOADCONTEXT
    iretq

kISRBoundRangeExceeded:
    KSAVECONTEXT

    mov rdi, 5
    call kCommonExceptionHandler

    KLOADCONTEXT
    iretq

kISRInvalidOpcode:
    KSAVECONTEXT

    mov rdi, 6
    call kCommonExceptionHandler

    KLOADCONTEXT
    iretq

kISRDeviceNotAvailable:
    KSAVECONTEXT

    mov rdi, 7
    call kDeviceNotAvailableHandler

    KLOADCONTEXT
    iretq

kISRDoubleFault:
    KSAVECONTEXT

    mov rdi, 8
    mov rsi, qword [ rbp + 8 ]
    call kCommonExceptionHandler

    KLOADCONTEXT
    add rsp, 8
    iretq

kISRCoprocessorSegmentOverrun:
    KSAVECONTEXT

    mov rdi, 9
    call kCommonExceptionHandler

    KLOADCONTEXT
    iretq

kISRInvalidTSS:
    KSAVECONTEXT

    mov rdi, 10
    mov rsi, qword [ rbp + 8 ]
    call kCommonExceptionHandler

    KLOADCONTEXT
    add rsp, 8
    iretq

kISRSegmentNotPresent:
    KSAVECONTEXT

    mov rdi, 11
    mov rsi, qword [ rbp + 8 ]
    call kCommonExceptionHandler

    KLOADCONTEXT
    add rsp, 8
    iretq

kISRStackSegmentFault:
    KSAVECONTEXT

    mov rdi, 12
    mov rsi, qword[ rbp + 8 ]
    call kCommonExceptionHandler

    KLOADCONTEXT
    add rsp, 8
    iretq

kISRGeneralProtection:
    KSAVECONTEXT

    mov rdi, 13
    mov rsi, qword[ rbp + 8 ]
    call kCommonExceptionHandler

    KLOADCONTEXT
    add rsp, 8
    iretq

kISRPageFault:
    KSAVECONTEXT

    mov rdi, 14
    mov rsi, qword[ rbp + 8 ]
    call kCommonExceptionHandler

    KLOADCONTEXT
    add rsp, 8
    iretq

kISR15:
    KSAVECONTEXT

    mov rdi, 15
    call kCommonExceptionHandler

    KLOADCONTEXT
    iretq

kISRFPUError:
    KSAVECONTEXT

    mov rdi, 16
    call kCommonExceptionHandler

    KLOADCONTEXT
    iretq

kISRAlignmentCheck:
    KSAVECONTEXT

    mov rdi, 17
    mov rsi, qword [ rbp + 8 ]
    call kCommonExceptionHandler

    KLOADCONTEXT
    add rsp, 8
    iretq

kISRMachineCheck:
    KSAVECONTEXT

    mov rdi, 18
    call kCommonExceptionHandler

    KLOADCONTEXT
    iretq

kISRSIMDError:
    KSAVECONTEXT

    mov rdi, 19
    call kCommonExceptionHandler

    KLOADCONTEXT
    iretq

kISRETCException:
    KSAVECONTEXT

    mov rdi, 20
    call kCommonExceptionHandler

    KLOADCONTEXT
    iretq

; 인터럽트 핸들러
kISRTimer:
    KSAVECONTEXT

    mov rdi, 32
    call kTimerHandler

    KLOADCONTEXT
    iretq

kISRKeyboard:
    KSAVECONTEXT

    mov rdi, 33
    call kKeyboardHandler

    KLOADCONTEXT
    iretq

kISRSlavePIC:
    KSAVECONTEXT

    mov rdi, 34
    call kCommonInterruptHandler

    KLOADCONTEXT
    iretq

kISRSerial2:
    KSAVECONTEXT

    mov rdi, 35
    call kCommonInterruptHandler

    KLOADCONTEXT
    iretq

kISRSerial1:
    KSAVECONTEXT

    mov rdi, 36
    call kCommonInterruptHandler

    KLOADCONTEXT
    iretq

kISRParallel2:
    KSAVECONTEXT

    mov rdi, 37
    call kCommonInterruptHandler

    KLOADCONTEXT
    iretq

kISRFloppy:
    KSAVECONTEXT

    mov rdi, 38
    call kCommonInterruptHandler

    KLOADCONTEXT
    iretq

kISRParallel1:
    KSAVECONTEXT

    mov rdi, 39
    call kCommonInterruptHandler

    KLOADCONTEXT
    iretq

kISRRTC:
    KSAVECONTEXT

    mov rdi, 40
    call kCommonInterruptHandler

    KLOADCONTEXT
    iretq

kISRReserved:
    KSAVECONTEXT

    mov rdi, 41
    call kCommonInterruptHandler

    KLOADCONTEXT
    iretq

kISRNotUsed1:
    KSAVECONTEXT

    mov rdi, 42
    call kCommonInterruptHandler

    KLOADCONTEXT
    iretq

kISRNotUsed2:
    KSAVECONTEXT

    mov rdi, 43
    call kCommonInterruptHandler

    KLOADCONTEXT
    iretq

kISRMouse:
    KSAVECONTEXT

    mov rdi, 44
    call kCommonInterruptHandler

    KLOADCONTEXT
    iretq

kISRCoprocessor:
    KSAVECONTEXT

    mov rdi, 45
    call kCommonInterruptHandler

    KLOADCONTEXT
    iretq

kISRHDD1:
    KSAVECONTEXT

    mov rdi, 46
    call kHDDHandler

    KLOADCONTEXT
    iretq

kISRHDD2:
    KSAVECONTEXT

    mov rdi, 47
    call kHDDHandler

    KLOADCONTEXT
    iretq

kISRETCInterrupt:
    KSAVECONTEXT

    mov rdi, 48
    call kCommonInterruptHandler

    KLOADCONTEXT
    iretq