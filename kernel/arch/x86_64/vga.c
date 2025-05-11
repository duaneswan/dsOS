/**
 * @file vga.c
 * @brief VGA text mode driver
 */

#include "../../include/kernel.h"
#include <stdint.h>
#include <stdbool.h>

// VGA text mode buffer address
#define VGA_BUFFER          0xB8000
#define VGA_WIDTH           80
#define VGA_HEIGHT          25

// VGA colors
#define VGA_COLOR_BLACK     0
#define VGA_COLOR_BLUE      1
#define VGA_COLOR_GREEN     2
#define VGA_COLOR_CYAN      3
#define VGA_COLOR_RED       4
#define VGA_COLOR_MAGENTA   5
#define VGA_COLOR_BROWN     6
#define VGA_COLOR_LGRAY     7
#define VGA_COLOR_DGRAY     8
#define VGA_COLOR_LBLUE     9
#define VGA_COLOR_LGREEN    10
#define VGA_COLOR_LCYAN     11
#define VGA_COLOR_LRED      12
#define VGA_COLOR_LMAGENTA  13
#define VGA_COLOR_YELLOW    14
#define VGA_COLOR_WHITE     15

// Cursor control ports
#define VGA_CTRL_REGISTER   0x3D4
#define VGA_DATA_REGISTER   0x3D5
#define VGA_CURSOR_HIGH     14
#define VGA_CURSOR_LOW      15

// VGA state variables
static uint16_t* vga_buffer = (uint16_t*)VGA_BUFFER;
static uint8_t vga_color;
static uint8_t cursor_x = 0;
static uint8_t cursor_y = 0;
static bool cursor_enabled = true;

/**
 * @brief Make a VGA color attribute from foreground and background colors
 * 
 * @param fg Foreground color (0-15)
 * @param bg Background color (0-15)
 * @return VGA color attribute
 */
uint8_t vga_make_color(uint8_t fg, uint8_t bg) {
    return (bg << 4) | (fg & 0x0F);
}

/**
 * @brief Make a VGA entry from character and color attribute
 * 
 * @param c Character
 * @param color Color attribute
 * @return VGA entry
 */
static uint16_t vga_make_entry(char c, uint8_t color) {
    return (uint16_t)c | ((uint16_t)color << 8);
}

/**
 * @brief Update the hardware cursor position
 */
static void vga_update_cursor(void) {
    if (!cursor_enabled) {
        return;
    }
    
    uint16_t position = cursor_y * VGA_WIDTH + cursor_x;
    
    outb(VGA_CTRL_REGISTER, VGA_CURSOR_LOW);
    outb(VGA_DATA_REGISTER, position & 0xFF);
    outb(VGA_CTRL_REGISTER, VGA_CURSOR_HIGH);
    outb(VGA_DATA_REGISTER, (position >> 8) & 0xFF);
}

/**
 * @brief Set the cursor position
 * 
 * @param x X coordinate (column)
 * @param y Y coordinate (row)
 */
void vga_set_cursor_pos(int x, int y) {
    // Validate position
    if (x < 0) x = 0;
    if (x >= VGA_WIDTH) x = VGA_WIDTH - 1;
    if (y < 0) y = 0;
    if (y >= VGA_HEIGHT) y = VGA_HEIGHT - 1;
    
    cursor_x = x;
    cursor_y = y;
    vga_update_cursor();
}

/**
 * @brief Enable or disable the cursor
 * 
 * @param enable True to enable, false to disable
 */
void vga_enable_cursor(bool enable) {
    cursor_enabled = enable;
    
    if (enable) {
        // Set cursor shape (start and end scanlines)
        outb(VGA_CTRL_REGISTER, 0x0A);
        outb(VGA_DATA_REGISTER, (inb(VGA_DATA_REGISTER) & 0xC0) | 0);  // Start scanline
        
        outb(VGA_CTRL_REGISTER, 0x0B);
        outb(VGA_DATA_REGISTER, (inb(VGA_DATA_REGISTER) & 0xE0) | 15); // End scanline
        
        // Update cursor position
        vga_update_cursor();
    } else {
        // Disable cursor (set start scanline beyond end scanline)
        outb(VGA_CTRL_REGISTER, 0x0A);
        outb(VGA_DATA_REGISTER, 0x20);
    }
}

/**
 * @brief Scroll the screen if needed
 */
static void vga_scroll(void) {
    if (cursor_y >= VGA_HEIGHT) {
        // Move everything up one line
        for (int y = 0; y < VGA_HEIGHT - 1; y++) {
            for (int x = 0; x < VGA_WIDTH; x++) {
                vga_buffer[y * VGA_WIDTH + x] = vga_buffer[(y + 1) * VGA_WIDTH + x];
            }
        }
        
        // Clear the last line
        for (int x = 0; x < VGA_WIDTH; x++) {
            vga_buffer[(VGA_HEIGHT - 1) * VGA_WIDTH + x] = vga_make_entry(' ', vga_color);
        }
        
        cursor_y = VGA_HEIGHT - 1;
    }
}

