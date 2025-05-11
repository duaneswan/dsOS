/**
 * @file boot_logo.c
 * @brief Boot logo display and animation
 */

#include "../include/kernel.h"
#include <stdint.h>
#include <stdbool.h>

// Boot logo dimensions and properties
#define LOGO_MAX_WIDTH     640
#define LOGO_MAX_HEIGHT    480
#define PNG_HEADER_SIZE    8
#define PNG_SIGNATURE      "\x89PNG\r\n\x1a\n"

// Logo state variables
static uint8_t* logo_data = NULL;
static uint32_t logo_width = 0;
static uint32_t logo_height = 0;
static uint32_t* framebuffer = NULL;
static uint32_t fb_width = 0;
static uint32_t fb_height = 0;
static uint32_t fb_pitch = 0;
static bool logo_loaded = false;

// PNG chunk types
#define PNG_CHUNK_IHDR     0x49484452  // "IHDR"
#define PNG_CHUNK_IDAT     0x49444154  // "IDAT"
#define PNG_CHUNK_IEND     0x49454E44  // "IEND"

// PNG color types
#define PNG_COLOR_GRAYSCALE       0
#define PNG_COLOR_RGB             2
#define PNG_COLOR_PALETTE         3
#define PNG_COLOR_GRAYSCALE_ALPHA 4
#define PNG_COLOR_RGBA            6

// PNG structure definitions
typedef struct {
    uint32_t width;
    uint32_t height;
    uint8_t bit_depth;
    uint8_t color_type;
    uint8_t compression;
    uint8_t filter;
    uint8_t interlace;
} png_ihdr_t;

typedef struct {
    uint32_t length;
    uint32_t type;
    uint8_t* data;
    uint32_t crc;
} png_chunk_t;

/**
 * @brief Swap endianness of a 32-bit value
 */
static uint32_t swap32(uint32_t value) {
    return ((value & 0xFF) << 24) | 
           ((value & 0xFF00) << 8) | 
           ((value & 0xFF0000) >> 8) | 
           ((value & 0xFF000000) >> 24);
}

/**
 * @brief Read a 32-bit value from a buffer with big-endian conversion
 */
static uint32_t read_uint32(const uint8_t* buffer) {
    return (buffer[0] << 24) | (buffer[1] << 16) | (buffer[2] << 8) | buffer[3];
}

/**
 * @brief Simple PNG decoder (only handles raw RGBA for simplicity)
 * 
 * This is a very simplified decoder that assumes:
 * - The PNG is in RGBA format (color_type 6)
 * - No compression (we're embedding the raw data for boot logo)
 * - No interlacing
 * 
 * For a real OS, this would be replaced with a proper PNG decoder.
 */
