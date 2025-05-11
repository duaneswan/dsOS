/**
 * @file serial.c
 * @brief Serial port driver
 */

#include "../../include/kernel.h"
#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdarg.h>

// Standard COM port addresses
#define COM1_PORT           0x3F8
#define COM2_PORT           0x2F8
#define COM3_PORT           0x3E8
#define COM4_PORT           0x2E8

// COM port registers (offsets from base port)
#define REG_DATA            0       // Data register (read/write)
#define REG_INTERRUPT       1       // Interrupt enable register (write)
#define REG_BAUD_LSB        0       // Baud rate divisor LSB (write with DLAB=1)
#define REG_BAUD_MSB        1       // Baud rate divisor MSB (write with DLAB=1)
#define REG_FIFO_CONTROL    2       // FIFO control register (write)
#define REG_LINE_CONTROL    3       // Line control register (read/write)
#define REG_MODEM_CONTROL   4       // Modem control register (write)
#define REG_LINE_STATUS     5       // Line status register (read)
#define REG_MODEM_STATUS    6       // Modem status register (read)
#define REG_SCRATCH         7       // Scratch register (read/write)

// Line control register bits
#define LCR_DATA_BITS_5     0x00    // 5 data bits
#define LCR_DATA_BITS_6     0x01    // 6 data bits
#define LCR_DATA_BITS_7     0x02    // 7 data bits
#define LCR_DATA_BITS_8     0x03    // 8 data bits
#define LCR_STOP_BITS_1     0x00    // 1 stop bit
#define LCR_STOP_BITS_2     0x04    // 2 stop bits
#define LCR_NO_PARITY       0x00    // No parity
#define LCR_ODD_PARITY      0x08    // Odd parity
#define LCR_EVEN_PARITY     0x18    // Even parity
#define LCR_MARK_PARITY     0x28    // Mark parity
#define LCR_SPACE_PARITY    0x38    // Space parity
#define LCR_DLAB            0x80    // Divisor Latch Access Bit

// FIFO control register bits
#define FCR_ENABLE_FIFO     0x01    // Enable FIFO
#define FCR_CLEAR_RECV      0x02    // Clear receive FIFO
#define FCR_CLEAR_TRANS     0x04    // Clear transmit FIFO
#define FCR_DMA_MODE        0x08    // Enable DMA mode
#define FCR_FIFO_64         0x20    // Enable 64-byte FIFO (16750 only)
#define FCR_TRIG_1          0x00    // Trigger level 1 byte
#define FCR_TRIG_4          0x40    // Trigger level 4 bytes
#define FCR_TRIG_8          0x80    // Trigger level 8 bytes
#define FCR_TRIG_14         0xC0    // Trigger level 14 bytes

// Line status register bits
#define LSR_DATA_READY      0x01    // Data Ready
#define LSR_OVERRUN_ERROR   0x02    // Overrun Error
#define LSR_PARITY_ERROR    0x04    // Parity Error
#define LSR_FRAMING_ERROR   0x08    // Framing Error
#define LSR_BREAK_SIGNAL    0x10    // Break Signal
#define LSR_THR_EMPTY       0x20    // Transmitter Holding Register Empty
#define LSR_TRANS_EMPTY     0x40    // Transmitter Empty
#define LSR_FIFO_ERROR      0x80    // FIFO Error

// Modem control register bits
#define MCR_DTR             0x01    // Data Terminal Ready
#define MCR_RTS             0x02    // Request To Send
#define MCR_OUT1            0x04    // Out1
#define MCR_OUT2            0x08    // Out2 (enables interrupts)
#define MCR_LOOPBACK        0x10    // Loopback mode

// Interrupt enable register bits
#define IER_RECV_DATA       0x01    // Received Data Available
#define IER_TRANS_EMPTY     0x02    // Transmitter Holding Register Empty
#define IER_LINE_STATUS     0x04    // Line Status
#define IER_MODEM_STATUS    0x08    // Modem Status

// Standard baud rates
#define BAUD_RATE_115200    115200
#define BAUD_RATE_57600     57600
#define BAUD_RATE_38400     38400
#define BAUD_RATE_19200     19200
#define BAUD_RATE_9600      9600
#define BAUD_RATE_4800      4800
#define BAUD_RATE_2400      2400
#define BAUD_RATE_1200      1200

// Default baud rate
#define DEFAULT_BAUD_RATE   BAUD_RATE_38400

// Default serial port
#define DEFAULT_COM_PORT    COM1_PORT

// Serial port descriptor
typedef struct {
    uint16_t port;          // Base I/O port address
    uint32_t baud_rate;     // Baud rate
    bool initialized;       // Whether port is initialized
    bool loopback_mode;     // Whether port is in loopback mode
} serial_port_t;

// Serial ports
static serial_port_t serial_ports[4] = {
    { COM1_PORT, 0, false, false },
    { COM2_PORT, 0, false, false },
    { COM3_PORT, 0, false, false },
    { COM4_PORT, 0, false, false }
};

