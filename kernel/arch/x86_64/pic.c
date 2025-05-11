/**
 * @file pic.c
 * @brief 8259 Programmable Interrupt Controller (PIC) implementation
 */

#include "../../include/kernel.h"
#include <stdint.h>

// PIC I/O ports
#define PIC1_COMMAND  0x20    // Master PIC command port
#define PIC1_DATA     0x21    // Master PIC data port
#define PIC2_COMMAND  0xA0    // Slave PIC command port
#define PIC2_DATA     0xA1    // Slave PIC data port

// PIC commands
#define PIC_EOI       0x20    // End of Interrupt command
#define PIC_INIT      0x11    // Initialize command

// Interrupt vector offsets
#define IRQ_OFFSET    0x20    // IRQ 0 mapped to INT 0x20

/**
 * @brief Send an End-Of-Interrupt signal to the PIC
 * 
 * @param irq IRQ line that has been serviced
 */
void pic_send_eoi(uint64_t irq) {
    // If IRQ came from the slave PIC, send EOI to slave
    if (irq >= 40) {
        outb(PIC2_COMMAND, PIC_EOI);
    }
    
    // Always send EOI to master PIC
    outb(PIC1_COMMAND, PIC_EOI);
}

/**
 * @brief Initialize the 8259 PIC
 * 
 * This remaps the IRQs to avoid conflicts with CPU exceptions
 */
void pic_init(void) {
    // Save interrupt mask registers
    uint8_t mask1 = inb(PIC1_DATA);
    uint8_t mask2 = inb(PIC2_DATA);
    
    // Initialize both PICs with ICW1
    outb(PIC1_COMMAND, PIC_INIT);  // ICW1: Start initialization sequence
    outb(PIC2_COMMAND, PIC_INIT);
    
    // Set vector offsets with ICW2
    outb(PIC1_DATA, IRQ_OFFSET);   // ICW2: Master PIC vector offset (0x20)
    outb(PIC2_DATA, IRQ_OFFSET+8); // ICW2: Slave PIC vector offset (0x28)
    
    // Set up cascading with ICW3
    outb(PIC1_DATA, 0x04);         // ICW3: Master has slave on IRQ2 (bit 2)
    outb(PIC2_DATA, 0x02);         // ICW3: Slave's cascade identity (2)
    
    // Set operation mode with ICW4
    outb(PIC1_DATA, 0x01);         // ICW4: 8086 mode, normal EOI
    outb(PIC2_DATA, 0x01);
    
    // Restore interrupt masks
    outb(PIC1_DATA, mask1);
    outb(PIC2_DATA, mask2);
    
    kprintf("PIC: Initialized and remapped to vectors 0x%x-0x%x\n", 
            IRQ_OFFSET, IRQ_OFFSET + 15);
}

/**
 * @brief Mask (disable) an IRQ line
 * 
 * @param irq IRQ line to mask (0-15)
 */
void pic_mask_irq(uint8_t irq) {
    uint16_t port;
    uint8_t mask;
    
    if (irq < 8) {
        port = PIC1_DATA;
        mask = inb(port) | (1 << irq);
    } else {
        port = PIC2_DATA;
        mask = inb(port) | (1 << (irq - 8));
    }
    
    outb(port, mask);
}

/**
 * @brief Unmask (enable) an IRQ line
 * 
 * @param irq IRQ line to unmask (0-15)
 */
void pic_unmask_irq(uint8_t irq) {
    uint16_t port;
    uint8_t mask;
    
    if (irq < 8) {
        port = PIC1_DATA;
        mask = inb(port) & ~(1 << irq);
    } else {
        port = PIC2_DATA;
        mask = inb(port) & ~(1 << (irq - 8));
    }
    
    outb(port, mask);
}

/**
 * @brief Disable the PIC (used when switching to APIC)
 */
void pic_disable(void) {
    // Mask all interrupts
    outb(PIC1_DATA, 0xFF);
    outb(PIC2_DATA, 0xFF);
    
    kprintf("PIC: Disabled\n");
}
