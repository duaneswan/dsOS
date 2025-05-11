/**
 * @file vga.c
 * @brief VGA text mode driver
 */

#include "../../include/kernel.h"
#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

// VGA text mode buffer address
#define VGA_TEXT_BUFFER 0xB8000

// VGA dimensions
#define VGA_WIDTH  80
#define VGA_HEIGHT 25

// VGA controller ports
#define VGA_CTRL_REGISTER   0x3D4
#define VGA_DATA_REGISTER   0x3D5
#define VGA_CURSOR_HIGH     0x0E
#define VGA_CURSOR_LOW      0x0F

// Current state
static uint16_t* vga_buffer = (uint16_t*)VGA_TEXT_BUFFER;
static uint8_t vga_color = 0;
static uint8_t vga_cursor_x = 0;
static uint8_t vga_cursor_y = 0;
static bool vga_cursor_enabled = true;

/**
 * @brief Create a VGA color attribute
 * 
 * @param fg Foreground color (0-15)
 * @param bg Background color (0-7)
 * @return color attribute
 */
uint8_t vga_make_color(uint8_t fg, uint8_t bg) {
    return (bg << 4) | (fg & 0x0F);
}

/**
 * @brief Create a VGA character with color
 * 
 * @param c Character
 * @param color Color attribute
 * @return VGA character entry
 */
static uint16_t vga_make_char(char c, uint8_t color) {
    return (uint16_t)c | ((uint16_t)color << 8);
}

/**
 * @brief Set the hardware cursor position
 * 
 * @param x X position (0-79)
 * @param y Y position (0-24)
 */
void vga_set_cursor_pos(int x, int y) {
    // Bounds checking
    if (x < 0) x = 0;
    if (x >= VGA_WIDTH) x = VGA_WIDTH - 1;
    if (y < 0) y = 0;
    if (y >= VGA_HEIGHT) y = VGA_HEIGHT - 1;
    
    // Save current position
    vga_cursor_x = x;
    vga_cursor_y = y;
    
    // Only update hardware cursor if enabled
    if (vga_cursor_enabled) {
        uint16_t pos = y * VGA_WIDTH + x;
        
        outb(VGA_CTRL_REGISTER, VGA_CURSOR_HIGH);
        outb(VGA_DATA_REGISTER, (pos >> 8) & 0xFF);
        outb(VGA_CTRL_REGISTER, VGA_CURSOR_LOW);
        outb(VGA_DATA_REGISTER, pos & 0xFF);
    }
}

/**
 * @brief Enable or disable the hardware cursor
 * 
 * @param enable True to enable, false to disable
 */
void vga_enable_cursor(bool enable) {
    vga_cursor_enabled = enable;
    
    if (enable) {
        // Enable cursor with size (start and end scan lines)
        // Typical values: start=14, end=15 (underline cursor)
        outb(VGA_CTRL_REGISTER, 0x0A);
        outb(VGA_DATA_REGISTER, (inb(VGA_DATA_REGISTER) & 0xC0) | 14);
        outb(VGA_CTRL_REGISTER, 0x0B);
        outb(VGA_DATA_REGISTER, (inb(VGA_DATA_REGISTER) & 0xE0) | 15);
        
        // Update cursor position
        vga_set_cursor_pos(vga_cursor_x, vga_cursor_y);
    } else {
        // Disable cursor (set bit 5 of cursor start register)
        outb(VGA_CTRL_REGISTER, 0x0A);
        outb(VGA_DATA_REGISTER, 0x20);
    }
}

/**
 * @brief Get cursor position
 * 
 * @param x Pointer to store X position
 * @param y Pointer to store Y position
 */
void vga_get_cursor_pos(int* x, int* y) {
    if (x) *x = vga_cursor_x;
    if (y) *y = vga_cursor_y;
}

/**
 * @brief Set current text color
 * 
 * @param fg Foreground color (0-15)
 * @param bg Background color (0-7)
 */
void vga_set_color(uint8_t fg, uint8_t bg) {
    vga_color = vga_make_color(fg, bg);
}

/**
 * @brief Clear the screen with the specified color
 * 
 * @param color Background color for cleared screen
 */
void vga_clear_screen(uint8_t color) {
    uint16_t blank = vga_make_char(' ', vga_make_color(VGA_COLOR_WHITE, color));
    
    for (int y = 0; y < VGA_HEIGHT; y++) {
        for (int x = 0; x < VGA_WIDTH; x++) {
            vga_buffer[y * VGA_WIDTH + x] = blank;
        }
    }
    
    vga_set_cursor_pos(0, 0);
}