/**
 * @brief Set text mode color
 * 
 * @param fg Foreground color (0-15)
 * @param bg Background color (0-15)
 */
void vga_set_color(uint8_t fg, uint8_t bg) {
    vga_color = vga_make_color(fg, bg);
}

/**
 * @brief Clear the screen
 */
void vga_clear(void) {
    // Fill the entire VGA buffer with spaces using the current color
    for (int y = 0; y < VGA_HEIGHT; y++) {
        for (int x = 0; x < VGA_WIDTH; x++) {
            vga_buffer[y * VGA_WIDTH + x] = vga_make_entry(' ', vga_color);
        }
    }
    
    // Reset cursor position
    cursor_x = 0;
    cursor_y = 0;
    vga_update_cursor();
}

/**
 * @brief Initialize the VGA driver
 */
void vga_init(void) {
    // Set default color (light gray on black)
    vga_set_color(VGA_COLOR_LGRAY, VGA_COLOR_BLACK);
    
    // Clear the screen
    vga_clear();
    
    // Enable the cursor
    vga_enable_cursor(true);
    
    kprintf("VGA: Initialized text mode %dx%d\n", VGA_WIDTH, VGA_HEIGHT);
}

/**
 * @brief Put a character at a specific position with specific color
 * 
 * @param c Character to put
 * @param x X coordinate (column)
 * @param y Y coordinate (row)
 * @param color Color attribute
 */
void vga_putchar_at(char c, int x, int y, uint8_t color) {
    // Check if position is valid
    if (x >= 0 && x < VGA_WIDTH && y >= 0 && y < VGA_HEIGHT) {
        vga_buffer[y * VGA_WIDTH + x] = vga_make_entry(c, color);
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
            cursor_x = 0;
            cursor_y++;
            break;
            
        case '\r':  // Carriage return
            cursor_x = 0;
            break;
            
        case '\t':  // Tab (8 spaces)
            cursor_x = (cursor_x + 8) & ~7;
            break;
            
        case '\b':  // Backspace
            if (cursor_x > 0) {
                cursor_x--;
                vga_putchar_at(' ', cursor_x, cursor_y, vga_color);
            }
            break;
            
        default:  // Regular character
            vga_putchar_at(c, cursor_x, cursor_y, vga_color);
            cursor_x++;
            break;
    }
    
    // Handle line wrapping
    if (cursor_x >= VGA_WIDTH) {
        cursor_x = 0;
        cursor_y++;
    }
    
    // Handle scrolling
    vga_scroll();
    
    // Update cursor position
    vga_update_cursor();
}

/**
 * @brief Print a string at the current cursor position
 * 
 * @param str String to print
 */
void vga_print(const char* str) {
    while (*str) {
        vga_putchar(*str++);
    }
}

/**
 * @brief Print a string at a specific position with specific color
 * 
 * @param str String to print
 * @param x X coordinate (column)
 * @param y Y coordinate (row)
 * @param color Color attribute
 */
void vga_print_at(const char* str, int x, int y, uint8_t color) {
    int original_x = cursor_x;
    int original_y = cursor_y;
    uint8_t original_color = vga_color;
    
    cursor_x = x;
    cursor_y = y;
    vga_color = color;
    
    while (*str && cursor_x < VGA_WIDTH) {
        if (*str == '\n') {
            // Handle newline specially in direct positioning mode
            break;
        }
        vga_putchar_at(*str++, cursor_x++, cursor_y, color);
    }
    
    cursor_x = original_x;
    cursor_y = original_y;
    vga_color = original_color;
    vga_update_cursor();
}

/**
 * @brief Get the character at a specific position
 * 
 * @param x X coordinate (column)
 * @param y Y coordinate (row)
 * @return Character at position (0 if invalid position)
 */
char vga_getchar_at(int x, int y) {
    // Check if position is valid
    if (x >= 0 && x < VGA_WIDTH && y >= 0 && y < VGA_HEIGHT) {
        return vga_buffer[y * VGA_WIDTH + x] & 0xFF;
    }
    return 0;
}

/**
 * @brief Get the color attribute at a specific position
 * 
 * @param x X coordinate (column)
 * @param y Y coordinate (row)
 * @return Color attribute at position (0 if invalid position)
 */
uint8_t vga_getcolor_at(int x, int y) {
    // Check if position is valid
    if (x >= 0 && x < VGA_WIDTH && y >= 0 && y < VGA_HEIGHT) {
        return (vga_buffer[y * VGA_WIDTH + x] >> 8) & 0xFF;
    }
    return 0;
}
