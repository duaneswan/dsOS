/**
 * @file idt.c
 * @brief Interrupt Descriptor Table (IDT) implementation
 */

#include "../../include/kernel.h"
#include <stdint.h>

// IDT entry structure
struct idt_entry {
    uint16_t base_low;
    uint16_t selector;
    uint8_t ist;        // Interrupt Stack Table index
    uint8_t flags;
    uint16_t base_mid;
    uint32_t base_high;
    uint32_t reserved;
} __attribute__((packed));

// IDT pointer structure
struct idt_ptr {
    uint16_t limit;
    uint64_t base;
} __attribute__((packed));

// IDT entries (256 total)
#define IDT_ENTRIES 256
static struct idt_entry idt[IDT_ENTRIES];
static struct idt_ptr idt_pointer;

// External assembly functions to load IDT and handle IRQs
extern void idt_flush(uint64_t);
extern void isr0(void);
extern void isr1(void);
extern void isr2(void);
extern void isr3(void);
extern void isr4(void);
extern void isr5(void);
extern void isr6(void);
extern void isr7(void);
extern void isr8(void);
extern void isr9(void);
extern void isr10(void);
extern void isr11(void);
extern void isr12(void);
extern void isr13(void);
extern void isr14(void);
extern void isr15(void);
extern void isr16(void);
extern void isr17(void);
extern void isr18(void);
extern void isr19(void);
extern void isr20(void);
extern void isr21(void);
extern void isr22(void);
extern void isr23(void);
extern void isr24(void);
extern void isr25(void);
extern void isr26(void);
extern void isr27(void);
extern void isr28(void);
extern void isr29(void);
extern void isr30(void);
extern void isr31(void);

extern void irq0(void);
extern void irq1(void);
extern void irq2(void);
extern void irq3(void);
extern void irq4(void);
extern void irq5(void);
extern void irq6(void);
extern void irq7(void);
extern void irq8(void);
extern void irq9(void);
extern void irq10(void);
extern void irq11(void);
extern void irq12(void);
extern void irq13(void);
extern void irq14(void);
extern void irq15(void);

// Interrupt handlers
typedef void (*interrupt_handler_t)(void);

// Function pointer array for interrupt handlers
interrupt_handler_t interrupt_handlers[IDT_ENTRIES];

/**
 * @brief Set an IDT gate
 * 
 * @param num Gate number
 * @param base Handler address
 * @param selector Code segment selector
 * @param ist IST entry (0-7)
 * @param flags Type and attributes
 */
static void idt_set_gate(int num, uint64_t base, uint16_t selector, uint8_t ist, uint8_t flags) {
    idt[num].base_low = base & 0xFFFF;
    idt[num].base_mid = (base >> 16) & 0xFFFF;
    idt[num].base_high = (base >> 32) & 0xFFFFFFFF;
    
    idt[num].selector = selector;
    idt[num].ist = ist & 0x7;    // Only 3 bits used
    idt[num].flags = flags;
    idt[num].reserved = 0;
}

/**
 * @brief Register an interrupt handler
 * 
 * @param num Interrupt number
 * @param handler Handler function
 */
void register_interrupt_handler(uint8_t num, interrupt_handler_t handler) {
    interrupt_handlers[num] = handler;
}

/**
 * @brief Generic interrupt handler
 * 
 * @param interrupt Interrupt number
 * @param error_code Error code (if applicable)
 */
void handle_interrupt(uint64_t interrupt, uint64_t error_code, uint64_t rip) {
    // Call the registered handler if available
    if (interrupt_handlers[interrupt]) {
        interrupt_handlers[interrupt]();
        return;
    }
    
    // Handle CPU exceptions specially
    if (interrupt < 32) {
        cli();
        
        char* exception_messages[] = {
            "Division By Zero",
            "Debug",
            "Non Maskable Interrupt",
            "Breakpoint",
            "Into Detected Overflow",
            "Out of Bounds",
            "Invalid Opcode",
            "No Coprocessor",
            "Double Fault",
            "Coprocessor Segment Overrun",
            "Bad TSS",
            "Segment Not Present",
            "Stack Fault",
            "General Protection Fault",
            "Page Fault",
            "Unknown Interrupt",
            "Coprocessor Fault",
            "Alignment Check",
            "Machine Check",
            "SIMD Floating-Point Exception",
            "Virtualization Exception",
            "Control Protection Exception",
            "Reserved",
            "Hypervisor Injection Exception",
            "VMM Communication Exception",
            "Security Exception",
            "Reserved",
            "Reserved",
            "Reserved",
            "Reserved",
            "Reserved",
            "Reserved"
        };
        
        char err_msg[128];
        snprintf(err_msg, sizeof(err_msg), "Exception: %s (INT %llu, ERR %llu, RIP %p)",
                 exception_messages[interrupt], interrupt, error_code, rip);
        
        panic(PANIC_NORMAL, err_msg, __FILE__, __LINE__);
    } else {
        // Just log unhandled interrupts
        kprintf("Unhandled interrupt: %llu\n", interrupt);
    }
}

/**
 * @brief Initialize the IDT
 */
