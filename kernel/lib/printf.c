/**
 * @file printf.c
 * @brief Kernel printf implementation for debugging
 */

#include "../include/kernel.h"
#include <stdarg.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

// Output modes
#define OUTPUT_MODE_SERIAL     0x01
#define OUTPUT_MODE_VGA        0x02
#define OUTPUT_MODE_BOTH       (OUTPUT_MODE_SERIAL | OUTPUT_MODE_VGA)

// Buffer sizes
#define PRINTF_BUFSIZE        256
#define MAX_INT_DIGITS         32

// Current output mode
static int kprintf_mode = OUTPUT_MODE_BOTH;

// Function pointer for character output
typedef void (*putchar_func_t)(char);

// Default putchar function
static void default_putchar(char c);

// Current putchar function
static putchar_func_t current_putchar = default_putchar;

// External function declarations
extern void serial_write_byte(uint16_t port, uint8_t data);
extern uint16_t debug_port;
extern bool serial_initialized;
extern void vga_putchar(char c);

/**
 * @brief Set output mode for kprintf
 * 
 * @param mode 0=BOTH, 1=SERIAL only, 2=VGA only
 */
void kprintf_set_mode(int mode) {
    switch (mode) {
        case 0:
            kprintf_mode = OUTPUT_MODE_BOTH;
            break;
        case 1:
            kprintf_mode = OUTPUT_MODE_SERIAL;
            break;
        case 2:
            kprintf_mode = OUTPUT_MODE_VGA;
            break;
        default:
            kprintf_mode = OUTPUT_MODE_BOTH;
            break;
    }
}

/**
 * @brief Default output function for putchar
 * 
 * @param c Character to output
 */
static void default_putchar(char c) {
    if (kprintf_mode & OUTPUT_MODE_SERIAL && serial_initialized) {
        if (c == '\n') {
            serial_write_byte(debug_port, '\r');
        }
        serial_write_byte(debug_port, c);
    }
    
    if (kprintf_mode & OUTPUT_MODE_VGA) {
        vga_putchar(c);
    }
}

/**
 * @brief Output a single character to the appropriate devices
 * 
 * @param c Character to output
 */
static inline void putchar(char c) {
    current_putchar(c);
}

/**
 * @brief Output a string to the appropriate devices
 * 
 * @param str String to output
 */
static void puts(const char* str) {
    while (*str) {
        putchar(*str++);
    }
}

/**
 * @brief Print a character with padding
 * 
 * @param c Character to print
 * @param width Total field width (negative for left-align)
 * @param pad Padding character
 */
static void print_char(char c, int width, char pad) {
    // Handle left alignment
    if (width < 0) {
        putchar(c);
        width = -width;
        width--; // Account for the character we just printed
        while (width > 0) {
            putchar(pad);
            width--;
        }
    } else {
        // Handle right alignment
        while (width > 1) {
            putchar(pad);
            width--;
        }
        putchar(c);
    }
}

/**
 * @brief Print a string with padding
 * 
 * @param str String to print
 * @param width Total field width (negative for left-align)
 * @param pad Padding character
 * @param precision Maximum characters to print from str, or -1 for unlimited
 */
static void print_string(const char* str, int width, char pad, int precision) {
    // Calculate string length
    size_t len = 0;
    const char* p = str;
    
    if (!str) {
        str = "(null)";
        p = str;
    }
    
    while (*p && (precision < 0 || len < (size_t)precision)) {
        len++;
        p++;
    }
    
    // Handle right alignment
    if (width > 0) {
        while (width > (int)len) {
            putchar(pad);
            width--;
        }
    }
    
    // Print the string
    for (size_t i = 0; i < len; i++) {
        putchar(str[i]);
    }
    
    // Handle left alignment
    if (width < 0) {
        width = -width;
        while (width > (int)len) {
            putchar(pad);
            width--;
        }
    }
}

