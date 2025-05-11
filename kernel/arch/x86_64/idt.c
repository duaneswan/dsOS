/**
 * @file idt.c
 * @brief Interrupt Descriptor Table (IDT) implementation
 */

#include "../../include/kernel.h"
#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

// IDT entry structure
typedef struct {
    uint16_t offset_low;    // Lower 16 bits of ISR address
    uint16_t selector;      // Segment selector for ISR
    uint8_t ist;            // Interrupt Stack Table offset
    uint8_t type_attr;      // Type and attributes
    uint16_t offset_mid;    // Middle 16 bits of ISR address
    uint32_t offset_high;   // Upper 32 bits of ISR address
    uint32_t reserved;      // Reserved, must be 0
} PACKED idt_entry_t;

// IDT pointer structure
typedef struct {
    uint16_t limit;         // Size of IDT - 1
    uint64_t base;          // Base address of IDT
} PACKED idt_ptr_t;

// IDT attributes
#define IDT_PRESENT     0x80    // Present bit
#define IDT_DPL_0       0x00    // Ring 0 (kernel)
#define IDT_DPL_3       0x60    // Ring 3 (user)
#define IDT_INT_GATE    0x0E    // Interrupt gate
#define IDT_TRAP_GATE   0x0F    // Trap gate

// Number of IDT entries
#define IDT_ENTRIES     256

// The IDT table
static idt_entry_t idt[IDT_ENTRIES];

// The IDT pointer
static idt_ptr_t idt_ptr;

// Interrupt handlers
static interrupt_handler_t interrupt_handlers[IDT_ENTRIES];

// Exception messages
static const char* exception_messages[32] = {
    "Division By Zero",
    "Debug",
    "Non Maskable Interrupt",
    "Breakpoint",
    "Overflow",
    "Bound Range Exceeded",
    "Invalid Opcode",
    "Device Not Available",
    "Double Fault",
    "Coprocessor Segment Overrun",
    "Invalid TSS",
    "Segment Not Present",
    "Stack-Segment Fault",
    "General Protection Fault",
    "Page Fault",
    "Reserved",
    "x87 Floating-Point Exception",
    "Alignment Check",
    "Machine Check",
    "SIMD Floating-Point Exception",
    "Virtualization Exception",
    "Control Protection Exception",
    "Reserved",
    "Reserved",
    "Reserved",
    "Reserved",
    "Reserved",
    "Reserved",
    "Hypervisor Injection Exception",
    "VMM Communication Exception",
    "Security Exception",
    "Reserved"
};

// Forward declaration for the ISR handlers
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

// IRQs (32-47)
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

// General-purpose interrupt (48-255)
extern void int_dispatch(void);

/**
 * @brief Set an entry in the IDT
 * 
 * @param num Entry number
 * @param base Handler address
 * @param selector Code segment selector
 * @param flags Entry flags
 */
static void idt_set_gate(uint8_t num, uint64_t base, uint16_t selector, uint8_t flags) {
    idt[num].offset_low = (uint16_t)(base & 0xFFFF);
    idt[num].offset_mid = (uint16_t)((base >> 16) & 0xFFFF);
    idt[num].offset_high = (uint32_t)((base >> 32) & 0xFFFFFFFF);
    idt[num].selector = selector;
    idt[num].reserved = 0;
    idt[num].type_attr = flags;
    idt[num].ist = 0;
}

/**
 * @brief Initialize the IDT
 */
