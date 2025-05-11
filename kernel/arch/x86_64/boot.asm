; dKernel x86_64 Entry Point
; This file contains the entry point for the 64-bit kernel
; It sets up the initial execution environment and calls the C kernel_main function

section .multiboot
align 8

; We're using Multiboot2 for compatibility
MULTIBOOT2_HEADER_MAGIC equ 0xe85250d6
MULTIBOOT_ARCHITECTURE_I386 equ 0
MULTIBOOT_HEADER_LENGTH equ multiboot_header_end - multiboot_header
CHECKSUM equ -(MULTIBOOT2_HEADER_MAGIC + MULTIBOOT_ARCHITECTURE_I386 + MULTIBOOT_HEADER_LENGTH)

multiboot_header:
    dd MULTIBOOT2_HEADER_MAGIC
    dd MULTIBOOT_ARCHITECTURE_I386
    dd MULTIBOOT_HEADER_LENGTH
    dd CHECKSUM
    
    ; End tags
    dw 0    ; type
    dw 0    ; flags
    dd 8    ; size
multiboot_header_end:

; Constants
KERNEL_VIRTUAL_BASE equ 0xFFFFFFFF80000000
PAGE_PRESENT    equ 1 << 0
PAGE_WRITE      equ 1 << 1
PAGE_SIZE_BIT   equ 1 << 7
PML4_INDEX_SHIFT equ 39
PAGE_SIZE equ 0x1000

section .data
align 4096
; Page tables for identity mapping and higher-half kernel
global boot_page_directory_ptr_tab
global boot_page_directory_tab
global boot_page_table

boot_page_directory_ptr_tab:    ; PML4
    times 512 dq 0

boot_page_directory_tab:        ; PDPT
    times 512 dq 0

boot_page_table:                ; PD
    times 512 dq 0

; GDT for 64-bit mode
gdt64:
    dq 0                        ; Null descriptor
.code_descriptor:
    dq (1 << 43) | (1 << 44) | (1 << 47) | (1 << 53) ; Code segment
.data_descriptor:
    dq (1 << 44) | (1 << 47)    ; Data segment
.pointer:
    dw $ - gdt64 - 1            ; Size
    dq gdt64                    ; Address

section .bss
align 16
; Reserve space for bootstrap stack
global boot_stack_bottom
global boot_stack_top

boot_stack_bottom:
    resb 16384                  ; 16 KiB
boot_stack_top:

; Storage for multiboot info pointer
global multiboot_info_ptr
multiboot_info_ptr:
    resq 1

section .text
bits 32
global _start
_start:
    ; Save multiboot info pointer
    mov [multiboot_info_ptr - KERNEL_VIRTUAL_BASE], ebx
    
    ; Disable interrupts
    cli
    
    ; Set up initial identity paging to bridge from 32-bit to 64-bit
    ; PML4[0] -> PDPT[0]
    mov eax, boot_page_directory_tab - KERNEL_VIRTUAL_BASE
    or eax, PAGE_PRESENT | PAGE_WRITE
    mov [boot_page_directory_ptr_tab - KERNEL_VIRTUAL_BASE], eax
    
    ; PML4[511] -> PDPT[0] (for higher-half kernel mapping)
    mov eax, boot_page_directory_tab - KERNEL_VIRTUAL_BASE
    or eax, PAGE_PRESENT | PAGE_WRITE
    mov [boot_page_directory_ptr_tab - KERNEL_VIRTUAL_BASE + 511*8], eax
    
    ; PDPT[0] -> PD[0]
    mov eax, boot_page_table - KERNEL_VIRTUAL_BASE
    or eax, PAGE_PRESENT | PAGE_WRITE
    mov [boot_page_directory_tab - KERNEL_VIRTUAL_BASE], eax
    
    ; Map the first 2MB of memory (identity mapping)
    mov eax, PAGE_PRESENT | PAGE_WRITE | PAGE_SIZE_BIT  ; Present, writable, 2MB
    mov [boot_page_table - KERNEL_VIRTUAL_BASE], eax
    
    ; Set up other necessary mappings for kernel, stack, etc.
    ; For simplicity, we're mapping 4GB physical to both 0x0 (identity) and higher-half
    mov ecx, 512                ; Number of 2MB pages to map (covering 1GB)
    mov eax, 0 | PAGE_PRESENT | PAGE_WRITE | PAGE_SIZE_BIT
    mov edi, boot_page_table - KERNEL_VIRTUAL_BASE
.map_pd_identity:
    mov [edi], eax
    add eax, 0x200000           ; Next 2MB
    add edi, 8                  ; Next entry
    loop .map_pd_identity
    
    ; Enable PAE
    mov eax, cr4
    or eax, 1 << 5              ; Set PAE bit
    mov cr4, eax
    
    ; Load PML4 table
    mov eax, boot_page_directory_ptr_tab - KERNEL_VIRTUAL_BASE
    mov cr3, eax
    
    ; Set up for long mode
    mov ecx, 0xC0000080         ; EFER MSR
    rdmsr
    or eax, 1 << 8              ; Set LME bit
    wrmsr
    
    ; Enable paging and protected mode
    mov eax, cr0
    or eax, 1 << 31 | 1         ; Set PG and PE bits
    mov cr0, eax
    
    ; Load GDT for 64-bit mode
    lgdt [gdt64.pointer - KERNEL_VIRTUAL_BASE]
    
    ; Jump to 64-bit code segment
    jmp 0x08:long_mode_start - KERNEL_VIRTUAL_BASE

bits 64
long_mode_start:
    ; Update segment registers
    mov ax, 0x10        ; Data segment selector
    mov ds, ax
    mov es, ax
    mov fs, ax
    mov gs, ax
    mov ss, ax
    
    ; Switch to higher-half addresses for code
    mov rax, higher_half_start
    jmp rax

higher_half_start:
    ; Now we're running in the higher half
    ; Set up the stack
    mov rsp, boot_stack_top
    mov rbp, rsp
    
    ; Clear rflags
    push 0
    popf
    
    ; Call kernel main with multiboot info
    mov rdi, [multiboot_info_ptr]
    
    ; Call the C kernel_main function
    extern kernel_main
    call kernel_main
    
    ; If we return from kernel_main, halt the CPU
halt:
    cli                 ; Disable interrupts
    hlt                 ; Halt the CPU
    jmp halt            ; If that didn't work, loop
