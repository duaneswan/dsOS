/**
 * @file printf.c
 * @brief Kernel printf implementation
 */

#include "../include/kernel.h"
#include <stdarg.h>
#include <stdbool.h>
#include <stdint.h>
#include <stddef.h>

// Maximum number conversion length
#define MAX_NUMBER_LENGTH 32

// Print mode flags
#define PRINTF_MODE_CONSOLE  0   // Output to console (VGA)
#define PRINTF_MODE_SERIAL   1   // Output to serial port
#define PRINTF_MODE_BOTH     2   // Output to console and serial port

// Current output mode
static int printf_mode = PRINTF_MODE_CONSOLE;

// Forward declaration of console putchar function (defined in vga.c)
extern void terminal_putchar(char c);

/**
 * @brief Set the printf output mode
 * 
 * @param mode Target mode (CONSOLE, SERIAL, or BOTH)
 */
void kprintf_set_mode(int mode) {
    printf_mode = mode;
}

/**
 * @brief Get the current printf output mode
 * 
 * @return Current mode
 */
int kprintf_get_mode(void) {
    return printf_mode;
}

/**
 * @brief Output a character to the appropriate destination(s)
 * 
 * @param c Character to output
 * @return Number of characters written (0 or 1)
 */
static int print_char(char c) {
    switch (printf_mode) {
        case PRINTF_MODE_CONSOLE:
            terminal_putchar(c);
            break;
            
        case PRINTF_MODE_SERIAL:
            if (debug_port != NULL && serial_is_initialized(debug_port)) {
                serial_write_char(debug_port, c);
            }
            break;
            
        case PRINTF_MODE_BOTH:
            terminal_putchar(c);
            if (debug_port != NULL && serial_is_initialized(debug_port)) {
                serial_write_char(debug_port, c);
            }
            break;
            
        default:
            return 0;
    }
    
    return 1;
}

/**
 * @brief Output a string to the appropriate destination(s)
 * 
 * @param str String to output
 * @return Number of characters written
 */
static int print_string(const char* str) {
    int count = 0;
    
    if (str == NULL) {
        return print_string("(null)");
    }
    
    while (*str) {
        count += print_char(*str++);
    }
    
    return count;
}

/**
 * @brief Print a number with the specified base
 * 
 * @param value Value to print
 * @param base Number base (e.g., 10 for decimal, 16 for hex)
 * @param uppercase Whether to use uppercase letters for hex digits
 * @param width Minimum field width
 * @param pad Padding character
 * @param is_signed Whether to handle as a signed value
 * @return Number of characters written
 */
static int print_number(long long value, int base, bool uppercase, int width, char pad, bool is_signed) {
    char buffer[MAX_NUMBER_LENGTH];
    char* digits = uppercase ? "0123456789ABCDEF" : "0123456789abcdef";
    int count = 0;
    int i = 0;
    bool negative = false;
    unsigned long long abs_value;
    
    // Handle negative numbers
    if (is_signed && value < 0) {
        negative = true;
        abs_value = -value;
    } else {
        abs_value = (unsigned long long)value;
    }
    
    // Handle zero specially
    if (abs_value == 0) {
        buffer[i++] = '0';
    } else {
        // Convert number to string (in reverse)
        while (abs_value > 0) {
            buffer[i++] = digits[abs_value % base];
            abs_value /= base;
        }
    }
    
    // Account for negative sign in width
    if (negative) {
        width--;
    }
    
    // Pad if needed
    while (i < width) {
        buffer[i++] = pad;
    }
    
    // Add negative sign if needed
    if (negative) {
        buffer[i++] = '-';
    }
    
    // Print in reverse order
    while (i > 0) {
        count += print_char(buffer[--i]);
    }
    
    return count;
}

/**
 * @brief Print an unsigned number with the specified base
 * 
 * @param value Value to print
 * @param base Number base
 * @param uppercase Whether to use uppercase letters for hex digits
 * @param width Minimum field width
 * @param pad Padding character
 * @return Number of characters written
 */
