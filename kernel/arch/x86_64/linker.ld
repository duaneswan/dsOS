/* Linker script for the dsOS kernel */
OUTPUT_FORMAT("elf64-x86-64")
ENTRY(_start)

KERNEL_VIRT_BASE = 0xFFFFFFFF80000000;
KERNEL_PHYS_BASE = 0x100000; /* 1MB mark */

SECTIONS
{
    /* Higher half kernel at 0xFFFFFFFF80000000 */
    . = KERNEL_VIRT_BASE + KERNEL_PHYS_BASE;
    
    .text ALIGN(4K) : AT(ADDR(.text) - KERNEL_VIRT_BASE)
    {
        KEEP(*(.multiboot)) /* Multiboot header */
        *(.text)
        *(.text.*)
    }
    
    .rodata ALIGN(4K) : AT(ADDR(.rodata) - KERNEL_VIRT_BASE)
    {
        *(.rodata)
        *(.rodata.*)
    }
    
    .data ALIGN(4K) : AT(ADDR(.data) - KERNEL_VIRT_BASE)
    {
        *(.data)
        *(.data.*)
    }
    
    .bss ALIGN(4K) : AT(ADDR(.bss) - KERNEL_VIRT_BASE)
    {
        *(COMMON)
        *(.bss)
        *(.bss.*)
    }
    
    /* Stack setup */
    .bootstrap_stack ALIGN(4K) : AT(ADDR(.bootstrap_stack) - KERNEL_VIRT_BASE)
    {
        /* Allocate 16K for bootstrap stack */
        . += 16K;
    }
    
    _kernel_start = KERNEL_VIRT_BASE + KERNEL_PHYS_BASE;
    _kernel_end = .;
    _kernel_size = _kernel_end - _kernel_start;
    
    /DISCARD/ :
    {
        *(.comment)
        *(.eh_frame)
        *(.note.*)
    }
}