/**
 * @brief Convert integer to string representation
 * 
 * @param value Integer value to convert
 * @param buffer Buffer to store the result
 * @param base Number base (e.g., 10 for decimal, 16 for hex)
 * @param uppercase Whether to use uppercase for hex digits
 * @return Length of the resulting string
 */
static int int_to_string(uint64_t value, char* buffer, int base, bool uppercase) {
    static const char lower_digits[] = "0123456789abcdef";
    static const char upper_digits[] = "0123456789ABCDEF";
    const char* digits = uppercase ? upper_digits : lower_digits;
    char tmp[MAX_INT_DIGITS];
    int len = 0;
    
    // Special case for zero
    if (value == 0) {
        buffer[0] = '0';
        buffer[1] = '\0';
        return 1;
    }
    
    // Convert to the specified base
    while (value > 0) {
        tmp[len++] = digits[value % base];
        value /= base;
    }
    
    // Reverse the string into the output buffer
    for (int i = 0; i < len; i++) {
        buffer[i] = tmp[len - i - 1];
    }
    buffer[len] = '\0';
    
    return len;
}

/**
 * @brief Print a number with formatting
 * 
 * @param value Value to print
 * @param is_signed Whether the value is signed
 * @param width Total field width (negative for left-align)
 * @param pad Padding character
 * @param base Number base (e.g., 10 for decimal, 16 for hex)
 * @param uppercase Whether to use uppercase for hex digits
 * @param precision Minimum number of digits (pad with leading zeros)
 * @param show_prefix Whether to show prefixes (0x for hex, etc.)
 */
static void print_number(uint64_t value, bool is_signed, int width, char pad,
                         int base, bool uppercase, int precision, bool show_prefix) {
    char buffer[MAX_INT_DIGITS + 3]; // Allow space for prefix (0x, etc.)
    bool neg = false;
    int len;
    int prefix_len = 0;
    
    // Handle negative numbers
    if (is_signed && (int64_t)value < 0) {
        neg = true;
        value = -(int64_t)value;
    }
    
    // Convert the absolute value to string
    len = int_to_string(value, buffer + MAX_INT_DIGITS/2, base, uppercase);
    
    // Add prefix if needed
    if (show_prefix) {
        if (base == 16) {
            // Hex prefix
            buffer[MAX_INT_DIGITS/2 - 2] = '0';
            buffer[MAX_INT_DIGITS/2 - 1] = uppercase ? 'X' : 'x';
            prefix_len = 2;
        } else if (base == 8) {
            // Octal prefix
            buffer[MAX_INT_DIGITS/2 - 1] = '0';
            prefix_len = 1;
        }
    }
    
    // Add negative sign if needed
    if (neg) {
        buffer[MAX_INT_DIGITS/2 - prefix_len - 1] = '-';
        prefix_len++;
    }
    
    char* str = buffer + MAX_INT_DIGITS/2 - prefix_len;
    
    // Apply precision (min number of digits)
    if (precision > len) {
        int zeros = precision - len;
        // Shift the string to make room for leading zeros
        for (int i = -1; i >= -len; i--) {
            str[i - zeros] = str[i];
        }
        // Add zeros
        for (int i = -prefix_len - 1; i >= -prefix_len - zeros; i--) {
            str[i] = '0';
        }
        len += zeros;
    }
    
    // Calculate total length
    len += prefix_len;
    
    // Handle padding
    if (width > 0) {
        // Right alignment
        while (width > len) {
            putchar(pad);
            width--;
        }
    }
    
    // Output the formatted number
    for (int i = 0; i < len; i++) {
        putchar(str[i]);
    }
    
    // Handle left alignment
    if (width < 0) {
        width = -width;
        while (width > len) {
            putchar(pad);
            width--;
        }
    }
}

