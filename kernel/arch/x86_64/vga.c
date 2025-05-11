/**
 * @file vga.c
 * @brief VGA text mode driver for kernel output
 */

#include "../../include/kernel.h"
#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

// VGA memory-mapped I/O location
#define VGA_BUFFER_ADDR 0xB8000
#define VGA_WIDTH 80
#define VGA_HEIGHT 25

// VGA I/O ports
#define VGA_CTRL_REG 0x3D4
#define VGA_DATA_REG 0x3D5
#define VGA_CURSOR_LOW 0x0F
#define VGA_CURSOR_HIGH 0x0E

// VGA state
static volatile uint16_t* vga_buffer = (volatile uint16_t*)VGA_BUFFER_ADDR;
static int vga_cursor_x = 0;
static int vga_cursor_y = 0;
uint8_t vga_color = 0x07; // Light gray on black by default

/**
 * @brief Create a VGA entry for a character with attributes
 * 
 * @param c Character to display
 * @param fg Foreground color
 * @param bg Background color
 * @return 16-bit VGA entry
 */
static inline uint16_t vga_entry(char c, enum vga_color fg, enum vga_color bg) {
    uint8_t color = (bg << 4) | fg;
    uint16_t ch = c & 0xFF;
    uint16_t attr = ((uint16_t)color) << 8;
    return ch | attr;
}

/**
 * @brief Set the VGA color attributes
 * 
 * @param fg Foreground color
 * @param bg Background color
 */
void vga_set_color(enum vga_color fg, enum vga_color bg) {
    vga_color = (bg << 4) | fg;
}

/**
 * @brief Update the hardware cursor position
 */
static void vga_update_cursor(void) {
    uint16_t pos = vga_cursor_y * VGA_WIDTH + vga_cursor_x;
    
    outb(VGA_CTRL_REG, VGA_CURSOR_HIGH);
    outb(VGA_DATA_REG, (pos >> 8) & 0xFF);
    outb(VGA_CTRL_REG, VGA_CURSOR_LOW);
    outb(VGA_DATA_REG, pos & 0xFF);
}

/**
 * @brief Set cursor position
 * 
 * @param row Row (0 to VGA_HEIGHT-1)
 * @param col Column (0 to VGA_WIDTH-1)
 */
void vga_set_cursor(int row, int col) {
    if (row >= 0 && row < VGA_HEIGHT && col >= 0 && col < VGA_WIDTH) {
        vga_cursor_x = col;
        vga_cursor_y = row;
        vga_update_cursor();
    }
}

/**
 * @brief Scroll the screen up by one line
 */
static void vga_scroll(void) {
    // Move everything up one line
    for (int y = 0; y < VGA_HEIGHT - 1; y++) {
        for (int x = 0; x < VGA_WIDTH; x++) {
            vga_buffer[y * VGA_WIDTH + x] = vga_buffer[(y + 1) * VGA_WIDTH + x];
        }
    }
    
    // Clear the bottom line
    for (int x = 0; x < VGA_WIDTH; x++) {
        vga_buffer[(VGA_HEIGHT - 1) * VGA_WIDTH + x] = vga_entry(' ', vga_color & 0x0F, vga_color >> 4);
    }
}

/**
 * @brief Output a single character to the screen
 * 
 * @param c Character to output
 */
void vga_putchar(char c) {
    // Handle special characters
    if (c == '\n') {
        vga_cursor_x = 0;
        vga_cursor_y++;
    } else if (c == '\r') {
        vga_cursor_x = 0;
    } else if (c == '\t') {
        // Tab aligns to 8-character boundaries
        vga_cursor_x = (vga_cursor_x + 8) & ~(8 - 1);
    } else if (c == '\b') {
        // Backspace moves cursor back one and erases the character
        if (vga_cursor_x > 0) {
            vga_cursor_x--;
            vga_buffer[vga_cursor_y * VGA_WIDTH + vga_cursor_x] = vga_entry(' ', vga_color & 0x0F, vga_color >> 4);
        }
    } else {
        // Regular character
        vga_buffer[vga_cursor_y * VGA_WIDTH + vga_cursor_x] = vga_entry(c, vga_color & 0x0F, vga_color >> 4);
        vga_cursor_x++;
    }
    
    // Handle line wrapping
    if (vga_cursor_x >= VGA_WIDTH) {
        vga_cursor_x = 0;
        vga_cursor_y++;
    }
    
    // Handle scrolling
    if (vga_cursor_y >= VGA_HEIGHT) {
        vga_scroll();
        vga_cursor_y = VGA_HEIGHT - 1;
    }
    
    // Update the hardware cursor
    vga_update_cursor();
}

