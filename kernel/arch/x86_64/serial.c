/**
 * @file serial.c
 * @brief Serial port driver
 */

#include "../../include/kernel.h"
#include <stdarg.h>
#include <stdbool.h>

// COM port base addresses
#define COM1_PORT 0x3F8
#define COM2_PORT 0x2F8
#define COM3_PORT 0x3E8
#define COM4_PORT 0x2E8

// UART register offsets
#define UART_DATA        0x00  // Data register (R/W)
#define UART_INT_ENABLE  0x01  // Interrupt enable (R/W)
#define UART_FIFO_CTRL   0x02  // FIFO control (W)
#define UART_LINE_CTRL   0x03  // Line control (R/W)
#define UART_MODEM_CTRL  0x04  // Modem control (R/W)
#define UART_LINE_STATUS 0x05  // Line status (R)
#define UART_MODEM_STATUS 0x06 // Modem status (R)
#define UART_SCRATCH     0x07  // Scratch register (R/W)
#define UART_DIV_LOW     0x00  // Divisor latch low byte (R/W)
#define UART_DIV_HIGH    0x01  // Divisor latch high byte (R/W)

// UART line status register bits
#define UART_LSR_DR      0x01  // Data ready
#define UART_LSR_OE      0x02  // Overrun error
#define UART_LSR_PE      0x04  // Parity error
#define UART_LSR_FE      0x08  // Framing error
#define UART_LSR_BI      0x10  // Break interrupt
#define UART_LSR_THRE    0x20  // Transmitter holding register empty
#define UART_LSR_TEMT    0x40  // Transmitter empty
#define UART_LSR_FIFO_ERR 0x80 // FIFO error

// UART line control register bits
#define UART_LCR_CS5     0x00  // 5 bits per char
#define UART_LCR_CS6     0x01  // 6 bits per char
#define UART_LCR_CS7     0x02  // 7 bits per char
#define UART_LCR_CS8     0x03  // 8 bits per char
#define UART_LCR_STOP1   0x00  // 1 stop bit
#define UART_LCR_STOP2   0x04  // 2 stop bits (or 1.5 depending on char size)
#define UART_LCR_NO_PARITY 0x00 // No parity
#define UART_LCR_ODD_PARITY 0x08 // Odd parity
#define UART_LCR_EVEN_PARITY 0x18 // Even parity
#define UART_LCR_MARK_PARITY 0x28 // Mark parity
#define UART_LCR_SPACE_PARITY 0x38 // Space parity
#define UART_LCR_BREAK   0x40  // Set break condition
#define UART_LCR_DLAB    0x80  // Divisor latch access bit

// UART FIFO control register bits
#define UART_FCR_ENABLE  0x01  // Enable FIFO
#define UART_FCR_CLEAR_RX 0x02 // Clear receive FIFO
#define UART_FCR_CLEAR_TX 0x04 // Clear transmit FIFO
#define UART_FCR_DMA     0x08  // DMA mode select
#define UART_FCR_TRIGGER_1 0x00 // Trigger level 1 (1 byte)
#define UART_FCR_TRIGGER_4 0x40 // Trigger level 2 (4 bytes)
#define UART_FCR_TRIGGER_8 0x80 // Trigger level 3 (8 bytes)
#define UART_FCR_TRIGGER_14 0xC0 // Trigger level 4 (14 bytes)

// UART modem control register bits
#define UART_MCR_DTR     0x01  // Data terminal ready
#define UART_MCR_RTS     0x02  // Request to send
#define UART_MCR_OUT1    0x04  // Auxiliary output 1
#define UART_MCR_OUT2    0x08  // Auxiliary output 2 (enables UART interrupts)
#define UART_MCR_LOOPBACK 0x10 // Loopback mode

// Serial port structure
struct serial_port_t {
    uint16_t port;             // Base I/O port address
    uint32_t baud_rate;        // Current baud rate
    bool initialized;          // Whether the port is initialized
    uint8_t line_config;       // Line configuration
};

