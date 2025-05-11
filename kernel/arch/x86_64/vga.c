/**
 * @file vga.c
 * @brief VGA driver implementation for text and graphics modes
 */

#include "../../include/kernel.h"
#include <stdint.h>
#include <stdbool.h>

// VGA text mode defines
#define VGA_WIDTH 80
#define VGA_HEIGHT 25
#define VGA_SIZE (VGA_WIDTH * VGA_HEIGHT)
#define VGA_TEXT_BUFFER 0xB8000

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

// VGA registers
#define VGA_AC_INDEX      0x3C0
#define VGA_AC_WRITE      0x3C0
#define VGA_AC_READ       0x3C1
#define VGA_MISC_WRITE    0x3C2
#define VGA_SEQ_INDEX     0x3C4
#define VGA_SEQ_DATA      0x3C5
#define VGA_DAC_INDEX_READ  0x3C7
#define VGA_DAC_INDEX_WRITE 0x3C8
#define VGA_DAC_DATA      0x3C9
#define VGA_MISC_READ     0x3CC
#define VGA_GC_INDEX      0x3CE
#define VGA_GC_DATA       0x3CF
#define VGA_CRTC_INDEX    0x3D4
#define VGA_CRTC_DATA     0x3D5
#define VGA_INSTAT_READ   0x3DA

#define VGA_NUM_SEQ_REGS  5
#define VGA_NUM_CRTC_REGS 25
#define VGA_NUM_GC_REGS   9
#define VGA_NUM_AC_REGS   21

// VGA framebuffer state
static uint16_t* vga_buffer = (uint16_t*)VGA_TEXT_BUFFER;
static uint8_t vga_color = 0x07; // Light gray on black
static int vga_row = 0;
static int vga_col = 0;

// Framebuffer for graphics mode
static uint8_t* framebuffer = NULL;
static uint32_t fb_width = 0;
static uint32_t fb_height = 0;
static uint32_t fb_pitch = 0;
static uint8_t fb_bpp = 0;  // Bits per pixel

// Mode flags
static bool is_graphics_mode = false;

/**
 * @brief Create a VGA color attribute from foreground and background colors
 * 
 * @param fg Foreground color
 * @param bg Background color
 * @return Combined color attribute
 */
static inline uint8_t vga_entry_color(enum vga_color fg, enum vga_color bg) {
    return fg | (bg << 4);
}

/**
 * @brief Create a VGA character entry from a character and color attribute
 * 
 * @param c Character
 * @param color Color attribute
 * @return VGA character entry
 */
static inline uint16_t vga_entry(unsigned char c, uint8_t color) {
    return (uint16_t)c | ((uint16_t)color << 8);
}

/**
 * @brief Wait for VGA to be ready for register changes
 */
static void vga_wait(void) {
    // Wait for VSync to ensure registers can be modified safely
    // Read from the Input Status Register 1
    // Bit 3 is the Vertical Retrace bit
    inb(VGA_INSTAT_READ);
}

/**
 * @brief Clear the VGA text screen
 */
void vga_clear_screen(void) {
    if (is_graphics_mode) {
        // In graphics mode, clear the framebuffer
        if (framebuffer) {
            memset(framebuffer, 0, fb_height * fb_pitch);
        }
    } else {
        // In text mode, fill the buffer with spaces
        for (size_t i = 0; i < VGA_SIZE; i++) {
            vga_buffer[i] = vga_entry(' ', vga_color);
        }
    }
    
    // Reset cursor position
    vga_row = 0;
    vga_col = 0;
}

/**
 * @brief Set the VGA text cursor position
 * 
 * @param row Row (0-24)
 * @param col Column (0-79)
 */