/**
 * @brief Write a string to the screen
 * 
 * @param s String to output
 */
void vga_puts(const char* s) {
    while (*s) {
        vga_putchar(*s++);
    }
}

/**
 * @brief Clear the screen and reset the cursor
 */
void vga_clear_screen(void) {
    for (int y = 0; y < VGA_HEIGHT; y++) {
        for (int x = 0; x < VGA_WIDTH; x++) {
            vga_buffer[y * VGA_WIDTH + x] = vga_entry(' ', vga_color & 0x0F, vga_color >> 4);
        }
    }
    
    vga_cursor_x = 0;
    vga_cursor_y = 0;
    vga_update_cursor();
}

/**
 * @brief Draw a box on the screen
 * 
 * @param x1 Left coordinate (inclusive)
 * @param y1 Top coordinate (inclusive)
 * @param x2 Right coordinate (inclusive)
 * @param y2 Bottom coordinate (inclusive)
 * @param double_line Use double line characters instead of single line
 */
void vga_draw_box(int x1, int y1, int x2, int y2, bool double_line) {
    // Box drawing characters
    char corner_tl, corner_tr, corner_bl, corner_br, line_h, line_v;
    
    if (double_line) {
        corner_tl = 0xC9; // ╔
        corner_tr = 0xBB; // ╗
        corner_bl = 0xC8; // ╚
        corner_br = 0xBC; // ╝
        line_h = 0xCD;    // ═
        line_v = 0xBA;    // ║
    } else {
        corner_tl = 0xDA; // ┌
        corner_tr = 0xBF; // ┐
        corner_bl = 0xC0; // └
        corner_br = 0xD9; // ┘
        line_h = 0xC4;    // ─
        line_v = 0xB3;    // │
    }
    
    // Clamp to screen boundaries
    if (x1 < 0) x1 = 0;
    if (y1 < 0) y1 = 0;
    if (x2 >= VGA_WIDTH) x2 = VGA_WIDTH - 1;
    if (y2 >= VGA_HEIGHT) y2 = VGA_HEIGHT - 1;
    
    // Check if box is valid
    if (x1 > x2 || y1 > y2) return;
    
    // Save current cursor position
    int save_x = vga_cursor_x;
    int save_y = vga_cursor_y;
    
    // Draw top line
    vga_set_cursor(y1, x1);
    vga_putchar(corner_tl);
    for (int x = x1 + 1; x < x2; x++) {
        vga_putchar(line_h);
    }
    vga_putchar(corner_tr);
    
    // Draw sides
    for (int y = y1 + 1; y < y2; y++) {
        vga_set_cursor(y, x1);
        vga_putchar(line_v);
        vga_set_cursor(y, x2);
        vga_putchar(line_v);
    }
    
    // Draw bottom line
    vga_set_cursor(y2, x1);
    vga_putchar(corner_bl);
    for (int x = x1 + 1; x < x2; x++) {
        vga_putchar(line_h);
    }
    vga_putchar(corner_br);
    
    // Restore cursor position
    vga_set_cursor(save_y, save_x);
}

/**
 * @brief Fill a rectangular area with a character and attribute
 * 
 * @param x1 Left coordinate (inclusive)
 * @param y1 Top coordinate (inclusive)
 * @param x2 Right coordinate (inclusive)
 * @param y2 Bottom coordinate (inclusive)
 * @param c Character to fill with
 */
