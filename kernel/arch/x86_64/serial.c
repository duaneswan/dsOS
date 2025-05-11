/**
 * @file serial.c
 * @brief Serial port driver
 */

#include "../../include/kernel.h"
#include <stdint.h>
#include <stdbool.h>

// Serial port registers
#define SERIAL_COM1            0x3F8      // COM1 base port
#define SERIAL_COM2            0x2F8      // COM2 base port
#define SERIAL_COM3            0x3E8      // COM3 base port
#define SERIAL_COM4            0x2E8      // COM4 base port

// Serial port register offsets
#define SERIAL_DATA            0          // Data register (R/W)
#define SERIAL_INT_ENABLE      1          // Interrupt enable register (W)
#define SERIAL_INT_ID_FIFO     2          // Interrupt ID and FIFO control registers (R/W)
#define SERIAL_LINE_CONTROL    3          // Line control register (R/W)
#define SERIAL_MODEM_CONTROL   4          // Modem control register (R/W)
#define SERIAL_LINE_STATUS     5          // Line status register (R)
#define SERIAL_MODEM_STATUS    6          // Modem status register (R)
#define SERIAL_SCRATCH         7          // Scratch register (R/W)

// Line control register bits
#define SERIAL_LCR_5BITS       0x00       // 5 bits per character
#define SERIAL_LCR_6BITS       0x01       // 6 bits per character
#define SERIAL_LCR_7BITS       0x02       // 7 bits per character
#define SERIAL_LCR_8BITS       0x03       // 8 bits per character
#define SERIAL_LCR_1STOP       0x00       // 1 stop bit
#define SERIAL_LCR_2STOP       0x04       // 2 stop bits (1.5 if 5 bits per char)
#define SERIAL_LCR_NO_PARITY   0x00       // No parity
#define SERIAL_LCR_ODD_PARITY  0x08       // Odd parity
#define SERIAL_LCR_EVEN_PARITY 0x18       // Even parity
#define SERIAL_LCR_MARK_PARITY 0x28       // Mark parity (always 1)
#define SERIAL_LCR_SPACE_PARITY 0x38      // Space parity (always 0)
#define SERIAL_LCR_DLAB        0x80       // Divisor latch access bit

// FIFO control register bits
#define SERIAL_FCR_ENABLE      0x01       // Enable FIFO
#define SERIAL_FCR_CLEAR_RX    0x02       // Clear receive FIFO
#define SERIAL_FCR_CLEAR_TX    0x04       // Clear transmit FIFO
#define SERIAL_FCR_DMA_MODE    0x08       // Enable DMA mode
#define SERIAL_FCR_TRIG_1      0x00       // 1 byte trigger level
#define SERIAL_FCR_TRIG_4      0x40       // 4 byte trigger level
#define SERIAL_FCR_TRIG_8      0x80       // 8 byte trigger level
#define SERIAL_FCR_TRIG_14     0xC0       // 14 byte trigger level

// Line status register bits
#define SERIAL_LSR_DATA_READY  0x01       // Data ready
#define SERIAL_LSR_OVERRUN_ERR 0x02       // Overrun error
#define SERIAL_LSR_PARITY_ERR  0x04       // Parity error
#define SERIAL_LSR_FRAMING_ERR 0x08       // Framing error
#define SERIAL_LSR_BREAK_IND   0x10       // Break indicator
#define SERIAL_LSR_THR_EMPTY   0x20       // Transmitter holding register empty
#define SERIAL_LSR_TRANS_EMPTY 0x40       // Transmitter empty
#define SERIAL_LSR_FIFO_ERR    0x80       // FIFO error

// Modem control register bits
#define SERIAL_MCR_DTR         0x01       // Data terminal ready
#define SERIAL_MCR_RTS         0x02       // Request to send
#define SERIAL_MCR_AUX1        0x04       // Auxiliary output 1
#define SERIAL_MCR_AUX2        0x08       // Auxiliary output 2
#define SERIAL_MCR_LOOPBACK    0x10       // Loopback mode

// Common baud rates
#define SERIAL_BAUD_115200     1          // 115200 bps
#define SERIAL_BAUD_57600      2          // 57600 bps
#define SERIAL_BAUD_38400      3          // 38400 bps
#define SERIAL_BAUD_19200      6          // 19200 bps
#define SERIAL_BAUD_9600       12         // 9600 bps
#define SERIAL_BAUD_4800       24         // 4800 bps
#define SERIAL_BAUD_2400       48         // 2400 bps
#define SERIAL_BAUD_1200       96         // 1200 bps

// Current serial port
static uint16_t serial_port = 0;

/**
 * @brief Check if serial port exists
 * 
 * @param port Serial port base address
 * @return true if port exists, false otherwise
 */
static bool serial_exists(uint16_t port) {
    // Save the original value of the scratch register
    uint8_t scratch = inb(port + SERIAL_SCRATCH);
    
    // Write a test pattern to the scratch register
    outb(port + SERIAL_SCRATCH, 0x55);
    if (inb(port + SERIAL_SCRATCH) != 0x55) {
        return false;
    }
    
    // Write a different test pattern to confirm
    outb(port + SERIAL_SCRATCH, 0xAA);
    if (inb(port + SERIAL_SCRATCH) != 0xAA) {
        return false;
    }
    
    // Restore the original value
    outb(port + SERIAL_SCRATCH, scratch);
    return true;
}

