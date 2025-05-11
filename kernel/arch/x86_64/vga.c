/**
 * @file vga.c
 * @brief VGA text mode console driver
 */

#include "../../include/kernel.h"
#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

// VGA text mode dimensions
#define VGA_WIDTH 80
#define VGA_HEIGHT 25

// VGA text mode memory
#define VGA_MEMORY 0xB8000

// VGA colors
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
    VGA_COLOR_WHITE = 15,
};

// VGA text mode state
static uint16_t* vga_buffer;
static uint8_t vga_color;
static int cursor_x;
static int cursor_y;
static bool cursor_enabled;

/**
 * @brief Create an 8-bit color attribute from foreground and background colors
 * 
 * @param fg Foreground color
 * @param bg Background color
 * @return 8-bit color attribute
 */
uint8_t vga_make_color(uint8_t fg, uint8_t bg) {
    return fg | (bg << 4);
}

/**
 * @brief Create a 16-bit VGA entry from character and color
 * 
 * @param c Character
 * @param color Color attribute
 * @return 16-bit VGA entry
 */
static uint16_t vga_make_entry(char c, uint8_t color) {
    uint16_t c16 = c;
    uint16_t color16 = color;
    return c16 | (color16 << 8);
}

/**
 * @brief Initialize the VGA text mode console
 */
void vga_init(void) {
    vga_buffer = (uint16_t*)VGA_MEMORY;
    vga_color = vga_make_color(VGA_COLOR_LIGHT_GREY, VGA_COLOR_BLACK);
    
    // Clear the screen
    vga_clear();
    
    // Enable the cursor
    vga_enable_cursor(true);
    
    // Set cursor position to top left
    vga_set_cursor_pos(0, 0);
}

/**
 * @brief Clear the screen with the current color
 */
void vga_clear(void) {
    // Fill the screen with spaces in the current color
    for (int y = 0; y < VGA_HEIGHT; y++) {
        for (int x = 0; x < VGA_WIDTH; x++) {
            vga_buffer[y * VGA_WIDTH + x] = vga_make_entry(' ', vga_color);
        }
    }
    
    // Reset cursor position
    cursor_x = 0;
    cursor_y = 0;
    
    // Update hardware cursor
    if (cursor_enabled) {
        vga_set_cursor_pos(cursor_x, cursor_y);
    }
}

/**
 * @brief Set the foreground and background colors
 * 
 * @param fg Foreground color
 * @param bg Background color
 */
void vga_set_color(uint8_t fg, uint8_t bg) {
    vga_color = vga_make_color(fg, bg);
}

/**
 * @brief Enable or disable the hardware cursor
 * 
 * @param enable true to enable, false to disable
 */
void vga_enable_cursor(bool enable) {
    cursor_enabled = enable;
    
    if (enable) {
        // Set cursor shape (small cursor)
        outb(0x3D4, 0x0A);
        outb(0x3D5, (inb(0x3D5) & 0xC0) | 14);
        outb(0x3D4, 0x0B);
        outb(0x3D5, (inb(0x3D5) & 0xE0) | 15);
    } else {
        // Disable cursor
        outb(0x3D4, 0x0A);
        outb(0x3D5, 0x20);
    }
}

/**
 * @brief Set the hardware cursor position
 * 
 * @param x X position (column)
 * @param y Y position (row)
 */
void vga_set_cursor_pos(int x, int y) {
    if (x < 0 || x >= VGA_WIDTH || y < 0 || y >= VGA_HEIGHT) {
        return;
    }
    
    cursor_x = x;
    cursor_y = y;
    
    // Update hardware cursor if enabled
    if (cursor_enabled) {
        uint16_t pos = y * VGA_WIDTH + x;
        outb(0x3D4, 0x0F);
        outb(0x3D5, (uint8_t)(pos & 0xFF));
        outb(0x3D4, 0x0E);
        outb(0x3D5, (uint8_t)((pos >> 8) & 0xFF));
    }
}

/**
 * @brief Scroll the screen up one line
 */
static void vga_scroll(void) {
    // Move all lines up one line
    for (int y = 0; y < VGA_HEIGHT - 1; y++) {
        for (int x = 0; x < VGA_WIDTH; x++) {
            vga_buffer[y * VGA_WIDTH + x] = vga_buffer[(y + 1) * VGA_WIDTH + x];
        }
    }
    
    // Clear the bottom line
    for (int x = 0; x < VGA_WIDTH; x++) {
        vga_buffer[(VGA_HEIGHT - 1) * VGA_WIDTH + x] = vga_make_entry(' ', vga_color);
    }
}

/**
 * @brief Print a character to the screen
 * 
 * @param c Character to print
 */
void vga_putchar(char c) {
    // Handle special characters
    switch (c) {
        case '\n':  // Newline
            cursor_x = 0;
            cursor_y++;
            break;
            
        case '\r':  // Carriage return
            cursor_x = 0;
            break;
            
        case '\b':  // Backspace
            if (cursor_x > 0) {
                cursor_x--;
            } else if (cursor_y > 0) {
                cursor_y--;
                cursor_x = VGA_WIDTH - 1;
            }
            // Clear the character
            vga_buffer[cursor_y * VGA_WIDTH + cursor_x] = vga_make_entry(' ', vga_color);
            break;
            
        case '\t':  // Tab
            // Tab to the next tab stop (every 8 characters)
            cursor_x = (cursor_x + 8) & ~7;
            break;
            
        default:  // Regular character
            // Put the character at the current position
            vga_buffer[cursor_y * VGA_WIDTH + cursor_x] = vga_make_entry(c, vga_color);
            cursor_x++;
            break;
    }
    
    // Handle wrapping
    if (cursor_x >= VGA_WIDTH) {
        cursor_x = 0;
        cursor_y++;
    }
    
    // Handle scrolling
    if (cursor_y >= VGA_HEIGHT) {
        vga_scroll();
        cursor_y = VGA_HEIGHT - 1;
    }
    
    // Update hardware cursor
    if (cursor_enabled) {
        vga_set_cursor_pos(cursor_x, cursor_y);
    }
}

/**
 * @brief Print a string to the screen
 * 
 * @param str String to print
 */
void vga_print(const char* str) {
    while (*str) {
        vga_putchar(*str++);
    }
}

/**
 * @brief Print a character at a specific position
 * 
 * @param c Character to print
 * @param x X position
 * @param y Y position
 * @param color Color attribute
 */
void vga_putchar_at(char c, int x, int y, uint8_t color) {
    if (x >= 0 && x < VGA_WIDTH && y >= 0 && y < VGA_HEIGHT) {
        vga_buffer[y * VGA_WIDTH + x] = vga_make_entry(c, color);
    }
}

/**
 * @brief Print a string at a specific position
 * 
 * @param str String to print
 * @param x X position
 * @param y Y position
 * @param color Color attribute
 */
void vga_print_at(const char* str, int x, int y, uint8_t color) {
    int pos_x = x;
    int pos_y = y;
    
    while (*str) {
        // Handle special characters
        if (*str == '\n') {
            pos_x = x;
            pos_y++;
        } else {
            vga_putchar_at(*str, pos_x, pos_y, color);
            pos_x++;
        }
        
        // Handle wrapping
        if (pos_x >= VGA_WIDTH) {
            pos_x = x;
            pos_y++;
        }
        
        // Handle scrolling (stop at the bottom)
        if (pos_y >= VGA_HEIGHT) {
            break;
        }
        
        str++;
    }
}
