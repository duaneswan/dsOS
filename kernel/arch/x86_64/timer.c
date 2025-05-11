/**
 * @file timer.c
 * @brief Programmable Interval Timer (PIT) driver
 */

#include "../../include/kernel.h"
#include <stdint.h>
#include <stdbool.h>

// IRQ lines (from pic.c)
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

// PIC interrupt offsets (from pic.c)
#define PIC1_OFFSET       0x20        // Master PIC base interrupt number
#define PIC2_OFFSET       0x28        // Slave PIC base interrupt number

// PIT (8253/8254) ports
#define PIT_CHANNEL0      0x40    // Channel 0 data port (read/write)
#define PIT_CHANNEL1      0x41    // Channel 1 data port (read/write)
#define PIT_CHANNEL2      0x42    // Channel 2 data port (read/write)
#define PIT_COMMAND       0x43    // Mode/Command register (write only)

// PIT operating modes
#define PIT_MODE_INTERRUPT     0x00    // Mode 0: Interrupt on terminal count
#define PIT_MODE_ONESHOT       0x02    // Mode 1: Hardware re-triggerable one-shot
#define PIT_MODE_RATE          0x04    // Mode 2: Rate generator
#define PIT_MODE_SQUARE        0x06    // Mode 3: Square wave generator
#define PIT_MODE_SW_STROBE     0x08    // Mode 4: Software triggered strobe
#define PIT_MODE_HW_STROBE     0x0A    // Mode 5: Hardware triggered strobe

// PIT command bits
#define PIT_ACCESS_LATCH       0x00    // Latch count value command
#define PIT_ACCESS_LOW         0x10    // Access low byte only
#define PIT_ACCESS_HIGH        0x20    // Access high byte only
#define PIT_ACCESS_BOTH        0x30    // Access both bytes

// PIT channel selection
#define PIT_CHANNEL0_SELECT    0x00    // Select channel 0
#define PIT_CHANNEL1_SELECT    0x40    // Select channel 1
#define PIT_CHANNEL2_SELECT    0x80    // Select channel 2
#define PIT_READBACK_COMMAND   0xC0    // Read-back command

// PIT oscillator frequency
#define PIT_FREQUENCY       1193182    // 1.193182 MHz

// Timer state variables
static uint32_t timer_frequency = 0;
static uint64_t timer_ticks = 0;
static uint64_t timer_ms = 0;

// Timer callback function type
typedef void (*timer_callback_t)(void);

// Timer callback
static timer_callback_t timer_callback = NULL;

/**
 * @brief Set the PIT channel frequency
 * 
 * @param channel PIT channel (0-2)
 * @param frequency Desired frequency in Hz
 * @param mode PIT operating mode
 */
static void pit_set_frequency(uint8_t channel, uint32_t frequency, uint8_t mode) {
    // Clamp frequency to valid range
    if (frequency < 19) {
        frequency = 19;  // Minimum frequency (~18.2 Hz)
    } else if (frequency > PIT_FREQUENCY) {
        frequency = PIT_FREQUENCY;  // Maximum frequency
    }
    
    // Calculate divisor from frequency
    uint16_t divisor = PIT_FREQUENCY / frequency;
    
    // Prepare command byte
    uint8_t cmd = 0;
    
    switch (channel) {
        case 0:
            cmd = PIT_CHANNEL0_SELECT;
            break;
        case 1:
            cmd = PIT_CHANNEL1_SELECT;
            break;
        case 2:
            cmd = PIT_CHANNEL2_SELECT;
            break;
        default:
            return;  // Invalid channel
    }
    
    // Set access mode (both bytes) and operating mode
    cmd |= PIT_ACCESS_BOTH | mode;
    
    // Send command
    outb(PIT_COMMAND, cmd);
    
    // Set divisor (low byte, then high byte)
    switch (channel) {
        case 0:
            outb(PIT_CHANNEL0, divisor & 0xFF);
            outb(PIT_CHANNEL0, (divisor >> 8) & 0xFF);
            break;
        case 1:
            outb(PIT_CHANNEL1, divisor & 0xFF);
            outb(PIT_CHANNEL1, (divisor >> 8) & 0xFF);
            break;
        case 2:
            outb(PIT_CHANNEL2, divisor & 0xFF);
            outb(PIT_CHANNEL2, (divisor >> 8) & 0xFF);
            break;
    }
}

/**
 * @brief Get the current PIT channel count
 * 
 * @param channel PIT channel (0-2)
 * @return Current count value
 */