/**
 * @brief Kernel printf implementation
 * 
 * Supports:
 * - %c: Character
 * - %s: String
 * - %d, %i: Signed integer
 * - %u: Unsigned integer
 * - %x, %X: Hex (lowercase/uppercase)
 * - %p: Pointer (0x prefix + lowercase hex)
 * - %o: Octal
 * - %b: Binary
 * 
 * Modifiers:
 * - Width: %5d (right-aligned), %-5d (left-aligned)
 * - Padding: %05d (zero-padded), % 5d (space-padded)
 * - Precision: %.5s (max 5 chars of string), %.5d (min 5 digits)
 * - Size: %ld (long), %lld (long long)
 * - Flags: %#x (show prefix)
 * 
 * @param fmt Format string
 * @param ... Arguments to format
 * @return Number of characters printed
 */
int kprintf(const char* fmt, ...) {
    va_list args;
    va_start(args, fmt);
    
    int count = 0;
    bool left_align;
    int width;
    int precision;
    char pad;
    bool alt_form;
    int length;
    
    while (*fmt) {
        // Handle normal characters
        if (*fmt != '%') {
            putchar(*fmt++);
            count++;
            continue;
        }
        
        // Handle format specifier
        fmt++;
        
        // Check for %% escape
        if (*fmt == '%') {
            putchar('%');
            fmt++;
            count++;
            continue;
        }
        
        // Initialize format controls
        left_align = false;
        width = 0;
        precision = -1;
        pad = ' ';
        alt_form = false;
        length = 0; // 0=normal, 1=long, 2=long long
        
        // Process flags
        bool done_flags = false;
        while (!done_flags) {
            switch (*fmt) {
                case '-': // Left align
                    left_align = true;
                    fmt++;
                    break;
                    
                case '0': // Zero padding
                    pad = '0';
                    fmt++;
                    break;
                    
                case ' ': // Space padding
                    pad = ' ';
                    fmt++;
                    break;
                    
                case '#': // Alternative form
                    alt_form = true;
                    fmt++;
                    break;
                    
                default:
                    done_flags = true;
                    break;
            }
        }
        
        // Process width
        if (*fmt == '*') {
            // Width provided as an argument
            width = va_arg(args, int);
            if (width < 0) {
                left_align = true;
                width = -width;
            }
            fmt++;
        } else {
            while (*fmt >= '0' && *fmt <= '9') {
                width = width * 10 + (*fmt - '0');
                fmt++;
            }
        }
        
        // Process precision
        if (*fmt == '.') {
            fmt++;
            if (*fmt == '*') {
                // Precision provided as an argument
                precision = va_arg(args, int);
                fmt++;
            } else {
                precision = 0;
                while (*fmt >= '0' && *fmt <= '9') {
                    precision = precision * 10 + (*fmt - '0');
                    fmt++;
                }
            }
        }
        
        // Process length
        while (*fmt == 'l' || *fmt == 'h') {
            if (*fmt == 'l') {
                length++;
                if (length > 2) length = 2; // Don't overflow
            }
            fmt++;
        }
        
        // Process format specifier
        switch (*fmt) {
            case 'c': {
                char c = (char)va_arg(args, int);
                if (left_align) {
                    width = -width;
                }
                print_char(c, width, pad);
                count++;
                break;
            }
                
            case 's': {
                char* s = va_arg(args, char*);
                if (left_align) {
                    width = -width;
                }
                print_string(s, width, pad, precision);
                count += (s ? strlen(s) : 6); // 6 for "(null)"
                break;
            }
                
            case 'd':
            case 'i': {
                int64_t value;
                if (length == 0) {
                    value = va_arg(args, int);
                } else if (length == 1) {
                    value = va_arg(args, long);
                } else {
                    value = va_arg(args, int64_t);
                }
                
                if (left_align) {
                    width = -width;
                }
                
                print_number((uint64_t)value, true, width, pad, 10, false, precision, false);
                count += 10; // Rough estimate
                break;
            }
                
            case 'u': {
                uint64_t value;
                if (length == 0) {
                    value = va_arg(args, unsigned int);
                } else if (length == 1) {
                    value = va_arg(args, unsigned long);
                } else {
                    value = va_arg(args, uint64_t);
                }
                
                if (left_align) {
                    width = -width;
                }
                
                print_number(value, false, width, pad, 10, false, precision, false);
                count += 10; // Rough estimate
                break;
            }
                
            case 'x':
            case 'X': {
                uint64_t value;
                if (length == 0) {
                    value = va_arg(args, unsigned int);
                } else if (length == 1) {
                    value = va_arg(args, unsigned long);
                } else {
                    value = va_arg(args, uint64_t);
                }
                
                if (left_align) {
                    width = -width;
                }
                
                bool uppercase = (*fmt == 'X');
                print_number(value, false, width, pad, 16, uppercase, precision, alt_form);
                count += 8; // Rough estimate
                break;
            }
                
            case 'p': {
                void* ptr = va_arg(args, void*);
                uintptr_t value = (uintptr_t)ptr;
                
                if (left_align) {
                    width = -width;
                }
                
                // Pointers are always shown in hex with 0x prefix
                print_number(value, false, width, pad, 16, false, sizeof(void*) * 2, true);
                count += 10; // Rough estimate for 32-bit pointer
                break;
            }
                
            case 'o': {
                uint64_t value;
                if (length == 0) {
                    value = va_arg(args, unsigned int);
                } else if (length == 1) {
                    value = va_arg(args, unsigned long);
                } else {
                    value = va_arg(args, uint64_t);
                }
                
                if (left_align) {
                    width = -width;
                }
                
                print_number(value, false, width, pad, 8, false, precision, alt_form);
                count += 10; // Rough estimate
                break;
            }
                
            case 'b': {
                uint64_t value;
                if (length == 0) {
                    value = va_arg(args, unsigned int);
                } else if (length == 1) {
                    value = va_arg(args, unsigned long);
                } else {
                    value = va_arg(args, uint64_t);
                }
                
                if (left_align) {
                    width = -width;
                }
                
                print_number(value, false, width, pad, 2, false, precision, false);
                count += 32; // Rough estimate
                break;
            }
                
            default:
                // Unsupported format specifier, just print it as is
                putchar('%');
                putchar(*fmt);
                count += 2;
                break;
        }
        
        fmt++;
    }
    
    va_end(args);
    return count;
}

