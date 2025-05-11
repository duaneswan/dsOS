/**
 * @file kernel.c
 * @brief Main kernel implementation
 * 
 * This file contains the kernel entry point and core functionality.
 */

#include <kernel.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdarg.h>

// Forward declarations for subsystem init functions
void gdt_init(void);
void idt_init(void);
void mm_init(uintptr_t mem_upper);
void serial_init(void);
void vga_init(void);
void pic_init(void);
void timer_init(void);
void kbd_init(void);
void hos_init(void);
void sched_init(void);

// Global kernel status flags
bool init_done = false;    // Kernel initialization complete
bool fb_ready = false;     // Framebuffer initialized and ready
bool kbd_ready = false;    // Keyboard initialized and ready
bool recovery_mode = false;  // Recovery mode flag

// Terminal state
uint32_t terminal_row = 0;
uint32_t terminal_column = 0;

// Framebuffer state
void* framebuffer_base = NULL;
uint32_t framebuffer_width = 0;
uint32_t framebuffer_height = 0;
uint32_t framebuffer_pitch = 0;
uint32_t framebuffer_bpp = 0;

// Early VGA console support for debugging
#define VGA_WIDTH 80
#define VGA_HEIGHT 25
#define VGA_MEMORY 0xB8000

static uint16_t* vga_buffer = (uint16_t*)VGA_MEMORY;
static uint8_t vga_color = 0x07; // Light gray on black
static int vga_row = 0;
static int vga_col = 0;

// COM1 port for serial output
#define COM1 0x3F8

/**
 * @brief Put a character to the VGA console
 * 
 * @param c Character to display
 */
static void vga_putchar(char c) {
    if (c == '\n') {
        vga_col = 0;
        vga_row++;
        if (vga_row >= VGA_HEIGHT) {
            // Scroll screen
            for (int y = 0; y < VGA_HEIGHT - 1; y++) {
                for (int x = 0; x < VGA_WIDTH; x++) {
                    vga_buffer[y * VGA_WIDTH + x] = vga_buffer[(y + 1) * VGA_WIDTH + x];
                }
            }
            
            // Clear bottom row
            for (int x = 0; x < VGA_WIDTH; x++) {
                vga_buffer[(VGA_HEIGHT - 1) * VGA_WIDTH + x] = (vga_color << 8) | ' ';
            }
            vga_row = VGA_HEIGHT - 1;
        }
        return;
    }
    
    if (c == '\r') {
        vga_col = 0;
        return;
    }
    
    vga_buffer[vga_row * VGA_WIDTH + vga_col] = (vga_color << 8) | c;
    vga_col++;
    
    if (vga_col >= VGA_WIDTH) {
        vga_col = 0;
        vga_row++;
        if (vga_row >= VGA_HEIGHT) {
            vga_row = 0; // Wrap to top
        }
    }
}

/**
 * @brief Put a character to the serial port
 * 
 * @param c Character to send
 */
static void serial_putchar(char c) {
    // Wait for transmission buffer to be empty
    while ((inb(COM1 + 5) & 0x20) == 0);
    
    // Send the character
    outb(COM1, c);
}

/**
 * @brief Clear the VGA screen
 */
static void vga_clear(void) {
    for (int y = 0; y < VGA_HEIGHT; y++) {
        for (int x = 0; x < VGA_WIDTH; x++) {
            vga_buffer[y * VGA_WIDTH + x] = (vga_color << 8) | ' ';
        }
    }
    vga_row = 0;
    vga_col = 0;
}

/**
 * @brief Set the VGA console color
 * 
 * @param fg Foreground color (0-15)
 * @param bg Background color (0-7)
 */
static void vga_set_color(uint8_t fg, uint8_t bg) {
    vga_color = (bg << 4) | (fg & 0x0F);
}

/**
 * @brief Write a string to the VGA console
 * 
 * @param str String to write
 */
static void vga_puts(const char* str) {
    for (size_t i = 0; str[i] != '\0'; i++) {
        vga_putchar(str[i]);
    }
}

/**
 * @brief Write a string to the serial port
 * 
 * @param str String to write
 */
static void serial_puts(const char* str) {
    for (size_t i = 0; str[i] != '\0'; i++) {
        serial_putchar(str[i]);
    }
}

/**
 * @brief Convert an integer to a string
 * 
 * @param num Integer to convert
 * @param base Base for conversion (10 for decimal, 16 for hex)
 * @param buf Buffer to store result
 * @param width Minimum field width
 * @param pad Padding character
 * @return Length of the resulting string
 */
