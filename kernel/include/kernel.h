/**
 * @file kernel.h
 * @brief Main kernel header with core definitions and functions
 */

#ifndef _KERNEL_H
#define _KERNEL_H

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

/**
 * @brief Define ssize_t as it's not in standard headers
 */
typedef long ssize_t;

/**
 * @brief Kernel version information
 */
#define KERNEL_NAME     "dKernel"
#define KERNEL_VERSION  "0.1.0"
#define KERNEL_DATE     __DATE__
#define KERNEL_TIME     __TIME__
#define KERNEL_YEAR     "2025"

/**
 * @brief Panic types
 */
#define PANIC_NORMAL         0   // Standard kernel panic
#define PANIC_HOS_BREACH     1   // Hidden OS protection breach
#define PANIC_HARDWARE_FAULT 2   // Hardware fault detection

/**
 * @brief VGA constants
 */
enum vga_color {
    VGA_COLOR_BLACK = 0,
    VGA_COLOR_BLUE = 1,
    VGA_COLOR_GREEN = 2,
    VGA_COLOR_CYAN = 3,
    VGA_COLOR_RED = 4,
    VGA_COLOR_MAGENTA = 5,
    VGA_COLOR_BROWN = 6,
    VGA_COLOR_LIGHT_GREY = 7,
    VGA_COLOR_DARK_GREY = 8,
    VGA_COLOR_LIGHT_BLUE = 9,
    VGA_COLOR_LIGHT_GREEN = 10,
    VGA_COLOR_LIGHT_CYAN = 11,
    VGA_COLOR_LIGHT_RED = 12,
    VGA_COLOR_LIGHT_MAGENTA = 13,
    VGA_COLOR_LIGHT_BROWN = 14,
    VGA_COLOR_WHITE = 15
};

/**
 * @brief Critical CPU operations
 */
static inline void cli(void) {
    __asm__ volatile("cli");
}

static inline void sti(void) {
    __asm__ volatile("sti");
}

static inline void hlt(void) {
    __asm__ volatile("hlt");
}

static inline void outb(uint16_t port, uint8_t val) {
    __asm__ volatile("outb %0, %1" : : "a"(val), "Nd"(port));
}

static inline uint8_t inb(uint16_t port) {
    uint8_t ret;
    __asm__ volatile("inb %1, %0" : "=a"(ret) : "Nd"(port));
    return ret;
}

static inline void io_wait(void) {
    outb(0x80, 0);
}

static inline void lidt(void* base, uint16_t size) {
    struct {
        uint16_t length;
        uint64_t base;
    } __attribute__((packed)) IDTR;

    IDTR.length = size - 1;
    IDTR.base = (uint64_t)base;
    __asm__ volatile("lidt (%0)" : : "r"(&IDTR));
}

static inline void lgdt(void* base, uint16_t size) {
    struct {
        uint16_t length;
        uint64_t base;
    } __attribute__((packed)) GDTR;

    GDTR.length = size - 1;
    GDTR.base = (uint64_t)base;
    __asm__ volatile("lgdt (%0)" : : "r"(&GDTR));
}

/**
 * @brief Debug and panic functions
 */
void panic(int type, const char* msg, const char* file, int line);
void hos_breach(const char* reason, uintptr_t addr);
void dump_memory(void* addr, size_t size);

/**
 * @brief To be used for assertions in kernel code
 */
#define kassert(expr) \
    if (!(expr)) { \
        panic(PANIC_NORMAL, "Assertion failed: " #expr, __FILE__, __LINE__); \
    }

/**
 * @brief String and memory functions
 */
void* memcpy(void* dest, const void* src, size_t n);
void* memmove(void* dest, const void* src, size_t n);
void* memset(void* s, int c, size_t n);
int memcmp(const void* s1, const void* s2, size_t n);
size_t strlen(const char* s);
char* strcpy(char* dest, const char* src);
char* strncpy(char* dest, const char* src, size_t n);
int strcmp(const char* s1, const char* s2);
int strncmp(const char* s1, const char* s2, size_t n);
char* strcat(char* dest, const char* src);
char* strncat(char* dest, const char* src, size_t n);
char* strchr(const char* s, int c);
char* strrchr(const char* s, int c);
char* strstr(const char* haystack, const char* needle);

/**
 * @brief Printing and formatting functions
 */
int kprintf(const char* fmt, ...);
int snprintf(char* buffer, size_t size, const char* fmt, ...);
void kprintf_set_mode(int mode);

/**
 * @brief System initialization functions
 */
void gdt_init(void);
void idt_init(void);
void pic_init(void);
void timer_init(uint32_t frequency);
void keyboard_init(void);
void serial_init(void);
void vga_init(void);

/**
 * @brief Memory management initialization
 */
void mm_init(uintptr_t mem_upper);
void heap_init(uintptr_t start, size_t size);

/**
 * @brief Hidden OS protection functions
 */
void hos_init(uintptr_t partition_start, size_t partition_size);
bool hos_verify_block(uint64_t block_num, const void* data, size_t size);

/**
 * @brief Boot logo functions
 */
void load_boot_logo(void);
void animate_boot_logo(bool fade_in);

/**
 * @brief SwanFS file system functions
 */
int sw_mount(const char* device, const char* mountpoint);
int sw_umount(const char* mountpoint);
int sw_open(const char* pathname, int flags, int mode);
int sw_close(int fd);
ssize_t sw_read(int fd, void* buf, size_t count);
ssize_t sw_write(int fd, const void* buf, size_t count);
int sw_mkdir(const char* pathname, uint32_t mode);
int sw_stat(const char* pathname, void* buf);
int sw_unlink(const char* pathname);

/**
 * @brief Boot menu functions
 */
int boot_menu_show(void);
void boot_menu_normal(void);
void boot_menu_terminal(void);
void boot_menu_recovery(void);
void boot_menu_reboot(void);

/**
 * @brief Progress indicator
 */
void progress_init(const char* message);
void progress_update(int percent);
void progress_done(void);

#endif /* _KERNEL_H */
