/**
 * @file panic.c
 * @brief Kernel panic and assertion functions
 */

#include "../include/kernel.h"
#include <stdarg.h>
#include <stdbool.h>

// Flag to prevent recursive panics
static volatile bool in_panic = false;

// Text colors for panic and breach screens
#define PANIC_TEXT_COLOR  0xFFFFFF   // White
#define PANIC_BG_COLOR    0x0000AA   // Blue
#define BREACH_TEXT_COLOR 0xFFFFFF   // White
#define BREACH_BG_COLOR   0xAA0000   // Red

// Register dump structure
typedef struct {
    uint64_t rax, rbx, rcx, rdx;
    uint64_t rsi, rdi, rbp, rsp;
    uint64_t r8, r9, r10, r11;
    uint64_t r12, r13, r14, r15;
    uint64_t rip, rflags;
    uint64_t cs, ds, es, fs, gs, ss;
    uint64_t cr0, cr2, cr3, cr4;
} registers_t;

// Forward declarations
static void dump_registers(const registers_t* regs);
static void print_backtrace(void);
static void halt_system(void);
static void fill_screen(uint32_t color);

/**
 * @brief Kernel panic - fatal error handler
 * 
 * @param file Source file where panic occurred
 * @param line Line number where panic occurred
 * @param fmt Format string for panic message
 * @param ... Additional arguments for format string
 */
void panic(const char* file, int line, const char* fmt, ...) {
    // Avoid recursive panics
    if (in_panic) {
        kprintf("\nRecursive panic detected!\n");
        halt_system();
    }
    in_panic = true;
    
    // Disable interrupts
    disable_interrupts();
    
    // Switch to console output mode
    kprintf_set_mode(PRINTF_MODE_BOTH);
    
    // If we have a framebuffer, draw blue screen
    if (fb_ready && framebuffer_base != NULL) {
        fill_screen(PANIC_BG_COLOR);
    }
    
    // Print panic header
    kprintf("\n\n");
    kprintf("********************************\n");
    kprintf("*** KERNEL PANIC - NOT SYNCING\n");
    kprintf("********************************\n\n");
    
    // Print file and line information
    kprintf("At %s:%d\n\n", file, line);
    
    // Print panic message
    va_list args;
    va_start(args, fmt);
    vkprintf(fmt, args);
    va_end(args);
    kprintf("\n\n");
    
    // Print backtrace
    print_backtrace();
    
    // Halt the system
    kprintf("\nSystem halted.\n");
    halt_system();
}

/**
 * @brief Assert a condition, panic if condition is false
 * 
 * @param condition Condition to assert
 * @param file Source file where assertion occurred
 * @param line Line number where assertion occurred
 * @param fmt Format string for assertion message
 * @param ... Additional arguments for format string
 */
void kassertf(bool condition, const char* file, int line, const char* fmt, ...) {
    if (condition) {
        return;  // Assertion passed
    }
    
    // Avoid recursive panics
    if (in_panic) {
        kprintf("\nRecursive panic detected during assertion!\n");
        halt_system();
    }
    in_panic = true;
    
    // Disable interrupts
    disable_interrupts();
    
    // Switch to console output mode
    kprintf_set_mode(PRINTF_MODE_BOTH);
    
    // If we have a framebuffer, draw blue screen
    if (fb_ready && framebuffer_base != NULL) {
        fill_screen(PANIC_BG_COLOR);
    }
    
    // Print assertion header
    kprintf("\n\n");
    kprintf("********************************\n");
    kprintf("*** ASSERTION FAILED\n");
    kprintf("********************************\n\n");
    
    // Print file and line information
    kprintf("At %s:%d\n\n", file, line);
    
    // Print assertion message
    va_list args;
    va_start(args, fmt);
    vkprintf(fmt, args);
    va_end(args);
    kprintf("\n\n");
    
    // Print backtrace
    print_backtrace();
    
    // Halt the system
    kprintf("\nSystem halted.\n");
    halt_system();
}

/**
 * @brief Hidden OS security breach handler
 * 
 * @param file Source file where breach was detected
 * @param line Line number where breach was detected
 * @param reason Reason for the breach
 * @param regs Register state at the time of breach
 */
