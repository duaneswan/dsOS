/**
 * @file gdt.c
 * @brief Global Descriptor Table (GDT) implementation
 */

#include "../../include/kernel.h"
#include <stdint.h>
#include <stddef.h>

// GDT entry structure
typedef struct {
    uint16_t limit_low;     // The lower 16 bits of the limit
    uint16_t base_low;      // The lower 16 bits of the base
    uint8_t base_middle;    // The next 8 bits of the base
    uint8_t access;         // Access flags, determine ring privilege
    uint8_t granularity;    // Granularity flags + limit bits 16-19
    uint8_t base_high;      // The last 8 bits of the base
} PACKED gdt_entry_t;

// GDT pointer structure
typedef struct {
    uint16_t limit;         // Upper 16 bits of all selector limits
    uint64_t base;          // Address of the first gdt_entry_t
} PACKED gdt_ptr_t;

// TSS structure
typedef struct {
    uint32_t reserved1;
    uint64_t rsp0;         // Stack pointer for ring 0
    uint64_t rsp1;         // Stack pointer for ring 1
    uint64_t rsp2;         // Stack pointer for ring 2
    uint64_t reserved2;
    uint64_t ist1;         // Interrupt stack table pointer 1
    uint64_t ist2;         // Interrupt stack table pointer 2
    uint64_t ist3;         // Interrupt stack table pointer 3
    uint64_t ist4;         // Interrupt stack table pointer 4
    uint64_t ist5;         // Interrupt stack table pointer 5
    uint64_t ist6;         // Interrupt stack table pointer 6
    uint64_t ist7;         // Interrupt stack table pointer 7
    uint64_t reserved3;
    uint16_t reserved4;
    uint16_t iomap_base;   // I/O map base address
} PACKED tss_entry_t;

// TSS Descriptor (64-bit)
typedef struct {
    uint16_t length;       // Length of the TSS
    uint16_t base_low;     // Base address (low 16 bits)
    uint8_t base_mid;      // Base address (middle 8 bits)
    uint8_t flags1;        // Type and privilege flags
    uint8_t flags2;        // Additional flags
    uint8_t base_high;     // Base address (high 8 bits)
    uint32_t base_upper;   // Base address (upper 32 bits)
    uint32_t reserved;     // Reserved, must be 0
} PACKED tss_descriptor_t;

// GDT segment descriptors
#define GDT_TYPE_CODE     0x9A  // Code segment (execute/read)
#define GDT_TYPE_DATA     0x92  // Data segment (read/write)
#define GDT_TYPE_TSS      0x89  // TSS segment

#define GDT_FLAG_LONG     0x20  // Long mode (64-bit)
#define GDT_FLAG_SIZE     0x40  // 32-bit protected mode
#define GDT_FLAG_GRAN     0x80  // 4KiB granularity

// Number of GDT entries
#define GDT_ENTRIES       6

// GDT table and pointer
static uint64_t gdt[GDT_ENTRIES];  // 64-bit GDT entries
static gdt_ptr_t gdt_ptr;

// TSS entry and kernel stacks
static tss_entry_t tss;
static uint8_t kernel_stack[KERNEL_STACK_SIZE] ALIGN(16);

// External assembly function
extern void gdt_flush(uint64_t gdt_ptr);
extern void tss_flush(uint16_t tss_selector);

/**
 * @brief Set a GDT entry
 * 
 * @param num Entry number
 * @param base Base address
 * @param limit Limit
 * @param access Access flags
 * @param gran Granularity flags
 */
static void gdt_set_gate(int num, uint64_t base, uint32_t limit, uint8_t access, uint8_t gran) {
    // Set the descriptor base address
    uint64_t descriptor = 0;
    
    // Create the low 32 bits of the descriptor
    descriptor |= limit & 0xFFFF;                      // Limit bits 0-15
    descriptor |= (base & 0xFFFF) << 16;               // Base bits 0-15
    descriptor |= (base >> 16 & 0xFF) << 32;           // Base bits 16-23
    descriptor |= (uint64_t)access << 40;              // Access byte
    descriptor |= ((limit >> 16) & 0xF) << 48;         // Limit bits 16-19
    descriptor |= (uint64_t)gran << 52;                // Granularity nybble
    descriptor |= (base >> 24 & 0xFF) << 56;           // Base bits 24-31
    
    // Assign the descriptor to the GDT
    gdt[num] = descriptor;
}

/**
 * @brief Set a system descriptor (TSS)
 * 
 * @param num Entry number
 * @param base Base address
 * @param limit Limit
 * @param access Access flags
 * @param gran Granularity flags
 */
static void gdt_set_system(int num, uint64_t base, uint32_t limit, uint8_t access, uint8_t gran) {
    // First 64 bits are similar to a normal descriptor
    gdt_set_gate(num, base, limit, access, gran);
    
    // Set upper 64 bits for system descriptor
    uint64_t upper = 0;
    upper |= (base >> 32) & 0xFFFFFFFF;  // Upper 32 bits of base
    
    // Assign the upper part to the next GDT entry
    gdt[num + 1] = upper;
}

/**
 * @brief Initialize the TSS
 */
static void tss_init(void) {
    uint64_t tss_base = (uint64_t)&tss;
    uint32_t tss_limit = sizeof(tss) - 1;
    
    // Clear the TSS
    memset(&tss, 0, sizeof(tss));
    
    // Set up the kernel stack pointer
    tss.rsp0 = (uint64_t)kernel_stack + KERNEL_STACK_SIZE;
    
    // Set the I/O map base address
    tss.iomap_base = sizeof(tss);
    
    // Set the TSS descriptor in the GDT
    gdt_set_system(5, tss_base, tss_limit, GDT_TYPE_TSS, 0);
}

/**
 * @brief Initialize the GDT
 */
void gdt_init(void) {
    // Set up the GDT pointer
    gdt_ptr.limit = (sizeof(uint64_t) * GDT_ENTRIES) - 1;
    gdt_ptr.base = (uint64_t)&gdt;
    
    // NULL descriptor (index 0)
    gdt_set_gate(0, 0, 0, 0, 0);
    
    // Kernel code segment (index 1)
    gdt_set_gate(1, 0, 0xFFFFF, GDT_TYPE_CODE, GDT_FLAG_LONG | GDT_FLAG_GRAN);
    
    // Kernel data segment (index 2)
    gdt_set_gate(2, 0, 0xFFFFF, GDT_TYPE_DATA, GDT_FLAG_GRAN);
    
    // User code segment (index 3)
    gdt_set_gate(3, 0, 0xFFFFF, GDT_TYPE_CODE | 0x60, GDT_FLAG_LONG | GDT_FLAG_GRAN);
    
    // User data segment (index 4)
    gdt_set_gate(4, 0, 0xFFFFF, GDT_TYPE_DATA | 0x60, GDT_FLAG_GRAN);
    
    // Initialize the TSS (index 5 & 6 - takes 2 slots)
    tss_init();
    
    // Load the GDT and reload segment registers
    gdt_flush((uint64_t)&gdt_ptr);
    
    // Load the TSS
    tss_flush(0x28);  // 0x28 = 5 * 8 (TSS is at index 5)
    
    kprintf("GDT: Initialized with %d entries\n", GDT_ENTRIES);
}

/**
 * @brief Set the kernel stack for the TSS
 * 
 * @param stack Stack pointer
 */
void gdt_set_kernel_stack(uint64_t stack) {
    tss.rsp0 = stack;
}