static int print_unsigned(unsigned long long value, int base, bool uppercase, int width, char pad) {
    return print_number(value, base, uppercase, width, pad, false);
}

/**
 * @brief Parse format flags and modifiers
 * 
 * @param format Format string pointer (will be updated)
 * @param width Pointer to store field width
 * @param pad Pointer to store padding character
 * @param is_long Pointer to store long/longlong flag
 */
static void parse_format_flags(const char** format, int* width, char* pad, int* is_long) {
    const char* ptr = *format;
    
    // Default values
    *width = 0;
    *pad = ' ';
    *is_long = 0;
    
    // Check for zero padding
    if (*ptr == '0') {
        *pad = '0';
        ptr++;
    }
    
    // Parse width
    while (*ptr >= '0' && *ptr <= '9') {
        *width = (*width * 10) + (*ptr - '0');
        ptr++;
    }
    
    // Parse length modifiers
    if (*ptr == 'l') {
        *is_long = 1;
        ptr++;
        if (*ptr == 'l') {
            *is_long = 2;
            ptr++;
        }
    }
    
    // Update format pointer
    *format = ptr;
}

/**
 * @brief Print formatted data to a buffer
 * 
 * @param buffer Buffer to store output, or NULL to print directly
 * @param size Buffer size (if buffer is not NULL)
 * @param format Format string
 * @param args Variable arguments
 * @return Number of characters written or that would have been written
 */
static int vprintf_internal(char* buffer, size_t size, const char* format, va_list args) {
    int count = 0;
    bool buffered = (buffer != NULL);
    int pos = 0;
    
    // Function to add a character to the buffer or print it
    auto void add_char(char c) {
        if (buffered) {
            if ((size_t)pos < size - 1) {
                buffer[pos++] = c;
            }
        } else {
            print_char(c);
        }
        count++;
    }
    
    // Parse format string
    while (*format) {
        if (*format != '%') {
            // Regular character
            add_char(*format++);
            continue;
        }
        
        // Handle format specifier
        format++;
        
        // Handle %% (literal %)
        if (*format == '%') {
            add_char('%');
            format++;
            continue;
        }
        
        // Parse flags and modifiers
        int width = 0;
        char pad = ' ';
        int is_long = 0;
        parse_format_flags(&format, &width, &pad, &is_long);
        
        // Handle the format specifier
        switch (*format) {
            case 'c': {
                // Character
                char c = (char)va_arg(args, int);
                add_char(c);
                break;
            }
                
            case 's': {
                // String
                const char* str = va_arg(args, const char*);
                if (str == NULL) {
                    str = "(null)";
                }
                
                // Calculate string length
                int len = 0;
                const char* s = str;
                while (*s) {
                    len++;
                    s++;
                }
                
                // Pad if necessary
                while (len < width) {
                    add_char(pad);
                    width--;
                }
                
                // Output the string
                while (*str) {
                    add_char(*str++);
                }
                break;
            }
                
            case 'd':
            case 'i': {
                // Signed decimal
                if (is_long == 0) {
                    int value = va_arg(args, int);
                    int len = print_number(value, 10, false, width, pad, true);
                    count += len;
                } else if (is_long == 1) {
                    long value = va_arg(args, long);
                    int len = print_number(value, 10, false, width, pad, true);
                    count += len;
                } else {
                    long long value = va_arg(args, long long);
                    int len = print_number(value, 10, false, width, pad, true);
                    count += len;
                }
                break;
            }
                
            case 'u': {
                // Unsigned decimal
                if (is_long == 0) {
                    unsigned int value = va_arg(args, unsigned int);
                    int len = print_unsigned(value, 10, false, width, pad);
                    count += len;
                } else if (is_long == 1) {
                    unsigned long value = va_arg(args, unsigned long);
                    int len = print_unsigned(value, 10, false, width, pad);
                    count += len;
                } else {
                    unsigned long long value = va_arg(args, unsigned long long);
                    int len = print_unsigned(value, 10, false, width, pad);
                    count += len;
                }
                break;
            }
                
            case 'x':
            case 'X': {
                // Hexadecimal
                bool uppercase = (*format == 'X');
                if (is_long == 0) {
                    unsigned int value = va_arg(args, unsigned int);
                    int len = print_unsigned(value, 16, uppercase, width, pad);
                    count += len;
                } else if (is_long == 1) {
                    unsigned long value = va_arg(args, unsigned long);
                    int len = print_unsigned(value, 16, uppercase, width, pad);
                    count += len;
                } else {
                    unsigned long long value = va_arg(args, unsigned long long);
                    int len = print_unsigned(value, 16, uppercase, width, pad);
                    count += len;
                }
                break;
            }
                
            case 'p': {
                // Pointer (treat as %#lx)
                add_char('0');
                add_char('x');
                void* value = va_arg(args, void*);
                int len = print_unsigned((unsigned long)value, 16, false, width, pad);
                count += len;
                break;
            }
                
            case 'o': {
                // Octal
                if (is_long == 0) {
                    unsigned int value = va_arg(args, unsigned int);
                    int len = print_unsigned(value, 8, false, width, pad);
                    count += len;
                } else if (is_long == 1) {
                    unsigned long value = va_arg(args, unsigned long);
                    int len = print_unsigned(value, 8, false, width, pad);
                    count += len;
                } else {
                    unsigned long long value = va_arg(args, unsigned long long);
                    int len = print_unsigned(value, 8, false, width, pad);
                    count += len;
                }
                break;
            }
                
            case 'b': {
                // Binary (non-standard)
                if (is_long == 0) {
                    unsigned int value = va_arg(args, unsigned int);
                    int len = print_unsigned(value, 2, false, width, pad);
                    count += len;
                } else if (is_long == 1) {
                    unsigned long value = va_arg(args, unsigned long);
                    int len = print_unsigned(value, 2, false, width, pad);
                    count += len;
                } else {
                    unsigned long long value = va_arg(args, unsigned long long);
                    int len = print_unsigned(value, 2, false, width, pad);
                    count += len;
                }
                break;
            }
                
            default:
                // Unknown format specifier, just print it
                add_char('%');
                add_char(*format);
                break;
        }
        
        format++;
    }
    
    // Null-terminate the buffer if buffered
    if (buffered && size > 0) {
        buffer[pos] = '\0';
    }
    
    return count;
}

