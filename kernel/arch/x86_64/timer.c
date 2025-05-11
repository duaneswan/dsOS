/**
 * @file timer.c
 * @brief Programmable Interval Timer (PIT) driver
 */

#include "../../include/kernel.h"
#include <stdint.h>
#include <stdbool.h>

// PIT ports and frequencies
#define PIT_CHANNEL0    0x40    // Channel 0 data port
#define PIT_CHANNEL1    0x41    // Channel 1 data port
#define PIT_CHANNEL2    0x42    // Channel 2 data port
#define PIT_COMMAND     0x43    // Mode/command register
#define PIT_BASE_FREQ   1193182 // Base frequency of the PIT (Hz)

// IRQ line for the PIT
#define PIT_IRQ         0

// Timer state
static uint32_t timer_frequency = 0;    // Current timer frequency
static uint64_t timer_ticks = 0;        // Number of ticks since boot
static uint64_t last_tick_ms = 0;       // Last timer tick in milliseconds

// Sleep timer callback
typedef struct {
    bool active;                // Whether this callback is active
    uint64_t target_ticks;      // Target tick count to trigger
    void (*callback)(void);     // Function to call when triggered
} sleep_timer_t;

// Sleep timer array
#define MAX_SLEEP_TIMERS 16
static sleep_timer_t sleep_timers[MAX_SLEEP_TIMERS];
static bool sleep_enabled = false;

/**
 * @brief Calculate the divisor for a given frequency
 * 
 * @param frequency Desired frequency in Hz
 * @return Divisor value
 */
static uint16_t calculate_divisor(uint32_t frequency) {
    return (uint16_t)(PIT_BASE_FREQ / frequency);
}

/**
 * @brief Set the PIT frequency
 * 
 * @param frequency Desired frequency in Hz
 */
static void set_pit_frequency(uint32_t frequency) {
    uint16_t divisor = calculate_divisor(frequency);
    
    // Send command: Channel 0, lobyte/hibyte access, rate generator mode
    outb(PIT_COMMAND, 0x36);
    
    // Set divisor (low byte, then high byte)
    outb(PIT_CHANNEL0, divisor & 0xFF);
    outb(PIT_CHANNEL0, (divisor >> 8) & 0xFF);
}

/**
 * @brief PIT timer interrupt handler
 */
static void timer_handler(void) {
    timer_ticks++;
    last_tick_ms = timer_ticks * (1000 / timer_frequency);
    
    // Check for sleep timers
    if (sleep_enabled) {
        for (int i = 0; i < MAX_SLEEP_TIMERS; i++) {
            if (sleep_timers[i].active && timer_ticks >= sleep_timers[i].target_ticks) {
                sleep_timers[i].active = false;
                if (sleep_timers[i].callback) {
                    sleep_timers[i].callback();
                }
            }
        }
    }
    
    // Send EOI to PIC (acknowledge interrupt)
    pic_send_eoi(PIT_IRQ);
}

/**
 * @brief Initialize the PIT timer
 * 
 * @param frequency Desired frequency in Hz
 */
void timer_init(uint32_t frequency) {
    // Save frequency
    timer_frequency = frequency;
    timer_ticks = 0;
    last_tick_ms = 0;
    
    // Set PIT frequency
    set_pit_frequency(frequency);
    
    // Register interrupt handler
    register_interrupt_handler(PIT_IRQ + 32, timer_handler);
    
    // Unmask IRQ0 (enable timer interrupts)
    pic_unmask_irq(PIT_IRQ);
    
    kprintf("Timer: Initialized at %u Hz\n", frequency);
}

/**
 * @brief Get the number of timer ticks since boot
 * 
 * @return Number of timer ticks
 */
uint64_t timer_get_ticks(void) {
    return timer_ticks;
}

/**
 * @brief Get the number of milliseconds since boot
 * 
 * @return Number of milliseconds
 */
uint64_t timer_get_ms(void) {
    return last_tick_ms;
}

/**
 * @brief Convert ticks to milliseconds
 * 
 * @param ticks Number of ticks
 * @return Equivalent number of milliseconds
 */
uint64_t timer_ticks_to_ms(uint64_t ticks) {
    return ticks * (1000 / timer_frequency);
}

/**
 * @brief Convert milliseconds to ticks
 * 
 * @param ms Number of milliseconds
 * @return Equivalent number of ticks
 */
