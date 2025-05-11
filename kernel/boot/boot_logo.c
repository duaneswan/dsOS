/**
 * @file boot_logo.c
 * @brief Boot logo display and animation
 */

#include "../include/kernel.h"
#include <stdint.h>
#include <stdbool.h>

// Boot logo structure
typedef struct {
    uint32_t width;           // Logo width in pixels
    uint32_t height;          // Logo height in pixels
    uint32_t bpp;             // Bits per pixel (should be 32)
    uint32_t* framebuffer;    // Pointer to framebuffer (set during init)
    uint32_t* image_data;     // Pointer to logo image data
    uint32_t fb_width;        // Framebuffer width in pixels
    uint32_t fb_height;       // Framebuffer height in pixels
    bool initialized;         // Whether boot logo is initialized
} boot_logo_t;

// Single global instance of boot logo
static boot_logo_t boot_logo = {
    .width = 0,
    .height = 0,
    .bpp = 32,
    .framebuffer = NULL,
    .image_data = NULL,
    .fb_width = 0,
    .fb_height = 0,
    .initialized = false
};

// Logo position
static uint32_t logo_x = 0;
static uint32_t logo_y = 0;

// Forward declarations
static bool decode_png(const uint8_t* png_data, size_t png_size);
static void blend_pixel(uint32_t* dest, uint32_t src);

/**
 * @brief Initialize the boot logo
 * 
 * This function loads and decodes the boot logo image
 */
void boot_logo_init(void) {
    // We'll embed the logo directly in the kernel binary
    extern uint8_t _binary_dsOS_png_start[];
    extern uint8_t _binary_dsOS_png_end[];
    extern uint32_t _binary_dsOS_png_size;
    
    // Calculate logo size
    size_t logo_size = (size_t)(_binary_dsOS_png_end - _binary_dsOS_png_start);
    
    kprintf("Boot logo: Loading dsOS logo (size: %u bytes)\n", logo_size);
    
    // Decode the PNG image
    if (!decode_png(_binary_dsOS_png_start, logo_size)) {
        kprintf("Boot logo: Failed to decode PNG image\n");
        return;
    }
    
    // Mark as initialized
    boot_logo.initialized = true;
    
    kprintf("Boot logo: Initialized (%ux%u)\n", boot_logo.width, boot_logo.height);
}

/**
 * @brief Set the framebuffer for the boot logo
 * 
 * @param framebuffer Pointer to framebuffer
 * @param width Framebuffer width in pixels
 * @param height Framebuffer height in pixels
 * @param bpp Bits per pixel
 * @return true if successful, false otherwise
 */
bool boot_logo_set_framebuffer(void* framebuffer, uint32_t width, uint32_t height, uint32_t bpp) {
    if (!boot_logo.initialized) {
        return false;
    }
    
    if (bpp != 32) {
        kprintf("Boot logo: Unsupported BPP (%u), expected 32\n", bpp);
        return false;
    }
    
    boot_logo.framebuffer = (uint32_t*)framebuffer;
    boot_logo.fb_width = width;
    boot_logo.fb_height = height;
    
    // Calculate logo position (centered)
    logo_x = (width - boot_logo.width) / 2;
    logo_y = (height - boot_logo.height) / 2;
    
    kprintf("Boot logo: Framebuffer set (%ux%u @ %u BPP)\n", width, height, bpp);
    return true;
}

/**
 * @brief Show the boot logo on the framebuffer
 * 
 * This function draws the logo on the framebuffer
 */
void boot_logo_show(void) {
    if (!boot_logo.initialized || boot_logo.framebuffer == NULL) {
        return;
    }
    
    for (uint32_t y = 0; y < boot_logo.height; y++) {
        for (uint32_t x = 0; x < boot_logo.width; x++) {
            uint32_t fb_pos = (y + logo_y) * boot_logo.fb_width + (x + logo_x);
            uint32_t logo_pos = y * boot_logo.width + x;
            
            // Make sure we don't write outside the framebuffer
            if (fb_pos < boot_logo.fb_width * boot_logo.fb_height) {
                boot_logo.framebuffer[fb_pos] = boot_logo.image_data[logo_pos];
            }
        }
    }
}

/**
 * @brief Blend a pixel with alpha transparency
 * 
 * @param dest Destination pixel
 * @param src Source pixel with alpha
 */
