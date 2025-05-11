/**
 * @file panic.c
 * @brief Kernel panic and assertion functions
 */

#include "../include/kernel.h"
#include <stdint.h>
#include <stdbool.h>
#include <stdarg.h>

// Panic types
#define PANIC_NORMAL      0   // Normal kernel panic (white on blue)
#define PANIC_CRITICAL    1   // Critical failure (white on red)
#define PANIC_HOS_BREACH  2   // Hidden OS security breach (yellow on red)

// Screen dimensions for panic screen
#define PANIC_COLS        80
#define PANIC_ROWS        25

// Panic screen colors
#define PANIC_NORMAL_BG   0x1  // Blue background
#define PANIC_CRITICAL_BG 0x4  // Red background
#define PANIC_HOS_BG      0x4  // Red background
#define PANIC_NORMAL_FG   0xF  // White text
#define PANIC_HOS_FG      0xE  // Yellow text

// Strings for panic screens
static const char* panic_header = " dsOS Kernel Panic ";
static const char* hos_breach_header = " dsOS Security Alert: Hidden OS Protection Breach ";
static const char* panic_footer = " System Halted ";
static const char* reboot_message = "Press Alt+Ctrl+Del to restart";

// HOS breach types
static const char* hos_breach_types[] = {
    "Unknown Violation",
    "Read Violation",
    "Write Violation",
    "Execute Violation",
    "Hash Verification Failure",
    "Disappearance Detected"
};

// Registers for panic screen
typedef struct {
    uint64_t rax, rbx, rcx, rdx;
    uint64_t rsi, rdi, rbp, rsp;
    uint64_t r8, r9, r10, r11;
    uint64_t r12, r13, r14, r15;
    uint64_t rip, rflags;
    uint64_t cr0, cr2, cr3, cr4;
} PACKED panic_regs_t;

// Get current register values
static void get_registers(panic_regs_t* regs) {
    __asm__ volatile(
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
        "lea 0(%%rip), %%rax\n"
        "mov %%rax, %16\n"
        "pushfq\n"
        "pop %%rax\n"
        "mov %%rax, %17\n"
        "mov %%cr0, %%rax\n"
        "mov %%rax, %18\n"
        "mov %%cr2, %%rax\n"
        "mov %%rax, %19\n"
        "mov %%cr3, %%rax\n"
        "mov %%rax, %20\n"
        "mov %%cr4, %%rax\n"
        "mov %%rax, %21\n"
        : "=m" (regs->rax), "=m" (regs->rbx), "=m" (regs->rcx), "=m" (regs->rdx),
          "=m" (regs->rsi), "=m" (regs->rdi), "=m" (regs->rbp), "=m" (regs->rsp),
          "=m" (regs->r8), "=m" (regs->r9), "=m" (regs->r10), "=m" (regs->r11),
          "=m" (regs->r12), "=m" (regs->r13), "=m" (regs->r14), "=m" (regs->r15),
          "=m" (regs->rip), "=m" (regs->rflags), "=m" (regs->cr0), "=m" (regs->cr2),
          "=m" (regs->cr3), "=m" (regs->cr4)
        :
        : "rax", "memory"
    );
    
    // Adjust RSP - it was modified by our function call
    regs->rsp += 8 * 8; // Approximate adjustment
}

/**
 * @brief Draw a box on the screen using text mode characters
 * 
 * @param x X position
 * @param y Y position
 * @param width Box width
 * @param height Box height
 * @param color Color attribute
 */
static void draw_box(int x, int y, int width, int height, uint8_t color) {
    int i, j;
    
    // Draw top border
    vga_putchar_at(0xC9, x, y, color); // ┌
    for (i = 1; i < width - 1; i++) {
        vga_putchar_at(0xCD, x + i, y, color); // ─
    }
    vga_putchar_at(0xBB, x + width - 1, y, color); // ┐
    
    // Draw sides
    for (i = 1; i < height - 1; i++) {
        vga_putchar_at(0xBA, x, y + i, color); // │
        for (j = 1; j < width - 1; j++) {
            vga_putchar_at(' ', x + j, y + i, color); // space
        }
        vga_putchar_at(0xBA, x + width - 1, y + i, color); // │
    }
    
    // Draw bottom border
    vga_putchar_at(0xC8, x, y + height - 1, color); // └
    for (i = 1; i < width - 1; i++) {
        vga_putchar_at(0xCD, x + i, y + height - 1, color); // ─
    }
    vga_putchar_at(0xBC, x + width - 1, y + height - 1, color); // ┘
}

