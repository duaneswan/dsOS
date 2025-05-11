/**
 * @file serial.c
 * @brief Serial port driver for debugging output
 */

#include "../../include/kernel.h"
#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

// Serial port base addresses
#define COM1 0x3F8
#define COM2 0x2F8
#define COM3 0x3E8
#define COM4 0x2E8

// Register offsets
#define REG_DATA        0   // Data register (read/write)
#define REG_INT_ENABLE  1   // Interrupt enable register (write)
#define REG_INT_ID      2   // Interrupt identification register (read)
#define REG_FIFO_CTRL   2   // FIFO control register (write)
#define REG_LINE_CTRL   3   // Line control register
#define REG_MODEM_CTRL  4   // Modem control register
#define REG_LINE_STATUS 5   // Line status register
#define REG_MODEM_STATUS 6  // Modem status register
#define REG_SCRATCH     7   // Scratch register

// Line status register bits
#define LSR_DATA_READY  0x01  // Data ready
#define LSR_OVERRUN     0x02  // Overrun error
#define LSR_PARITY_ERR  0x04  // Parity error
#define LSR_FRAMING_ERR 0x08  // Framing error
#define LSR_BREAK       0x10  // Break signal received
#define LSR_TX_BUFFER   0x20  // Transmitter holding register empty
#define LSR_TX_EMPTY    0x40  // Transmitter empty
#define LSR_FIFO_ERR    0x80  // FIFO error

// Line control register bits
#define LCR_5BIT        0x00  // 5-bit characters
#define LCR_6BIT        0x01  // 6-bit characters
#define LCR_7BIT        0x02  // 7-bit characters
#define LCR_8BIT        0x03  // 8-bit characters
#define LCR_STOP_BIT    0x04  // Use 2 stop bits (1.5 if 5-bit characters)
#define LCR_PARITY      0x08  // Enable parity
#define LCR_EVEN_PARITY 0x10  // Use even parity (otherwise odd)
#define LCR_STICK_PARITY 0x20 // Stick parity
#define LCR_BREAK       0x40  // Set break condition
#define LCR_DLAB        0x80  // Divisor latch access bit

// FIFO control register bits
#define FCR_ENABLE      0x01  // Enable FIFO
#define FCR_CLEAR_RX    0x02  // Clear receive FIFO
#define FCR_CLEAR_TX    0x04  // Clear transmit FIFO
#define FCR_DMA_MODE    0x08  // DMA mode select
#define FCR_TRIGGER_1   0x00  // Trigger level 1 byte
#define FCR_TRIGGER_4   0x40  // Trigger level 4 bytes
#define FCR_TRIGGER_8   0x80  // Trigger level 8 bytes
#define FCR_TRIGGER_14  0xC0  // Trigger level 14 bytes

// Modem control register bits
#define MCR_DTR         0x01  // Data terminal ready
#define MCR_RTS         0x02  // Request to send
#define MCR_OUT1        0x04  // User-defined output 1
#define MCR_OUT2        0x08  // User-defined output 2 (must be set for interrupts)
#define MCR_LOOPBACK    0x10  // Loopback mode

// Driver state
uint16_t debug_port = COM1;
bool serial_initialized = false;

/**
 * @brief Check if the serial port exists and is functional
 * 
 * @param port Base address of the serial port
 * @return true if the port exists, false otherwise
 */
static bool serial_port_exists(uint16_t port) {
    // Save original modem control register value
    uint8_t original = inb(port + REG_MODEM_CTRL);
    
    // Try to set and verify some bits
    outb(port + REG_MODEM_CTRL, 0x1E); // 00011110 - All bits except loopback
    io_wait();
    
    if (inb(port + REG_MODEM_CTRL) != 0x1E) {
        // Restore original value
        outb(port + REG_MODEM_CTRL, original);
        return false;
    }
    
    // Try to set and verify different bits
    outb(port + REG_MODEM_CTRL, 0x0F); // 00001111 - Different bits
    io_wait();
    
    if (inb(port + REG_MODEM_CTRL) != 0x0F) {
        // Restore original value
        outb(port + REG_MODEM_CTRL, original);
        return false;
    }
    
    // Restore original value
    outb(port + REG_MODEM_CTRL, original);
    return true;
}

/**
 * @brief Set the baud rate for the serial port
 * 
 * @param port Base address of the serial port
 * @param divisor Divisor for the baud rate (115200 / desired_baud_rate)
 */
static void serial_set_baud(uint16_t port, uint16_t divisor) {
    // Get current line control register value
    uint8_t lcr = inb(port + REG_LINE_CTRL);
    
    // Set DLAB (Divisor Latch Access Bit) to access divisor latches
    outb(port + REG_LINE_CTRL, lcr | LCR_DLAB);
    
    // Set divisor (low byte then high byte)
    outb(port + REG_DATA, divisor & 0xFF);
    outb(port + REG_INT_ENABLE, (divisor >> 8) & 0xFF);
    
    // Clear DLAB to access normal registers
    outb(port + REG_LINE_CTRL, lcr & ~LCR_DLAB);
}

