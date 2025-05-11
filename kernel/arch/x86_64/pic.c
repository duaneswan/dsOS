/**
 * @file pic.c
 * @brief Programmable Interrupt Controller (PIC) driver
 */

#include "../../include/kernel.h"
#include <stdint.h>
#include <stdbool.h>

// PIC ports
#define PIC1_COMMAND    0x20
#define PIC1_DATA       0x21
#define PIC2_COMMAND    0xA0
#define PIC2_DATA       0xA1

// PIC commands
#define PIC_EOI         0x20    // End of Interrupt
#define PIC_READ_IRR    0x0A    // Read Interrupt Request Register
#define PIC_READ_ISR    0x0B    // Read In-Service Register

// PIC initialization
#define ICW1_ICW4       0x01    // ICW4 needed
#define ICW1_SINGLE     0x02    // Single (cascade) mode
#define ICW1_INTERVAL4  0x04    // Call address interval 4 (8)
#define ICW1_LEVEL      0x08    // Level triggered (edge) mode
#define ICW1_INIT       0x10    // Initialization

#define ICW4_8086       0x01    // 8086/88 (MCS-80/85) mode
#define ICW4_AUTO       0x02    // Auto (normal) EOI
#define ICW4_BUF_SLAVE  0x08    // Buffered mode/slave
#define ICW4_BUF_MASTER 0x0C    // Buffered mode/master
#define ICW4_SFNM       0x10    // Special fully nested (not)

// Default PIC vectors
#define PIC1_OFFSET     0x20    // IRQ 0-7: interrupts 0x20-0x27
#define PIC2_OFFSET     0x28    // IRQ 8-15: interrupts 0x28-0x2F

// Number of IRQs per PIC
#define PIC_NUM_IRQS    8

// IRQ masks (1=disabled, 0=enabled)
static uint16_t irq_mask = 0xFFFF; // All IRQs masked initially

/**
 * @brief Send End-of-Interrupt to the PIC
 * 
 * @param irq IRQ number
 */
void pic_send_eoi(uint8_t irq) {
    // For IRQs 8-15, we need to send EOI to both PICs
    if (irq >= 8) {
        outb(PIC2_COMMAND, PIC_EOI);
    }
    
    // For all IRQs, we need to send EOI to master PIC
    outb(PIC1_COMMAND, PIC_EOI);
}

/**
 * @brief Set the IRQ mask
 * 
 * @param irq IRQ number
 * @param enabled Whether the IRQ should be enabled (true) or disabled (false)
 */
void pic_set_mask(uint8_t irq, bool enabled) {
    uint16_t port;
    uint8_t value;
    
    if (irq < 8) {
        port = PIC1_DATA;
    } else {
        port = PIC2_DATA;
        irq -= 8;
    }
    
    if (enabled) {
        value = inb(port) & ~(1 << irq);
    } else {
        value = inb(port) | (1 << irq);
    }
    
    outb(port, value);
}

/**
 * @brief Set all IRQ masks at once
 * 
 * @param mask 16-bit mask (low 8 bits = PIC1, high 8 bits = PIC2)
 */
void pic_set_masks(uint16_t mask) {
    irq_mask = mask;
    outb(PIC1_DATA, mask & 0xFF);
    outb(PIC2_DATA, (mask >> 8) & 0xFF);
}

/**
 * @brief Get the current IRQ mask
 * 
 * @return 16-bit mask (low 8 bits = PIC1, high 8 bits = PIC2)
 */
uint16_t pic_get_masks(void) {
    return (inb(PIC2_DATA) << 8) | inb(PIC1_DATA);
}

/**
 * @brief Check if an IRQ is in service
 * 
 * @param irq IRQ number
 * @return true if the IRQ is in service, false otherwise
 */