void vga_set_cursor(int row, int col) {
    if (row >= VGA_HEIGHT || col >= VGA_WIDTH || row < 0 || col < 0) {
        return;
    }
    
    vga_row = row;
    vga_col = col;
    
    // Update the hardware cursor position
    unsigned short pos = row * VGA_WIDTH + col;
    
    // Low cursor byte
    outb(VGA_CRTC_INDEX, 0x0F);
    outb(VGA_CRTC_DATA, (unsigned char)(pos & 0xFF));
    
    // High cursor byte
    outb(VGA_CRTC_INDEX, 0x0E);
    outb(VGA_CRTC_DATA, (unsigned char)((pos >> 8) & 0xFF));
}

/**
 * @brief Set the VGA text color
 * 
 * @param fg Foreground color
 * @param bg Background color
 */
void vga_set_color(enum vga_color fg, enum vga_color bg) {
    vga_color = vga_entry_color(fg, bg);
}

/**
 * @brief Scroll the VGA text screen up by one line
 */
static void vga_scroll(void) {
    // Move each line up one row
    for (int y = 0; y < VGA_HEIGHT - 1; y++) {
        for (int x = 0; x < VGA_WIDTH; x++) {
            vga_buffer[y * VGA_WIDTH + x] = vga_buffer[(y + 1) * VGA_WIDTH + x];
        }
    }
    
    // Clear the bottom row
    for (int x = 0; x < VGA_WIDTH; x++) {
        vga_buffer[(VGA_HEIGHT - 1) * VGA_WIDTH + x] = vga_entry(' ', vga_color);
    }
}

/**
 * @brief Put a character to the VGA text screen
 * 
 * @param c Character to display
 */
void vga_putchar(char c) {
    // Handle special characters
    if (c == '\n') {
        vga_col = 0;
        vga_row++;
        if (vga_row >= VGA_HEIGHT) {
            vga_scroll();
            vga_row = VGA_HEIGHT - 1;
        }
        return;
    }
    
    if (c == '\r') {
        vga_col = 0;
        return;
    }
    
    if (c == '\b') {
        if (vga_col > 0) {
            vga_col--;
            vga_buffer[vga_row * VGA_WIDTH + vga_col] = vga_entry(' ', vga_color);
        }
        return;
    }
    
    // Write the character to the screen
    vga_buffer[vga_row * VGA_WIDTH + vga_col] = vga_entry(c, vga_color);
    vga_col++;
    
    // Handle line wrapping
    if (vga_col >= VGA_WIDTH) {
        vga_col = 0;
        vga_row++;
        if (vga_row >= VGA_HEIGHT) {
            vga_scroll();
            vga_row = VGA_HEIGHT - 1;
        }
    }
    
    // Update cursor position
    vga_set_cursor(vga_row, vga_col);
}

/**
 * @brief Write a string to the VGA text screen
 * 
 * @param str String to write
 */
void vga_puts(const char* str) {
    for (size_t i = 0; str[i] != '\0'; i++) {
        vga_putchar(str[i]);
    }
}

/**
 * @brief Initialize VGA text mode
 */
static void vga_init_text_mode(void) {
    // Ensure we're in text mode
    is_graphics_mode = false;
    
    // Initialize text buffer
    vga_buffer = (uint16_t*)VGA_TEXT_BUFFER;
    
    // Set default color
    vga_set_color(VGA_COLOR_LIGHT_GREY, VGA_COLOR_BLACK);
    
    // Clear the screen
    vga_clear_screen();
    
    // Enable cursor
    outb(VGA_CRTC_INDEX, 0x0A);
    outb(VGA_CRTC_DATA, (inb(VGA_CRTC_DATA) & 0xC0) | 0);
    outb(VGA_CRTC_INDEX, 0x0B);
    outb(VGA_CRTC_DATA, (inb(VGA_CRTC_DATA) & 0xE0) | 15);
    
    // Set cursor position
    vga_set_cursor(0, 0);
    
    kprintf("VGA: Initialized text mode %dx%d\n", VGA_WIDTH, VGA_HEIGHT);
}

/**
 * @brief Set a pixel in graphics mode
 * 
 * @param x X coordinate
 * @param y Y coordinate
 * @param color Color value (format depends on bpp)
 */
