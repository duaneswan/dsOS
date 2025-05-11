/**
 * @file timer.c
 * @brief Programmable Interval Timer (PIT) implementation
 */

#include "../../include/kernel.h"
#include <stdint.h>

// PIT (Programmable Interval Timer) ports
#define PIT_CHANNEL0 0x40    // Channel 0 data port
#define PIT_CHANNEL1 0x41    // Channel 1 data port
#define PIT_CHANNEL2 0x42    // Channel 2 data port
#define PIT_COMMAND  0x43    // Mode/Command register

// PIT commands
#define PIT_MODE3    0x36    // Square wave generator mode
#define PIT_LATCH    0x00    // Latch command

// PIT frequency constants
#define PIT_FREQUENCY 1193182 // Base frequency (Hz)
#define TICKS_PER_SECOND 100  // Desired interrupts per second

// Kernel tick counter
static volatile uint64_t tick_count = 0;

// Timer callback function pointer
typedef void (*timer_callback_t)(void);
static timer_callback_t timer_callback = NULL;

/**
 * @brief Timer interrupt handler
 */
static void timer_handler(void) {
    // Increment the tick counter
    tick_count++;
    
    // Call the registered callback if available
    if (timer_callback) {
        timer_callback();
    }
}

/**
 * @brief Register a timer callback function
 * 
 * @param callback Function to call on each timer interrupt
 */
void timer_register_callback(timer_callback_t callback) {
    timer_callback = callback;
}

/**
 * @brief Get the current system tick count
 * 
 * @return Current tick count since system boot
 */
uint64_t timer_get_ticks(void) {
    return tick_count;
}

/**
 * @brief Convert milliseconds to ticks
 * 
 * @param ms Milliseconds to convert
 * @return Equivalent number of ticks
 */
uint64_t timer_ms_to_ticks(uint64_t ms) {
    return (ms * TICKS_PER_SECOND) / 1000;
}

/**
 * @brief Simple delay function (busy wait)
 * 
 * @param ms Milliseconds to delay
 */
void timer_delay(uint64_t ms) {
    uint64_t start = tick_count;
    uint64_t ticks = timer_ms_to_ticks(ms);
    
    while (tick_count - start < ticks) {
        // Wait for timer interrupt to increment tick_count
        __asm__ volatile("pause");
    }
}

/**
 * @brief Initialize the Programmable Interval Timer
 */
void timer_init(void) {
    // Calculate the PIT reload value for the desired frequency
    uint16_t reload_value = PIT_FREQUENCY / TICKS_PER_SECOND;
    
    // Send the command byte to the PIT command port
    outb(PIT_COMMAND, PIT_MODE3);
    
    // Send the reload value as low byte then high byte
    outb(PIT_CHANNEL0, reload_value & 0xFF);
    outb(PIT_CHANNEL0, (reload_value >> 8) & 0xFF);
    
    // Register the timer handler for IRQ0
    extern void register_interrupt_handler(uint8_t, interrupt_handler_t);
    register_interrupt_handler(32, (interrupt_handler_t)timer_handler);
    
    // Unmask (enable) IRQ0 in the PIC
    extern void pic_unmask_irq(uint8_t);
    pic_unmask_irq(0);
    
    kprintf("Timer: PIT initialized at %u Hz\n", TICKS_PER_SECOND);
}
