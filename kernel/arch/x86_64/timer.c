/**
 * @file timer.c
 * @brief Programmable Interval Timer (PIT) driver
 */

#include "../../include/kernel.h"
#include <stdint.h>
#include <stdbool.h>

// PIT registers and constants
#define PIT_CHANNEL0    0x40    // Channel 0 data port
#define PIT_CHANNEL1    0x41    // Channel 1 data port
#define PIT_CHANNEL2    0x42    // Channel 2 data port
#define PIT_COMMAND     0x43    // Command register port

// PIT command register bits
#define PIT_CHANNEL0_SELECT 0x00    // Select channel 0
#define PIT_CHANNEL1_SELECT 0x40    // Select channel 1
#define PIT_CHANNEL2_SELECT 0x80    // Select channel 2
#define PIT_READBACK_CMD   0xC0    // Read-back command

#define PIT_ACCESS_LATCH   0x00    // Latch count value command
#define PIT_ACCESS_LOBYTE  0x10    // Access low byte only
#define PIT_ACCESS_HIBYTE  0x20    // Access high byte only
#define PIT_ACCESS_WORD    0x30    // Access low byte then high byte

#define PIT_MODE0          0x00    // Interrupt on terminal count
#define PIT_MODE1          0x02    // Hardware retriggerable one-shot
#define PIT_MODE2          0x04    // Rate generator
#define PIT_MODE3          0x06    // Square wave generator
#define PIT_MODE4          0x08    // Software triggered strobe
#define PIT_MODE5          0x0A    // Hardware triggered strobe

#define PIT_BINARY         0x00    // Binary count mode (16-bit)
#define PIT_BCD            0x01    // BCD count mode (4 decades)

// PIT frequency
#define PIT_BASE_FREQUENCY 1193182    // 1.193182 MHz
#define PIT_IRQ            0          // IRQ 0

// Define the timer callback function type
typedef void (*timer_callback_t)(uint64_t tick_count);

// Timer state
static volatile uint64_t timer_ticks = 0;
static uint32_t timer_frequency = 0;
static timer_callback_t timer_callback = NULL;

/**
 * @brief Timer interrupt handler
 */
static void timer_handler(void) {
    timer_ticks++;
    
    // Call the registered callback if any
    if (timer_callback) {
        timer_callback(timer_ticks);
    }
    
    // Send EOI to PIC
    pic_send_eoi(PIT_IRQ);
}

/**
 * @brief Initialize the PIT
 * 
 * @param frequency Timer frequency in Hz
 */
void timer_init(uint32_t frequency) {
    uint32_t divisor;
    
    // Save the frequency
    timer_frequency = frequency;
    
    // Calculate the divisor
    divisor = PIT_BASE_FREQUENCY / frequency;
    if (divisor > 65535) {
        kprintf("TIMER: Warning: Requested frequency %u Hz is too low, clamping to %u Hz\n",
                frequency, PIT_BASE_FREQUENCY / 65535);
        divisor = 65535;
        timer_frequency = PIT_BASE_FREQUENCY / divisor;
    }
    
    // Configure PIT channel 0 in mode 3 (square wave)
    outb(PIT_COMMAND, PIT_CHANNEL0_SELECT | PIT_ACCESS_WORD | PIT_MODE3 | PIT_BINARY);
    
    // Set the divisor
    uint8_t low = (uint8_t)(divisor & 0xFF);
    uint8_t high = (uint8_t)((divisor >> 8) & 0xFF);
    outb(PIT_CHANNEL0, low);
    outb(PIT_CHANNEL0, high);
    
    // Register the timer interrupt handler
    register_interrupt_handler(PIT_IRQ + 0x20, timer_handler);
    
    // Unmask the timer IRQ
    pic_set_mask(PIT_IRQ, true);
    
    kprintf("TIMER: Initialized at %u Hz (divisor=%u)\n", timer_frequency, divisor);
}

/**
 * @brief Register a timer callback function
 * 
 * @param callback Function to call on each timer interrupt
 * @return Previous callback function, or NULL if none
 */
timer_callback_t timer_register_callback(timer_callback_t callback) {
    timer_callback_t old_callback = timer_callback;
    timer_callback = callback;
    return old_callback;
}

/**
 * @brief Get the current timer tick count
 * 
 * @return Current tick count since boot
 */
uint64_t timer_get_ticks(void) {
    return timer_ticks;
}

/**
 * @brief Get the current timer frequency
 * 
 * @return Timer frequency in Hz
 */
uint32_t timer_get_frequency(void) {
    return timer_frequency;
}

/**
 * @brief Convert milliseconds to timer ticks
 * 
 * @param ms Milliseconds
 * @return Equivalent number of timer ticks
 */
uint64_t timer_ms_to_ticks(uint32_t ms) {
    return ((uint64_t)ms * timer_frequency) / 1000;
}

/**
 * @brief Convert ticks to milliseconds
 * 
 * @param ticks Timer ticks
 * @return Equivalent number of milliseconds
 */
uint32_t timer_ticks_to_ms(uint64_t ticks) {
    return (uint32_t)((ticks * 1000) / timer_frequency);
}

/**
 * @brief Busy-wait for a specified number of milliseconds
 * 
 * @param ms Number of milliseconds to wait
 */
void timer_sleep(uint32_t ms) {
    uint64_t target_ticks = timer_ticks + timer_ms_to_ticks(ms);
    
    // Enable interrupts during sleep
    sti();
    
    // Wait until we reach the target tick count
    while (timer_ticks < target_ticks) {
        // Use HLT to save power
        __asm__ volatile("hlt");
    }
}

/**
 * @brief Read the current PIT counter value
 * 
 * @return Current counter value (16-bit)
 */
uint16_t timer_read_counter(void) {
    uint16_t count;
    
    // Latch the count value
    outb(PIT_COMMAND, PIT_CHANNEL0_SELECT | PIT_ACCESS_LATCH);
    
    // Read the count value (low byte first, then high byte)
    uint8_t low = inb(PIT_CHANNEL0);
    uint8_t high = inb(PIT_CHANNEL0);
    
    // Combine the bytes
    count = (high << 8) | low;
    
    return count;
}

/**
 * @brief Get the current time since boot in milliseconds
 * 
 * @return Milliseconds since boot
 */
uint64_t timer_get_ms(void) {
    return timer_ticks_to_ms(timer_ticks);
}