// Default debug port (COM1)
serial_port_t* debug_port = NULL;

// Available COM ports
static serial_port_t com_ports[4] = {
    { COM1_PORT, 0, false, 0 },
    { COM2_PORT, 0, false, 0 },
    { COM3_PORT, 0, false, 0 },
    { COM4_PORT, 0, false, 0 }
};

/**
 * @brief Check if a serial port exists
 * 
 * @param port Base port address
 * @return true if port exists, false otherwise
 */
static bool serial_port_exists(uint16_t port) {
    // Save original values
    uint8_t original_mcr = inb(port + UART_MODEM_CTRL);
    uint8_t original_scratch = inb(port + UART_SCRATCH);
    
    // Try to write and read from scratch register
    outb(port + UART_MODEM_CTRL, 0);  // Turn off all
    outb(port + UART_SCRATCH, 0x55);
    if (inb(port + UART_SCRATCH) != 0x55) {
        // Restore original values
        outb(port + UART_MODEM_CTRL, original_mcr);
        return false;
    }
    
    // Test with a different value
    outb(port + UART_SCRATCH, 0xAA);
    if (inb(port + UART_SCRATCH) != 0xAA) {
        // Restore original values
        outb(port + UART_MODEM_CTRL, original_mcr);
        outb(port + UART_SCRATCH, original_scratch);
        return false;
    }
    
    // Restore original values
    outb(port + UART_MODEM_CTRL, original_mcr);
    outb(port + UART_SCRATCH, original_scratch);
    return true;
}

/**
 * @brief Calculate divisor for the given baud rate
 * 
 * @param baud_rate Desired baud rate
 * @return Divisor value
 */
static uint16_t serial_get_divisor(uint32_t baud_rate) {
    const uint32_t base_clock = 115200;  // Base clock frequency for 16550 UART
    return base_clock / baud_rate;
}

/**
 * @brief Initialize a serial port
 * 
 * @param port Base port address
 * @param baud_rate Desired baud rate
 * @return Pointer to the initialized port, or NULL on failure
 */
serial_port_t* serial_init(uint16_t port, uint32_t baud_rate) {
    // Find the port in our array
    serial_port_t* serial_port = NULL;
    for (int i = 0; i < 4; i++) {
        if (com_ports[i].port == port) {
            serial_port = &com_ports[i];
            break;
        }
    }
    
    // Check if we found the port and if it exists
    if (serial_port == NULL || !serial_port_exists(port)) {
        return NULL;
    }
    
    // Skip if already initialized
    if (serial_port->initialized) {
        return serial_port;
    }
    
    // Calculate divisor for baud rate
    uint16_t divisor = serial_get_divisor(baud_rate);
    
    // Disable interrupts
    outb(port + UART_INT_ENABLE, 0x00);
    
    // Set DLAB (Divisor Latch Access Bit) to access divisor
    outb(port + UART_LINE_CTRL, UART_LCR_DLAB);
    
    // Set divisor (low byte and high byte)
    outb(port + UART_DIV_LOW, divisor & 0xFF);
    outb(port + UART_DIV_HIGH, (divisor >> 8) & 0xFF);
    
    // 8 bits, no parity, 1 stop bit
    uint8_t line_config = UART_LCR_CS8 | UART_LCR_NO_PARITY | UART_LCR_STOP1;
    outb(port + UART_LINE_CTRL, line_config);
    
    // Enable FIFO, clear them, 14-byte threshold
    outb(port + UART_FIFO_CTRL, UART_FCR_ENABLE | UART_FCR_CLEAR_RX | UART_FCR_CLEAR_TX | UART_FCR_TRIGGER_14);
    
    // IRQs enabled, RTS/DSR set, Aux output 2 (required for interrupts)
    outb(port + UART_MODEM_CTRL, UART_MCR_DTR | UART_MCR_RTS | UART_MCR_OUT2);
    
    // Update port information
    serial_port->initialized = true;
    serial_port->baud_rate = baud_rate;
    serial_port->line_config = line_config;
    
    return serial_port;
}

