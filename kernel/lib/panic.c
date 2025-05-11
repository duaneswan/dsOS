/**
 * @file panic.c
 * @brief Kernel panic and assertion handlers
 */

#include "../include/kernel.h"
#include <stdbool.h>
#include <stdarg.h>

// Whether we're currently in a panic state
static bool is_panicking = false;

/**
 * @brief Print register values during a panic or breach
 * 
 * @param regs Register state at the time of the panic
 */
static void print_registers(const registers_t* regs) {
    if (regs == NULL) {
        kprintf("No register state available\n");
        return;
    }
    
    kprintf("RAX: 0x%016lx  RBX: 0x%016lx  RCX: 0x%016lx\n", regs->rax, regs->rbx, regs->rcx);
    kprintf("RDX: 0x%016lx  RSI: 0x%016lx  RDI: 0x%016lx\n", regs->rdx, regs->rsi, regs->rdi);
    kprintf("RBP: 0x%016lx  RSP: 0x%016lx  R8:  0x%016lx\n", regs->rbp, regs->rsp, regs->r8);
    kprintf("R9:  0x%016lx  R10: 0x%016lx  R11: 0x%016lx\n", regs->r9, regs->r10, regs->r11);
    kprintf("R12: 0x%016lx  R13: 0x%016lx  R14: 0x%016lx\n", regs->r12, regs->r13, regs->r14);
    kprintf("R15: 0x%016lx  RIP: 0x%016lx  RFLAGS: 0x%016lx\n", regs->r15, regs->rip, regs->rflags);
    kprintf("CS:  0x%04x  DS: 0x%04x  SS: 0x%04x  ES: 0x%04x  FS: 0x%04x  GS: 0x%04x\n", 
            regs->cs, regs->ds, regs->ss, regs->es, regs->fs, regs->gs);
    
    // Print the instruction bytes at RIP if we can
    if (regs->rip != 0 && regs->rip < 0xFFFFFFFF80000000) {
        kprintf("Instruction bytes at RIP: ");
        uint8_t* code = (uint8_t*)regs->rip;
        for (int i = 0; i < 8; i++) {
            kprintf("%02x ", code[i]);
        }
        kprintf("\n");
    }
}

/**
 * @brief Print the call stack trace during panic
 * 
 * @param max_frames Maximum number of stack frames to print
 */
static void print_stacktrace(int max_frames) {
    // Get the current RBP (frame pointer)
    uint64_t* rbp;
    __asm__ volatile("mov %%rbp, %0" : "=r"(rbp));
    
    kprintf("Stack trace:\n");
    
    // Each stack frame contains:
    // [rbp] -> previous rbp
    // [rbp+8] -> return address
    for (int frame = 0; frame < max_frames; frame++) {
        // Check if rbp is valid (aligned and within a reasonable range)
        if (rbp == NULL || (uint64_t)rbp < 0x1000 || (uint64_t)rbp & 0x7) {
            break;
        }
        
        uint64_t rip = *(rbp + 1); // Return address
        
        // If the return address seems invalid, stop
        if (rip == 0 || rip < 0x1000) {
            break;
        }
        
        kprintf("  [%d] 0x%016lx\n", frame, rip);
        
        // Move to the previous frame
        rbp = (uint64_t*)(*rbp);
    }
}

/**
 * @brief Kernel panic handler - displays error and halts the system
 * 
 * @param file Source file where panic was called
 * @param line Line number where panic was called
 * @param regs Register state at the time of the panic (may be NULL)
 * @param fmt Format string for panic message
 * @param ... Additional arguments for format string
 */
__attribute__((noreturn)) void panic(const char* file, int line, registers_t* regs, const char* fmt, ...) {
    // Prevent recursive panics
    if (is_panicking) {
        kprintf("\nNested panic detected! Halting immediately.\n");
        halt();
    }
    
    is_panicking = true;
    
    // Disable interrupts
    disable_interrupts();
    
    // Clear the screen and set a black background with white text
    vga_clear_screen(VGA_COLOR_BLACK);
    vga_set_color(VGA_COLOR_WHITE, VGA_COLOR_BLACK);
    
    // Print the panic header
    kprintf("\n\n");
    kprintf("================================================================================\n");
    kprintf("                               KERNEL PANIC                                     \n");
    kprintf("================================================================================\n");
    
    // Print the panic reason
    va_list args;
    va_start(args, fmt);
    kprintf("Reason: ");
    vkprintf(fmt, args);
    va_end(args);
    kprintf("\n");
    
    // Print source location
    kprintf("File: %s, Line: %d\n", file, line);
    
    // Print the timestamp if available
    if (timer_get_ms != NULL) {
        kprintf("Uptime: %lu ms\n", timer_get_ms());
    }
    
    kprintf("--------------------------------------------------------------------------------\n");
    
    // Print register state if available
    print_registers(regs);
    
    // Print stack trace
    print_stacktrace(10);
    
    kprintf("--------------------------------------------------------------------------------\n");
    kprintf("System halted.\n");
    
    // Also output to debug serial port if available
    if (debug_port != NULL && serial_is_initialized(debug_port)) {
        va_list args;
        va_start(args, fmt);
        
        char buffer[1024];
        vsnprintf(buffer, sizeof(buffer), fmt, args);
        
        serial_printf(debug_port, "\n\nKERNEL PANIC: %s\n", buffer);
        serial_printf(debug_port, "File: %s, Line: %d\n", file, line);
        serial_printf(debug_port, "System halted.\n");
        
        va_end(args);
    }
    
    // Never return
    halt();
}

/**
 * @brief Assertion failure handler - calls panic() for failed assertions
 * 
 * @param file Source file where assertion failed
 * @param line Line number where assertion failed
 * @param expr String representation of the failed expression
 */
void assertion_failed(const char* file, int line, const char* expr) {
    panic(file, line, NULL, "Assertion failed: %s", expr);
}

/**
 * @brief Hidden OS breach handler - displays warning and halts system
 * 
 * @param type Type of breach (read, write, execute, disappear)
 * @param address Memory address where breach occurred
 * @param regs Register state at time of breach
 */
__attribute__((noreturn)) void hos_breach(const char* type, uint64_t address, registers_t* regs) {
    // Disable interrupts
    disable_interrupts();
    
    // Set screen to red with white text
    vga_clear_screen(VGA_COLOR_RED);
    vga_set_color(VGA_COLOR_WHITE, VGA_COLOR_RED);
    
    // Print the breach warning
    kprintf("\n\n");
    kprintf("================================================================================\n");
    kprintf("                            HIDDEN OS BREACH DETECTED                           \n");
    kprintf("================================================================================\n");
    
    kprintf("Breach type: %s\n", type);
    kprintf("Address: 0x%016lx\n", address);
    
    kprintf("--------------------------------------------------------------------------------\n");
    
    // Print register state
    print_registers(regs);
    
    // Print stack trace
    print_stacktrace(10);
    
    kprintf("--------------------------------------------------------------------------------\n");
    kprintf("System halted for security.\n");
    
    // Also output to debug serial port if available
    if (debug_port != NULL && serial_is_initialized(debug_port)) {
        serial_printf(debug_port, "\n\nHIDDEN OS BREACH DETECTED: %s at 0x%016lx\n", 
                     type, address);
        serial_printf(debug_port, "System halted for security.\n");
    }
    
    // Never return
    halt();
}