// Debug port
serial_port_t* debug_port = &serial_ports[0];  // COM1 by default

/**
 * @brief Read from a serial port register
 * 
 * @param port Serial port base address
 * @param reg Register offset
 * @return Value read from register
 */
static inline uint8_t serial_read(uint16_t port, uint8_t reg) {
    return inb(port + reg);
}

/**
 * @brief Write to a serial port register
 * 
 * @param port Serial port base address
 * @param reg Register offset
 * @param value Value to write
 */
static inline void serial_write(uint16_t port, uint8_t reg, uint8_t value) {
    outb(port + reg, value);
}

/**
 * @brief Check if a serial port exists
 * 
 * @param port Serial port base address
 * @return true if port exists, false otherwise
 */
static bool serial_exists(uint16_t port) {
    // Save the original value of the scratch register
    uint8_t original = serial_read(port, REG_SCRATCH);
    
    // Write a test pattern to the scratch register
    serial_write(port, REG_SCRATCH, 0x55);
    if (serial_read(port, REG_SCRATCH) != 0x55) {
        return false;
    }
    
    // Write a different test pattern
    serial_write(port, REG_SCRATCH, 0xAA);
    if (serial_read(port, REG_SCRATCH) != 0xAA) {
        return false;
    }
    
    // Restore the original value
    serial_write(port, REG_SCRATCH, original);
    
    return true;
}

/**
 * @brief Convert the port number to an index
 * 
 * @param port Serial port base address
 * @return Index (0-3) or -1 if invalid
 */
static int serial_port_to_index(uint16_t port) {
    switch (port) {
        case COM1_PORT: return 0;
        case COM2_PORT: return 1;
        case COM3_PORT: return 2;
        case COM4_PORT: return 3;
        default: return -1;
    }
}

/**
 * @brief Calculate the baud rate divisor
 * 
 * @param baud_rate Desired baud rate
 * @return Divisor value
 */
static uint16_t serial_calculate_divisor(uint32_t baud_rate) {
    return (uint16_t)(115200 / baud_rate);
}

/**
 * @brief Check if a serial port is initialized
 * 
 * @param port Serial port pointer
 * @return true if initialized, false otherwise
 */
bool serial_is_initialized(serial_port_t* port) {
    if (port == NULL) {
        return false;
    }
    return port->initialized;
}

/**
 * @brief Initialize a serial port
 * 
 * @param port Serial port base address
 * @param baud_rate Baud rate to use
 * @return Pointer to serial port descriptor, or NULL if initialization failed
 */
serial_port_t* serial_init(uint16_t port, uint32_t baud_rate) {
    int index = serial_port_to_index(port);
    if (index < 0) {
        return NULL;
    }
    
    serial_port_t* serial_port = &serial_ports[index];
    
    // Check if port exists
    if (!serial_exists(port)) {
        kprintf("SERIAL: Port 0x%x does not exist\n", port);
        return NULL;
    }
    
    // Set baud rate
    uint16_t divisor = serial_calculate_divisor(baud_rate);
    
    // Disable interrupts
    serial_write(port, REG_INTERRUPT, 0x00);
    
    // Set DLAB to access baud rate divisor
    serial_write(port, REG_LINE_CONTROL, LCR_DLAB);
    
    // Set baud rate divisor
    serial_write(port, REG_BAUD_LSB, divisor & 0xFF);
    serial_write(port, REG_BAUD_MSB, (divisor >> 8) & 0xFF);
    
    // Set data format: 8 bits, no parity, 1 stop bit
    serial_write(port, REG_LINE_CONTROL, LCR_DATA_BITS_8 | LCR_NO_PARITY | LCR_STOP_BITS_1);
    
    // Enable and configure FIFO
    serial_write(port, REG_FIFO_CONTROL, FCR_ENABLE_FIFO | FCR_CLEAR_RECV | FCR_CLEAR_TRANS | FCR_TRIG_14);
    
    // Configure modem control: DTR, RTS, OUT2 enabled
    serial_write(port, REG_MODEM_CONTROL, MCR_DTR | MCR_RTS | MCR_OUT2);
    
    // Update serial port descriptor
    serial_port->port = port;
    serial_port->baud_rate = baud_rate;
    serial_port->initialized = true;
    serial_port->loopback_mode = false;
    
    kprintf("SERIAL: Initialized port 0x%x at %u baud\n", port, baud_rate);
    
    return serial_port;
}

/**
 * @brief Check if a serial port can receive data
 * 
 * @param port Serial port pointer
 * @return true if data available, false otherwise
 */
bool serial_can_receive(serial_port_t* port) {
    if (!serial_is_initialized(port)) {
        return false;
    }
    
    return (serial_read(port->port, REG_LINE_STATUS) & LSR_DATA_READY) != 0;
}

