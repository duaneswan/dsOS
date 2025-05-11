/**
 * @file gdt.c
 * @brief Global Descriptor Table (GDT) implementation
 */

#include <kernel.h>
#include <stdint.h>

// GDT entry structure
struct gdt_entry {
    uint16_t limit_low;
    uint16_t base_low;
    uint8_t base_middle;
    uint8_t access;
    uint8_t granularity;
    uint8_t base_high;
} __attribute__((packed));

// GDT pointer structure
struct gdt_ptr {
    uint16_t limit;
    uint64_t base;
} __attribute__((packed));

// TSS (Task State Segment) structure
struct tss_entry {
    uint32_t reserved1;
    uint64_t rsp0;
    uint64_t rsp1;
    uint64_t rsp2;
    uint64_t reserved2;
    uint64_t ist1;
    uint64_t ist2;
    uint64_t ist3;
    uint64_t ist4;
    uint64_t ist5;
    uint64_t ist6;
    uint64_t ist7;
    uint64_t reserved3;
    uint16_t reserved4;
    uint16_t iomap_base;
} __attribute__((packed));

// System segment descriptor structure (for TSS and LDT)
struct system_segment_descriptor {
    uint16_t limit_low;
    uint16_t base_low;
    uint8_t base_middle_low;
    uint8_t access;
    uint8_t granularity;
    uint8_t base_middle_high;
    uint32_t base_high;
    uint32_t reserved;
} __attribute__((packed));

// GDT entries
#define GDT_ENTRIES 7
static struct gdt_entry gdt[GDT_ENTRIES];
static struct system_segment_descriptor gdt_sys[1]; // For TSS
static struct gdt_ptr gdt_pointer;
static struct tss_entry tss;

// External assembly function to load GDT
extern void gdt_flush(uint64_t);
extern void tss_flush(uint16_t);

/**
 * @brief Set a GDT entry
 * 
 * @param num Entry number
 * @param base Base address
 * @param limit Limit
 * @param access Access byte
 * @param gran Granularity byte
 */
static void gdt_set_gate(int num, uint32_t base, uint32_t limit, uint8_t access, uint8_t gran) {
    gdt[num].base_low = (base & 0xFFFF);
    gdt[num].base_middle = (base >> 16) & 0xFF;
    gdt[num].base_high = (base >> 24) & 0xFF;
    
    gdt[num].limit_low = (limit & 0xFFFF);
    gdt[num].granularity = ((limit >> 16) & 0x0F) | (gran & 0xF0);
    
    gdt[num].access = access;
}

/**
 * @brief Set a system segment descriptor (TSS)
 * 
 * @param num Entry number
 * @param base Base address
 * @param limit Limit
 * @param access Access byte
 * @param gran Granularity byte
 */
static void gdt_set_sys(int num, uint64_t base, uint32_t limit, uint8_t access, uint8_t gran) {
    gdt_sys[num].base_low = (base & 0xFFFF);
    gdt_sys[num].base_middle_low = (base >> 16) & 0xFF;
    gdt_sys[num].base_middle_high = (base >> 24) & 0xFF;
    gdt_sys[num].base_high = (base >> 32) & 0xFFFFFFFF;
    
    gdt_sys[num].limit_low = (limit & 0xFFFF);
    gdt_sys[num].granularity = ((limit >> 16) & 0x0F) | (gran & 0xF0);
    
    gdt_sys[num].access = access;
    gdt_sys[num].reserved = 0;
}

/**
 * @brief Initialize the TSS
 * 
 * @param stack Kernel stack address
 */
static void tss_init(uint64_t stack) {
    // Clear the TSS
    for (int i = 0; i < sizeof(tss); i++) {
        ((uint8_t*)&tss)[i] = 0;
    }
    
    // Set the kernel stack
    tss.rsp0 = stack;
    tss.iomap_base = sizeof(tss);
    
    // Configure the TSS descriptor
    uint64_t tss_base = (uint64_t)&tss;
    uint32_t tss_limit = sizeof(tss) - 1;
    
    // Configure the system segment descriptor for TSS
    gdt_set_sys(0, tss_base, tss_limit, 0x89, 0x00);
}

/**
 * @brief Initialize the GDT
 */
void gdt_init(void) {
    // Set up GDT pointer
    gdt_pointer.limit = (sizeof(struct gdt_entry) * GDT_ENTRIES) + 
                         (sizeof(struct system_segment_descriptor) * 1) - 1;
    gdt_pointer.base = (uint64_t)&gdt;
    
    // Null descriptor
    gdt_set_gate(0, 0, 0, 0, 0);
    
    // Kernel code segment (ring 0)
    gdt_set_gate(1, 0, 0xFFFFF, 0x9A, 0xA0); // Code segment, executable, readable
    
    // Kernel data segment (ring 0)
    gdt_set_gate(2, 0, 0xFFFFF, 0x92, 0xA0); // Data segment, writable
    
    // User code segment (ring 3)
    gdt_set_gate(3, 0, 0xFFFFF, 0xFA, 0xA0); // Code segment, executable, readable
    
    // User data segment (ring 3)
    gdt_set_gate(4, 0, 0xFFFFF, 0xF2, 0xA0); // Data segment, writable
    
    // TSS (Task State Segment)
    gdt_set_gate(5, 0, 0, 0, 0); // Placeholder for low 64 bits
    gdt_set_gate(6, 0, 0, 0, 0); // Placeholder for high 64 bits
    
    // Initialize TSS
    extern uint8_t boot_stack_top[];
    tss_init((uint64_t)boot_stack_top);
    
    // Load the GDT
    gdt_flush((uint64_t)&gdt_pointer);
    
    // Load the TSS
    tss_flush(0x28); // 5th entry (5 * 8 bytes = 40 = 0x28)
    
    kprintf("GDT: Initialized with %d entries\n", GDT_ENTRIES);
}