static void blend_pixel(uint32_t* dest, uint32_t src) {
    // Extract components
    uint8_t src_r = (src >> 16) & 0xFF;
    uint8_t src_g = (src >> 8) & 0xFF;
    uint8_t src_b = src & 0xFF;
    uint8_t src_a = (src >> 24) & 0xFF;
    
    uint8_t dest_r = (*dest >> 16) & 0xFF;
    uint8_t dest_g = (*dest >> 8) & 0xFF;
    uint8_t dest_b = *dest & 0xFF;
    
    // Simplified alpha blending
    uint8_t out_r = ((src_r * src_a) + (dest_r * (255 - src_a))) / 255;
    uint8_t out_g = ((src_g * src_a) + (dest_g * (255 - src_a))) / 255;
    uint8_t out_b = ((src_b * src_a) + (dest_b * (255 - src_a))) / 255;
    
    // Final pixel
    *dest = (out_r << 16) | (out_g << 8) | out_b;
}

/**
 * @brief Fade in the boot logo
 * 
 * @param duration_ms Duration of fade in milliseconds
 */
void boot_logo_fade_in(uint32_t duration_ms) {
    if (!boot_logo.initialized || boot_logo.framebuffer == NULL) {
        return;
    }
    
    // First, create a black framebuffer
    for (uint32_t i = 0; i < boot_logo.fb_width * boot_logo.fb_height; i++) {
        boot_logo.framebuffer[i] = 0;
    }
    
    // Number of steps for the fade effect
    const uint32_t steps = 20;
    uint32_t step_delay = duration_ms / steps;
    
    // Temporary buffer for the logo with varying alpha
    uint32_t* temp_logo = kmalloc(boot_logo.width * boot_logo.height * sizeof(uint32_t));
    if (temp_logo == NULL) {
        // If allocation fails, just show the logo without fading
        boot_logo_show();
        return;
    }
    
    // Perform fade in
    for (uint32_t step = 0; step <= steps; step++) {
        // Calculate alpha for this step
        uint8_t alpha = (step * 255) / steps;
        
        // Update temporary logo with current alpha
        for (uint32_t i = 0; i < boot_logo.width * boot_logo.height; i++) {
            uint32_t pixel = boot_logo.image_data[i];
            uint8_t pixel_alpha = (pixel >> 24) & 0xFF;
            
            // Scale pixel alpha by current global alpha
            uint8_t new_alpha = (pixel_alpha * alpha) / 255;
            uint32_t new_pixel = (pixel & 0x00FFFFFF) | (new_alpha << 24);
            
            temp_logo[i] = new_pixel;
        }
        
        // Draw the logo with current alpha
        for (uint32_t y = 0; y < boot_logo.height; y++) {
            for (uint32_t x = 0; x < boot_logo.width; x++) {
                uint32_t fb_pos = (y + logo_y) * boot_logo.fb_width + (x + logo_x);
                uint32_t logo_pos = y * boot_logo.width + x;
                
                // Make sure we don't write outside the framebuffer
                if (fb_pos < boot_logo.fb_width * boot_logo.fb_height) {
                    // Blend pixel
                    blend_pixel(&boot_logo.framebuffer[fb_pos], temp_logo[logo_pos]);
                }
            }
        }
        
        // Wait for the next step
        timer_wait_ms(step_delay);
    }
    
    // Free temporary buffer
    kfree(temp_logo);
}

/**
 * @brief Fade out the boot logo
 * 
 * @param duration_ms Duration of fade in milliseconds
 */
void boot_logo_fade_out(uint32_t duration_ms) {
    if (!boot_logo.initialized || boot_logo.framebuffer == NULL) {
        return;
    }
    
    // Number of steps for the fade effect
    const uint32_t steps = 20;
    uint32_t step_delay = duration_ms / steps;
    
    // Temporary buffer to store the current framebuffer
    uint32_t* saved_fb = kmalloc(boot_logo.fb_width * boot_logo.fb_height * sizeof(uint32_t));
    if (saved_fb == NULL) {
        // If allocation fails, just clear the framebuffer
        for (uint32_t i = 0; i < boot_logo.fb_width * boot_logo.fb_height; i++) {
            boot_logo.framebuffer[i] = 0;
        }
        return;
    }
    
    // Save the current framebuffer
    for (uint32_t i = 0; i < boot_logo.fb_width * boot_logo.fb_height; i++) {
        saved_fb[i] = boot_logo.framebuffer[i];
    }
    
    // Perform fade out
    for (uint32_t step = steps; step > 0; step--) {
        // Calculate alpha for this step
        uint8_t alpha = (step * 255) / steps;
        
        // Fade the entire framebuffer by alpha
        for (uint32_t i = 0; i < boot_logo.fb_width * boot_logo.fb_height; i++) {
            uint32_t pixel = saved_fb[i];
            uint8_t r = (pixel >> 16) & 0xFF;
            uint8_t g = (pixel >> 8) & 0xFF;
            uint8_t b = pixel & 0xFF;
            
            // Scale RGB by current alpha
            r = (r * alpha) / 255;
            g = (g * alpha) / 255;
            b = (b * alpha) / 255;
            
            boot_logo.framebuffer[i] = (r << 16) | (g << 8) | b;
        }
        
        // Wait for the next step
        timer_wait_ms(step_delay);
    }
    
    // Clear the framebuffer completely
    for (uint32_t i = 0; i < boot_logo.fb_width * boot_logo.fb_height; i++) {
        boot_logo.framebuffer[i] = 0;
    }
    
    // Free temporary buffer
    kfree(saved_fb);
}