void idt_init(void) {
    // Set up IDT pointer
    idt_pointer.limit = sizeof(struct idt_entry) * IDT_ENTRIES - 1;
    idt_pointer.base = (uint64_t)&idt;
    
    // Clear the IDT and interrupt handlers
    for (int i = 0; i < IDT_ENTRIES; i++) {
        idt_set_gate(i, 0, 0, 0, 0);
        interrupt_handlers[i] = NULL;
    }
    
    // Set up CPU exception handlers (ISRs 0-31)
    idt_set_gate(0, (uint64_t)isr0, 0x08, 0, 0x8E);   // Division by zero
    idt_set_gate(1, (uint64_t)isr1, 0x08, 0, 0x8E);   // Debug
    idt_set_gate(2, (uint64_t)isr2, 0x08, 0, 0x8E);   // Non-maskable interrupt
    idt_set_gate(3, (uint64_t)isr3, 0x08, 0, 0x8E);   // Breakpoint
    idt_set_gate(4, (uint64_t)isr4, 0x08, 0, 0x8E);   // Overflow
    idt_set_gate(5, (uint64_t)isr5, 0x08, 0, 0x8E);   // Bound range exceeded
    idt_set_gate(6, (uint64_t)isr6, 0x08, 0, 0x8E);   // Invalid opcode
    idt_set_gate(7, (uint64_t)isr7, 0x08, 0, 0x8E);   // Device not available
    idt_set_gate(8, (uint64_t)isr8, 0x08, 0, 0x8E);   // Double fault
    idt_set_gate(9, (uint64_t)isr9, 0x08, 0, 0x8E);   // Coprocessor segment overrun
    idt_set_gate(10, (uint64_t)isr10, 0x08, 0, 0x8E); // Invalid TSS
    idt_set_gate(11, (uint64_t)isr11, 0x08, 0, 0x8E); // Segment not present
    idt_set_gate(12, (uint64_t)isr12, 0x08, 0, 0x8E); // Stack-segment fault
    idt_set_gate(13, (uint64_t)isr13, 0x08, 0, 0x8E); // General protection fault
    idt_set_gate(14, (uint64_t)isr14, 0x08, 0, 0x8E); // Page fault
    idt_set_gate(15, (uint64_t)isr15, 0x08, 0, 0x8E); // Reserved
    idt_set_gate(16, (uint64_t)isr16, 0x08, 0, 0x8E); // x87 FPU error
    idt_set_gate(17, (uint64_t)isr17, 0x08, 0, 0x8E); // Alignment check
    idt_set_gate(18, (uint64_t)isr18, 0x08, 0, 0x8E); // Machine check
    idt_set_gate(19, (uint64_t)isr19, 0x08, 0, 0x8E); // SIMD floating-point exception
    idt_set_gate(20, (uint64_t)isr20, 0x08, 0, 0x8E); // Virtualization exception
    idt_set_gate(21, (uint64_t)isr21, 0x08, 0, 0x8E); // Control protection exception
    idt_set_gate(22, (uint64_t)isr22, 0x08, 0, 0x8E); // Reserved
    idt_set_gate(23, (uint64_t)isr23, 0x08, 0, 0x8E); // Reserved
    idt_set_gate(24, (uint64_t)isr24, 0x08, 0, 0x8E); // Reserved
    idt_set_gate(25, (uint64_t)isr25, 0x08, 0, 0x8E); // Reserved
    idt_set_gate(26, (uint64_t)isr26, 0x08, 0, 0x8E); // Reserved
    idt_set_gate(27, (uint64_t)isr27, 0x08, 0, 0x8E); // Reserved
    idt_set_gate(28, (uint64_t)isr28, 0x08, 0, 0x8E); // Reserved
    idt_set_gate(29, (uint64_t)isr29, 0x08, 0, 0x8E); // Reserved
    idt_set_gate(30, (uint64_t)isr30, 0x08, 0, 0x8E); // Reserved
    idt_set_gate(31, (uint64_t)isr31, 0x08, 0, 0x8E); // Reserved
    
    // Set up hardware interrupt handlers (IRQs 0-15 -> ISRs 32-47)
    idt_set_gate(32, (uint64_t)irq0, 0x08, 0, 0x8E);  // Timer (PIT)
    idt_set_gate(33, (uint64_t)irq1, 0x08, 0, 0x8E);  // Keyboard
    idt_set_gate(34, (uint64_t)irq2, 0x08, 0, 0x8E);  // Cascade for 8259A Slave controller
    idt_set_gate(35, (uint64_t)irq3, 0x08, 0, 0x8E);  // COM2
    idt_set_gate(36, (uint64_t)irq4, 0x08, 0, 0x8E);  // COM1
    idt_set_gate(37, (uint64_t)irq5, 0x08, 0, 0x8E);  // LPT2
    idt_set_gate(38, (uint64_t)irq6, 0x08, 0, 0x8E);  // Floppy Disk
    idt_set_gate(39, (uint64_t)irq7, 0x08, 0, 0x8E);  // LPT1 / spurious
    idt_set_gate(40, (uint64_t)irq8, 0x08, 0, 0x8E);  // CMOS real-time clock
    idt_set_gate(41, (uint64_t)irq9, 0x08, 0, 0x8E);  // Free / SCSI / NIC
    idt_set_gate(42, (uint64_t)irq10, 0x08, 0, 0x8E); // Free / SCSI / NIC
    idt_set_gate(43, (uint64_t)irq11, 0x08, 0, 0x8E); // Free / SCSI / NIC
    idt_set_gate(44, (uint64_t)irq12, 0x08, 0, 0x8E); // PS2 Mouse
    idt_set_gate(45, (uint64_t)irq13, 0x08, 0, 0x8E); // FPU / Coprocessor
    idt_set_gate(46, (uint64_t)irq14, 0x08, 0, 0x8E); // Primary ATA Hard Disk
    idt_set_gate(47, (uint64_t)irq15, 0x08, 0, 0x8E); // Secondary ATA Hard Disk
    
    // Load the IDT
    idt_flush((uint64_t)&idt_pointer);
    
    kprintf("IDT: Initialized with %d entries\n", IDT_ENTRIES);
}
