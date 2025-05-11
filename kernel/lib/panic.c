/**
 * @file panic.c
 * @brief Kernel panic and critical error handling
 */

#include "../include/kernel.h"
#include "../include/memory.h"
#include <stdint.h>
#include <stdbool.h>

// Panic types color codes
#define COLOR_NORMAL         0x4F    // White on red
#define COLOR_HOS_BREACH     0x5F    // White on magenta
#define COLOR_HARDWARE_FAULT 0x1F    // White on blue

// Panic title strings
static const char* panic_titles[] = {
    "KERNEL PANIC",      // PANIC_NORMAL
    "HOS BREACH",        // PANIC_HOS_BREACH
    "HARDWARE FAULT"     // PANIC_HARDWARE_FAULT
};

// Original VGA color (for restoring)
static uint8_t original_vga_color = 0x07;

// External declarations 
extern void vga_set_color(enum vga_color fg, enum vga_color bg);
extern void vga_clear_screen(void);
extern void vga_set_cursor(int row, int col);
extern uint8_t vga_color;
extern void kprintf_set_mode(int mode);

/**
 * @brief Print register state during panic
 * 
 * @param interrupt_number Interrupt number if applicable
 */
static void print_register_state(uint64_t interrupt_number) {
    uint64_t rax, rbx, rcx, rdx, rsi, rdi, rbp, rsp, r8, r9, r10, r11, r12, r13, r14, r15, rip, rflags, cr0, cr2, cr3, cr4;
    
    // Get register values (using inline assembly)
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
        : "=r" (rax), "=r" (rbx), "=r" (rcx), "=r" (rdx),
          "=r" (rsi), "=r" (rdi), "=r" (rbp), "=r" (rsp),
          "=r" (r8), "=r" (r9), "=r" (r10), "=r" (r11),
          "=r" (r12), "=r" (r13), "=r" (r14), "=r" (r15)
        :
        : "memory"
    );
    
    // Get RIP (approximate - will be the address after this instruction)
    __asm__ volatile("lea (%%rip), %0" : "=r" (rip));
    
    // Get RFLAGS
    __asm__ volatile("pushfq; popq %0" : "=r" (rflags) :: "memory");
    
    // Get control registers
    __asm__ volatile("mov %%cr0, %0" : "=r" (cr0));
    __asm__ volatile("mov %%cr2, %0" : "=r" (cr2));
    __asm__ volatile("mov %%cr3, %0" : "=r" (cr3));
    __asm__ volatile("mov %%cr4, %0" : "=r" (cr4));
    
    // Print general purpose registers
    kprintf("RAX: 0x%016llx  RBX: 0x%016llx  RCX: 0x%016llx  RDX: 0x%016llx\n",
          (unsigned long long)rax, (unsigned long long)rbx,
          (unsigned long long)rcx, (unsigned long long)rdx);
    kprintf("RSI: 0x%016llx  RDI: 0x%016llx  RBP: 0x%016llx  RSP: 0x%016llx\n",
          (unsigned long long)rsi, (unsigned long long)rdi,
          (unsigned long long)rbp, (unsigned long long)rsp);
    kprintf("R8:  0x%016llx  R9:  0x%016llx  R10: 0x%016llx  R11: 0x%016llx\n",
          (unsigned long long)r8, (unsigned long long)r9,
          (unsigned long long)r10, (unsigned long long)r11);
    kprintf("R12: 0x%016llx  R13: 0x%016llx  R14: 0x%016llx  R15: 0x%016llx\n",
          (unsigned long long)r12, (unsigned long long)r13,
          (unsigned long long)r14, (unsigned long long)r15);
    
    // Print RIP, RFLAGS, and interrupt number
    kprintf("RIP: 0x%016llx  RFLAGS: 0x%016llx", (unsigned long long)rip, (unsigned long long)rflags);
    
    if (interrupt_number != (uint64_t)-1) {
        kprintf("  INT: 0x%02llx\n", (unsigned long long)interrupt_number);
    } else {
        kprintf("\n");
    }
    
    // Print control registers
    kprintf("CR0: 0x%016llx  CR2: 0x%016llx\n", (unsigned long long)cr0, (unsigned long long)cr2);
    kprintf("CR3: 0x%016llx  CR4: 0x%016llx\n", (unsigned long long)cr3, (unsigned long long)cr4);
}

/**
 * @brief Print stack trace during panic
 * 
 * @param rbp Base pointer from which to start unwinding
 * @param max_frames Maximum number of frames to print
 */
static void print_stack_trace(uint64_t rbp, int max_frames) {
    kprintf("\nStack trace:\n");
    
    // Frame structure on stack:
    // rbp+0: Previous RBP
    // rbp+8: Return address
    
    int frame = 0;
    uint64_t* frame_ptr = (uint64_t*)rbp;
    
    while (frame_ptr && frame < max_frames) {
        uint64_t return_addr = frame_ptr[1];
        
        // Check if the addresses look valid
        if (return_addr < KERNEL_VIRTUAL_BASE || frame_ptr[0] < KERNEL_VIRTUAL_BASE) {
            break;
        }
        
        // Print the frame
        kprintf("  [%d] 0x%016llx\n", frame, (unsigned long long)return_addr);
        
        // Move to previous frame
        frame_ptr = (uint64_t*)frame_ptr[0];
        frame++;
    }
}

