; GDT assembly implementation

[BITS 64]
[GLOBAL gdt_flush]
[GLOBAL tss_flush]

; Load the GDT
gdt_flush:
    lgdt    [rdi]          ; Load GDT with address in RDI
    
    ; Reload segment registers
    mov     ax, 0x10       ; Data segment selector (0x10 = index 2)
    mov     ds, ax
    mov     es, ax
    mov     fs, ax
    mov     gs, ax
    mov     ss, ax
    
    ; Long jump to reload CS register
    push    0x08           ; Code segment selector (0x08 = index 1)
    lea     rax, [rel .reload_cs]
    push    rax
    retfq                  ; Far return to reload CS
    
.reload_cs:
    ret

; Load the TSS
tss_flush:
    mov     ax, di         ; Selector in DI
    ltr     ax             ; Load task register
    ret