void idt_init(void) {
    // Set up the IDT pointer
    idt_ptr.limit = sizeof(idt) - 1;
    idt_ptr.base = (uint64_t)&idt;
    
    // Clear the IDT and handler table
    memset(&idt, 0, sizeof(idt));
    memset(&interrupt_handlers, 0, sizeof(interrupt_handlers));
    
    // Set up exception handlers (0-31)
    idt_set_gate(0, (uint64_t)isr0, 0x08, IDT_PRESENT | IDT_DPL_0 | IDT_INT_GATE);
    idt_set_gate(1, (uint64_t)isr1, 0x08, IDT_PRESENT | IDT_DPL_0 | IDT_INT_GATE);
    idt_set_gate(2, (uint64_t)isr2, 0x08, IDT_PRESENT | IDT_DPL_0 | IDT_INT_GATE);
    idt_set_gate(3, (uint64_t)isr3, 0x08, IDT_PRESENT | IDT_DPL_0 | IDT_INT_GATE);
    idt_set_gate(4, (uint64_t)isr4, 0x08, IDT_PRESENT | IDT_DPL_0 | IDT_INT_GATE);
    idt_set_gate(5, (uint64_t)isr5, 0x08, IDT_PRESENT | IDT_DPL_0 | IDT_INT_GATE);
    idt_set_gate(6, (uint64_t)isr6, 0x08, IDT_PRESENT | IDT_DPL_0 | IDT_INT_GATE);
    idt_set_gate(7, (uint64_t)isr7, 0x08, IDT_PRESENT | IDT_DPL_0 | IDT_INT_GATE);
    idt_set_gate(8, (uint64_t)isr8, 0x08, IDT_PRESENT | IDT_DPL_0 | IDT_INT_GATE);
    idt_set_gate(9, (uint64_t)isr9, 0x08, IDT_PRESENT | IDT_DPL_0 | IDT_INT_GATE);
    idt_set_gate(10, (uint64_t)isr10, 0x08, IDT_PRESENT | IDT_DPL_0 | IDT_INT_GATE);
    idt_set_gate(11, (uint64_t)isr11, 0x08, IDT_PRESENT | IDT_DPL_0 | IDT_INT_GATE);
    idt_set_gate(12, (uint64_t)isr12, 0x08, IDT_PRESENT | IDT_DPL_0 | IDT_INT_GATE);
    idt_set_gate(13, (uint64_t)isr13, 0x08, IDT_PRESENT | IDT_DPL_0 | IDT_INT_GATE);
    idt_set_gate(14, (uint64_t)isr14, 0x08, IDT_PRESENT | IDT_DPL_0 | IDT_INT_GATE);
    idt_set_gate(15, (uint64_t)isr15, 0x08, IDT_PRESENT | IDT_DPL_0 | IDT_INT_GATE);
    idt_set_gate(16, (uint64_t)isr16, 0x08, IDT_PRESENT | IDT_DPL_0 | IDT_INT_GATE);
    idt_set_gate(17, (uint64_t)isr17, 0x08, IDT_PRESENT | IDT_DPL_0 | IDT_INT_GATE);
    idt_set_gate(18, (uint64_t)isr18, 0x08, IDT_PRESENT | IDT_DPL_0 | IDT_INT_GATE);
    idt_set_gate(19, (uint64_t)isr19, 0x08, IDT_PRESENT | IDT_DPL_0 | IDT_INT_GATE);
    idt_set_gate(20, (uint64_t)isr20, 0x08, IDT_PRESENT | IDT_DPL_0 | IDT_INT_GATE);
    idt_set_gate(21, (uint64_t)isr21, 0x08, IDT_PRESENT | IDT_DPL_0 | IDT_INT_GATE);
    idt_set_gate(22, (uint64_t)isr22, 0x08, IDT_PRESENT | IDT_DPL_0 | IDT_INT_GATE);
    idt_set_gate(23, (uint64_t)isr23, 0x08, IDT_PRESENT | IDT_DPL_0 | IDT_INT_GATE);
    idt_set_gate(24, (uint64_t)isr24, 0x08, IDT_PRESENT | IDT_DPL_0 | IDT_INT_GATE);
    idt_set_gate(25, (uint64_t)isr25, 0x08, IDT_PRESENT | IDT_DPL_0 | IDT_INT_GATE);
    idt_set_gate(26, (uint64_t)isr26, 0x08, IDT_PRESENT | IDT_DPL_0 | IDT_INT_GATE);
    idt_set_gate(27, (uint64_t)isr27, 0x08, IDT_PRESENT | IDT_DPL_0 | IDT_INT_GATE);
    idt_set_gate(28, (uint64_t)isr28, 0x08, IDT_PRESENT | IDT_DPL_0 | IDT_INT_GATE);
    idt_set_gate(29, (uint64_t)isr29, 0x08, IDT_PRESENT | IDT_DPL_0 | IDT_INT_GATE);
    idt_set_gate(30, (uint64_t)isr30, 0x08, IDT_PRESENT | IDT_DPL_0 | IDT_INT_GATE);
    idt_set_gate(31, (uint64_t)isr31, 0x08, IDT_PRESENT | IDT_DPL_0 | IDT_INT_GATE);
    
    // Set up IRQ handlers (32-47)
    idt_set_gate(32, (uint64_t)irq0, 0x08, IDT_PRESENT | IDT_DPL_0 | IDT_INT_GATE);
    idt_set_gate(33, (uint64_t)irq1, 0x08, IDT_PRESENT | IDT_DPL_0 | IDT_INT_GATE);
    idt_set_gate(34, (uint64_t)irq2, 0x08, IDT_PRESENT | IDT_DPL_0 | IDT_INT_GATE);
    idt_set_gate(35, (uint64_t)irq3, 0x08, IDT_PRESENT | IDT_DPL_0 | IDT_INT_GATE);
    idt_set_gate(36, (uint64_t)irq4, 0x08, IDT_PRESENT | IDT_DPL_0 | IDT_INT_GATE);
    idt_set_gate(37, (uint64_t)irq5, 0x08, IDT_PRESENT | IDT_DPL_0 | IDT_INT_GATE);
    idt_set_gate(38, (uint64_t)irq6, 0x08, IDT_PRESENT | IDT_DPL_0 | IDT_INT_GATE);
    idt_set_gate(39, (uint64_t)irq7, 0x08, IDT_PRESENT | IDT_DPL_0 | IDT_INT_GATE);
    idt_set_gate(40, (uint64_t)irq8, 0x08, IDT_PRESENT | IDT_DPL_0 | IDT_INT_GATE);
    idt_set_gate(41, (uint64_t)irq9, 0x08, IDT_PRESENT | IDT_DPL_0 | IDT_INT_GATE);
    idt_set_gate(42, (uint64_t)irq10, 0x08, IDT_PRESENT | IDT_DPL_0 | IDT_INT_GATE);
    idt_set_gate(43, (uint64_t)irq11, 0x08, IDT_PRESENT | IDT_DPL_0 | IDT_INT_GATE);
    idt_set_gate(44, (uint64_t)irq12, 0x08, IDT_PRESENT | IDT_DPL_0 | IDT_INT_GATE);
    idt_set_gate(45, (uint64_t)irq13, 0x08, IDT_PRESENT | IDT_DPL_0 | IDT_INT_GATE);
    idt_set_gate(46, (uint64_t)irq14, 0x08, IDT_PRESENT | IDT_DPL_0 | IDT_INT_GATE);
    idt_set_gate(47, (uint64_t)irq15, 0x08, IDT_PRESENT | IDT_DPL_0 | IDT_INT_GATE);
    
    // Set up general-purpose interrupt handlers (48-255)
    for (int i = 48; i < 256; i++) {
        idt_set_gate(i, (uint64_t)int_dispatch, 0x08, IDT_PRESENT | IDT_DPL_0 | IDT_INT_GATE);
    }
    
    // Load the IDT (implemented in assembly)
    idt_flush((uint64_t)&idt_ptr);
    
    kprintf("IDT: Initialized with %d entries\n", IDT_ENTRIES);
}