/**
 * @brief Kernel panic handler
 * 
 * @param type Panic type (PANIC_NORMAL, PANIC_HOS_BREACH, or PANIC_HARDWARE_FAULT)
 * @param msg Error message
 * @param file Source file where panic was triggered
 * @param line Line number in source file
 */
void panic(int type, const char* msg, const char* file, int line) {
    // Save original VGA color
    original_vga_color = vga_color;
    
    // Set output mode to both serial and VGA
    kprintf_set_mode(0);
    
    // Set panic color based on type
    uint8_t color;
    switch (type) {
        case PANIC_HOS_BREACH:
            color = COLOR_HOS_BREACH;
            break;
        case PANIC_HARDWARE_FAULT:
            color = COLOR_HARDWARE_FAULT;
            break;
        case PANIC_NORMAL:
        default:
            color = COLOR_NORMAL;
            break;
    }
    
    // Disable interrupts
    cli();
    
    // Set VGA to panic color
    vga_set_color((enum vga_color)((color >> 4) & 0xF), (enum vga_color)(color & 0xF));
    vga_clear_screen();
    vga_set_cursor(0, 0);
    
    // Print panic header
    kprintf("****************************************************************************\n");
    kprintf("*                                                                          *\n");
    kprintf("*                              %s                              *\n", type < 3 ? panic_titles[type] : "KERNEL PANIC");
    kprintf("*                                                                          *\n");
    kprintf("****************************************************************************\n\n");
    
    // Print panic information
    kprintf("Error: %s\n", msg);
    kprintf("File:  %s\n", file);
    kprintf("Line:  %d\n\n", line);
    
    // Print register state
    kprintf("Register dump:\n");
    print_register_state(-1);
    
    // Get current RBP for stack trace
    uint64_t current_rbp;
    __asm__ volatile("mov %%rbp, %0" : "=r"(current_rbp));
    
    // Print stack trace
    print_stack_trace(current_rbp, 16);
    
    // Print final message
    kprintf("\nSystem halted.\n");
    
    // Halt the system
    while (1) {
        hlt();
    }
}

/**
 * @brief Handler for Hidden OS (hOS) breaches
 * 
 * @param reason Description of the breach
 * @param addr Address related to the breach
 */
void hos_breach(const char* reason, uintptr_t addr) {
    // Save original VGA color
    original_vga_color = vga_color;
    
    // Set output mode to both serial and VGA
    kprintf_set_mode(0);
    
    // Disable interrupts
    cli();
    
    // Set VGA to HOS breach color
    vga_set_color((enum vga_color)((COLOR_HOS_BREACH >> 4) & 0xF), (enum vga_color)(COLOR_HOS_BREACH & 0xF));
    vga_clear_screen();
    vga_set_cursor(0, 0);
    
    // Print HOS breach header
    kprintf("****************************************************************************\n");
    kprintf("*                                                                          *\n");
    kprintf("*                              HOS BREACH                                  *\n");
    kprintf("*                                                                          *\n");
    kprintf("****************************************************************************\n\n");
    
    // Print breach information
    kprintf("Hidden OS protection has been breached!\n\n");
    kprintf("Reason: %s\n", reason);
    kprintf("Address: 0x%016lx\n\n", addr);
    
    // Print register state
    kprintf("Register dump:\n");
    print_register_state(-1);
    
    // Get current RBP for stack trace
    uint64_t current_rbp;
    __asm__ volatile("mov %%rbp, %0" : "=r"(current_rbp));
    
    // Print stack trace
    print_stack_trace(current_rbp, 16);
    
    // Print final message
    kprintf("\nSystem halted.\n");
    
    // Halt the system
    while (1) {
        hlt();
    }
}

/**
 * @brief Dump memory around a specific address for debugging
 * 
 * @param addr Base address to dump
 * @param size Number of bytes to dump
 */
void dump_memory(void* addr, size_t size) {
    uint8_t* ptr = (uint8_t*)addr;
    uintptr_t base_addr = (uintptr_t)addr;
    
    kprintf("Memory dump at %p (%zu bytes):\n", addr, size);
    
    for (size_t i = 0; i < size; i += 16) {
        // Print address
        kprintf("%016lx: ", base_addr + i);
        
        // Print hex bytes
        for (size_t j = 0; j < 16; j++) {
            if (i + j < size) {
                kprintf("%02x ", ptr[i + j]);
            } else {
                kprintf("   ");
            }
            
            if (j == 7) {
                kprintf(" ");
            }
        }
        
        // Print ASCII representation
        kprintf(" |");
        for (size_t j = 0; j < 16; j++) {
            if (i + j < size) {
                char c = ptr[i + j];
                kprintf("%c", (c >= 32 && c <= 126) ? c : '.');
            } else {
                kprintf(" ");
            }
        }
        kprintf("|\n");
    }
}
