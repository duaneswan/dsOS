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