/**
 * @brief Scroll the screen up by one line
 */
static void vga_scroll(void) {
    // Get a blank character with current color
    uint16_t blank = vga_make_char(' ', vga_color);
    
    // Move all lines up by one
    for (int y = 0; y < VGA_HEIGHT - 1; y++) {
        for (int x = 0; x < VGA_WIDTH; x++) {
            vga_buffer[y * VGA_WIDTH + x] = vga_buffer[(y + 1) * VGA_WIDTH + x];
        }
    }
    
    // Clear the last line
    for (int x = 0; x < VGA_WIDTH; x++) {
        vga_buffer[(VGA_HEIGHT - 1) * VGA_WIDTH + x] = blank;
    }
}

/**
 * @brief Put a character at the specified position
 * 
 * @param c Character to put
 * @param color Color attribute
 * @param x X position
 * @param y Y position
 */
static void vga_putchar_at(char c, uint8_t color, int x, int y) {
    // Bounds checking
    if (x < 0 || x >= VGA_WIDTH || y < 0 || y >= VGA_HEIGHT) {
        return;
    }
    
    // Put the character
    vga_buffer[y * VGA_WIDTH + x] = vga_make_char(c, color);
}

/**
 * @brief Handle a new line
 */
static void vga_newline(void) {
    vga_cursor_x = 0;
    vga_cursor_y++;
    
    // Scroll if at bottom of screen
    if (vga_cursor_y >= VGA_HEIGHT) {
        vga_scroll();
        vga_cursor_y = VGA_HEIGHT - 1;
    }
}

/**
 * @brief Put a character at the current cursor position
 * 
 * @param c Character to put
 */
void vga_putchar(char c) {
    // Handle special characters
    switch (c) {
        case '\n':  // Newline
            vga_newline();
            break;
            
        case '\r':  // Carriage return
            vga_cursor_x = 0;
            break;
            
        case '\b':  // Backspace
            if (vga_cursor_x > 0) {
                vga_cursor_x--;
                vga_putchar_at(' ', vga_color, vga_cursor_x, vga_cursor_y);
            } else if (vga_cursor_y > 0) {
                vga_cursor_y--;
                vga_cursor_x = VGA_WIDTH - 1;
                vga_putchar_at(' ', vga_color, vga_cursor_x, vga_cursor_y);
            }
            break;
            
        case '\t':  // Tab
            // Move to next tab stop (every 8 columns)
            vga_cursor_x = (vga_cursor_x + 8) & ~7;
            if (vga_cursor_x >= VGA_WIDTH) {
                vga_newline();
            }
            break;
            
        default:  // Normal character
            vga_putchar_at(c, vga_color, vga_cursor_x, vga_cursor_y);
            vga_cursor_x++;
            
            // Wrap if reached end of line
            if (vga_cursor_x >= VGA_WIDTH) {
                vga_newline();
            }
            break;
    }
    
    // Update cursor position
    vga_set_cursor_pos(vga_cursor_x, vga_cursor_y);
}

/**
 * @brief Put a string at the current cursor position
 * 
 * @param str String to put
 */
void vga_print(const char* str) {
    while (*str) {
        vga_putchar(*str++);
    }
}

/**
 * @brief Initialize the VGA driver
 */
void vga_init(void) {
    // Set default colors (white on black)
    vga_set_color(VGA_COLOR_WHITE, VGA_COLOR_BLACK);
    
    // Clear the screen
    vga_clear_screen(VGA_COLOR_BLACK);
    
    // Enable cursor
    vga_enable_cursor(true);
    
    kprintf("VGA: Initialized text mode %dx%d\n", VGA_WIDTH, VGA_HEIGHT);
}

/**
 * @brief Put a formatted string to VGA (similar to printf)
 * 
 * @param fmt Format string
 * @param ... Arguments
 * @return Number of characters written
 */
int vga_printf(const char* fmt, ...) {
    char buffer[1024];
    va_list args;
    
    va_start(args, fmt);
    int ret = vsnprintf(buffer, sizeof(buffer), fmt, args);
    va_end(args);
    
    vga_print(buffer);
    return ret;
}

/**
 * @brief Terminal implementation for kernel printf
 * 
 * @param c Character to output
 */
void terminal_putchar(char c) {
    vga_putchar(c);
}
