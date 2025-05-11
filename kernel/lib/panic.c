/**
 * @file panic.c
 * @brief Kernel panic implementation
 */

#include "../include/kernel.h"
#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>

// Panic types
#define PANIC_NORMAL       0    // Normal panic (gray screen)
#define PANIC_CRITICAL     1    // Critical panic (red screen)
#define PANIC_HOS_BREACH   2    // Hidden OS breach (special red screen)

// VGA colors
#define VGA_COLOR_BLACK        0x0
#define VGA_COLOR_BLUE         0x1
#define VGA_COLOR_GREEN        0x2
#define VGA_COLOR_CYAN         0x3
#define VGA_COLOR_RED          0x4
#define VGA_COLOR_MAGENTA      0x5
#define VGA_COLOR_BROWN        0x6
#define VGA_COLOR_LIGHT_GREY   0x7
#define VGA_COLOR_DARK_GREY    0x8
#define VGA_COLOR_LIGHT_BLUE   0x9
#define VGA_COLOR_LIGHT_GREEN  0xA
#define VGA_COLOR_LIGHT_CYAN   0xB
#define VGA_COLOR_LIGHT_RED    0xC
#define VGA_COLOR_LIGHT_MAGENTA 0xD
#define VGA_COLOR_LIGHT_BROWN  0xE
#define VGA_COLOR_WHITE        0xF

// Forward declarations
static void fill_screen(uint8_t color);
static void draw_centered_text(const char* text, int y, uint8_t color);
static void draw_text(const char* text, int x, int y, uint8_t color);

/**
 * @brief Kernel panic handler
 * 
 * @param type Panic type (PANIC_NORMAL, PANIC_CRITICAL, PANIC_HOS_BREACH)
 * @param message Panic message
 * @param file Source file where panic occurred
 * @param line Line number where panic occurred
 */
void panic(int type, const char* message, const char* file, int line) {
    // Disable interrupts
    cli();
    
    // Set VGA output only
    kprintf_set_mode(2);
    
    // Prepare screen based on panic type
    switch (type) {
        case PANIC_CRITICAL:
            fill_screen(VGA_COLOR_RED);
            draw_centered_text("CRITICAL KERNEL ERROR", 5, VGA_COLOR_WHITE);
            break;
            
        case PANIC_HOS_BREACH:
            fill_screen(VGA_COLOR_RED);
            draw_centered_text("HIDDEN OS SECURITY BREACH", 5, VGA_COLOR_WHITE);
            break;
            
        case PANIC_NORMAL:
        default:
            fill_screen(VGA_COLOR_LIGHT_GREY);
            draw_centered_text("KERNEL PANIC", 5, VGA_COLOR_BLACK);
            break;
    }
    
    // Display panic information
    char buf[80];
    
    draw_centered_text(message, 8, (type == PANIC_NORMAL) ? VGA_COLOR_BLACK : VGA_COLOR_WHITE);
    
    snprintf(buf, sizeof(buf), "Source: %s:%d", file, line);
    draw_centered_text(buf, 10, (type == PANIC_NORMAL) ? VGA_COLOR_BLACK : VGA_COLOR_WHITE);
    
    // Output to serial as well
    kprintf_set_mode(1);
    kprintf("\n\n*** KERNEL PANIC ***\n");
    kprintf("Message: %s\n", message);
    kprintf("Source: %s:%d\n", file, line);
    
    // Halt the system
    halt();
}

/**
 * @brief Assert function implementation
 * 
 * @param condition Condition to check
 * @param message Message if condition fails
 * @param file Source file
 * @param line Line number
 */
void kassert_func(bool condition, const char* message, const char* file, int line) {
    if (!condition) {
        panic(PANIC_NORMAL, message, file, line);
    }
}

/**
 * @brief Hidden OS breach handler
 * 
 * @param breach_type Type of breach
 * @param address Address that was violated
 * @param expected Expected value
 * @param actual Actual value
 */
void hos_breach(int breach_type, uintptr_t address, uint64_t expected, uint64_t actual) {
    char message[80];
    
    switch (breach_type) {
        case 1:
            snprintf(message, sizeof(message), "Read violation at 0x%lx", address);
            break;
        case 2:
            snprintf(message, sizeof(message), "Write violation at 0x%lx", address);
            break;
        case 3:
            snprintf(message, sizeof(message), "Exec violation at 0x%lx", address);
            break;
        case 4:
            snprintf(message, sizeof(message), "Hash mismatch at 0x%lx", address);
            break;
        case 5:
            snprintf(message, sizeof(message), "Missing block at 0x%lx", address);
            break;
        default:
            snprintf(message, sizeof(message), "Unknown violation at 0x%lx", address);
            break;
    }
    
    panic(PANIC_HOS_BREACH, message, __FILE__, __LINE__);
}

/**
 * @brief Halt the system
 */
void halt(void) {
    // Output message to serial port
    kprintf("System halted.\n");
    
    // Disable interrupts and enter infinite loop
    cli();
    while (1) {
        // Wait for the world to end
        __asm__ volatile("hlt");
    }
}

/**
 * @brief Fill the screen with a color
 * 
 * @param color Color to fill with
 */
static void fill_screen(uint8_t color) {
    // Assuming vga_clear(color) exists
    // For now, we'll just do it directly
    uint16_t* vga_buffer = (uint16_t*)0xB8000;
    uint16_t value = (color << 8) | ' '; // Space character with color
    
    for (int i = 0; i < 25 * 80; i++) {
        vga_buffer[i] = value;
    }
}

/**
 * @brief Draw centered text on screen
 * 
 * @param text Text to draw
 * @param y Y position
 * @param color Text color
 */
static void draw_centered_text(const char* text, int y, uint8_t color) {
    size_t len = strlen(text);
    int x = (80 - len) / 2;
    if (x < 0) x = 0;
    
    draw_text(text, x, y, color);
}

/**
 * @brief Draw text at a specific position
 * 
 * @param text Text to draw
 * @param x X position
 * @param y Y position
 * @param color Text color
 */
static void draw_text(const char* text, int x, int y, uint8_t color) {
    uint16_t* vga_buffer = (uint16_t*)0xB8000;
    size_t len = strlen(text);
    
    // Ensure coordinates are within bounds
    if (x < 0) x = 0;
    if (x >= 80) x = 79;
    if (y < 0) y = 0;
    if (y >= 25) y = 24;
    
    // Calculate starting position
    uint16_t* pos = vga_buffer + (y * 80 + x);
    
    // Draw text
    for (size_t i = 0; i < len && (x + i) < 80; i++) {
        pos[i] = (color << 8) | text[i];
    }
}
