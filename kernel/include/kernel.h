/**
 * @file kernel.h
 * @brief Main kernel header for dsOS
 * 
 * This file contains the core definitions and interfaces for the
 * dsOS kernel (dKernel).
 */

#ifndef _KERNEL_H
#define _KERNEL_H

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

/**
 * @brief Kernel version information
 */
#define KERNEL_VERSION_MAJOR    0
#define KERNEL_VERSION_MINOR    1
#define KERNEL_VERSION_PATCH    0

/**
 * @brief Constants for kernel panic and error handling
 */
#define PANIC_NORMAL            0   // Normal panic (bug or unrecoverable error)
#define PANIC_HOS_BREACH        1   // Hidden OS breach detected
#define PANIC_HARDWARE_FAULT    2   // Hardware fault detected

/**
 * @brief Status flags for kernel subsystems
 */
extern bool init_done;     // Kernel initialization complete
extern bool fb_ready;      // Framebuffer initialized and ready
extern bool kbd_ready;     // Keyboard initialized and ready

/**
 * @brief Print a message to the kernel debug console
 * 
 * @param fmt Format string (printf-style)
 * @param ... Format arguments
 */
void kprintf(const char* fmt, ...);

/**
 * @brief Trigger a kernel panic (non-returning)
 * 
 * @param type Type of panic (PANIC_* constants)
 * @param msg Error message
 * @param file Source file where panic was triggered
 * @param line Line number where panic was triggered
 */
void panic(int type, const char* msg, const char* file, int line);

/**
 * @brief Kernel assertion macro
 * 
 * If the condition is false, triggers a kernel panic
 */
#define kassert(cond) \
    do { \
        if (!(cond)) { \
            panic(PANIC_NORMAL, "Assertion failed: " #cond, __FILE__, __LINE__); \
        } \
    } while (0)

/**
 * @brief Report a breach in the Hidden OS protection system
 * 
 * Displays a red screen with error message and register dump
 * 
 * @param reason String describing the breach reason
 * @param addr Address where breach was detected
 */
void hos_breach(const char* reason, uintptr_t addr);

/**
 * @brief Return current CPU flags
 */
static inline uint64_t read_flags(void) {
    uint64_t flags;
    asm volatile("pushfq; popq %0" : "=r"(flags));
    return flags;
}

/**
 * @brief Disable interrupts
 */
static inline void cli(void) {
    asm volatile("cli");
}

/**
 * @brief Enable interrupts
 */
static inline void sti(void) {
    asm volatile("sti");
}

/**
 * @brief Halt the CPU
 */
static inline void hlt(void) {
    asm volatile("hlt");
}

/**
 * @brief Read from an I/O port
 * 
 * @param port Port number
 * @return Value read from port
 */
static inline uint8_t inb(uint16_t port) {
    uint8_t ret;
    asm volatile("inb %1, %0" : "=a"(ret) : "Nd"(port));
    return ret;
}

/**
 * @brief Write to an I/O port
 * 
 * @param port Port number
 * @param val Value to write
 */
static inline void outb(uint16_t port, uint8_t val) {
    asm volatile("outb %0, %1" : : "a"(val), "Nd"(port));
}

/**
 * @brief Kernel entry point
 * 
 * @param mb_info Multiboot information structure
 */
void kernel_main(uintptr_t mb_info);

#endif /* _KERNEL_H */
