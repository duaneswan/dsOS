/**
 * @file serial.c
 * @brief Serial port driver implementation
 */

#include "../../include/kernel.h"
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

// Serial port registers
#define SERIAL_PORT_COM1 0x3F8
#define SERIAL_PORT_COM2 0x2F8
#define SERIAL_PORT_COM3 0x3E8
#define SERIAL_PORT_COM4 0x2E8

// Serial port register offsets
#define SERIAL_REG_DATA          0 // Data register (RW)
#define SERIAL_REG_INT_ENABLE    1 // Interrupt enable (W)
#define SERIAL_REG_FIFO_CTRL     2 // FIFO control register (W)
#define SERIAL_REG_LINE_CTRL     3 // Line control register (W)
#define SERIAL_REG_MODEM_CTRL    4 // Modem control register (W)
#define SERIAL_REG_LINE_STATUS   5 // Line status register (R)
#define SERIAL_REG_MODEM_STATUS  6 // Modem status register (R)
#define SERIAL_REG_SCRATCH       7 // Scratch register (RW)

// Line control register bits
#define SERIAL_LCR_5BITS         0x00 // 5 data bits
#define SERIAL_LCR_6BITS         0x01 // 6 data bits
#define SERIAL_LCR_7BITS         0x02 // 7 data bits
#define SERIAL_LCR_8BITS         0x03 // 8 data bits
#define SERIAL_LCR_STOP          0x04 // Stop bits (0=1, 1=2)
#define SERIAL_LCR_PARITY        0x08 // Parity enable
#define SERIAL_LCR_EVEN_PARITY   0x10 // Even parity
#define SERIAL_LCR_STICK_PARITY  0x20 // Stick parity
#define SERIAL_LCR_BREAK         0x40 // Break enable
#define SERIAL_LCR_DLAB          0x80 // Divisor latch access bit

// FIFO control register bits
#define SERIAL_FCR_ENABLE        0x01 // FIFO enable
#define SERIAL_FCR_CLEAR_RECV    0x02 // Receiver FIFO reset
#define SERIAL_FCR_CLEAR_XMIT    0x04 // Transmitter FIFO reset
#define SERIAL_FCR_DMA           0x08 // DMA mode select
#define SERIAL_FCR_TRIG_1        0x00 // Trigger level (1 byte)
#define SERIAL_FCR_TRIG_4        0x40 // Trigger level (4 bytes)
#define SERIAL_FCR_TRIG_8        0x80 // Trigger level (8 bytes)
#define SERIAL_FCR_TRIG_14       0xC0 // Trigger level (14 bytes)

// Modem control register bits
#define SERIAL_MCR_DTR           0x01 // Data terminal ready
#define SERIAL_MCR_RTS           0x02 // Request to send
#define SERIAL_MCR_OUT1          0x04 // Output 1
#define SERIAL_MCR_OUT2          0x08 // Output 2 (enables IRQs)
#define SERIAL_MCR_LOOPBACK      0x10 // Loopback mode

// Line status register bits
#define SERIAL_LSR_DATA_READY    0x01 // Data ready
#define SERIAL_LSR_OVERRUN_ERR   0x02 // Overrun error
#define SERIAL_LSR_PARITY_ERR    0x04 // Parity error
#define SERIAL_LSR_FRAMING_ERR   0x08 // Framing error
#define SERIAL_LSR_BREAK         0x10 // Break indicator
#define SERIAL_LSR_THRE          0x20 // Transmitter holding register empty
#define SERIAL_LSR_TEMT          0x40 // Transmitter empty
#define SERIAL_LSR_FIFO_ERR      0x80 // FIFO error

// Default baud rates
#define SERIAL_BAUD_115200       1     // 115200 bps
#define SERIAL_BAUD_57600        2     // 57600 bps
#define SERIAL_BAUD_38400        3     // 38400 bps
#define SERIAL_BAUD_19200        6     // 19200 bps
#define SERIAL_BAUD_9600         12    // 9600 bps
#define SERIAL_BAUD_4800         24    // 4800 bps
#define SERIAL_BAUD_2400         48    // 2400 bps
#define SERIAL_BAUD_1200         96    // 1200 bps

// Serial port state
uint16_t debug_port = SERIAL_PORT_COM1;
bool serial_initialized = false;