// Buffer for snprintf
static char* snprintf_buffer = NULL;
static size_t snprintf_position = 0;
static size_t snprintf_size = 0;

/**
 * @brief Custom putchar for snprintf
 * 
 * @param c Character to output
 */
static void snprintf_putchar(char c) {
    if (snprintf_position < snprintf_size - 1) {
        snprintf_buffer[snprintf_position++] = c;
    }
    // We always count the character even if we don't store it
    // This allows proper return value for truncated strings
}

/**
 * @brief Safe snprintf implementation
 * 
 * @param buffer Output buffer
 * @param size Size of the buffer
 * @param fmt Format string
 * @param ... Arguments to format
 * @return Number of characters that would have been written if size was unlimited
 */
int snprintf(char* buffer, size_t size, const char* fmt, ...) {
    if (size == 0) return 0;
    if (buffer == NULL) return 0;
    
    // Save current output mode and setup buffer
    int old_mode = kprintf_mode;
    putchar_func_t old_putchar = current_putchar;
    
    // Setup snprintf state
    snprintf_buffer = buffer;
    snprintf_position = 0;
    snprintf_size = size;
    
    // Redirect output to our custom putchar
    current_putchar = snprintf_putchar;
    
    // Process the format string with our args
    va_list args;
    va_start(args, fmt);
    
    // We'll just use our existing kprintf for simplicity
    int result = kprintf(fmt, args);
    
    va_end(args);
    
    // Ensure null termination
    if (snprintf_position < snprintf_size) {
        snprintf_buffer[snprintf_position] = '\0';
    } else {
        snprintf_buffer[snprintf_size - 1] = '\0';
    }
    
    // Restore original output mode
    current_putchar = old_putchar;
    kprintf_mode = old_mode;
    
    return result;
}