static bool decode_png(const uint8_t* data, uint32_t size) {
    // Check PNG signature
    if (size < PNG_HEADER_SIZE || memcmp(data, PNG_SIGNATURE, PNG_HEADER_SIZE) != 0) {
        kprintf("BOOT_LOGO: Invalid PNG signature\n");
        return false;
    }
    
    // Parse PNG chunks
    uint32_t offset = PNG_HEADER_SIZE;
    png_ihdr_t ihdr;
    bool has_ihdr = false;
    
    while (offset + 12 <= size) {
        uint32_t chunk_length = read_uint32(&data[offset]);
        uint32_t chunk_type = read_uint32(&data[offset + 4]);
        
        // Ensure we don't read beyond buffer
        if (offset + 12 + chunk_length > size) {
            kprintf("BOOT_LOGO: Truncated PNG chunk\n");
            return false;
        }
        
        // Process chunk based on type
        switch (chunk_type) {
            case PNG_CHUNK_IHDR:
                if (chunk_length != 13) {
                    kprintf("BOOT_LOGO: Invalid IHDR chunk size\n");
                    return false;
                }
                
                // Parse IHDR data
                ihdr.width = read_uint32(&data[offset + 8]);
                ihdr.height = read_uint32(&data[offset + 12]);
                ihdr.bit_depth = data[offset + 16];
                ihdr.color_type = data[offset + 17];
                ihdr.compression = data[offset + 18];
                ihdr.filter = data[offset + 19];
                ihdr.interlace = data[offset + 20];
                
                has_ihdr = true;
                
                // Check restrictions for our simple decoder
                if (ihdr.color_type != PNG_COLOR_RGBA || ihdr.bit_depth != 8 ||
                    ihdr.compression != 0 || ihdr.filter != 0 || ihdr.interlace != 0) {
                    kprintf("BOOT_LOGO: Unsupported PNG format\n");
                    return false;
                }
                
                // Check dimensions
                if (ihdr.width > LOGO_MAX_WIDTH || ihdr.height > LOGO_MAX_HEIGHT) {
                    kprintf("BOOT_LOGO: PNG dimensions too large\n");
                    return false;
                }
                
                // Allocate memory for logo data
                logo_data = kmalloc(ihdr.width * ihdr.height * 4);
                if (logo_data == NULL) {
                    kprintf("BOOT_LOGO: Failed to allocate memory\n");
                    return false;
                }
                
                logo_width = ihdr.width;
                logo_height = ihdr.height;
                break;
                
            case PNG_CHUNK_IDAT:
                if (!has_ihdr || logo_data == NULL) {
                    kprintf("BOOT_LOGO: IDAT chunk before IHDR\n");
                    return false;
                }
                
                // For our simple boot logo, we're assuming raw RGBA data
                // Copy the raw pixel data (simplified for boot logo only)
                memcpy(logo_data, &data[offset + 8], chunk_length);
                break;
                
            case PNG_CHUNK_IEND:
                // End of PNG file
                if (!has_ihdr || logo_data == NULL) {
                    kprintf("BOOT_LOGO: Invalid PNG structure\n");
                    return false;
                }
                
                return true;
                
            default:
                // Skip unknown chunks
                break;
        }
        
        // Move to next chunk
        offset += 12 + chunk_length;
    }
    
    kprintf("BOOT_LOGO: No IEND chunk found\n");
    return false;
}

/**
 * @brief Center the logo on screen
 */
static void center_logo(uint32_t* dest, uint32_t* src) {
    uint32_t start_x = (fb_width - logo_width) / 2;
    uint32_t start_y = (fb_height - logo_height) / 2;
    
    for (uint32_t y = 0; y < logo_height; y++) {
        for (uint32_t x = 0; x < logo_width; x++) {
            uint32_t fb_pos = (start_y + y) * (fb_pitch / 4) + start_x + x;
            uint32_t logo_pos = y * logo_width + x;
            
            if (fb_pos < (fb_height * (fb_pitch / 4)) && logo_pos < (logo_width * logo_height)) {
                dest[fb_pos] = src[logo_pos];
            }
        }
    }
}

/**
 * @brief Initialize boot logo system
 */
void boot_logo_init(void) {
    // Get framebuffer information from video driver
    framebuffer = (uint32_t*)gfx_get_framebuffer();
    
    if (framebuffer == NULL) {
        kprintf("BOOT_LOGO: No framebuffer available\n");
        return;
    }
    
    // Get framebuffer dimensions
    fb_width = 800;   // Assuming default resolution, would be set by gfx_get_width()
    fb_height = 600;  // Assuming default resolution, would be set by gfx_get_height()
    fb_pitch = fb_width * 4;  // Bytes per scanline, would be set by gfx_get_pitch()
    
    // Load the embedded logo
    // In a real implementation, this would load from a file
    // For this example, we'll use a placeholder
    extern uint8_t _binary_dsos_png_start[];
    extern uint8_t _binary_dsos_png_end[];
    extern uint32_t _binary_dsos_png_size;
    
    uint8_t* logo_file = _binary_dsos_png_start;
    uint32_t logo_size = (uint32_t)(_binary_dsos_png_end - _binary_dsos_png_start);
    
    if (decode_png(logo_file, logo_size)) {
        logo_loaded = true;
        kprintf("BOOT_LOGO: Loaded %dx%d logo\n", logo_width, logo_height);
    } else {
        kprintf("BOOT_LOGO: Failed to load logo\n");
    }
}

/**
 * @brief Display the boot logo
 */
void boot_logo_show(void) {
    if (!logo_loaded || framebuffer == NULL) {
        return;
    }
    
    // Copy logo to framebuffer
    center_logo(framebuffer, (uint32_t*)logo_data);
    
    // Swap buffers to display
    gfx_swap_buffers();
}

/**
 * @brief Apply alpha blending between two colors
 */