/**
 * @brief Initialize the serial port for debugging
 * 
 * @param port Base address of the serial port to initialize
 * @param baud_rate Baud rate in bps (bits per second)
 * @return true if initialization succeeded, false otherwise
 */
static bool serial_init_port(uint16_t port, uint32_t baud_rate) {
    // Check if the port exists
    if (!serial_port_exists(port)) {
        return false;
    }
    
    // Calculate divisor for baud rate
    uint16_t divisor = 115200 / baud_rate;
    
    // Disable interrupts
    outb(port + REG_INT_ENABLE, 0x00);
    
    // Set baud rate
    serial_set_baud(port, divisor);
    
    // Set line control: 8 bits, no parity, 1 stop bit
    outb(port + REG_LINE_CTRL, LCR_8BIT);
    
    // Enable and configure FIFO
    outb(port + REG_FIFO_CTRL, FCR_ENABLE | FCR_CLEAR_RX | FCR_CLEAR_TX | FCR_TRIGGER_14);
    
    // Set modem control: DTR, RTS, OUT2
    outb(port + REG_MODEM_CTRL, MCR_DTR | MCR_RTS | MCR_OUT2);
    
    // Verify FIFO is working by putting the controller in loopback mode
    outb(port + REG_MODEM_CTRL, inb(port + REG_MODEM_CTRL) | MCR_LOOPBACK);
    outb(port + REG_DATA, 0x55);
    io_wait();
    
    if (inb(port + REG_DATA) != 0x55) {
        // FIFO test failed
        outb(port + REG_MODEM_CTRL, inb(port + REG_MODEM_CTRL) & ~MCR_LOOPBACK);
        return false;
    }
    
    // Disable loopback and enable normal operation
    outb(port + REG_MODEM_CTRL, MCR_DTR | MCR_RTS | MCR_OUT2);
    
    return true;
}

/**
 * @brief Check if the serial transmitter is ready to send data
 * 
 * @param port Base address of the serial port
 * @return true if the transmitter is ready, false otherwise
 */
static bool serial_is_transmit_ready(uint16_t port) {
    return (inb(port + REG_LINE_STATUS) & LSR_TX_BUFFER) != 0;
}

/**
 * @brief Check if there is data available to read from the serial port
 * 
 * @param port Base address of the serial port
 * @return true if data is available, false otherwise
 */
static bool serial_is_data_ready(uint16_t port) {
    return (inb(port + REG_LINE_STATUS) & LSR_DATA_READY) != 0;
}

/**
 * @brief Write a byte to the serial port
 * 
 * @param port Base address of the serial port
 * @param data Byte to write
 */
void serial_write_byte(uint16_t port, uint8_t data) {
    // Wait for transmitter to be ready
    while (!serial_is_transmit_ready(port)) {
        // Add a small delay to avoid tight loops
        io_wait();
    }
    
    // Write the data byte
    outb(port + REG_DATA, data);
}

/**
 * @brief Read a byte from the serial port
 * 
 * @param port Base address of the serial port
 * @return Byte read from the port, or 0 if no data available
 */
uint8_t serial_read_byte(uint16_t port) {
    // Wait for data to be available
    while (!serial_is_data_ready(port)) {
        // Add a small delay to avoid tight loops
        io_wait();
    }
    
    // Read the data byte
    return inb(port + REG_DATA);
}

/**
 * @brief Write a string to the serial port
 * 
 * @param port Base address of the serial port
 * @param str Null-terminated string to write
 */
void serial_write_string(uint16_t port, const char* str) {
    while (*str) {
        // Convert LF to CRLF for terminal compatibility
        if (*str == '\n') {
            serial_write_byte(port, '\r');
        }
        serial_write_byte(port, *str++);
    }
}

/**
 * @brief Initialize the serial driver
 */
void serial_init(void) {
    // Try to initialize COM ports in order
    if (serial_init_port(COM1, 115200)) {
        debug_port = COM1;
        serial_initialized = true;
    } else if (serial_init_port(COM2, 115200)) {
        debug_port = COM2;
        serial_initialized = true;
    } else if (serial_init_port(COM3, 115200)) {
        debug_port = COM3;
        serial_initialized = true;
    } else if (serial_init_port(COM4, 115200)) {
        debug_port = COM4;
        serial_initialized = true;
    } else {
        // No COM port found
        serial_initialized = false;
        return;
    }
    
    // Send initial message
    serial_write_string(debug_port, "\r\n--- dsOS Serial Debug Console ---\r\n");
    serial_write_string(debug_port, "Serial port initialized at 115200 bps\r\n");
}