/**
 * @brief Print a centered string
 * 
 * @param str String to print
 * @param y Y position
 * @param color Color attribute
 */
static void print_centered(const char* str, int y, uint8_t color) {
    int len = strlen(str);
    int x = (PANIC_COLS - len) / 2;
    if (x < 0) x = 0;
    
    vga_print_at(str, x, y, color);
}

/**
 * @brief Print register values
 * 
 * @param regs Register structure
 * @param x Starting X position
 * @param y Starting Y position
 * @param color Color attribute
 */
static void print_registers(panic_regs_t* regs, int x, int y, uint8_t color) {
    char buf[64];
    
    vga_print_at("CPU Registers:", x, y++, color);
    y++;
    
    snprintf(buf, sizeof(buf), "RAX: %016llX  RBX: %016llX", regs->rax, regs->rbx);
    vga_print_at(buf, x, y++, color);
    
    snprintf(buf, sizeof(buf), "RCX: %016llX  RDX: %016llX", regs->rcx, regs->rdx);
    vga_print_at(buf, x, y++, color);
    
    snprintf(buf, sizeof(buf), "RSI: %016llX  RDI: %016llX", regs->rsi, regs->rdi);
    vga_print_at(buf, x, y++, color);
    
    snprintf(buf, sizeof(buf), "RBP: %016llX  RSP: %016llX", regs->rbp, regs->rsp);
    vga_print_at(buf, x, y++, color);
    
    snprintf(buf, sizeof(buf), "R8:  %016llX  R9:  %016llX", regs->r8, regs->r9);
    vga_print_at(buf, x, y++, color);
    
    snprintf(buf, sizeof(buf), "R10: %016llX  R11: %016llX", regs->r10, regs->r11);
    vga_print_at(buf, x, y++, color);
    
    snprintf(buf, sizeof(buf), "R12: %016llX  R13: %016llX", regs->r12, regs->r13);
    vga_print_at(buf, x, y++, color);
    
    snprintf(buf, sizeof(buf), "R14: %016llX  R15: %016llX", regs->r14, regs->r15);
    vga_print_at(buf, x, y++, color);
    
    snprintf(buf, sizeof(buf), "RIP: %016llX  RFLAGS: %016llX", regs->rip, regs->rflags);
    vga_print_at(buf, x, y++, color);
    
    y++;
    vga_print_at("Control Registers:", x, y++, color);
    y++;
    
    snprintf(buf, sizeof(buf), "CR0: %016llX  CR2: %016llX", regs->cr0, regs->cr2);
    vga_print_at(buf, x, y++, color);
    
    snprintf(buf, sizeof(buf), "CR3: %016llX  CR4: %016llX", regs->cr3, regs->cr4);
    vga_print_at(buf, x, y++, color);
}

/**
 * @brief Kernel panic handler
 * 
 * @param type Panic type (PANIC_NORMAL, PANIC_CRITICAL, PANIC_HOS_BREACH)
 * @param message Panic message
 * @param file Source file where panic occurred
 * @param line Line number where panic occurred
 */
