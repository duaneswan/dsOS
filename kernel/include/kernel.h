/**
 * @file kernel.h
 * @brief Core kernel definitions, constants, and function prototypes
 */

#ifndef _KERNEL_H
#define _KERNEL_H

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

/**
 * @brief Kernel version information
 */
#define KERNEL_VERSION_MAJOR 0
#define KERNEL_VERSION_MINOR 1
#define KERNEL_VERSION_PATCH 0

/**
 * @brief Panic types
 */
#define PANIC_NORMAL         0
#define PANIC_HOS_BREACH     1
#define PANIC_HARDWARE_FAULT 2

/**
 * @brief Assertion macro for debugging
 */
#define kassert(condition) \
    do { \
        if (!(condition)) { \
            panic(PANIC_NORMAL, "Assertion failed: " #condition, __FILE__, __LINE__); \
        } \
    } while (0)

/**
 * @brief CPU control instructions
 */
static inline void cli(void) { __asm__ volatile("cli"); }
static inline void sti(void) { __asm__ volatile("sti"); }
static inline void hlt(void) { __asm__ volatile("hlt"); }

/**
 * @brief Read RFLAGS register
 * 
 * @return Current value of RFLAGS
 */
static inline uint64_t read_flags(void) {
    uint64_t rflags;
    __asm__ volatile("pushfq; popq %0" : "=rm"(rflags) :: "memory");
    return rflags;
}

/**
 * @brief Port I/O operations
 */
static inline void outb(uint16_t port, uint8_t val) {
    __asm__ volatile("outb %0, %1" :: "a"(val), "Nd"(port));
}

static inline uint8_t inb(uint16_t port) {
    uint8_t ret;
    __asm__ volatile("inb %1, %0" : "=a"(ret) : "Nd"(port));
    return ret;
}

static inline void outw(uint16_t port, uint16_t val) {
    __asm__ volatile("outw %0, %1" :: "a"(val), "Nd"(port));
}

static inline uint16_t inw(uint16_t port) {
    uint16_t ret;
    __asm__ volatile("inw %1, %0" : "=a"(ret) : "Nd"(port));
    return ret;
}

static inline void outl(uint16_t port, uint32_t val) {
    __asm__ volatile("outl %0, %1" :: "a"(val), "Nd"(port));
}

static inline uint32_t inl(uint16_t port) {
    uint32_t ret;
    __asm__ volatile("inl %1, %0" : "=a"(ret) : "Nd"(port));
    return ret;
}

/**
 * @brief IO wait - writes to an unused port to introduce a small delay
 */
static inline void io_wait(void) {
    outb(0x80, 0);
}

/**
 * @brief Memory operations
 */
void* memcpy(void* dest, const void* src, size_t n);
void* memset(void* s, int c, size_t n);
void* memmove(void* dest, const void* src, size_t n);
int memcmp(const void* s1, const void* s2, size_t n);

/**
 * @brief String operations
 */
size_t strlen(const char* s);
char* strcpy(char* dest, const char* src);
char* strncpy(char* dest, const char* src, size_t n);
int strcmp(const char* s1, const char* s2);
int strncmp(const char* s1, const char* s2, size_t n);
char* strcat(char* dest, const char* src);
char* strncat(char* dest, const char* src, size_t n);

/**
 * @brief Printf family functions
 */
void kprintf(const char* fmt, ...);
int snprintf(char* buffer, size_t size, const char* fmt, ...);

/**
 * @brief Kernel panic function (does not return)
 */
void panic(int type, const char* msg, const char* file, int line);

/**
 * @brief Report a breach in Hidden OS protection
 */
void hos_breach(const char* reason, uintptr_t addr);

/**
 * @brief Kernel entry point
 */
void kernel_main(uintptr_t mb_info);

#endif /* _KERNEL_H */