static uint32_t blend_color(uint32_t c1, uint32_t c2, uint8_t alpha) {
    uint8_t r1 = (c1 >> 16) & 0xFF;
    uint8_t g1 = (c1 >> 8) & 0xFF;
    uint8_t b1 = c1 & 0xFF;
    
    uint8_t r2 = (c2 >> 16) & 0xFF;
    uint8_t g2 = (c2 >> 8) & 0xFF;
    uint8_t b2 = c2 & 0xFF;
    
    uint8_t r = (r1 * (255 - alpha) + r2 * alpha) / 255;
    uint8_t g = (g1 * (255 - alpha) + g2 * alpha) / 255;
    uint8_t b = (b1 * (255 - alpha) + b2 * alpha) / 255;
    
    return (r << 16) | (g << 8) | b;
}

/**
 * @brief Fade in the boot logo
 * 
 * @param duration_ms Duration of fade effect in milliseconds
 */
void boot_logo_fade_in(uint32_t duration_ms) {
    if (!logo_loaded || framebuffer == NULL) {
        return;
    }
    
    uint32_t* temp_buffer = kmalloc(fb_height * fb_pitch);
    if (temp_buffer == NULL) {
        kprintf("BOOT_LOGO: Failed to allocate temporary buffer for fade effect\n");
        return;
    }
    
    // Clear temp buffer to black
    memset(temp_buffer, 0, fb_height * fb_pitch);
    
    // Copy logo to temp buffer
    center_logo(temp_buffer, (uint32_t*)logo_data);
    
    // Calculate frames and timing
    uint32_t frames = duration_ms / 16;  // ~60 FPS
    if (frames < 10) frames = 10;        // Minimum 10 frames for smooth fade
    
    // Perform fade-in
    for (uint32_t i = 0; i <= frames; i++) {
        uint8_t alpha = (i * 255) / frames;
        
        // Apply alpha blending
        for (uint32_t y = 0; y < fb_height; y++) {
            for (uint32_t x = 0; x < fb_width; x++) {
                uint32_t pos = y * (fb_pitch / 4) + x;
                framebuffer[pos] = blend_color(0, temp_buffer[pos], alpha);
            }
        }
        
        // Update display
        gfx_swap_buffers();
        
        // Small delay between frames
        // In a real OS, this would use a timer or scheduler
        for (volatile int j = 0; j < 100000; j++) {}
    }
    
    kfree(temp_buffer);
}

/**
 * @brief Fade out the boot logo
 * 
 * @param duration_ms Duration of fade effect in milliseconds
 */
void boot_logo_fade_out(uint32_t duration_ms) {
    if (!logo_loaded || framebuffer == NULL) {
        return;
    }
    
    // Save current framebuffer content
    uint32_t* temp_buffer = kmalloc(fb_height * fb_pitch);
    if (temp_buffer == NULL) {
        kprintf("BOOT_LOGO: Failed to allocate temporary buffer for fade effect\n");
        return;
    }
    
    // Copy current framebuffer to temp buffer
    memcpy(temp_buffer, framebuffer, fb_height * fb_pitch);
    
    // Calculate frames and timing
    uint32_t frames = duration_ms / 16;  // ~60 FPS
    if (frames < 10) frames = 10;        // Minimum 10 frames for smooth fade
    
    // Perform fade-out
    for (uint32_t i = 0; i <= frames; i++) {
        uint8_t alpha = 255 - (i * 255) / frames;
        
        // Apply alpha blending
        for (uint32_t y = 0; y < fb_height; y++) {
            for (uint32_t x = 0; x < fb_width; x++) {
                uint32_t pos = y * (fb_pitch / 4) + x;
                framebuffer[pos] = blend_color(0, temp_buffer[pos], alpha);
            }
        }
        
        // Update display
        gfx_swap_buffers();
        
        // Small delay between frames
        // In a real OS, this would use a timer or scheduler
        for (volatile int j = 0; j < 100000; j++) {}
    }
    
    // Clear screen to black after fade-out
    memset(framebuffer, 0, fb_height * fb_pitch);
    gfx_swap_buffers();
    
    kfree(temp_buffer);
}

/**
 * @brief Clean up boot logo resources
 */
void boot_logo_cleanup(void) {
    if (logo_data != NULL) {
        kfree(logo_data);
        logo_data = NULL;
    }
    
    logo_loaded = false;
}
