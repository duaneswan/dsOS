/**
 * @file serial.c
 * @brief Serial port driver implementation for debugging
 */

#include "../../include/kernel.h"
#include <stdint.h>
#include <stdbool.h>

// Serial port addresses
#define SERIAL_COM1       0x3F8      // COM1 base port
#define SERIAL_COM2       0x2F8      // COM2 base port
#define SERIAL_COM3       0x3E8      // COM3 base port
#define SERIAL_COM4       0x2E8      // COM4 base port

// Serial port register offsets
#define SERIAL_DATA       0          // Data register (R/W)
#define SERIAL_INTR_EN    1          // Interrupt enable register (W)
#define SERIAL_DIVISOR_LO 0          // Divisor latch low byte (W)
#define SERIAL_DIVISOR_HI 1          // Divisor latch high byte (W)
#define SERIAL_FIFO_CTRL  2          // FIFO control register (W)
#define SERIAL_LINE_CTRL  3          // Line control register (W)
#define SERIAL_MODEM_CTRL 4          // Modem control register (W)
#define SERIAL_LINE_STAT  5          // Line status register (R)
#define SERIAL_MODEM_STAT 6          // Modem status register (R)
#define SERIAL_SCRATCH    7          // Scratch register (R/W)

// Line control register bits
#define SERIAL_LCR_5BITS  0x00       // 5 data bits
#define SERIAL_LCR_6BITS  0x01       // 6 data bits
#define SERIAL_LCR_7BITS  0x02       // 7 data bits
#define SERIAL_LCR_8BITS  0x03       // 8 data bits
#define SERIAL_LCR_STOP   0x04       // 2 stop bits (instead of 1)
#define SERIAL_LCR_PARITY 0x08       // Enable parity
#define SERIAL_LCR_EVEN   0x10       // Even parity (vs odd)
#define SERIAL_LCR_STICK  0x20       // Stick parity
#define SERIAL_LCR_BREAK  0x40       // Set break
#define SERIAL_LCR_DLAB   0x80       // Divisor latch access

// FIFO control register bits
#define SERIAL_FCR_ENABLE     0x01   // Enable FIFO
#define SERIAL_FCR_CLEAR_RX   0x02   // Clear receive FIFO
#define SERIAL_FCR_CLEAR_TX   0x04   // Clear transmit FIFO
#define SERIAL_FCR_DMA        0x08   // DMA mode
#define SERIAL_FCR_TRIGGER_1  0x00   // Trigger level 1 byte
#define SERIAL_FCR_TRIGGER_4  0x40   // Trigger level 4 bytes
#define SERIAL_FCR_TRIGGER_8  0x80   // Trigger level 8 bytes
#define SERIAL_FCR_TRIGGER_14 0xC0   // Trigger level 14 bytes

// Modem control register bits
#define SERIAL_MCR_DTR        0x01   // Data terminal ready
#define SERIAL_MCR_RTS        0x02   // Request to send
#define SERIAL_MCR_OUT1       0x04   // OUT1
#define SERIAL_MCR_OUT2       0x08   // OUT2 (enables interrupts)
#define SERIAL_MCR_LOOP       0x10   // Loopback mode

// Line status register bits
#define SERIAL_LSR_DR         0x01   // Data ready
#define SERIAL_LSR_OE         0x02   // Overrun error
#define SERIAL_LSR_PE         0x04   // Parity error
#define SERIAL_LSR_FE         0x08   // Framing error
#define SERIAL_LSR_BI         0x10   // Break indicator
#define SERIAL_LSR_THRE       0x20   // Transmitter holding register empty
#define SERIAL_LSR_TEMT       0x40   // Transmitter empty
#define SERIAL_LSR_ERR        0x80   // Error in FIFO

// Common baud rates
#define SERIAL_BAUD_115200    1      // 115200 bps
#define SERIAL_BAUD_57600     2      // 57600 bps
#define SERIAL_BAUD_38400     3      // 38400 bps
#define SERIAL_BAUD_19200     6      // 19200 bps
#define SERIAL_BAUD_9600      12     // 9600 bps
#define SERIAL_BAUD_4800      24     // 4800 bps
#define SERIAL_BAUD_2400      48     // 2400 bps
#define SERIAL_BAUD_1200      96     // 1200 bps

// Active serial port for debugging
static uint16_t debug_port = SERIAL_COM1;
static bool serial_initialized = false;

/**
 * @brief Initialize a serial port
 * 
 * @param port Serial port base address (e.g. SERIAL_COM1)
 * @param baud Baud rate divisor (e.g. SERIAL_BAUD_9600)
 * @return true if initialization was successful, false otherwise
 */