uint64_t timer_ms_to_ticks(uint64_t ms) {
    return ms * timer_frequency / 1000;
}

/**
 * @brief Wait for a specified number of milliseconds
 * 
 * This function blocks until the specified time has passed
 * 
 * @param ms Number of milliseconds to wait
 */
void timer_wait_ms(uint32_t ms) {
    uint64_t target_ticks = timer_ticks + timer_ms_to_ticks(ms);
    while (timer_ticks < target_ticks) {
        // Wait for the timer interrupt to increment timer_ticks
        asm volatile("pause");
    }
}

/**
 * @brief Initialize the sleep timer system
 */
void sleep_timer_init(void) {
    // Initialize all sleep timers to inactive
    for (int i = 0; i < MAX_SLEEP_TIMERS; i++) {
        sleep_timers[i].active = false;
        sleep_timers[i].callback = NULL;
        sleep_timers[i].target_ticks = 0;
    }
    
    sleep_enabled = true;
    kprintf("Timer: Sleep timer system initialized\n");
}

/**
 * @brief Register a sleep timer callback
 * 
 * @param ms Number of milliseconds to wait before callback
 * @param callback Function to call when timer expires
 * @return Timer ID (or -1 if no free timers)
 */
int sleep_timer_register(uint32_t ms, void (*callback)(void)) {
    if (!sleep_enabled) {
        return -1;
    }
    
    // Find a free timer slot
    for (int i = 0; i < MAX_SLEEP_TIMERS; i++) {
        if (!sleep_timers[i].active) {
            sleep_timers[i].active = true;
            sleep_timers[i].target_ticks = timer_ticks + timer_ms_to_ticks(ms);
            sleep_timers[i].callback = callback;
            return i;
        }
    }
    
    return -1;  // No free timer slots
}

/**
 * @brief Cancel a registered sleep timer
 * 
 * @param timer_id Timer ID returned from sleep_timer_register
 * @return true if timer was canceled, false if invalid ID
 */
bool sleep_timer_cancel(int timer_id) {
    if (timer_id < 0 || timer_id >= MAX_SLEEP_TIMERS) {
        return false;
    }
    
    sleep_timers[timer_id].active = false;
    return true;
}

/**
 * @brief Get the current PIT frequency
 * 
 * @return Current frequency in Hz
 */
uint32_t timer_get_frequency(void) {
    return timer_frequency;
}

/**
 * @brief Change the PIT frequency
 * 
 * @param frequency New frequency in Hz
 */
void timer_set_frequency(uint32_t frequency) {
    if (frequency == timer_frequency) {
        return;  // No change needed
    }
    
    // Disable interrupts temporarily
    bool interrupts_enabled = (get_eflags() & (1 << 9)) != 0;
    if (interrupts_enabled) {
        disable_interrupts();
    }
    
    // Update frequency and reset state
    timer_frequency = frequency;
    
    // Set new PIT frequency
    set_pit_frequency(frequency);
    
    // Restore interrupts if they were enabled
    if (interrupts_enabled) {
        enable_interrupts();
    }
    
    kprintf("Timer: Frequency changed to %u Hz\n", frequency);
}

/**
 * @brief Get the EFLAGS register value
 * 
 * @return EFLAGS register value
 */
uint64_t get_eflags(void) {
    uint64_t eflags;
    asm volatile("pushfq; popq %0" : "=r"(eflags));
    return eflags;
}

/**
 * @brief Get the current time in a human-readable format since epoch
 * 
 * Note: This is a placeholder. A real implementation would interface with CMOS/RTC
 * 
 * @param year Pointer to store year (or NULL)
 * @param month Pointer to store month (or NULL)
 * @param day Pointer to store day (or NULL)
 * @param hour Pointer to store hour (or NULL)
 * @param minute Pointer to store minute (or NULL)
 * @param second Pointer to store second (or NULL)
 */
void timer_get_datetime(uint16_t* year, uint8_t* month, uint8_t* day,
                       uint8_t* hour, uint8_t* minute, uint8_t* second) {
    // Placeholder values - these should come from RTC
    if (year) *year = 2025;
    if (month) *month = 5;
    if (day) *day = 11;
    if (hour) *hour = 3;
    if (minute) *minute = 0;
    if (second) *second = 0;
    
    // TODO: Implement real date/time from RTC
}