/**
 * @brief Check if the transmit buffer is empty
 * 
 * @param port Serial port base address
 * @return true if transmit buffer is empty, false otherwise
 */
static bool serial_transmit_empty(uint16_t port) {
    return (inb(port + SERIAL_LINE_STATUS) & SERIAL_LSR_THR_EMPTY) != 0;
}

/**
 * @brief Check if data is available to read
 * 
 * @param port Serial port base address
 * @return true if data is available, false otherwise
 */
static bool serial_data_ready(uint16_t port) {
    return (inb(port + SERIAL_LINE_STATUS) & SERIAL_LSR_DATA_READY) != 0;
}

/**
 * @brief Set the baud rate
 * 
 * @param port Serial port base address
 * @param divisor Baud rate divisor
 */
static void serial_set_baud(uint16_t port, uint16_t divisor) {
    // Enable DLAB to access divisor latches
    uint8_t lcr = inb(port + SERIAL_LINE_CONTROL);
    outb(port + SERIAL_LINE_CONTROL, lcr | SERIAL_LCR_DLAB);
    
    // Set divisor (low byte, then high byte)
    outb(port + SERIAL_DATA, divisor & 0xFF);
    outb(port + SERIAL_INT_ENABLE, divisor >> 8);
    
    // Disable DLAB
    outb(port + SERIAL_LINE_CONTROL, lcr);
}

/**
 * @brief Initialize a serial port
 * 
 * @param port Serial port base address
 * @param baud Baud rate divisor
 * @return true if initialization was successful, false otherwise
 */
static bool serial_init_port(uint16_t port, uint16_t baud) {
    // Check if the port exists
    if (!serial_exists(port)) {
        return false;
    }
    
    // Set the baud rate
    serial_set_baud(port, baud);
    
    // Set line control: 8 bits, 1 stop bit, no parity
    outb(port + SERIAL_LINE_CONTROL, SERIAL_LCR_8BITS | SERIAL_LCR_1STOP | SERIAL_LCR_NO_PARITY);
    
    // Enable and configure FIFOs
    outb(port + SERIAL_INT_ID_FIFO, SERIAL_FCR_ENABLE | SERIAL_FCR_CLEAR_RX | SERIAL_FCR_CLEAR_TX | SERIAL_FCR_TRIG_14);
    
    // Set modem control: Enable DTR, RTS, and AUX2 (enables interrupts)
    outb(port + SERIAL_MODEM_CONTROL, SERIAL_MCR_DTR | SERIAL_MCR_RTS | SERIAL_MCR_AUX2);
    
    // Perform a loopback test
    outb(port + SERIAL_MODEM_CONTROL, SERIAL_MCR_LOOPBACK);
    outb(port + SERIAL_DATA, 0x55);
    
    // Check if the data was received correctly
    if (inb(port + SERIAL_DATA) != 0x55) {
        return false;
    }
    
    // Disable loopback and enable hardware flow control
    outb(port + SERIAL_MODEM_CONTROL, SERIAL_MCR_DTR | SERIAL_MCR_RTS | SERIAL_MCR_AUX2);
    
    return true;
}

/**
 * @brief Initialize the serial port driver
 */
void serial_init(void) {
    // Try to initialize COM1 first
    if (serial_init_port(SERIAL_COM1, SERIAL_BAUD_115200)) {
        serial_port = SERIAL_COM1;
        kprintf("SERIAL: Initialized COM1 at 115200 bps\n");
    }
    // If COM1 fails, try COM2
    else if (serial_init_port(SERIAL_COM2, SERIAL_BAUD_115200)) {
        serial_port = SERIAL_COM2;
        kprintf("SERIAL: Initialized COM2 at 115200 bps\n");
    }
    // If both fail, report error
    else {
        kprintf("SERIAL: Failed to initialize any serial port\n");
        serial_port = 0;
    }
}

/**
 * @brief Write a character to the serial port
 * 
 * @param c Character to write
 */
void serial_write_char(char c) {
    // If serial port is not initialized, do nothing
    if (serial_port == 0) {
        return;
    }
    
    // Wait for the transmit buffer to be empty
    while (!serial_transmit_empty(serial_port)) {
        io_wait();
    }
    
    // Send the character
    outb(serial_port + SERIAL_DATA, c);
    
    // If the character is a newline, also send a carriage return
    if (c == '\n') {
        serial_write_char('\r');
    }
}

/**
 * @brief Read a character from the serial port
 * 
 * @return Character read, or 0 if no data available
 */
char serial_read_char(void) {
    // If serial port is not initialized, return 0
    if (serial_port == 0) {
        return 0;
    }
    
    // Check if data is available
    if (!serial_data_ready(serial_port)) {
        return 0;
    }
    
    // Read the character
    return inb(serial_port + SERIAL_DATA);
}

/**
 * @brief Write a string to the serial port
 * 
 * @param str String to write
 */
void serial_write_string(const char* str) {
    while (*str) {
        serial_write_char(*str++);
    }
}

/**
 * @brief Check if the serial port is initialized
 * 
 * @return true if serial port is initialized, false otherwise
 */
bool serial_is_initialized(void) {
    return serial_port != 0;
}