bool serial_init_port(uint16_t port, uint16_t baud) {
    // Disable interrupts
    outb(port + SERIAL_INTR_EN, 0x00);
    
    // Set DLAB to access divisor latches
    outb(port + SERIAL_LINE_CTRL, SERIAL_LCR_DLAB);
    
    // Set divisor (low byte then high byte)
    outb(port + SERIAL_DIVISOR_LO, baud & 0xFF);
    outb(port + SERIAL_DIVISOR_HI, (baud >> 8) & 0xFF);
    
    // 8 data bits, no parity, 1 stop bit
    outb(port + SERIAL_LINE_CTRL, SERIAL_LCR_8BITS);
    
    // Enable FIFO, clear buffers, and set trigger level
    outb(port + SERIAL_FIFO_CTRL, SERIAL_FCR_ENABLE | SERIAL_FCR_CLEAR_RX | 
                                   SERIAL_FCR_CLEAR_TX | SERIAL_FCR_TRIGGER_14);
    
    // DTR, RTS, and OUT2 enabled (normal operation)
    outb(port + SERIAL_MODEM_CTRL, SERIAL_MCR_DTR | SERIAL_MCR_RTS | SERIAL_MCR_OUT2);
    
    // Test the serial port (loopback test)
    outb(port + SERIAL_MODEM_CTRL, SERIAL_MCR_LOOP | SERIAL_MCR_DTR | SERIAL_MCR_RTS);
    
    // Send a test byte
    outb(port + SERIAL_DATA, 0xAE);
    
    // Check if we received the same byte
    if (inb(port + SERIAL_DATA) != 0xAE) {
        // Failed loopback test
        return false;
    }
    
    // Return to normal operation
    outb(port + SERIAL_MODEM_CTRL, SERIAL_MCR_DTR | SERIAL_MCR_RTS | SERIAL_MCR_OUT2);
    
    return true;
}

/**
 * @brief Check if the serial transmitter is empty
 * 
 * @param port Serial port base address
 * @return true if transmitter is empty, false otherwise
 */
static bool serial_transmit_empty(uint16_t port) {
    return (inb(port + SERIAL_LINE_STAT) & SERIAL_LSR_THRE) != 0;
}

/**
 * @brief Send a byte to the serial port
 * 
 * @param port Serial port base address
 * @param data Byte to send
 */
void serial_write_byte(uint16_t port, uint8_t data) {
    // Wait until the transmit buffer is empty
    while (!serial_transmit_empty(port)) {
        // Do nothing, busy wait
    }
    
    // Send the byte
    outb(port + SERIAL_DATA, data);
}

/**
 * @brief Check if data is available to read from the serial port
 * 
 * @param port Serial port base address
 * @return true if data is available, false otherwise
 */
static bool serial_data_ready(uint16_t port) {
    return (inb(port + SERIAL_LINE_STAT) & SERIAL_LSR_DR) != 0;
}

/**
 * @brief Read a byte from the serial port
 * 
 * @param port Serial port base address
 * @return Byte read from the serial port, or 0 if no data available
 */
uint8_t serial_read_byte(uint16_t port) {
    // Check if data is available
    if (!serial_data_ready(port)) {
        return 0;
    }
    
    // Read the byte
    return inb(port + SERIAL_DATA);
}

/**
 * @brief Write a string to the serial port
 * 
 * @param port Serial port base address
 * @param str String to write
 */
void serial_write_string(uint16_t port, const char* str) {
    for (size_t i = 0; str[i] != '\0'; i++) {
        serial_write_byte(port, str[i]);
    }
}

/**
 * @brief Write a string to the debug serial port
 * 
 * @param str String to write
 */
void serial_debug_print(const char* str) {
    if (serial_initialized) {
        serial_write_string(debug_port, str);
    }
}

/**
 * @brief Write a formatted string to the debug serial port
 * 
 * @param fmt Format string
 * @param ... Arguments to format
 */
void serial_debug_printf(const char* fmt, ...) {
    if (!serial_initialized) {
        return;
    }
    
    char buffer[1024];
    va_list args;
    va_start(args, fmt);
    int len = vsnprintf(buffer, sizeof(buffer), fmt, args);
    va_end(args);
    
    // Ensure null termination
    buffer[1023] = '\0';
    
    serial_write_string(debug_port, buffer);
}

/**
 * @brief Initialize the serial ports for debugging
 */
void serial_init(void) {
    // Initialize COM1 at 38400 bps
    if (serial_init_port(SERIAL_COM1, SERIAL_BAUD_38400)) {
        debug_port = SERIAL_COM1;
        serial_initialized = true;
        serial_debug_print("Serial: COM1 initialized at 38400 bps\r\n");
    } else {
        // Try COM2 if COM1 failed
        if (serial_init_port(SERIAL_COM2, SERIAL_BAUD_38400)) {
            debug_port = SERIAL_COM2;
            serial_initialized = true;
            serial_debug_print("Serial: COM2 initialized at 38400 bps\r\n");
        } else {
            // Both COM1 and COM2 failed
            serial_initialized = false;
        }
    }
}