/**
 * @brief Initialize all available COM ports
 */
void serial_init_all(void) {
    // Try to initialize COM1-COM4 at standard baud rates
    for (int i = 0; i < 4; i++) {
        serial_port_t* port = serial_init(com_ports[i].port, 115200);
        if (port != NULL && debug_port == NULL && i == 0) {
            // Set COM1 as debug port if available
            debug_port = port;
            
            // Print debug info to serial
            serial_write_str(debug_port, "\r\n\r\n");
            serial_write_str(debug_port, "===================================\r\n");
            serial_write_str(debug_port, "dsOS Serial Debug Console Activated\r\n");
            serial_write_str(debug_port, "===================================\r\n");
        }
    }
}

/**
 * @brief Check if a serial port is initialized
 * 
 * @param port Serial port to check
 * @return true if initialized, false otherwise
 */
bool serial_is_initialized(serial_port_t* port) {
    return (port != NULL && port->initialized);
}

/**
 * @brief Check if the transmitter is empty
 * 
 * @param port Serial port to check
 * @return true if empty (ready to transmit), false otherwise
 */
static bool serial_transmitter_empty(serial_port_t* port) {
    return (inb(port->port + UART_LINE_STATUS) & UART_LSR_THRE) != 0;
}

/**
 * @brief Check if there is received data available
 * 
 * @param port Serial port to check
 * @return true if data is available, false otherwise
 */
static bool serial_data_ready(serial_port_t* port) {
    return (inb(port->port + UART_LINE_STATUS) & UART_LSR_DR) != 0;
}

/**
 * @brief Write a byte to the serial port
 * 
 * @param port Serial port to write to
 * @param c Byte to write
 * @return true on success, false on failure
 */
bool serial_write_char(serial_port_t* port, char c) {
    if (!serial_is_initialized(port)) {
        return false;
    }
    
    // Wait for transmitter to be empty
    while (!serial_transmitter_empty(port)) {
        // Could add a timeout here
    }
    
    // Send the byte
    outb(port->port + UART_DATA, c);
    
    return true;
}

/**
 * @brief Write a string to the serial port
 * 
 * @param port Serial port to write to
 * @param str String to write
 * @return true on success, false on failure
 */
bool serial_write_str(serial_port_t* port, const char* str) {
    if (!serial_is_initialized(port)) {
        return false;
    }
    
    while (*str) {
        // Handle CR+LF for newlines
        if (*str == '\n') {
            serial_write_char(port, '\r');
        }
        
        if (!serial_write_char(port, *str++)) {
            return false;
        }
    }
    
    return true;
}

/**
 * @brief Read a byte from the serial port
 * 
 * @param port Serial port to read from
 * @return Character read, or -1 on failure
 */
int serial_read_char(serial_port_t* port) {
    if (!serial_is_initialized(port)) {
        return -1;
    }
    
    // Wait for data to be available
    if (!serial_data_ready(port)) {
        return -1;  // No data available
    }
    
    // Read the byte
    return inb(port->port + UART_DATA);
}

/**
 * @brief Write formatted output to the serial port
 * 
 * @param port Serial port to write to
 * @param format Format string
 * @param ... Arguments
 * @return true on success, false on failure
 */
bool serial_printf(serial_port_t* port, const char* format, ...) {
    if (!serial_is_initialized(port)) {
        return false;
    }
    
    char buffer[1024];  // Buffer for formatted string
    va_list args;
    
    // Format the string
    va_start(args, format);
    int len = vsnprintf(buffer, sizeof(buffer), format, args);
    va_end(args);
    
    if (len < 0) {
        return false;
    }
    
    // Write the formatted string
    return serial_write_str(port, buffer);
}

/**
 * @brief Test the serial port using loopback mode
 * 
 * @param port Serial port to test
 * @return true if test passes, false otherwise
 */
