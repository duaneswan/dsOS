/**
 * @file printf.c
 * @brief Formatted output functions
 */

#include "../include/kernel.h"
#include <stdint.h>
#include <stddef.h>
#include <stdarg.h>
#include <stdbool.h>

// Output modes
#define PRINTF_MODE_VGA     0  // Output to VGA console
#define PRINTF_MODE_SERIAL  1  // Output to serial port
#define PRINTF_MODE_BOTH    2  // Output to both VGA and serial

// Maximum string length for temporary buffers
#define MAX_PRINTF_SIZE  4096

// Current output mode
static int printf_mode = PRINTF_MODE_BOTH;

// Flags for format specifiers
typedef struct {
    bool left_justify;   // '-' flag
    bool plus_sign;      // '+' flag
    bool space_sign;     // ' ' flag
    bool prefix;         // '#' flag
    bool zero_pad;       // '0' flag
    int width;           // Width field
    int precision;       // Precision field
    char length;         // Length modifier (h, l, L)
} format_flags_t;

/**
 * @brief Set the printf output mode
 * 
 * @param mode New output mode
 */
void kprintf_set_mode(int mode) {
    if (mode >= PRINTF_MODE_VGA && mode <= PRINTF_MODE_BOTH) {
        printf_mode = mode;
    }
}

/**
 * @brief Parse format specifier flags
 * 
 * @param format Format string
 * @param flags Flags structure to fill
 * @return Updated position in format string
 */
static const char* parse_flags(const char* format, format_flags_t* flags) {
    // Initialize flags
    flags->left_justify = false;
    flags->plus_sign = false;
    flags->space_sign = false;
    flags->prefix = false;
    flags->zero_pad = false;
    flags->width = -1;
    flags->precision = -1;
    flags->length = '\0';
    
    // Parse flags
    bool parsing_flags = true;
    while (parsing_flags) {
        switch (*format) {
            case '-':
                flags->left_justify = true;
                format++;
                break;
                
            case '+':
                flags->plus_sign = true;
                format++;
                break;
                
            case ' ':
                flags->space_sign = true;
                format++;
                break;
                
            case '#':
                flags->prefix = true;
                format++;
                break;
                
            case '0':
                flags->zero_pad = true;
                format++;
                break;
                
            default:
                parsing_flags = false;
                break;
        }
    }
    
    // Parse width
    if (*format >= '1' && *format <= '9') {
        flags->width = 0;
        while (*format >= '0' && *format <= '9') {
            flags->width = flags->width * 10 + (*format - '0');
            format++;
        }
    } else if (*format == '*') {
        flags->width = -2;  // Will be read from va_args
        format++;
    }
    
    // Parse precision
    if (*format == '.') {
        format++;
        if (*format == '*') {
            flags->precision = -2;  // Will be read from va_args
            format++;
        } else {
            flags->precision = 0;
            while (*format >= '0' && *format <= '9') {
                flags->precision = flags->precision * 10 + (*format - '0');
                format++;
            }
        }
    }
    
    // Parse length modifier
    if (*format == 'h' || *format == 'l' || *format == 'L') {
        flags->length = *format++;
    }
    
    return format;
}

/**
 * @brief Convert a number to a string with specified base
 * 
 * @param value Number to convert
 * @param buffer Output buffer
 * @param base Numerical base (2-16)
 * @param upper Use uppercase for hexadecimal
 * @return Length of the resulting string
 */
static int int_to_string(uint64_t value, char* buffer, int base, bool upper) {
    char* ptr = buffer;
    char* low = buffer;
    char digits[] = "0123456789abcdef";
    
    if (upper) {
        digits[10] = 'A';
        digits[11] = 'B';
        digits[12] = 'C';
        digits[13] = 'D';
        digits[14] = 'E';
        digits[15] = 'F';
    }
    
    // Special case for 0
    if (value == 0) {
        *ptr++ = '0';
        *ptr = '\0';
        return 1;
    }
    
    // Convert to specified base
    while (value) {
        *ptr++ = digits[value % base];
        value /= base;
    }
    
    *ptr = '\0';
    
    // Reverse the string
    ptr--;
    while (low < ptr) {
        char temp = *low;
        *low++ = *ptr;
        *ptr-- = temp;
    }
    
    return (ptr - buffer) + 1;
}