/**
 * @brief Register an interrupt handler
 * 
 * @param interrupt Interrupt number
 * @param handler Handler function
 */
void register_interrupt_handler(uint8_t interrupt, interrupt_handler_t handler) {
    interrupt_handlers[interrupt] = handler;
}

/**
 * @brief Default interrupt handler if no specific handler is registered
 * 
 * @param regs CPU registers
 */
static void default_interrupt_handler(void) {
    // Do nothing for unhandled interrupts
}

/**
 * @brief Exception handler
 * 
 * @param regs CPU registers
 */
void exception_handler(uint64_t rip, uint64_t cs, uint64_t rflags, uint64_t rsp, uint64_t ss, 
                      uint64_t int_no, uint64_t error_code) {
    if (int_no < 32) {
        // Handle CPU exception
        char error_msg[256];
        snprintf(error_msg, sizeof(error_msg), "Exception #%llu (%s) at 0x%llx, Error: 0x%llx",
                 int_no, exception_messages[int_no], rip, error_code);
        
        panic(PANIC_CRITICAL, error_msg, __FILE__, __LINE__);
    }
}

/**
 * @brief General interrupt handler
 * 
 * @param regs CPU registers
 */
void interrupt_handler(uint64_t int_no) {
    // Call the registered handler if any
    if (interrupt_handlers[int_no]) {
        interrupt_handlers[int_no]();
    } else {
        default_interrupt_handler();
    }
}

/**
 * @brief External function to flush the IDT
 * 
 * @param idt_ptr Pointer to the IDT pointer structure
 */
extern void idt_flush(uint64_t idt_ptr);