bool serial_test(serial_port_t* port) {
    if (!serial_is_initialized(port)) {
        return false;
    }
    
    // Save original modem control register value
    uint8_t original_mcr = inb(port->port + UART_MODEM_CTRL);
    
    // Enable loopback mode
    outb(port->port + UART_MODEM_CTRL, UART_MCR_LOOPBACK);
    
    // Test with a few characters
    const char test_chars[] = { 'A', 'B', 'C', 'D', 'E' };
    for (size_t i = 0; i < sizeof(test_chars); i++) {
        // Write character
        outb(port->port + UART_DATA, test_chars[i]);
        
        // Read it back (should be the same in loopback mode)
        uint8_t received = inb(port->port + UART_DATA);
        
        if (received != test_chars[i]) {
            // Test failed, restore MCR and return false
            outb(port->port + UART_MODEM_CTRL, original_mcr);
            return false;
        }
    }
    
    // Restore original MCR value
    outb(port->port + UART_MODEM_CTRL, original_mcr);
    
    return true;
}

/**
 * @brief Set the serial port parameters
 * 
 * @param port Serial port to configure
 * @param baud_rate Baud rate to set
 * @param data_bits Data bits (5-8)
 * @param stop_bits Stop bits (1-2)
 * @param parity Parity mode (0 = none, 1 = odd, 2 = even)
 * @return true on success, false on failure
 */
bool serial_configure(serial_port_t* port, uint32_t baud_rate, int data_bits, int stop_bits, int parity) {
    if (!serial_is_initialized(port)) {
        return false;
    }
    
    // Calculate divisor for baud rate
    uint16_t divisor = serial_get_divisor(baud_rate);
    
    // Determine line control value
    uint8_t line_config = 0;
    
    // Set data bits
    switch (data_bits) {
        case 5: line_config |= UART_LCR_CS5; break;
        case 6: line_config |= UART_LCR_CS6; break;
        case 7: line_config |= UART_LCR_CS7; break;
        case 8: line_config |= UART_LCR_CS8; break;
        default: return false;  // Invalid data bits
    }
    
    // Set stop bits
    if (stop_bits == 1) {
        line_config |= UART_LCR_STOP1;
    } else if (stop_bits == 2) {
        line_config |= UART_LCR_STOP2;
    } else {
        return false;  // Invalid stop bits
    }
    
    // Set parity
    switch (parity) {
        case 0: line_config |= UART_LCR_NO_PARITY; break;
        case 1: line_config |= UART_LCR_ODD_PARITY; break;
        case 2: line_config |= UART_LCR_EVEN_PARITY; break;
        default: return false;  // Invalid parity
    }
    
    // Disable interrupts
    outb(port->port + UART_INT_ENABLE, 0x00);
    
    // Set DLAB to access divisor
    outb(port->port + UART_LINE_CTRL, UART_LCR_DLAB);
    
    // Set divisor (low byte and high byte)
    outb(port->port + UART_DIV_LOW, divisor & 0xFF);
    outb(port->port + UART_DIV_HIGH, (divisor >> 8) & 0xFF);
    
    // Set line config
    outb(port->port + UART_LINE_CTRL, line_config);
    
    // Update port information
    port->baud_rate = baud_rate;
    port->line_config = line_config;
    
    return true;
}

/**
 * @brief Get the base port address for a COM port number
 * 
 * @param com_number COM port number (1-4)
 * @return Base port address, or 0 if invalid
 */
uint16_t serial_get_port_address(int com_number) {
    switch (com_number) {
        case 1: return COM1_PORT;
        case 2: return COM2_PORT;
        case 3: return COM3_PORT;
        case 4: return COM4_PORT;
        default: return 0;
    }
}

/**
 * @brief Get the COM port number for a base port address
 * 
 * @param port_address Base port address
 * @return COM port number (1-4), or 0 if invalid
 */
int serial_get_com_number(uint16_t port_address) {
    switch (port_address) {
        case COM1_PORT: return 1;
        case COM2_PORT: return 2;
        case COM3_PORT: return 3;
        case COM4_PORT: return 4;
        default: return 0;
    }
}
