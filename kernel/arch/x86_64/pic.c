/**
 * @file pic.c
 * @brief Programmable Interrupt Controller (PIC) driver
 */

#include "../../include/kernel.h"
#include <stdint.h>

// PIC controller ports
#define PIC1_COMMAND    0x20    // Command port for master PIC
#define PIC1_DATA       0x21    // Data port for master PIC
#define PIC2_COMMAND    0xA0    // Command port for slave PIC
#define PIC2_DATA       0xA1    // Data port for slave PIC

// PIC commands
#define PIC_EOI         0x20    // End of Interrupt command
#define PIC_INIT        0x11    // Initialization command

// IRQ mask status
static uint16_t irq_mask = 0xFFFF;  // All IRQs masked by default

/**
 * @brief Send a command to the PIC
 * 
 * @param pic PIC number (1 = master, 2 = slave)
 * @param cmd Command to send
 */
static void pic_send_command(uint8_t pic, uint8_t cmd) {
    uint16_t port = (pic == 1) ? PIC1_COMMAND : PIC2_COMMAND;
    outb(port, cmd);
    io_wait();  // Small delay to ensure PIC processes command
}

/**
 * @brief Write data to the PIC
 * 
 * @param pic PIC number (1 = master, 2 = slave)
 * @param data Data to write
 */
static void pic_write_data(uint8_t pic, uint8_t data) {
    uint16_t port = (pic == 1) ? PIC1_DATA : PIC2_DATA;
    outb(port, data);
    io_wait();  // Small delay to ensure PIC processes data
}

/**
 * @brief Read data from the PIC
 * 
 * @param pic PIC number (1 = master, 2 = slave)
 * @return Data read from PIC
 */
static uint8_t pic_read_data(uint8_t pic) {
    uint16_t port = (pic == 1) ? PIC1_DATA : PIC2_DATA;
    return inb(port);
}

/**
 * @brief Initialize the 8259 PIC
 * 
 * @param offset1 Interrupt vector offset for master PIC
 * @param offset2 Interrupt vector offset for slave PIC
 */
void pic_init(uint8_t offset1, uint8_t offset2) {
    // Save interrupt masks
    uint8_t mask1 = pic_read_data(1);
    uint8_t mask2 = pic_read_data(2);
    
    // ICW1: Start initialization sequence in cascade mode
    pic_send_command(1, PIC_INIT);
    pic_send_command(2, PIC_INIT);
    
    // ICW2: Set vector offsets
    pic_write_data(1, offset1);  // IRQ 0-7: interrupt vectors offset1 to offset1+7
    pic_write_data(2, offset2);  // IRQ 8-15: interrupt vectors offset2 to offset2+7
    
    // ICW3: Tell master PIC there is a slave at IRQ2
    pic_write_data(1, 0x04);     // Bit 2 = 1, slave on IRQ2
    pic_write_data(2, 0x02);     // Slave ID is 2
    
    // ICW4: Set 8086 mode
    pic_write_data(1, 0x01);     // 8086/88 mode
    pic_write_data(2, 0x01);     // 8086/88 mode
    
    // Restore saved interrupt masks
    pic_write_data(1, mask1);
    pic_write_data(2, mask2);
    
    // Update global mask state
    irq_mask = (mask2 << 8) | mask1;
    
    kprintf("PIC: Initialized with offsets 0x%02X and 0x%02X\n", offset1, offset2);
}

/**
 * @brief Send end-of-interrupt command to the PIC
 * 
 * @param irq IRQ number that was serviced
 */
void pic_send_eoi(uint8_t irq) {
    // For IRQs 8-15, send EOI to both PICs
    if (irq >= 8) {
        pic_send_command(2, PIC_EOI);
    }
    
    // Always send EOI to master PIC
    pic_send_command(1, PIC_EOI);
}

/**
 * @brief Disable (mask) an IRQ line
 * 
 * @param irq IRQ line to mask (0-15)
 */
void pic_mask_irq(uint8_t irq) {
    uint16_t port;
    uint8_t value;
    
    if (irq < 8) {
        // IRQ 0-7: Master PIC
        port = PIC1_DATA;
        value = pic_read_data(1) | (1 << irq);
    } else {
        // IRQ 8-15: Slave PIC
        port = PIC2_DATA;
        value = pic_read_data(2) | (1 << (irq - 8));
    }
    
    outb(port, value);
    
    // Update global mask state
    if (irq < 8) {
        irq_mask = (irq_mask & 0xFF00) | value;
    } else {
        irq_mask = (irq_mask & 0x00FF) | (value << 8);
    }
}

/**
 * @brief Enable (unmask) an IRQ line
 * 
 * @param irq IRQ line to unmask (0-15)
 */
void pic_unmask_irq(uint8_t irq) {
    uint16_t port;
    uint8_t value;
    
    if (irq < 8) {
        // IRQ 0-7: Master PIC
        port = PIC1_DATA;
        value = pic_read_data(1) & ~(1 << irq);
    } else {
        // IRQ 8-15: Slave PIC
        port = PIC2_DATA;
        value = pic_read_data(2) & ~(1 << (irq - 8));
        
        // Always make sure IRQ2 is unmasked (cascade line)
        pic_unmask_irq(2);
    }
    
    outb(port, value);
    
    // Update global mask state
    if (irq < 8) {
        irq_mask = (irq_mask & 0xFF00) | value;
    } else {
        irq_mask = (irq_mask & 0x00FF) | (value << 8);
    }
}

/**
 * @brief Get the IRQ mask status
 * 
 * @return Current IRQ mask (bits 0-15 correspond to IRQs 0-15)
 */
uint16_t pic_get_irq_mask(void) {
    return irq_mask;
}

/**
 * @brief Set the IRQ mask (mask multiple IRQs at once)
 * 
 * @param mask IRQ mask to set (bits 0-15 correspond to IRQs 0-15)
 */
void pic_set_irq_mask(uint16_t mask) {
    pic_write_data(1, mask & 0xFF);         // Set mask for IRQs 0-7
    pic_write_data(2, (mask >> 8) & 0xFF);  // Set mask for IRQs 8-15
    irq_mask = mask;
}

/**
 * @brief Check if an IRQ is in service
 * 
 * @param irq IRQ number to check
 * @return true if the IRQ is currently being serviced
 */
bool pic_is_irq_in_service(uint8_t irq) {
    uint16_t port = (irq < 8) ? PIC1_COMMAND : PIC2_COMMAND;
    uint8_t irq_bit = (irq < 8) ? irq : (irq - 8);
    
    // Read In-Service Register
    outb(port, 0x0B);  // OCW3: Read ISR
    uint8_t isr = inb(port);
    
    return (isr & (1 << irq_bit)) != 0;
}

/**
 * @brief Disable the PIC (typically used when switching to APIC)
 */
void pic_disable(void) {
    // Mask all interrupts
    pic_set_irq_mask(0xFFFF);
    
    kprintf("PIC: Disabled (all IRQs masked)\n");
}

/**
 * @brief Remap the PIC interrupts to avoid conflicts with CPU exceptions
 * 
 * In protected mode, the first 32 interrupts are reserved for CPU exceptions.
 * This function remaps IRQs 0-15 to interrupts 32-47 to avoid conflicts.
 */
void pic_remap(void) {
    pic_init(0x20, 0x28);  // Map IRQs 0-7 to interrupts 32-39, and IRQs 8-15 to interrupts 40-47
}