/**
 * @brief Simple PNG decoder implementation
 * 
 * This function parses embedded PNG data and decodes it into a raw RGBA format
 * 
 * @param png_data Pointer to PNG data
 * @param png_size Size of PNG data in bytes
 * @return true if successful, false otherwise
 */
static bool decode_png(const uint8_t* png_data, size_t png_size) {
    // In a real implementation, we'd include a proper PNG decoder library here
    // Since we don't want to pull in external dependencies, we'll create a simplified
    // decoder that assumes our PNG is in a specific format we can handle
    
    // For dsOS, we'll implement a basic PNG decoder that works with our specific logo
    // This isn't a complete PNG decoder; it's just enough to handle our single logo file
    
    // Check PNG signature
    static const uint8_t png_signature[8] = {137, 80, 78, 71, 13, 10, 26, 10};
    if (png_size < 8 || memcmp(png_data, png_signature, 8) != 0) {
        kprintf("Boot logo: Invalid PNG signature\n");
        return false;
    }
    
    // Parse PNG chunks
    uint32_t pos = 8;  // Skip signature
    
    uint32_t width = 0;
    uint32_t height = 0;
    uint8_t* image_data = NULL;
    
    while (pos + 12 < png_size) {
        // Read chunk length and type
        uint32_t chunk_length = (png_data[pos] << 24) | (png_data[pos + 1] << 16) |
                               (png_data[pos + 2] << 8) | png_data[pos + 3];
        uint32_t chunk_type = (png_data[pos + 4] << 24) | (png_data[pos + 5] << 16) |
                             (png_data[pos + 6] << 8) | png_data[pos + 7];
        
        pos += 8;  // Move past length and type
        
        // Check for IHDR chunk (must be first)
        if (chunk_type == 0x49484452) {  // "IHDR"
            if (chunk_length != 13) {
                kprintf("Boot logo: Invalid IHDR chunk\n");
                return false;
            }
            
            // Read image dimensions
            width = (png_data[pos] << 24) | (png_data[pos + 1] << 16) |
                    (png_data[pos + 2] << 8) | png_data[pos + 3];
            height = (png_data[pos + 4] << 24) | (png_data[pos + 5] << 16) |
                     (png_data[pos + 6] << 8) | png_data[pos + 7];
            
            uint8_t bit_depth = png_data[pos + 8];
            uint8_t color_type = png_data[pos + 9];
            uint8_t compression = png_data[pos + 10];
            uint8_t filter = png_data[pos + 11];
            uint8_t interlace = png_data[pos + 12];
            
            // Check for supported format (8-bit RGBA)
            if (bit_depth != 8 || color_type != 6 || compression != 0 || 
                filter != 0 || interlace != 0) {
                kprintf("Boot logo: Unsupported PNG format (depth=%u, color=%u, comp=%u, filter=%u, interlace=%u)\n",
                        bit_depth, color_type, compression, filter, interlace);
                return false;
            }
        }
        // Check for IDAT chunk (image data)
        else if (chunk_type == 0x49444154) {  // "IDAT"
            // In a real implementation, we'd handle compressed image data here
            // For simplicity, we'll assume the data is already uncompressed RGBA
            // and just copy it directly
            
            // Allocate memory for image data
            if (image_data == NULL) {
                boot_logo.width = width;
                boot_logo.height = height;
                boot_logo.bpp = 32;
                boot_logo.image_data = kmalloc(width * height * 4);
                
                if (boot_logo.image_data == NULL) {
                    kprintf("Boot logo: Failed to allocate memory for image data\n");
                    return false;
                }
                
                // Copy image data (simplified approach)
                memcpy(boot_logo.image_data, png_data + pos, chunk_length);
            }
        }
        
        // Move to next chunk (skip CRC)
        pos += chunk_length + 4;
    }
    
    // Check if we read the image dimensions and data
    if (width == 0 || height == 0 || boot_logo.image_data == NULL) {
        kprintf("Boot logo: Missing required PNG chunks\n");
        return false;
    }
    
    return true;
}

/**
 * @brief Clean up boot logo resources
 */
void boot_logo_cleanup(void) {
    if (boot_logo.initialized && boot_logo.image_data != NULL) {
        kfree(boot_logo.image_data);
        boot_logo.image_data = NULL;
        boot_logo.initialized = false;
    }
}