/**
 * @brief Kernel printf - output formatted string
 * 
 * @param format Format string
 * @param ... Variable arguments
 * @return Number of characters printed
 */
int kprintf(const char* format, ...) {
    va_list args;
    va_start(args, format);
    int ret = vprintf_internal(NULL, 0, format, args);
    va_end(args);
    return ret;
}

/**
 * @brief Kernel vprintf - output formatted string using va_list
 * 
 * @param format Format string
 * @param args Variable arguments
 * @return Number of characters printed
 */
int vkprintf(const char* format, va_list args) {
    return vprintf_internal(NULL, 0, format, args);
}

/**
 * @brief Format a string into a buffer
 * 
 * @param buffer Buffer to store formatted string
 * @param size Buffer size
 * @param format Format string
 * @param ... Variable arguments
 * @return Number of characters (excluding null terminator) that would have been written
 */
int snprintf(char* buffer, size_t size, const char* format, ...) {
    va_list args;
    va_start(args, format);
    int ret = vsnprintf(buffer, size, format, args);
    va_end(args);
    return ret;
}

/**
 * @brief Format a string into a buffer using va_list
 * 
 * @param buffer Buffer to store formatted string
 * @param size Buffer size
 * @param format Format string
 * @param args Variable arguments
 * @return Number of characters (excluding null terminator) that would have been written
 */
int vsnprintf(char* buffer, size_t size, const char* format, va_list args) {
    return vprintf_internal(buffer, size, format, args);
}
