; IDT assembly implementation

[BITS 64]
[GLOBAL idt_flush]
[EXTERN interrupt_handler]
[EXTERN exception_handler]

; Common ISR stub that calls C handlers
%macro ISR_NOERRCODE 1
    [GLOBAL isr%1]
    isr%1:
        cli                 ; Disable interrupts
        push    0           ; Push dummy error code
        push    %1          ; Push interrupt number
        jmp     isr_common  ; Jump to common handler
%endmacro

%macro ISR_ERRCODE 1
    [GLOBAL isr%1]
    isr%1:
        cli                 ; Disable interrupts
        push    %1          ; Push interrupt number
        jmp     isr_common  ; Jump to common handler
%endmacro

%macro IRQ 2
    [GLOBAL irq%1]
    irq%1:
        cli                 ; Disable interrupts
        push    0           ; Push dummy error code
        push    %2          ; Push interrupt number
        jmp     irq_common  ; Jump to common handler
%endmacro

; General purpose interrupt handler (for interrupts 48-255)
[GLOBAL int_dispatch]
int_dispatch:
    cli                     ; Disable interrupts
    push    rax             ; Save RAX
    mov     rax, [rsp+8]    ; Get interrupt number from stack
    push    0               ; Push dummy error code
    push    rax             ; Push interrupt number
    jmp     irq_common      ; Jump to common handler

; ISRs for exceptions (0-31)
ISR_NOERRCODE 0    ; Divide by zero
ISR_NOERRCODE 1    ; Debug
ISR_NOERRCODE 2    ; Non-maskable interrupt
ISR_NOERRCODE 3    ; Breakpoint
ISR_NOERRCODE 4    ; Overflow
ISR_NOERRCODE 5    ; Bound range exceeded
ISR_NOERRCODE 6    ; Invalid opcode
ISR_NOERRCODE 7    ; Device not available
ISR_ERRCODE   8    ; Double fault
ISR_NOERRCODE 9    ; Coprocessor segment overrun
ISR_ERRCODE   10   ; Invalid TSS
ISR_ERRCODE   11   ; Segment not present
ISR_ERRCODE   12   ; Stack-segment fault
ISR_ERRCODE   13   ; General protection fault
ISR_ERRCODE   14   ; Page fault
ISR_NOERRCODE 15   ; Reserved
ISR_NOERRCODE 16   ; x87 floating-point exception
ISR_ERRCODE   17   ; Alignment check
ISR_NOERRCODE 18   ; Machine check
ISR_NOERRCODE 19   ; SIMD floating-point exception
ISR_NOERRCODE 20   ; Virtualization exception
ISR_ERRCODE   21   ; Control protection exception
ISR_NOERRCODE 22   ; Reserved
ISR_NOERRCODE 23   ; Reserved
ISR_NOERRCODE 24   ; Reserved
ISR_NOERRCODE 25   ; Reserved
ISR_NOERRCODE 26   ; Reserved
ISR_NOERRCODE 27   ; Reserved
ISR_NOERRCODE 28   ; Hypervisor injection exception
ISR_NOERRCODE 29   ; VMM communication exception
ISR_ERRCODE   30   ; Security exception
ISR_NOERRCODE 31   ; Reserved

; ISRs for IRQs (32-47)
IRQ 0, 32    ; Programmable Interrupt Timer
IRQ 1, 33    ; Keyboard
IRQ 2, 34    ; Cascade for 8259A Slave controller
IRQ 3, 35    ; COM2
IRQ 4, 36    ; COM1
IRQ 5, 37    ; LPT2
IRQ 6, 38    ; Floppy Disk
IRQ 7, 39    ; LPT1 / Unreliable "spurious" interrupt
IRQ 8, 40    ; CMOS Real-time clock
IRQ 9, 41    ; Free for peripherals / legacy SCSI / NIC
IRQ 10, 42   ; Free for peripherals / SCSI / NIC
IRQ 11, 43   ; Free for peripherals / SCSI / NIC
IRQ 12, 44   ; PS2 Mouse
IRQ 13, 45   ; FPU / Coprocessor / Inter-processor
IRQ 14, 46   ; Primary ATA channel
IRQ 15, 47   ; Secondary ATA channel

; Common ISR handler for exceptions
isr_common:
    ; Save all registers
    push    rax
    push    rbx
    push    rcx
    push    rdx
    push    rsi
    push    rdi
    push    rbp
    push    r8
    push    r9
    push    r10
    push    r11
    push    r12
    push    r13
    push    r14
    push    r15

    ; Save segment registers
    mov     rax, ds
    push    rax
    mov     rax, es
    push    rax
    mov     rax, fs
    push    rax
    mov     rax, gs
    push    rax

    ; Load kernel data segment
    mov     ax, 0x10
    mov     ds, ax
    mov     es, ax
    mov     fs, ax
    mov     gs, ax

    ; Call C exception handler
    mov     rdi, [rsp + 152]   ; RIP
    mov     rsi, [rsp + 160]   ; CS
    mov     rdx, [rsp + 168]   ; RFLAGS
    mov     rcx, [rsp + 176]   ; RSP
    mov     r8, [rsp + 184]    ; SS
    mov     r9, [rsp + 144]    ; int_no
    mov     rax, [rsp + 136]   ; error_code
    push    rax                ; error_code as additional parameter
    call    exception_handler
    add     rsp, 8             ; Clean up error_code from stack

    ; Restore segment registers
    pop     rax
    mov     gs, ax
    pop     rax
    mov     fs, ax
    pop     rax
    mov     es, ax
    pop     rax
    mov     ds, ax

    ; Restore registers
    pop     r15
    pop     r14
    pop     r13
    pop     r12
    pop     r11
    pop     r10
    pop     r9
    pop     r8
    pop     rbp
    pop     rdi
    pop     rsi
    pop     rdx
    pop     rcx
    pop     rbx
    pop     rax

    ; Clean up error code and interrupt number
    add     rsp, 16

    ; Return from interrupt
    iretq

; Common IRQ handler for hardware interrupts
irq_common:
    ; Save all registers
    push    rax
    push    rbx
    push    rcx
    push    rdx
    push    rsi
    push    rdi
    push    rbp
    push    r8
    push    r9
    push    r10
    push    r11
    push    r12
    push    r13
    push    r14
    push    r15

    ; Save segment registers
    mov     rax, ds
    push    rax
    mov     rax, es
    push    rax
    mov     rax, fs
    push    rax
    mov     rax, gs
    push    rax

    ; Load kernel data segment
    mov     ax, 0x10
    mov     ds, ax
    mov     es, ax
    mov     fs, ax
    mov     gs, ax

    ; Call C interrupt handler with interrupt number
    mov     rdi, [rsp + 144]   ; int_no
    call    interrupt_handler

    ; Restore segment registers
    pop     rax
    mov     gs, ax
    pop     rax
    mov     fs, ax
    pop     rax
    mov     es, ax
    pop     rax
    mov     ds, ax

    ; Restore registers
    pop     r15
    pop     r14
    pop     r13
    pop     r12
    pop     r11
    pop     r10
    pop     r9
    pop     r8
    pop     rbp
    pop     rdi
    pop     rsi
    pop     rdx
    pop     rcx
    pop     rbx
    pop     rax

    ; Clean up error code and interrupt number
    add     rsp, 16

    ; Return from interrupt
    iretq

; Function to load the IDT
idt_flush:
    lidt    [rdi]      ; Load IDT with address in RDI
    ret