bool pic_is_irq_in_service(uint8_t irq) {
    uint16_t port;
    uint8_t value;
    
    if (irq < 8) {
        port = PIC1_COMMAND;
    } else {
        port = PIC2_COMMAND;
        irq -= 8;
    }
    
    outb(port, PIC_READ_ISR);
    value = inb(port);
    
    return (value & (1 << irq)) != 0;
}

/**
 * @brief Check if an IRQ is requested
 * 
 * @param irq IRQ number
 * @return true if the IRQ is requested, false otherwise
 */
bool pic_is_irq_requested(uint8_t irq) {
    uint16_t port;
    uint8_t value;
    
    if (irq < 8) {
        port = PIC1_COMMAND;
    } else {
        port = PIC2_COMMAND;
        irq -= 8;
    }
    
    outb(port, PIC_READ_IRR);
    value = inb(port);
    
    return (value & (1 << irq)) != 0;
}

/**
 * @brief Disable the PIC (useful for APIC mode)
 */
void pic_disable(void) {
    // Mask all interrupts
    outb(PIC1_DATA, 0xFF);
    outb(PIC2_DATA, 0xFF);
}

/**
 * @brief Initialize the PIC
 */
void pic_init(void) {
    uint8_t a1, a2;
    
    // Save masks
    a1 = inb(PIC1_DATA);
    a2 = inb(PIC2_DATA);
    
    // Start initialization sequence in cascade mode
    outb(PIC1_COMMAND, ICW1_INIT | ICW1_ICW4);
    io_wait();
    outb(PIC2_COMMAND, ICW1_INIT | ICW1_ICW4);
    io_wait();
    
    // Set vector offsets
    outb(PIC1_DATA, PIC1_OFFSET);
    io_wait();
    outb(PIC2_DATA, PIC2_OFFSET);
    io_wait();
    
    // Tell PICs about cascading
    outb(PIC1_DATA, 4);     // IRQ2 -> Slave PIC
    io_wait();
    outb(PIC2_DATA, 2);     // Slave ID = 2
    io_wait();
    
    // Set 8086 mode
    outb(PIC1_DATA, ICW4_8086);
    io_wait();
    outb(PIC2_DATA, ICW4_8086);
    io_wait();
    
    // Restore masks
    outb(PIC1_DATA, a1);
    outb(PIC2_DATA, a2);
    
    // By default, mask all interrupts except for cascade
    pic_set_masks(0xFFFB);  // Enable IRQ2 (cascade)
    
    kprintf("PIC: Initialized with offsets 0x%02x and 0x%02x\n", PIC1_OFFSET, PIC2_OFFSET);
}

/**
 * @brief Remap the PIC to use custom interrupt vectors
 * 
 * @param offset1 New offset for master PIC
 * @param offset2 New offset for slave PIC
 */
void pic_remap(uint8_t offset1, uint8_t offset2) {
    uint8_t a1, a2;
    
    // Save masks
    a1 = inb(PIC1_DATA);
    a2 = inb(PIC2_DATA);
    
    // Start initialization sequence in cascade mode
    outb(PIC1_COMMAND, ICW1_INIT | ICW1_ICW4);
    io_wait();
    outb(PIC2_COMMAND, ICW1_INIT | ICW1_ICW4);
    io_wait();
    
    // Set vector offsets
    outb(PIC1_DATA, offset1);
    io_wait();
    outb(PIC2_DATA, offset2);
    io_wait();
    
    // Tell PICs about cascading
    outb(PIC1_DATA, 4);     // IRQ2 -> Slave PIC
    io_wait();
    outb(PIC2_DATA, 2);     // Slave ID = 2
    io_wait();
    
    // Set 8086 mode
    outb(PIC1_DATA, ICW4_8086);
    io_wait();
    outb(PIC2_DATA, ICW4_8086);
    io_wait();
    
    // Restore masks
    outb(PIC1_DATA, a1);
    outb(PIC2_DATA, a2);
    
    kprintf("PIC: Remapped with offsets 0x%02x and 0x%02x\n", offset1, offset2);
}