/**
 * @brief Initialize a serial port
 * 
 * @param port Serial port (e.g., SERIAL_PORT_COM1)
 */
void serial_init(uint16_t port) {
    // Disable interrupts
    outb(port + SERIAL_REG_INT_ENABLE, 0x00);
    
    // Enable DLAB (set baud rate divisor)
    outb(port + SERIAL_REG_LINE_CTRL, SERIAL_LCR_DLAB);
    
    // Set divisor to 1 (115200 baud)
    outb(port + SERIAL_REG_DATA, SERIAL_BAUD_115200);
    outb(port + SERIAL_REG_INT_ENABLE, 0x00);
    
    // 8 data bits, no parity, one stop bit
    outb(port + SERIAL_REG_LINE_CTRL, SERIAL_LCR_8BITS);
    
    // Enable FIFO, clear them, with 14-byte threshold
    outb(port + SERIAL_REG_FIFO_CTRL, SERIAL_FCR_ENABLE | SERIAL_FCR_CLEAR_RECV | 
                                      SERIAL_FCR_CLEAR_XMIT | SERIAL_FCR_TRIG_14);
    
    // IRQs enabled, RTS/DSR set
    outb(port + SERIAL_REG_MODEM_CTRL, SERIAL_MCR_DTR | SERIAL_MCR_RTS | SERIAL_MCR_OUT2);
    
    // Set loopback mode, test the serial chip
    outb(port + SERIAL_REG_MODEM_CTRL, SERIAL_MCR_LOOPBACK);
    
    // Send a test byte
    outb(port + SERIAL_REG_DATA, 0xAE);
    
    // Check if we receive the test byte back
    if (inb(port + SERIAL_REG_DATA) != 0xAE) {
        // Serial port failed the test
        return;
    }
    
    // Disable loopback, enable normal operation
    outb(port + SERIAL_REG_MODEM_CTRL, SERIAL_MCR_DTR | SERIAL_MCR_RTS | SERIAL_MCR_OUT2);
    
    // Save the debug port
    debug_port = port;
    serial_initialized = true;
}

/**
 * @brief Check if the transmit buffer is empty
 * 
 * @param port Serial port
 * @return true if empty, false otherwise
 */
bool serial_is_transmit_empty(uint16_t port) {
    return inb(port + SERIAL_REG_LINE_STATUS) & SERIAL_LSR_THRE;
}

/**
 * @brief Check if data is available to read
 * 
 * @param port Serial port
 * @return true if data is available, false otherwise
 */
bool serial_has_received(uint16_t port) {
    return inb(port + SERIAL_REG_LINE_STATUS) & SERIAL_LSR_DATA_READY;
}

/**
 * @brief Write a byte to a serial port
 * 
 * @param port Serial port
 * @param byte Byte to write
 */
void serial_write_byte(uint16_t port, uint8_t byte) {
    // Wait for the transmit buffer to be empty
    while (!serial_is_transmit_empty(port));
    
    // Send the byte
    outb(port, byte);
}

/**
 * @brief Read a byte from a serial port
 * 
 * @param port Serial port
 * @return Byte read
 */
uint8_t serial_read_byte(uint16_t port) {
    // Wait for data to be available
    while (!serial_has_received(port));
    
    // Read the byte
    return inb(port);
}

/**
 * @brief Write a string to a serial port
 * 
 * @param port Serial port
 * @param str String to write
 */
void serial_write_str(uint16_t port, const char* str) {
    while (*str) {
        serial_write_byte(port, *str++);
    }
}

/**
 * @brief Set the baud rate of the serial port
 * 
 * @param port Serial port
 * @param divisor Baud rate divisor
 */
void serial_set_baud_rate(uint16_t port, uint16_t divisor) {
    // Enable DLAB (set baud rate divisor)
    outb(port + SERIAL_REG_LINE_CTRL, SERIAL_LCR_DLAB);
    
    // Set divisor (lo/hi bytes)
    outb(port + SERIAL_REG_DATA, divisor & 0xFF);
    outb(port + SERIAL_REG_INT_ENABLE, (divisor >> 8) & 0xFF);
    
    // Restore line control register
    outb(port + SERIAL_REG_LINE_CTRL, SERIAL_LCR_8BITS);
}
