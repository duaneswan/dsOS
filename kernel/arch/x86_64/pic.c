/**
 * @file pic.c
 * @brief Programmable Interrupt Controller (PIC) driver
 */

#include "../../include/kernel.h"
#include <stdint.h>
#include <stdbool.h>

// PIC 8259 ports
#define PIC1_COMMAND      0x20        // Master PIC command port
#define PIC1_DATA         0x21        // Master PIC data port
#define PIC2_COMMAND      0xA0        // Slave PIC command port
#define PIC2_DATA         0xA1        // Slave PIC data port

// PIC commands
#define PIC_EOI           0x20        // End of Interrupt command
#define PIC_READ_IRR      0x0A        // Read Interrupt Request Register
#define PIC_READ_ISR      0x0B        // Read In-Service Register

// PIC initialization command words
#define ICW1_ICW4         0x01        // ICW4 needed
#define ICW1_SINGLE       0x02        // Single mode
#define ICW1_INTERVAL4    0x04        // Call address interval 4
#define ICW1_LEVEL        0x08        // Level triggered mode
#define ICW1_INIT         0x10        // Initialization command

#define ICW4_8086         0x01        // 8086/88 mode
#define ICW4_AUTO         0x02        // Auto EOI
#define ICW4_BUF_SLAVE    0x08        // Buffered mode (slave)
#define ICW4_BUF_MASTER   0x0C        // Buffered mode (master)
#define ICW4_SFNM         0x10        // Special fully nested mode

// Default PIC interrupt offsets
#define PIC1_OFFSET       0x20        // Master PIC base interrupt number
#define PIC2_OFFSET       0x28        // Slave PIC base interrupt number

// Number of IRQ lines per PIC
#define PIC_IRQS_PER_CHIP 8

// Total number of IRQ lines
#define PIC_IRQS_TOTAL    16

// IRQ lines
#define IRQ_TIMER         0           // Timer IRQ
#define IRQ_KEYBOARD      1           // Keyboard IRQ
#define IRQ_CASCADE       2           // Cascade IRQ (used internally by the PICs)
#define IRQ_COM2          3           // COM2 IRQ
#define IRQ_COM1          4           // COM1 IRQ
#define IRQ_LPT2          5           // LPT2 IRQ
#define IRQ_FLOPPY        6           // Floppy disk IRQ
#define IRQ_LPT1          7           // LPT1 IRQ (spurious)
#define IRQ_RTC           8           // Real-time clock IRQ
#define IRQ_ACPI          9           // ACPI IRQ
#define IRQ_AVAILABLE1    10          // Available IRQ
#define IRQ_AVAILABLE2    11          // Available IRQ
#define IRQ_PS2_MOUSE     12          // PS/2 mouse IRQ
#define IRQ_FPU           13          // FPU IRQ
#define IRQ_ATA_PRIMARY   14          // Primary ATA IRQ
#define IRQ_ATA_SECONDARY 15          // Secondary ATA IRQ

// IRQ mask state
static uint16_t pic_irq_mask = 0xFFFF;  // All IRQs masked (disabled) initially

/**
 * @brief Send a command to the specified PIC
 * 
 * @param pic PIC number (1 = master, 2 = slave)
 * @param cmd Command to send
 */
static void pic_send_command(uint8_t pic, uint8_t cmd) {
    uint16_t port = (pic == 1) ? PIC1_COMMAND : PIC2_COMMAND;
    outb(port, cmd);
}

/**
 * @brief Send data to the specified PIC
 * 
 * @param pic PIC number (1 = master, 2 = slave)
 * @param data Data to send
 */
static void pic_send_data(uint8_t pic, uint8_t data) {
    uint16_t port = (pic == 1) ? PIC1_DATA : PIC2_DATA;
    outb(port, data);
}

/**
 * @brief Read data from the specified PIC
 * 
 * @param pic PIC number (1 = master, 2 = slave)
 * @return Data read from PIC
 */
static uint8_t pic_read_data(uint8_t pic) {
    uint16_t port = (pic == 1) ? PIC1_DATA : PIC2_DATA;
    return inb(port);
}

/**
 * @brief Set the IRQ mask for both PICs
 * 
 * @param mask 16-bit mask (1 = masked/disabled, 0 = enabled)
 */
static void pic_set_mask(uint16_t mask) {
    pic_irq_mask = mask;
    pic_send_data(1, mask & 0xFF);         // Low byte for master PIC
    pic_send_data(2, (mask >> 8) & 0xFF);  // High byte for slave PIC
}

/**
 * @brief Mask (disable) a specific IRQ line
 * 
 * @param irq IRQ line number (0-15)
 */
void pic_mask_irq(uint8_t irq) {
    if (irq >= PIC_IRQS_TOTAL) {
        return;
    }
    
    uint16_t mask = pic_irq_mask | (1 << irq);
    pic_set_mask(mask);
}

/**
 * @brief Unmask (enable) a specific IRQ line
 * 
 * @param irq IRQ line number (0-15)
 */