static int itoa(uint64_t num, int base, char* buf, int width, char pad) {
    static const char digits[] = "0123456789ABCDEF";
    char tmp[64];
    int i = 0, len;
    
    // Handle special case of zero
    if (num == 0) {
        tmp[i++] = '0';
    } else {
        // Convert to string in reverse order
        while (num > 0) {
            tmp[i++] = digits[num % base];
            num /= base;
        }
    }
    
    // Determine actual length
    len = i;
    
    // Pad if needed
    while (i < width) {
        tmp[i++] = pad;
    }
    
    // Reverse the string into the output buffer
    while (i > 0) {
        *buf++ = tmp[--i];
    }
    
    return len;
}

/**
 * @brief Print formatted output to debug console
 * 
 * @param fmt Format string (printf-style)
 * @param ... Format arguments
 */
void kprintf(const char* fmt, ...) {
    char buf[1024];
    va_list args;
    int i = 0;

    va_start(args, fmt);
    
    for (const char* p = fmt; *p != '\0'; p++) {
        if (*p != '%') {
            buf[i++] = *p;
            continue;
        }
        
        // Handle format specifier
        p++;
        
        // Check for width/padding
        int width = 0;
        char pad = ' ';
        
        if (*p == '0') {
            pad = '0';
            p++;
        }
        
        while (*p >= '0' && *p <= '9') {
            width = width * 10 + (*p - '0');
            p++;
        }
        
        switch (*p) {
            case 'd': {
                int val = va_arg(args, int);
                if (val < 0) {
                    buf[i++] = '-';
                    val = -val;
                }
                i += itoa(val, 10, &buf[i], width, pad);
                break;
            }
            
            case 'u':
                i += itoa(va_arg(args, unsigned int), 10, &buf[i], width, pad);
                break;
                
            case 'x':
                i += itoa(va_arg(args, unsigned int), 16, &buf[i], width, pad);
                break;
                
            case 'p':
                buf[i++] = '0';
                buf[i++] = 'x';
                i += itoa(va_arg(args, uintptr_t), 16, &buf[i], width ? width : 16, pad ? pad : '0');
                break;
                
            case 'c':
                buf[i++] = va_arg(args, int);
                break;
                
            case 's': {
                const char* s = va_arg(args, const char*);
                if (!s) s = "(null)";
                while (*s) {
                    buf[i++] = *s++;
                }
                break;
            }
            
            default:
                buf[i++] = *p;
                break;
        }
    }
    
    va_end(args);
    
    // Null-terminate and print
    buf[i] = '\0';
    
    // Output to both VGA and serial
    vga_puts(buf);
    serial_puts(buf);
}

/**
 * @brief Trigger a kernel panic (non-returning)
 * 
 * @param type Type of panic (PANIC_* constants)
 * @param msg Error message
 * @param file Source file where panic was triggered
 * @param line Line number where panic was triggered
 */
void panic(int type, const char* msg, const char* file, int line) {
    // Disable interrupts
    cli();
    
    // Set panic colors based on type
    switch (type) {
        case PANIC_HOS_BREACH:
            vga_set_color(0x0F, 0x04); // White on red
            break;
            
        case PANIC_HARDWARE_FAULT:
            vga_set_color(0x0F, 0x01); // White on blue
            break;
            
        default:
            vga_set_color(0x0F, 0x00); // White on black
            break;
    }
    
    // Clear screen
    vga_clear();
    
    // Print panic message
    kprintf("*** KERNEL PANIC ***\n\n");
    kprintf("Error: %s\n", msg);
    kprintf("Location: %s:%d\n\n", file, line);
    
    // Print register dump
    uint64_t rax, rbx, rcx, rdx, rsi, rdi, rbp, rsp, r8, r9, r10, r11, r12, r13, r14, r15, rflags;
    
    asm volatile (
        "mov %%rax, %0\n"
        "mov %%rbx, %1\n"
        "mov %%rcx, %2\n"
        "mov %%rdx, %3\n"
        "mov %%rsi, %4\n"
        "mov %%rdi, %5\n"
        "mov %%rbp, %6\n"
        "mov %%rsp, %7\n"
        "mov %%r8, %8\n"
        "mov %%r9, %9\n"
        "mov %%r10, %10\n"
        "mov %%r11, %11\n"
        "mov %%r12, %12\n"
        "mov %%r13, %13\n"
        "mov %%r14, %14\n"
        "mov %%r15, %15\n"
        : "=m"(rax), "=m"(rbx), "=m"(rcx), "=m"(rdx),
          "=m"(rsi), "=m"(rdi), "=m"(rbp), "=m"(rsp),
          "=m"(r8), "=m"(r9), "=m"(r10), "=m"(r11),
          "=m"(r12), "=m"(r13), "=m"(r14), "=m"(r15)
        :
        : "memory"
    );
    
    rflags = read_flags();
    
    kprintf("Register dump:\n");
    kprintf("RAX: 0x%016x  RBX: 0x%016x  RCX: 0x%016x  RDX: 0x%016x\n", rax, rbx, rcx, rdx);
    kprintf("RSI: 0x%016x  RDI: 0x%016x  RBP: 0x%016x  RSP: 0x%016x\n", rsi, rdi, rbp, rsp);
    kprintf("R8:  0x%016x  R9:  0x%016x  R10: 0x%016x  R11: 0x%016x\n", r8, r9, r10, r11);
    kprintf("R12: 0x%016x  R13: 0x%016x  R14: 0x%016x  R15: 0x%016x\n", r12, r13, r14, r15);
    kprintf("RFLAGS: 0x%016x\n\n", rflags);
    
    if (type == PANIC_HOS_BREACH) {
        kprintf("SECURITY BREACH DETECTED IN HIDDEN OS PROTECTION\n");
        kprintf("SYSTEM HALTED\n");
    } else {
        kprintf("System halted.\n");
    }
    
    // Hang forever
    while (1) {
        hlt();
    }
}