/**
 * @brief Format a string according to printf rules
 * 
 * @param buffer Output buffer
 * @param max_size Maximum buffer size
 * @param format Format string
 * @param args Variable arguments
 * @return Number of characters written
 */
static int vsprintf_internal(char* buffer, size_t max_size, const char* format, va_list args) {
    int written = 0;
    format_flags_t flags;
    
    // Iterate through format string
    while (*format && written < (int)max_size - 1) {
        // Handle normal characters
        if (*format != '%') {
            buffer[written++] = *format++;
            continue;
        }
        
        // Handle format specifiers
        format++;
        
        // Handle %% escape
        if (*format == '%') {
            buffer[written++] = '%';
            format++;
            continue;
        }
        
        // Parse format flags
        format = parse_flags(format, &flags);
        
        // Get width and precision from arguments if specified
        if (flags.width == -2) {
            flags.width = va_arg(args, int);
            if (flags.width < 0) {
                flags.width = -flags.width;
                flags.left_justify = true;
            }
        }
        
        if (flags.precision == -2) {
            flags.precision = va_arg(args, int);
            if (flags.precision < 0) {
                flags.precision = -1;
            }
        }
        
        // Process format specifier
        char specifier = *format++;
        switch (specifier) {
            case 'c': {
                // Character
                char c = (char)va_arg(args, int);
                buffer[written++] = c;
                break;
            }
                
            case 's': {
                // String
                const char* s = va_arg(args, const char*);
                if (s == NULL) {
                    s = "(null)";
                }
                
                size_t len = strlen(s);
                if (flags.precision >= 0 && len > (size_t)flags.precision) {
                    len = flags.precision;
                }
                
                // Apply width padding before if not left-justified
                if (!flags.left_justify) {
                    while (flags.width > (int)len && written < (int)max_size - 1) {
                        buffer[written++] = ' ';
                        flags.width--;
                    }
                }
                
                // Copy string
                for (size_t i = 0; i < len && written < (int)max_size - 1; i++) {
                    buffer[written++] = s[i];
                }
                
                // Apply width padding after if left-justified
                if (flags.left_justify) {
                    while (flags.width > (int)len && written < (int)max_size - 1) {
                        buffer[written++] = ' ';
                        flags.width--;
                    }
                }
                
                break;
            }
                
            case 'd':
            case 'i': {
                // Signed decimal integer
                int64_t value;
                if (flags.length == 'l') {
                    value = va_arg(args, long);
                } else if (flags.length == 'h') {
                    value = (short)va_arg(args, int);
                } else {
                    value = va_arg(args, int);
                }
                
                // Handle negative numbers
                bool negative = (value < 0);
                if (negative) {
                    value = -value;
                }
                
                // Convert to string
                char num_buffer[32];
                int len = int_to_string(value, num_buffer, 10, false);
                
                // Calculate total width including sign
                int total_width = len;
                if (negative || flags.plus_sign || flags.space_sign) {
                    total_width++;
                }
                
                // Apply width padding before if not left-justified
                if (!flags.left_justify && !flags.zero_pad) {
                    while (flags.width > total_width && written < (int)max_size - 1) {
                        buffer[written++] = ' ';
                        flags.width--;
                    }
                }
                
                // Add sign
                if (negative) {
                    buffer[written++] = '-';
                } else if (flags.plus_sign) {
                    buffer[written++] = '+';
                } else if (flags.space_sign) {
                    buffer[written++] = ' ';
                }
                
                // Apply zero padding if specified
                if (!flags.left_justify && flags.zero_pad) {
                    while (flags.width > len && written < (int)max_size - 1) {
                        buffer[written++] = '0';
                        flags.width--;
                    }
                }
                
                // Copy number
                for (int i = 0; i < len && written < (int)max_size - 1; i++) {
                    buffer[written++] = num_buffer[i];
                }
                
                // Apply width padding after if left-justified
                if (flags.left_justify) {
                    while (flags.width > total_width && written < (int)max_size - 1) {
                        buffer[written++] = ' ';
                        flags.width--;
                    }
                }
                
                break;
            }
                
            case 'u': {
                // Unsigned decimal integer
                uint64_t value;
                if (flags.length == 'l') {
                    value = va_arg(args, unsigned long);
                } else if (flags.length == 'h') {
                    value = (unsigned short)va_arg(args, unsigned int);
                } else {
                    value = va_arg(args, unsigned int);
                }
                
                // Convert to string
                char num_buffer[32];
                int len = int_to_string(value, num_buffer, 10, false);
                
                // Apply width padding before if not left-justified
                if (!flags.left_justify && !flags.zero_pad) {
                    while (flags.width > len && written < (int)max_size - 1) {
                        buffer[written++] = ' ';
                        flags.width--;
                    }
                }
                
                // Apply zero padding if specified
                if (!flags.left_justify && flags.zero_pad) {
                    while (flags.width > len && written < (int)max_size - 1) {
                        buffer[written++] = '0';
                        flags.width--;
                    }
                }
                
                // Copy number
                for (int i = 0; i < len && written < (int)max_size - 1; i++) {
                    buffer[written++] = num_buffer[i];
                }
                
                // Apply width padding after if left-justified
                if (flags.left_justify) {
                    while (flags.width > len && written < (int)max_size - 1) {
                        buffer[written++] = ' ';
                        flags.width--;
                    }
                }
                
                break;
            }
                
            case 'x':
            case 'X': {
                // Hexadecimal integer
                uint64_t value;
                if (flags.length == 'l') {
                    value = va_arg(args, unsigned long);
                } else if (flags.length == 'h') {
                    value = (unsigned short)va_arg(args, unsigned int);
                } else {
                    value = va_arg(args, unsigned int);
                }
                
                bool uppercase = (specifier == 'X');
                
                // Convert to string
                char num_buffer[32];
                int len = int_to_string(value, num_buffer, 16, uppercase);
                
                // Calculate total width including prefix
                int total_width = len;
                if (flags.prefix && value != 0) {
                    total_width += 2;
                }
                
                // Apply width padding before if not left-justified
                if (!flags.left_justify && !flags.zero_pad) {
                    while (flags.width > total_width && written < (int)max_size - 1) {
                        buffer[written++] = ' ';
                        flags.width--;
                    }
                }
                
                // Add prefix
                if (flags.prefix && value != 0) {
                    buffer[written++] = '0';
                    buffer[written++] = uppercase ? 'X' : 'x';
                }
                
                // Apply zero padding if specified
                if (!flags.left_justify && flags.zero_pad) {
                    while (flags.width > (total_width - (flags.prefix && value != 0 ? 2 : 0)) && written < (int)max_size - 1) {
                        buffer[written++] = '0';
                        flags.width--;
                    }
                }
                
                // Copy number
                for (int i = 0; i < len && written < (int)max_size - 1; i++) {
                    buffer[written++] = num_buffer[i];
                }
                
                // Apply width padding after if left-justified
                if (flags.left_justify) {
                    while (flags.width > total_width && written < (int)max_size - 1) {
                        buffer[written++] = ' ';
                        flags.width--;
                    }
                }
                
                break;
            }
                
            case 'o': {
                // Octal integer
                uint64_t value;
                if (flags.length == 'l') {
                    value = va_arg(args, unsigned long);
                } else if (flags.length == 'h') {
                    value = (unsigned short)va_arg(args, unsigned int);
                } else {
                    value = va_arg(args, unsigned int);
                }
                
                // Convert to string
                char num_buffer[32];
                int len = int_to_string(value, num_buffer, 8, false);
                
                // Calculate total width including prefix
                int total_width = len;
                if (flags.prefix && value != 0) {
                    total_width++;
                }
                
                // Apply width padding before if not left-justified
                if (!flags.left_justify && !flags.zero_pad) {
                    while (flags.width > total_width && written < (int)max_size - 1) {
                        buffer[written++] = ' ';
                        flags.width--;
                    }
                }
                
                // Add prefix
                if (flags.prefix && value != 0) {
                    buffer[written++] = '0';
                }
                
                // Apply zero padding if specified
                if (!flags.left_justify && flags.zero_pad) {
                    while (flags.width > (total_width - (flags.prefix && value != 0 ? 1 : 0)) && written < (int)max_size - 1) {
                        buffer[written++] = '0';
                        flags.width--;
                    }
                }
                
                // Copy number
                for (int i = 0; i < len && written < (int)max_size - 1; i++) {
                    buffer[written++] = num_buffer[i];
                }
                
                // Apply width padding after if left-justified
                if (flags.left_justify) {
                    while (flags.width > total_width && written < (int)max_size - 1) {
                        buffer[written++] = ' ';
                        flags.width--;
                    }
                }
                
                break;
            }
                
            case 'p': {
                // Pointer
                void* value = va_arg(args, void*);
                
                // Handle NULL pointer
                if (value == NULL) {
                    const char* null_str = "(nil)";
                    size_t len = strlen(null_str);
                    
                    // Apply width padding before if not left-justified
                    if (!flags.left_justify) {
                        while (flags.width > (int)len && written < (int)max_size - 1) {
                            buffer[written++] = ' ';
                            flags.width--;
                        }
                    }
                    
                    // Copy string
                    for (size_t i = 0; i < len && written < (int)max_size - 1; i++) {
                        buffer[written++] = null_str[i];
                    }
                    
                    // Apply width padding after if left-justified
                    if (flags.left_justify) {
                        while (flags.width > (int)len && written < (int)max_size - 1) {
                            buffer[written++] = ' ';
                            flags.width--;
                        }
                    }
                } else {
                    // Convert to hexadecimal
                    uintptr_t ptr_value = (uintptr_t)value;
                    
                    // Convert to string
                    char num_buffer[32];
                    int len = int_to_string(ptr_value, num_buffer, 16, false);
                    
                    // Calculate total width including prefix
                    int total_width = len + 2;  // Always add '0x' prefix for pointers
                    
                    // Apply width padding before if not left-justified
                    if (!flags.left_justify) {
                        while (flags.width > total_width && written < (int)max_size - 1) {
                            buffer[written++] = ' ';
                            flags.width--;
                        }
                    }
                    
                    // Add prefix
                    buffer[written++] = '0';
                    buffer[written++] = 'x';
                    
                    // Copy number
                    for (int i = 0; i < len && written < (int)max_size - 1; i++) {
                        buffer[written++] = num_buffer[i];
                    }
                    
                    // Apply width padding after if left-justified
                    if (flags.left_justify) {
                        while (flags.width > total_width && written < (int)max_size - 1) {
                            buffer[written++] = ' ';
                            flags.width--;
                        }
                    }
                }
                
                break;
            }
                
            case 'n': {
                // Store number of characters written so far
                int* ptr = va_arg(args, int*);
                if (ptr != NULL) {
                    *ptr = written;
                }
                break;
            }
                
            default:
                // Unknown format specifier, just print it
                buffer[written++] = '%';
                buffer[written++] = specifier;
                break;
        }
    }
    
    // Null-terminate the string
    buffer[written] = '\0';
    
    return written;
}