static uint16_t pit_get_count(uint8_t channel) {
    uint16_t count = 0;
    uint8_t port = 0;
    
    // Select channel port
    switch (channel) {
        case 0:
            port = PIT_CHANNEL0;
            break;
        case 1:
            port = PIT_CHANNEL1;
            break;
        case 2:
            port = PIT_CHANNEL2;
            break;
        default:
            return 0;  // Invalid channel
    }
    
    // Send latch command
    outb(PIT_COMMAND, channel << 6);
    
    // Read count (low byte, then high byte)
    count = inb(port);
    count |= inb(port) << 8;
    
    return count;
}

/**
 * @brief Timer interrupt handler
 */
static void timer_handler(void) {
    // Increment tick count
    timer_ticks++;
    
    // Update milliseconds
    timer_ms = timer_ticks * (1000 / timer_frequency);
    
    // Call registered callback if any
    if (timer_callback != NULL) {
        timer_callback();
    }
    
    // Send EOI to PIC
    pic_send_eoi(IRQ_TIMER);
}

/**
 * @brief Get current system uptime in ticks
 * 
 * @return Number of timer ticks since system start
 */
uint64_t timer_get_ticks(void) {
    return timer_ticks;
}

/**
 * @brief Get current system uptime in milliseconds
 * 
 * @return Number of milliseconds since system start
 */
uint64_t timer_get_ms(void) {
    return timer_ms;
}

/**
 * @brief Wait for the specified number of ticks
 * 
 * @param ticks Number of ticks to wait
 */
void timer_wait_ticks(uint64_t ticks) {
    uint64_t target = timer_ticks + ticks;
    while (timer_ticks < target) {
        __asm__ volatile("hlt"); // Pause the CPU until next interrupt
    }
}

/**
 * @brief Wait for the specified number of milliseconds
 * 
 * @param ms Number of milliseconds to wait
 */
void timer_wait_ms(uint32_t ms) {
    uint64_t ticks = (ms * timer_frequency) / 1000;
    if (ticks == 0) ticks = 1;  // Ensure at least one tick
    timer_wait_ticks(ticks);
}

/**
 * @brief Register a callback function to be called on each timer tick
 * 
 * @param callback Callback function pointer
 */
void timer_register_callback(timer_callback_t callback) {
    timer_callback = callback;
}

/**
 * @brief Initialize the PIT timer
 * 
 * @param frequency Desired timer frequency in Hz
 */
void timer_init(uint32_t frequency) {
    // Validate and save frequency
    if (frequency < 19) {
        frequency = 19;  // Minimum frequency (~18.2 Hz)
    } else if (frequency > 1000) {
        frequency = 1000;  // Maximum practical frequency
    }
    
    timer_frequency = frequency;
    
    // Configure PIT channel 0 for rate generator mode
    pit_set_frequency(0, frequency, PIT_MODE_SQUARE);
    
    // Register the timer handler
    register_interrupt_handler(IRQ_TIMER + PIC1_OFFSET, timer_handler);
    
    // Enable timer interrupt
    pic_unmask_irq(IRQ_TIMER);
    
    kprintf("TIMER: Initialized at %u Hz\n", frequency);
}

/**
 * @brief Initialize the system sleep timer (channel 1)
 */
void sleep_timer_init(void) {
    // Initialize PIT channel 1 for oneshot mode (used for delays)
    pit_set_frequency(1, 100, PIT_MODE_ONESHOT);
}

/**
 * @brief Configure the PC speaker (channel 2)
 * 
 * @param frequency Frequency in Hz (0 to turn off)
 */
void pc_speaker_set_frequency(uint32_t frequency) {
    if (frequency == 0) {
        // Turn off the PC speaker
        uint8_t tmp = inb(0x61) & 0xFC;
        outb(0x61, tmp);
    } else {
        // Set frequency
        pit_set_frequency(2, frequency, PIT_MODE_SQUARE);
        
        // Turn on the PC speaker
        uint8_t tmp = inb(0x61);
        if (tmp != (tmp | 3)) {
            outb(0x61, tmp | 3);
        }
    }
}

/**
 * @brief Play a beep sound for a specified duration
 * 
 * @param frequency Frequency in Hz
 * @param ms Duration in milliseconds
 */
void pc_speaker_beep(uint32_t frequency, uint32_t ms) {
    // Set the frequency
    pc_speaker_set_frequency(frequency);
    
    // Wait for the specified duration
    timer_wait_ms(ms);
    
    // Turn off the speaker
    pc_speaker_set_frequency(0);
}
