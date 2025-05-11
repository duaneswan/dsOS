/**
 * @file kernel.h
 * @brief Main kernel header file
 */

#ifndef _KERNEL_H
#define _KERNEL_H

#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>

/**
 * @brief POSIX-compatible type definitions
 */
typedef int64_t ssize_t;
typedef uint32_t mode_t;
typedef uint32_t dev_t;
typedef uint32_t ino_t;
typedef uint16_t uid_t;
typedef uint16_t gid_t;

/**
 * @brief OS version information
 */
#define OS_NAME     "dsOS"
#define OS_VERSION  "0.1"
#define OS_FULLNAME OS_NAME " v" OS_VERSION

/**
 * @brief Architecture definitions
 */
#define ARCH_X86_64

/**
 * @brief Kernel symbol visibility
 */
#define KERNEL_API

/**
 * @brief Useful macros
 */
#define ARRAY_SIZE(x) (sizeof(x) / sizeof((x)[0]))
#define ALIGN_UP(x, align) (((x) + ((align) - 1)) & ~((align) - 1))
#define ALIGN_DOWN(x, align) ((x) & ~((align) - 1))
#define IS_ALIGNED(x, align) (((x) & ((align) - 1)) == 0)
#define DIV_ROUND_UP(n, d) (((n) + (d) - 1) / (d))
#define UNUSED(x) ((void)(x))
#define PACKED __attribute__((packed))
#define NORETURN __attribute__((noreturn))
#define WEAK __attribute__((weak))
#define ALIGN(x) __attribute__((aligned(x)))
#define SECTION(x) __attribute__((section(x)))
#define ALWAYS_INLINE __attribute__((always_inline))
#define KERNEL_STACK_SIZE 16384

/**
 * @brief Assembly helpers
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

static inline uint8_t inb(uint16_t port) {
    uint8_t value;
    __asm__ volatile("inb %1, %0" : "=a"(value) : "d"(port));
    return value;
}

static inline void outb(uint16_t port, uint8_t value) {
    __asm__ volatile("outb %0, %1" : : "a"(value), "d"(port));
}

static inline uint16_t inw(uint16_t port) {
    uint16_t value;
    __asm__ volatile("inw %1, %0" : "=a"(value) : "d"(port));
    return value;
}

static inline void outw(uint16_t port, uint16_t value) {
    __asm__ volatile("outw %0, %1" : : "a"(value), "d"(port));
}

static inline uint32_t inl(uint16_t port) {
    uint32_t value;
    __asm__ volatile("inl %1, %0" : "=a"(value) : "d"(port));
    return value;
}

static inline void outl(uint16_t port, uint32_t value) {
    __asm__ volatile("outl %0, %1" : : "a"(value), "d"(port));
}

static inline void io_wait(void) {
    // Port 0x80 is used for 'dummy' I/O operations
    outb(0x80, 0);
}

static inline uint64_t rdmsr(uint32_t msr) {
    uint32_t eax, edx;
    __asm__ volatile("rdmsr" : "=a"(eax), "=d"(edx) : "c"(msr));
    return ((uint64_t)edx << 32) | eax;
}

static inline void wrmsr(uint32_t msr, uint64_t value) {
    uint32_t eax = value & 0xFFFFFFFF;
    uint32_t edx = value >> 32;
    __asm__ volatile("wrmsr" : : "a"(eax), "d"(edx), "c"(msr));
}

static inline void invlpg(void* addr) {
    __asm__ volatile("invlpg (%0)" : : "r"(addr) : "memory");
}

static inline void invalidate_tlb_entry(uintptr_t addr) {
    __asm__ volatile("invlpg (%0)" : : "r"(addr) : "memory");
}

static inline void wbinvd(void) {
    __asm__ volatile("wbinvd");
}

/**
 * @brief Panic-related definitions
 */
#define PANIC_NORMAL    0
#define PANIC_CRITICAL  1
#define PANIC_HOS_BREACH 2

void panic(int type, const char* message, const char* file, int line);
void kassert_func(bool condition, const char* message, const char* file, int line);
void hos_breach(int breach_type, uintptr_t address, uint64_t expected, uint64_t actual);
void halt(void) NORETURN;

#define kassert(cond) kassert_func((cond), #cond, __FILE__, __LINE__)

/**
 * @brief Memory manager functions (declared in memory.h)
 */