void vga_set_pixel(unsigned int x, unsigned int y, uint32_t color) {
    if (!is_graphics_mode || !framebuffer || x >= fb_width || y >= fb_height) {
        return;
    }
    
    unsigned int pixel_offset = y * fb_pitch + x * (fb_bpp / 8);
    
    // Set the pixel color based on bits per pixel
    switch (fb_bpp) {
        case 8:
            framebuffer[pixel_offset] = (uint8_t)color;
            break;
            
        case 16: {
            uint16_t* pixel = (uint16_t*)(framebuffer + pixel_offset);
            *pixel = (uint16_t)color;
            break;
        }
        
        case 24:
            framebuffer[pixel_offset + 0] = (uint8_t)(color & 0xFF);         // Blue
            framebuffer[pixel_offset + 1] = (uint8_t)((color >> 8) & 0xFF);  // Green
            framebuffer[pixel_offset + 2] = (uint8_t)((color >> 16) & 0xFF); // Red
            break;
            
        case 32: {
            uint32_t* pixel = (uint32_t*)(framebuffer + pixel_offset);
            *pixel = color;
            break;
        }
    }
}

/**
 * @brief Draw a horizontal line in graphics mode
 * 
 * @param x1 Start X coordinate
 * @param y Y coordinate
 * @param x2 End X coordinate
 * @param color Color value
 */
void vga_draw_hline(unsigned int x1, unsigned int y, unsigned int x2, uint32_t color) {
    if (x1 > x2) {
        unsigned int temp = x1;
        x1 = x2;
        x2 = temp;
    }
    
    for (unsigned int x = x1; x <= x2; x++) {
        vga_set_pixel(x, y, color);
    }
}

/**
 * @brief Draw a vertical line in graphics mode
 * 
 * @param x X coordinate
 * @param y1 Start Y coordinate
 * @param y2 End Y coordinate
 * @param color Color value
 */
void vga_draw_vline(unsigned int x, unsigned int y1, unsigned int y2, uint32_t color) {
    if (y1 > y2) {
        unsigned int temp = y1;
        y1 = y2;
        y2 = temp;
    }
    
    for (unsigned int y = y1; y <= y2; y++) {
        vga_set_pixel(x, y, color);
    }
}

/**
 * @brief Draw a rectangle in graphics mode
 * 
 * @param x X coordinate of top-left corner
 * @param y Y coordinate of top-left corner
 * @param width Width of rectangle
 * @param height Height of rectangle
 * @param color Color value
 */
void vga_draw_rect(unsigned int x, unsigned int y, unsigned int width, unsigned int height, uint32_t color) {
    for (unsigned int j = 0; j < height; j++) {
        vga_draw_hline(x, y + j, x + width - 1, color);
    }
}

/**
 * @brief Set up the framebuffer for graphics mode
 * 
 * @param address The physical address of the framebuffer
 * @param width Width in pixels
 * @param height Height in pixels
 * @param pitch Bytes per line (may include padding)
 * @param bpp Bits per pixel (8, 16, 24, or 32)
 */
void vga_setup_framebuffer(uint8_t* address, uint32_t width, uint32_t height, uint32_t pitch, uint8_t bpp) {
    framebuffer = address;
    fb_width = width;
    fb_height = height;
    fb_pitch = pitch;
    fb_bpp = bpp;
    
    is_graphics_mode = true;
    
    // Clear the framebuffer
    if (framebuffer) {
        memset(framebuffer, 0, fb_height * fb_pitch);
    }
    
    kprintf("VGA: Framebuffer set up at %p, %dx%d, %d bpp\n", 
            framebuffer, fb_width, fb_height, fb_bpp);
    
    // Signal that the framebuffer is ready
    extern bool fb_ready;
    fb_ready = true;
}

/**
 * @brief Initialize the VGA driver
 */
void vga_init(void) {
    // Start in text mode
    vga_init_text_mode();
}