/**
 * @brief Check if a serial port can send data
 * 
 * @param port Serial port pointer
 * @return true if transmitter is empty, false otherwise
 */
bool serial_can_send(serial_port_t* port) {
    if (!serial_is_initialized(port)) {
        return false;
    }
    
    return (serial_read(port->port, REG_LINE_STATUS) & LSR_THR_EMPTY) != 0;
}

/**
 * @brief Read a character from a serial port
 * 
 * @param port Serial port pointer
 * @return Character read, or -1 if no data available
 */
int serial_read_char(serial_port_t* port) {
    if (!serial_can_receive(port)) {
        return -1;
    }
    
    return serial_read(port->port, REG_DATA);
}

/**
 * @brief Write a character to a serial port
 * 
 * @param port Serial port pointer
 * @param c Character to write
 * @return true if successful, false otherwise
 */
bool serial_write_char(serial_port_t* port, char c) {
    if (!serial_is_initialized(port)) {
        return false;
    }
    
    // Wait until transmitter is empty
    while (!serial_can_send(port)) {
        // Add a timeout if needed
    }
    
    // Send the character
    serial_write(port->port, REG_DATA, c);
    
    return true;
}

/**
 * @brief Write a string to a serial port
 * 
 * @param port Serial port pointer
 * @param str String to write
 * @return true if successful, false otherwise
 */
bool serial_write_str(serial_port_t* port, const char* str) {
    if (!serial_is_initialized(port) || str == NULL) {
        return false;
    }
    
    while (*str) {
        // Handle newline by sending CR+LF
        if (*str == '\n') {
            serial_write_char(port, '\r');
        }
        
        serial_write_char(port, *str++);
    }
    
    return true;
}

/**
 * @brief Write a formatted string to a serial port
 * 
 * @param port Serial port pointer
 * @param format Format string
 * @param ... Variable arguments
 * @return true if successful, false otherwise
 */
bool serial_printf(serial_port_t* port, const char* format, ...) {
    if (!serial_is_initialized(port) || format == NULL) {
        return false;
    }
    
    va_list args;
    char buffer[1024];
    
    va_start(args, format);
    int len = vsnprintf(buffer, sizeof(buffer), format, args);
    va_end(args);
    
    if (len < 0 || len >= (int)sizeof(buffer)) {
        return false;
    }
    
    return serial_write_str(port, buffer);
}

/**
 * @brief Enable loopback mode for testing
 * 
 * @param port Serial port pointer
 * @param enable True to enable, false to disable
 * @return true if successful, false otherwise
 */
bool serial_set_loopback(serial_port_t* port, bool enable) {
    if (!serial_is_initialized(port)) {
        return false;
    }
    
    uint8_t mcr = serial_read(port->port, REG_MODEM_CONTROL);
    
    if (enable) {
        mcr |= MCR_LOOPBACK;
    } else {
        mcr &= ~MCR_LOOPBACK;
    }
    
    serial_write(port->port, REG_MODEM_CONTROL, mcr);
    port->loopback_mode = enable;
    
    return true;
}

/**
 * @brief Test a serial port using loopback mode
 * 
 * @param port Serial port pointer
 * @return true if test passed, false otherwise
 */
bool serial_test(serial_port_t* port) {
    if (!serial_is_initialized(port)) {
        return false;
    }
    
    // Save current loopback state
    bool old_loopback = port->loopback_mode;
    
    // Enable loopback mode
    serial_set_loopback(port, true);
    
    // Test sending/receiving a few different characters
    const char test_chars[] = "Hello, Serial!";
    
    for (size_t i = 0; i < sizeof(test_chars) - 1; i++) {
        serial_write_char(port, test_chars[i]);
        
        // In loopback mode, data should be immediately available to read
        if (!serial_can_receive(port)) {
            serial_set_loopback(port, old_loopback);
            return false;
        }
        
        int received = serial_read_char(port);
        if (received != test_chars[i]) {
            serial_set_loopback(port, old_loopback);
            return false;
        }
    }
    
    // Restore previous loopback state
    serial_set_loopback(port, old_loopback);
    
    return true;
}

/**
 * @brief Initialize all serial ports
 */
void serial_init_all(void) {
    // Try to initialize COM1 and COM2 (most common)
    serial_init(COM1_PORT, DEFAULT_BAUD_RATE);
    serial_init(COM2_PORT, DEFAULT_BAUD_RATE);
    
    // Set COM1 as debug port if available, otherwise try COM2
    if (serial_ports[0].initialized) {
        debug_port = &serial_ports[0];
    } else if (serial_ports[1].initialized) {
        debug_port = &serial_ports[1];
    }
    
    // If we have a debug port, test it
    if (serial_is_initialized(debug_port)) {
        if (serial_test(debug_port)) {
            serial_printf(debug_port, "SERIAL: Loopback test passed\n");
        } else {
            kprintf("SERIAL: Loopback test failed\n");
        }
    }
}