/**
 * @brief Format a string and write it to a buffer
 * 
 * @param buffer Output buffer
 * @param size Maximum buffer size
 * @param format Format string
 * @param ... Variable arguments
 * @return Number of characters written (excluding the null terminator)
 */
int snprintf(char* buffer, size_t size, const char* format, ...) {
    va_list args;
    int ret;
    
    // Check for NULL buffer or zero size
    if (buffer == NULL || size == 0) {
        return 0;
    }
    
    // Format the string
    va_start(args, format);
    ret = vsprintf_internal(buffer, size, format, args);
    va_end(args);
    
    return ret;
}

/**
 * @brief Format a string and write it to the console
 * 
 * @param format Format string
 * @param ... Variable arguments
 * @return Number of characters written
 */
int kprintf(const char* format, ...) {
    va_list args;
    char buffer[MAX_PRINTF_SIZE];
    int written;
    
    // Format the string
    va_start(args, format);
    written = vsprintf_internal(buffer, MAX_PRINTF_SIZE, format, args);
    va_end(args);
    
    // Output to VGA console
    if (printf_mode == PRINTF_MODE_VGA || printf_mode == PRINTF_MODE_BOTH) {
        for (int i = 0; i < written; i++) {
            vga_putchar(buffer[i]);
        }
    }
    
    // Output to serial port if initialized
    if ((printf_mode == PRINTF_MODE_SERIAL || printf_mode == PRINTF_MODE_BOTH) && 
        serial_is_initialized(debug_port)) {
        serial_write_str(debug_port, buffer);
    }
    
    return written;
}