/**
 * @brief Report a breach in the Hidden OS protection system
 * 
 * @param reason String describing the breach reason
 * @param addr Address where breach was detected
 */
void hos_breach(const char* reason, uintptr_t addr) {
    char buf[256];
    snprintf(buf, sizeof(buf), "HOS breach: %s at address 0x%016llx", reason, addr);
    panic(PANIC_HOS_BREACH, buf, __FILE__, __LINE__);
}

/**
 * @brief Extract boot information
 * 
 * @param mb_info Multiboot information structure
 */
static void extract_boot_info(uintptr_t mb_info) {
    // For now, we'll just check if we're in recovery mode
    // In a full implementation, we would extract more boot information
    
    // Check if recovery flag is set
    extern uint8_t recoveryFlag;
    if (recoveryFlag) {
        recovery_mode = true;
        kprintf("Boot: Recovery mode enabled\n");
    }
}

/**
 * @brief Initialize the serial port for debugging
 */
static void early_serial_init(void) {
    // Initialize COM1 serial port for debug output
    outb(COM1 + 1, 0x00);    // Disable interrupts
    outb(COM1 + 3, 0x80);    // Enable DLAB
    outb(COM1 + 0, 0x03);    // Set divisor to 3 (38400 baud)
    outb(COM1 + 1, 0x00);    // High byte of divisor
    outb(COM1 + 3, 0x03);    // 8 bits, no parity, one stop bit
    outb(COM1 + 2, 0xC7);    // Enable FIFO, clear them, with 14-byte threshold
    outb(COM1 + 4, 0x0B);    // IRQs enabled, RTS/DSR set
    
    // Test serial port
    outb(COM1 + 4, 0x1E);    // Set in loopback mode
    outb(COM1 + 0, 0xAE);    // Send test byte
    if (inb(COM1) != 0xAE) {
        // Serial port not working, fallback to VGA only
        return;
    }
    
    // Normal operation
    outb(COM1 + 4, 0x0F);
    
    // Send initial message
    serial_puts("dsOS kernel serial console initialized\r\n");
}

/**
 * @brief Kernel entry point
 * 
 * @param mb_info Multiboot information structure
 */
void kernel_main(uintptr_t mb_info) {
    // Initialize early console for debug output
    vga_clear();
    early_serial_init();
    
    // Display welcome message
    kprintf("dKernel v%d.%d.%d starting...\n",
            KERNEL_VERSION_MAJOR,
            KERNEL_VERSION_MINOR,
            KERNEL_VERSION_PATCH);
    
    // Extract boot information
    extract_boot_info(mb_info);
    
    // Initialize CPU-specific structures
    kprintf("Initializing CPU structures... ");
    gdt_init();
    idt_init();
    kprintf("done\n");
    
    // Initialize memory management
    kprintf("Initializing memory management... ");
    mm_init(0); // placeholder, will be replaced with actual memory size
    kprintf("done\n");
    
    // Initialize device drivers
    kprintf("Initializing device drivers... ");
    serial_init();
    vga_init();
    pic_init();
    kprintf("done\n");
    
    // Initialize timer and enable interrupts
    kprintf("Initializing system timer... ");
    timer_init();
    sti(); // Enable interrupts
    kprintf("done\n");
    
    // Initialize keyboard
    kprintf("Initializing keyboard... ");
    kbd_init();
    kbd_ready = true;
    kprintf("done\n");
    
    // Initialize hidden OS protection
    kprintf("Initializing Hidden OS protection... ");
    hos_init();
    kprintf("done\n");
    
    // Initialize scheduler
    kprintf("Initializing process scheduler... ");
    sched_init();
    kprintf("done\n");
    
    // Kernel initialization complete
    kprintf("Kernel initialization complete\n");
    init_done = true;
    
    // Initialize framebuffer for GUI
    fb_ready = true;
    
    // TODO: Pass control to userspace init process
    kprintf("Waiting for userspace to start...\n");
    
    // For now, just wait in a loop
    while (1) {
        hlt();
    }
}