void vga_fill_area(int x1, int y1, int x2, int y2, char c) {
    // Clamp to screen boundaries
    if (x1 < 0) x1 = 0;
    if (y1 < 0) y1 = 0;
    if (x2 >= VGA_WIDTH) x2 = VGA_WIDTH - 1;
    if (y2 >= VGA_HEIGHT) y2 = VGA_HEIGHT - 1;
    
    // Check if area is valid
    if (x1 > x2 || y1 > y2) return;
    
    // Save current cursor position
    int save_x = vga_cursor_x;
    int save_y = vga_cursor_y;
    
    // Fill the area
    for (int y = y1; y <= y2; y++) {
        vga_set_cursor(y, x1);
        for (int x = x1; x <= x2; x++) {
            vga_putchar(c);
        }
    }
    
    // Restore cursor position
    vga_set_cursor(save_y, save_x);
}

/**
 * @brief Write text at a specific position
 * 
 * @param row Row (0 to VGA_HEIGHT-1)
 * @param col Column (0 to VGA_WIDTH-1)
 * @param text Text to write
 * @param max_len Maximum number of characters to write
 */
void vga_write_at(int row, int col, const char* text, int max_len) {
    // Clamp to screen boundaries
    if (row < 0 || row >= VGA_HEIGHT || col < 0 || col >= VGA_WIDTH) {
        return;
    }
    
    // Save current cursor position
    int save_x = vga_cursor_x;
    int save_y = vga_cursor_y;
    
    // Set cursor to the specified position
    vga_set_cursor(row, col);
    
    // Write the text
    int len = 0;
    while (*text && len < max_len) {
        vga_putchar(*text++);
        len++;
        
        // Break if we've reached the screen edge
        if (vga_cursor_x >= VGA_WIDTH) {
            break;
        }
    }
    
    // Restore cursor position
    vga_set_cursor(save_y, save_x);
}

/**
 * @brief Initialize the VGA driver
 */
void vga_init(void) {
    // Default color: light gray on black
    vga_color = (VGA_COLOR_BLACK << 4) | VGA_COLOR_LIGHT_GREY;
    
    // Clear screen
    vga_clear_screen();
    
    // Print init message
    vga_puts("dKernel VGA Driver initialized\n");
}

/**
 * @brief Draw progress bar
 * 
 * @param row Row position
 * @param col Column position
 * @param width Width of the progress bar
 * @param percent Progress percentage (0-100)
 * @param show_percent Whether to display percentage text
 */
void vga_progress_bar(int row, int col, int width, int percent, bool show_percent) {
    // Validate parameters
    if (row < 0 || row >= VGA_HEIGHT || col < 0 || col + width > VGA_WIDTH) {
        return;
    }
    
    if (percent < 0) percent = 0;
    if (percent > 100) percent = 100;
    
    // Save current cursor position and color
    int save_x = vga_cursor_x;
    int save_y = vga_cursor_y;
    uint8_t save_color = vga_color;
    
    // Calculate filled portion
    int filled = (width - 2) * percent / 100;
    
    // Draw progress bar border
    vga_set_cursor(row, col);
    vga_putchar('[');
    
    // Draw filled portion
    vga_set_color(VGA_COLOR_GREEN, VGA_COLOR_BLACK);
    for (int i = 0; i < filled; i++) {
        vga_putchar('=');
    }
    
    // Draw empty portion
    vga_set_color(VGA_COLOR_DARK_GREY, VGA_COLOR_BLACK);
    for (int i = filled; i < width - 2; i++) {
        vga_putchar(' ');
    }
    
    // Draw closing bracket
    vga_set_color(save_color & 0x0F, save_color >> 4);
    vga_putchar(']');
    
    // Show percentage if requested
    if (show_percent) {
        char percent_str[6];
        snprintf(percent_str, 6, " %3d%%", percent);
        vga_puts(percent_str);
    }
    
    // Restore cursor position and color
    vga_color = save_color;
    vga_set_cursor(save_y, save_x);
}