void hos_breach(const char* file, int line, const char* reason, void* regs) {
    // Avoid recursive breaches
    if (in_panic) {
        kprintf("\nRecursive breach handler invoked!\n");
        halt_system();
    }
    in_panic = true;
    
    // Disable interrupts
    disable_interrupts();
    
    // Switch to console output mode
    kprintf_set_mode(PRINTF_MODE_BOTH);
    
    // If we have a framebuffer, draw red screen
    if (fb_ready && framebuffer_base != NULL) {
        fill_screen(BREACH_BG_COLOR);
    }
    
    // Print breach header
    kprintf("\n\n");
    kprintf("!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!\n");
    kprintf("!!! HIDDEN OS SECURITY BREACH DETECTED\n");
    kprintf("!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!\n\n");
    
    // Print file, line, and reason
    kprintf("At %s:%d\n", file, line);
    kprintf("Reason: %s\n\n", reason);
    
    // Dump registers if available
    if (regs != NULL) {
        dump_registers((registers_t*)regs);
    }
    
    // Print backtrace
    print_backtrace();
    
    // Halt the system
    kprintf("\nSystem halted for security reasons.\n");
    halt_system();
}

/**
 * @brief Print a backtrace of the call stack
 * 
 * Note: This is a simplified implementation that may not work in all cases
 */
static void print_backtrace(void) {
    kprintf("Call trace:\n");
    
    // Get current frame pointer
    void** ebp;
    asm volatile("mov %%rbp, %0" : "=r"(ebp) : :);
    
    // Walk the stack
    for (int frame = 0; frame < 10 && ebp != NULL; frame++) {
        void* return_address = ebp[1];
        if (return_address == NULL) {
            break;
        }
        
        kprintf(" [%d] %p\n", frame, return_address);
        
        // Move to next frame
        ebp = (void**)ebp[0];
        if (ebp == NULL || (uintptr_t)ebp < 0x1000 || (uintptr_t)ebp > 0xFFFFFFFFFFFFFF00) {
            // Invalid frame pointer
            break;
        }
    }
    
    kprintf("\n");
}

/**
 * @brief Dump register values
 * 
 * @param regs Register values to dump
 */
static void dump_registers(const registers_t* regs) {
    kprintf("Register dump:\n");
    kprintf(" RAX: %016llx  RBX: %016llx\n", regs->rax, regs->rbx);
    kprintf(" RCX: %016llx  RDX: %016llx\n", regs->rcx, regs->rdx);
    kprintf(" RSI: %016llx  RDI: %016llx\n", regs->rsi, regs->rdi);
    kprintf(" RBP: %016llx  RSP: %016llx\n", regs->rbp, regs->rsp);
    kprintf(" R8:  %016llx  R9:  %016llx\n", regs->r8, regs->r9);
    kprintf(" R10: %016llx  R11: %016llx\n", regs->r10, regs->r11);
    kprintf(" R12: %016llx  R13: %016llx\n", regs->r12, regs->r13);
    kprintf(" R14: %016llx  R15: %016llx\n", regs->r14, regs->r15);
    kprintf(" RIP: %016llx  RFLAGS: %016llx\n", regs->rip, regs->rflags);
    kprintf(" CS: %04llx  DS: %04llx  ES: %04llx\n", regs->cs, regs->ds, regs->es);
    kprintf(" FS: %04llx  GS: %04llx  SS: %04llx\n", regs->fs, regs->gs, regs->ss);
    kprintf(" CR0: %016llx  CR2: %016llx\n", regs->cr0, regs->cr2);
    kprintf(" CR3: %016llx  CR4: %016llx\n\n", regs->cr3, regs->cr4);
}

/**
 * @brief Fill the framebuffer with a solid color
 * 
 * @param color Color to fill the screen with (RGB)
 */
static void fill_screen(uint32_t color) {
    if (!fb_ready || framebuffer_base == NULL) {
        return;
    }
    
    uint32_t* fb = (uint32_t*)framebuffer_base;
    for (uint32_t i = 0; i < framebuffer_width * framebuffer_height; i++) {
        fb[i] = color;
    }
    
    // Reset terminal position
    terminal_row = 0;
    terminal_column = 0;
}

/**
 * @brief Halt the system
 * 
 * This function will never return
 */
static void halt_system(void) {
    while (1) {
        // Disable interrupts and halt CPU
        asm volatile("cli; hlt");
        
        // If execution continues (e.g., due to NMI), keep halting
    }
}