void* kmalloc(size_t size);
void* kzalloc(size_t size);
void* kmalloc_aligned(size_t size, size_t align);
void kfree(void* ptr);
void* krealloc(void* ptr, size_t size);

/**
 * @brief String functions (declared in string.h)
 */
void* memcpy(void* dest, const void* src, size_t n);
void* memmove(void* dest, const void* src, size_t n);
void* memset(void* s, int c, size_t n);
int memcmp(const void* s1, const void* s2, size_t n);
size_t strlen(const char* s);
char* strcpy(char* dest, const char* src);
char* strncpy(char* dest, const char* src, size_t n);
char* strcat(char* dest, const char* src);
char* strncat(char* dest, const char* src, size_t n);
int strcmp(const char* s1, const char* s2);
int strncmp(const char* s1, const char* s2, size_t n);
char* strchr(const char* s, int c);
char* strrchr(const char* s, int c);
char* strstr(const char* haystack, const char* needle);
char* strupr(char* s);
char* strlwr(char* s);
char* strdup(const char* s);

/**
 * @brief Console/print functions (declared in printf.h)
 */
int kprintf(const char* fmt, ...);
void kprintf_set_mode(int mode);
int snprintf(char* buffer, size_t size, const char* fmt, ...);

/**
 * @brief VGA console functions (declared in vga.h)
 */
void vga_init(void);
void vga_clear(void);
void vga_putchar(char c);
void vga_print(const char* str);
void vga_set_color(uint8_t fg, uint8_t bg);
uint8_t vga_make_color(uint8_t fg, uint8_t bg);
void vga_enable_cursor(bool enable);
void vga_set_cursor_pos(int x, int y);

/**
 * @brief Serial port functions (declared in serial.h)
 */
void serial_init(uint16_t port);
void serial_write_byte(uint16_t port, uint8_t byte);
uint8_t serial_read_byte(uint16_t port);
void serial_write_str(uint16_t port, const char* str);
bool serial_is_transmit_empty(uint16_t port);
bool serial_has_received(uint16_t port);

/**
 * @brief CPU/architecture specific functions (declared in cpu.h)
 */
void gdt_init(void);
void idt_init(void);
void pic_init(void);
void timer_init(uint32_t frequency);
void keyboard_init(void);

/**
 * @brief Interrupt handlers (declared in interrupt.h)
 */
typedef void (*interrupt_handler_t)(void);
void register_interrupt_handler(uint8_t interrupt, interrupt_handler_t handler);

/**
 * @brief Hidden OS (hOS) protection (declared in hos.h)
 */
void hos_init(void);
void hos_monitor_region(uintptr_t start, size_t size, bool exec, bool write);
void hos_hash_region(uintptr_t start, size_t size);
void hos_verify_region(uintptr_t start, size_t size);

/**
 * @brief Filesystem (swanFS) functions (declared in swanfs.h)
 */
int sw_open(const char* path, int flags, int mode);
ssize_t sw_read(int fd, void* buf, size_t count);
ssize_t sw_write(int fd, const void* buf, size_t count);
int sw_close(int fd);
int sw_mkdir(const char* path, mode_t mode);
int sw_unlink(const char* path);
int sw_mount(const char* source, const char* target);
int sw_unmount(const char* target);

/**
 * @brief Graphical functions (declared in gfx.h)
 */
void gfx_init(void);
void gfx_set_resolution(int width, int height, int bpp);
void* gfx_get_framebuffer(void);
void gfx_swap_buffers(void);
void gfx_draw_logo(void);

/**
 * @brief Boot image functions
 */
void boot_logo_init(void);
void boot_logo_show(void);
void boot_logo_fade_in(uint32_t duration_ms);
void boot_logo_fade_out(uint32_t duration_ms);

/**
 * @brief User management functions
 */
int user_create(const char* username, const char* password, const char* fullname);
int user_authenticate(const char* username, const char* password);
int user_set_password(const char* username, const char* old_password, const char* new_password);
int user_delete(const char* username);
int user_get_home(const char* username, char* home_path, size_t size);

/**
 * @brief Session management functions
 */
void session_init(void);
int session_start(const char* username);
int session_end(void);

/**
 * @brief System state
 */
KERNEL_API extern bool init_done;
KERNEL_API extern bool fb_ready;
KERNEL_API extern bool kbd_ready;
KERNEL_API extern bool graphics_mode;
KERNEL_API extern uintptr_t kernel_end;
KERNEL_API extern uint16_t debug_port;

#endif /* _KERNEL_H */
