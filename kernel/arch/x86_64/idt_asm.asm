; IDT and interrupt handling for x86_64
; This file contains the assembly functions for loading the IDT
; and handling interrupts

section .text
global idt_flush     ; Export the idt_flush function

; Interrupt handler functions
%macro ISR_NOERRCODE 1
global isr%1
isr%1:
    push 0           ; Push a dummy error code for consistency
    push %1          ; Push the interrupt number
    jmp isr_common   ; Jump to the common handler
%endmacro

%macro ISR_ERRCODE 1
global isr%1
isr%1:
    ; Error code is already pushed by the CPU
    push %1          ; Push the interrupt number
    jmp isr_common   ; Jump to the common handler
%endmacro

%macro IRQ 2
global irq%1
irq%1:
    push 0           ; Push a dummy error code
    push %2          ; Push the interrupt number
    jmp irq_common   ; Jump to the common handler
%endmacro

; Export ISR handlers for CPU exceptions
ISR_NOERRCODE 0      ; #DE: Divide Error
ISR_NOERRCODE 1      ; #DB: Debug Exception
ISR_NOERRCODE 2      ; NMI: Non-Maskable Interrupt
ISR_NOERRCODE 3      ; #BP: Breakpoint
ISR_NOERRCODE 4      ; #OF: Overflow
ISR_NOERRCODE 5      ; #BR: BOUND Range Exceeded
ISR_NOERRCODE 6      ; #UD: Invalid Opcode
ISR_NOERRCODE 7      ; #NM: Device Not Available
ISR_ERRCODE 8        ; #DF: Double Fault
ISR_NOERRCODE 9      ; Coprocessor Segment Overrun (obsolete)
ISR_ERRCODE 10       ; #TS: Invalid TSS
ISR_ERRCODE 11       ; #NP: Segment Not Present
ISR_ERRCODE 12       ; #SS: Stack-Segment Fault
ISR_ERRCODE 13       ; #GP: General Protection Fault
ISR_ERRCODE 14       ; #PF: Page Fault
ISR_NOERRCODE 15     ; Reserved
ISR_NOERRCODE 16     ; #MF: x87 FPU Error
ISR_ERRCODE 17       ; #AC: Alignment Check
ISR_NOERRCODE 18     ; #MC: Machine Check
ISR_NOERRCODE 19     ; #XM: SIMD Floating-Point Exception
ISR_NOERRCODE 20     ; #VE: Virtualization Exception
ISR_ERRCODE 21       ; #CP: Control Protection Exception
ISR_NOERRCODE 22     ; Reserved
ISR_NOERRCODE 23     ; Reserved
ISR_NOERRCODE 24     ; Reserved
ISR_NOERRCODE 25     ; Reserved
ISR_NOERRCODE 26     ; Reserved
ISR_NOERRCODE 27     ; Reserved
ISR_NOERRCODE 28     ; Reserved
ISR_NOERRCODE 29     ; Reserved
ISR_NOERRCODE 30     ; Reserved
ISR_NOERRCODE 31     ; Reserved

; Export IRQ handlers
IRQ 0, 32            ; Timer
IRQ 1, 33            ; Keyboard
IRQ 2, 34            ; Cascade for 8259A Slave controller
IRQ 3, 35            ; COM2
IRQ 4, 36            ; COM1
IRQ 5, 37            ; LPT2
IRQ 6, 38            ; Floppy Disk
IRQ 7, 39            ; LPT1 / spurious
IRQ 8, 40            ; CMOS real-time clock
IRQ 9, 41            ; Free / SCSI / NIC
IRQ 10, 42           ; Free / SCSI / NIC
IRQ 11, 43           ; Free / SCSI / NIC
IRQ 12, 44           ; PS2 Mouse
IRQ 13, 45           ; FPU / Coprocessor
IRQ 14, 46           ; Primary ATA Hard Disk
IRQ 15, 47           ; Secondary ATA Hard Disk

extern handle_interrupt       ; External C function to handle interrupts
extern pic_send_eoi          ; External C function to send EOI to PIC

; Common ISR handler
isr_common:
    ; Save all registers
    push rax
    push rbx
    push rcx
    push rdx
    push rsi
    push rdi
    push rbp
    push r8
    push r9
    push r10
    push r11
    push r12
    push r13
    push r14
    push r15

    ; Call the C handler
    ; RDI = int_no, RSI = err_code, RDX = RIP
    mov rdi, [rsp + 120]    ; Get the interrupt number
    mov rsi, [rsp + 128]    ; Get the error code
    mov rdx, [rsp + 136]    ; Get the RIP where the interrupt occurred
    call handle_interrupt

    ; Restore all registers
    pop r15
    pop r14
    pop r13
    pop r12
    pop r11
    pop r10
    pop r9
    pop r8
    pop rbp
    pop rdi
    pop rsi
    pop rdx
    pop rcx
    pop rbx
    pop rax

    ; Clean up the stack
    add rsp, 16              ; Remove error code and interrupt number

    ; Return from interrupt
    iretq

; Common IRQ handler
irq_common:
    ; Save all registers
    push rax
    push rbx
    push rcx
    push rdx
    push rsi
    push rdi
    push rbp
    push r8
    push r9
    push r10
    push r11
    push r12
    push r13
    push r14
    push r15

    ; Call the C handler
    ; RDI = int_no, RSI = err_code, RDX = RIP
    mov rdi, [rsp + 120]    ; Get the interrupt number
    mov rsi, [rsp + 128]    ; Get the error code
    mov rdx, [rsp + 136]    ; Get the RIP where the interrupt occurred
    call handle_interrupt

    ; Send EOI to PIC
    mov rdi, [rsp + 120]    ; Get the interrupt number
    call pic_send_eoi

    ; Restore all registers
    pop r15
    pop r14
    pop r13
    pop r12
    pop r11
    pop r10
    pop r9
    pop r8
    pop rbp
    pop rdi
    pop rsi
    pop rdx
    pop rcx
    pop rbx
    pop rax

    ; Clean up the stack
    add rsp, 16              ; Remove error code and interrupt number

    ; Return from interrupt
    iretq

; idt_flush - Load a new IDT
; Input: RDI = pointer to IDT descriptor
idt_flush:
    lidt [rdi]       ; Load the new IDT
    ret
