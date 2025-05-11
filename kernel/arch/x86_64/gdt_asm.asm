; GDT and TSS loading functions for x86_64
; These functions are used to load the GDT and TSS

section .text
global gdt_flush     ; Export the gdt_flush function
global tss_flush     ; Export the tss_flush function

; gdt_flush - Load a new GDT
; Input: RDI = pointer to GDT descriptor
gdt_flush:
    lgdt [rdi]       ; Load the new GDT

    ; Reload the segment registers
    ; We need to do a far jump to update CS
    push 0x08        ; 1st segment (kernel code)
    lea rax, [rel .reload_cs]
    push rax
    retfq            ; Far return to reload CS

.reload_cs:
    ; Reload data segment registers
    mov ax, 0x10     ; 2nd segment (kernel data)
    mov ds, ax
    mov es, ax
    mov fs, ax
    mov gs, ax
    mov ss, ax
    ret

; tss_flush - Load a TSS
; Input: RDI = TSS segment selector
tss_flush:
    mov ax, di       ; Move segment selector to AX
    ltr ax           ; Load task register
    ret