void pic_unmask_irq(uint8_t irq) {
    if (irq >= PIC_IRQS_TOTAL) {
        return;
    }
    
    uint16_t mask = pic_irq_mask & ~(1 << irq);
    pic_set_mask(mask);
}

/**
 * @brief Get the combined IRQ mask for both PICs
 * 
 * @return 16-bit IRQ mask
 */
uint16_t pic_get_irq_mask(void) {
    return pic_irq_mask;
}

/**
 * @brief Read the specified register from both PICs
 * 
 * @param reg Register to read (PIC_READ_IRR or PIC_READ_ISR)
 * @return Combined 16-bit value
 */
static uint16_t pic_read_register(uint8_t reg) {
    // Send command to read the register
    pic_send_command(1, reg);
    pic_send_command(2, reg);
    
    // Read data
    uint8_t master = inb(PIC1_COMMAND);
    uint8_t slave = inb(PIC2_COMMAND);
    
    // Combine master and slave data
    return (uint16_t)master | ((uint16_t)slave << 8);
}

/**
 * @brief Get the Interrupt Request Register (IRR) from both PICs
 * 
 * @return 16-bit IRR value
 */
uint16_t pic_get_irr(void) {
    return pic_read_register(PIC_READ_IRR);
}

/**
 * @brief Get the In-Service Register (ISR) from both PICs
 * 
 * @return 16-bit ISR value
 */
uint16_t pic_get_isr(void) {
    return pic_read_register(PIC_READ_ISR);
}

/**
 * @brief Send an End of Interrupt (EOI) command to the PIC
 * 
 * @param irq IRQ line number (0-15)
 */
void pic_send_eoi(uint8_t irq) {
    // For IRQs 8-15 (slave PIC), we need to send EOI to both PICs
    if (irq >= 8) {
        pic_send_command(2, PIC_EOI);
    }
    
    // Send EOI to master PIC for all IRQs
    pic_send_command(1, PIC_EOI);
}

/**
 * @brief Check if an IRQ is spurious
 * 
 * @param irq IRQ line number (0-15)
 * @return true if the IRQ is spurious, false otherwise
 */
bool pic_is_spurious_irq(uint8_t irq) {
    // IRQ 7 and 15 can be spurious
    if (irq != 7 && irq != 15) {
        return false;
    }
    
    // Read In-Service Register
    uint16_t isr = pic_get_isr();
    
    // Check if the IRQ is actually in service
    if (irq == 7) {
        // IRQ 7 - master PIC
        return !(isr & (1 << 7));
    } else {
        // IRQ 15 - slave PIC
        return !(isr & (1 << 15));
    }
}

/**
 * @brief Handle a spurious IRQ
 * 
 * @param irq IRQ line number (7 or 15)
 */
void pic_handle_spurious_irq(uint8_t irq) {
    if (irq == 7) {
        // For IRQ 7, don't send EOI to master PIC
    } else if (irq == 15) {
        // For IRQ 15, send EOI only to master PIC
        pic_send_command(1, PIC_EOI);
    }
}

/**
 * @brief Initialize the PICs with the specified offsets
 * 
 * @param master_offset Interrupt vector offset for master PIC
 * @param slave_offset Interrupt vector offset for slave PIC
 */
static void pic_remap(uint8_t master_offset, uint8_t slave_offset) {
    // Save current IRQ mask
    uint8_t master_mask = pic_read_data(1);
    uint8_t slave_mask = pic_read_data(2);
    
    // ICW1: Initialize command, require ICW4
    pic_send_command(1, ICW1_INIT | ICW1_ICW4);
    io_wait();
    pic_send_command(2, ICW1_INIT | ICW1_ICW4);
    io_wait();
    
    // ICW2: Set interrupt vector offsets
    pic_send_data(1, master_offset);
    io_wait();
    pic_send_data(2, slave_offset);
    io_wait();
    
    // ICW3: Tell master PIC that slave PIC is at IRQ2
    pic_send_data(1, 1 << 2);  // Bit mask for IRQ2
    io_wait();
    
    // ICW3: Tell slave PIC its cascade identity
    pic_send_data(2, 2);  // Cascade identity is 2
    io_wait();
    
    // ICW4: Set 8086 mode
    pic_send_data(1, ICW4_8086);
    io_wait();
    pic_send_data(2, ICW4_8086);
    io_wait();
    
    // Restore IRQ masks
    pic_send_data(1, master_mask);
    pic_send_data(2, slave_mask);
}

/**
 * @brief Disable the PIC to use APIC instead
 */
void pic_disable(void) {
    // Mask all interrupts from both PICs
    pic_set_mask(0xFFFF);
}

/**
 * @brief Initialize the PIC
 */
void pic_init(void) {
    // Remap PICs to avoid conflicts with CPU exception vectors
    pic_remap(PIC1_OFFSET, PIC2_OFFSET);
    
    // Mask all interrupts initially
    pic_set_mask(0xFFFF);
    
    kprintf("PIC: Initialized with offsets 0x%x and 0x%x\n", PIC1_OFFSET, PIC2_OFFSET);
}