NORETURN void panic(int type, const char* message, const char* file, int line) {
    uint8_t bg_color, fg_color;
    char buf[256];
    panic_regs_t regs;
    
    // Disable interrupts
    cli();
    
    // Choose colors based on panic type
    switch (type) {
        case PANIC_CRITICAL:
            bg_color = PANIC_CRITICAL_BG;
            fg_color = PANIC_NORMAL_FG;
            break;
            
        case PANIC_HOS_BREACH:
            bg_color = PANIC_HOS_BG;
            fg_color = PANIC_HOS_FG;
            break;
            
        case PANIC_NORMAL:
        default:
            bg_color = PANIC_NORMAL_BG;
            fg_color = PANIC_NORMAL_FG;
            break;
    }
    
    // Set colors for the panic screen
    uint8_t color = vga_make_color(fg_color, bg_color);
    uint8_t header_color = vga_make_color(bg_color, fg_color);
    
    // Clear the screen with the panic color
    vga_clear();
    vga_set_color(fg_color, bg_color);
    
    // Get register values
    get_registers(&regs);
    
    // Draw panic screen
    if (type == PANIC_HOS_BREACH) {
        print_centered(hos_breach_header, 1, header_color);
    } else {
        print_centered(panic_header, 1, header_color);
    }
    
    // Print the panic message
    print_centered(message, 3, color);
    
    // Print the source location
    snprintf(buf, sizeof(buf), "at %s:%d", file, line);
    print_centered(buf, 4, color);
    
    // Print registers
    print_registers(&regs, 2, 6, color);
    
    // Print footer
    print_centered(panic_footer, PANIC_ROWS - 3, header_color);
    print_centered(reboot_message, PANIC_ROWS - 2, color);
    
    // Output to serial port if available
    if (serial_is_initialized(debug_port)) {
        serial_write_str(debug_port, "\n\n***** KERNEL PANIC *****\n");
        serial_write_str(debug_port, message);
        serial_write_str(debug_port, "\nat ");
        serial_write_str(debug_port, file);
        serial_write_str(debug_port, ":");
        
        snprintf(buf, sizeof(buf), "%d\n", line);
        serial_write_str(debug_port, buf);
        
        // Register dump
        serial_write_str(debug_port, "\nRegister dump:\n");
        snprintf(buf, sizeof(buf), "RAX: %016llX  RBX: %016llX\n", regs.rax, regs.rbx);
        serial_write_str(debug_port, buf);
        snprintf(buf, sizeof(buf), "RCX: %016llX  RDX: %016llX\n", regs.rcx, regs.rdx);
        serial_write_str(debug_port, buf);
        snprintf(buf, sizeof(buf), "RSI: %016llX  RDI: %016llX\n", regs.rsi, regs.rdi);
        serial_write_str(debug_port, buf);
        snprintf(buf, sizeof(buf), "RBP: %016llX  RSP: %016llX\n", regs.rbp, regs.rsp);
        serial_write_str(debug_port, buf);
        snprintf(buf, sizeof(buf), "RIP: %016llX  RFLAGS: %016llX\n", regs.rip, regs.rflags);
        serial_write_str(debug_port, buf);
        snprintf(buf, sizeof(buf), "CR0: %016llX  CR2: %016llX\n", regs.cr0, regs.cr2);
        serial_write_str(debug_port, buf);
        snprintf(buf, sizeof(buf), "CR3: %016llX  CR4: %016llX\n", regs.cr3, regs.cr4);
        serial_write_str(debug_port, buf);
    }
    
    // Halt the CPU
    halt();
}

/**
 * @brief Hidden OS protection breach handler
 * 
 * @param breach_type Type of breach
 * @param address Memory address where breach occurred
 * @param expected Expected value (for hash verification)
 * @param actual Actual value
 */
NORETURN void hos_breach(int breach_type, uintptr_t address, uint64_t expected, uint64_t actual) {
    char message[256];
    
    if (breach_type < 0 || breach_type >= (int)ARRAY_SIZE(hos_breach_types)) {
        breach_type = 0; // Unknown violation
    }
    
    // Format the breach message
    snprintf(message, sizeof(message), "HOS Protection: %s at 0x%016llX", 
             hos_breach_types[breach_type], (unsigned long long)address);
    
    // Call the panic handler with the breach message
    panic(PANIC_HOS_BREACH, message, "(kernel)", 0);
}

/**
 * @brief Assertion function
 * 
 * @param condition Condition to check
 * @param message Message to show if assertion fails
 * @param file Source file where assertion is located
 * @param line Line number where assertion is located
 */
void kassert_func(bool condition, const char* message, const char* file, int line) {
    if (!condition) {
        char buf[256];
        snprintf(buf, sizeof(buf), "Assertion failed: %s", message);
        panic(PANIC_NORMAL, buf, file, line);
    }
}

/**
 * @brief Halt the system and stop all execution
 */
NORETURN void halt(void) {
    // Disable interrupts if not already disabled
    cli();
    
    // Infinite loop with CPU halt
    while (1) {
        __asm__ volatile("hlt");
    }
}
