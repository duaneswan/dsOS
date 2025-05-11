/**
 * @file printf.c
 * @brief Implementation of printf-like functionality for kernel debugging
 */

#include "../include/kernel.h"
#include <stdint.h>
#include <stddef.h>
#include <stdarg.h>
#include <stdbool.h>

// Output modes
#define PRINT_MODE_NORMAL    0  // Print to both serial and VGA
#define PRINT_MODE_SERIAL    1  // Print to serial only
#define PRINT_MODE_VGA       2  // Print to VGA only

// Function declarations
extern void vga_putchar(char c);
extern void serial_write_byte(uint16_t port, uint8_t data);
extern uint16_t debug_port;
extern bool serial_initialized;

// Output mode
static int print_mode = PRINT_MODE_NORMAL;

/**
 * @brief Set the printf output mode
 * 
 * @param mode Output mode (PRINT_MODE_NORMAL, PRINT_MODE_SERIAL, or PRINT_MODE_VGA)
 */
void kprintf_set_mode(int mode) {
    if (mode >= PRINT_MODE_NORMAL && mode <= PRINT_MODE_VGA) {
        print_mode = mode;
    }
}

/**
 * @brief Print a character to the kernel console
 * 
 * @param c Character to print
 */
static void kputchar(char c) {
    // Output to VGA if appropriate
    if (print_mode == PRINT_MODE_NORMAL || print_mode == PRINT_MODE_VGA) {
        vga_putchar(c);
    }
    
    // Output to serial if appropriate
    if ((print_mode == PRINT_MODE_NORMAL || print_mode == PRINT_MODE_SERIAL) && serial_initialized) {
        // Convert LF to CRLF for serial
        if (c == '\n') {
            serial_write_byte(debug_port, '\r');
        }
        serial_write_byte(debug_port, c);
    }
}

/**
 * @brief Print a string to the kernel console
 * 
 * @param s String to print
 */
static void kputs(const char* s) {
    while (*s) {
        kputchar(*s++);
    }
}

/**
 * @brief Convert an unsigned integer to a string with the given base
 * 
 * @param value Value to convert
 * @param buffer Buffer to store the result
 * @param base Base for conversion (2-16)
 * @param pad_char Character to use for padding
 * @param pad_width Width to pad to (0 for no padding)
 * @param uppercase Use uppercase letters for hex (base 16)
 * @return Pointer to the beginning of the result in the buffer
 */
static char* uitoa(
    uint64_t value,
    char* buffer,
    int base,
    char pad_char,
    int pad_width,
    bool uppercase
) {
    static const char* digits_lower = "0123456789abcdef";
    static const char* digits_upper = "0123456789ABCDEF";
    const char* digits = uppercase ? digits_upper : digits_lower;
    char* ptr = buffer;
    char* start;
    
    // Check for invalid base
    if (base < 2 || base > 16) {
        *ptr = '\0';
        return buffer;
    }
    
    // Convert to string in reverse order
    do {
        *ptr++ = digits[value % base];
        value /= base;
        pad_width--;
    } while (value > 0);
    
    // Add padding if requested
    while (pad_width > 0) {
        *ptr++ = pad_char;
        pad_width--;
    }
    
    // Null-terminate
    *ptr = '\0';
    
    // Reverse the string
    start = buffer;
    ptr--;
    while (start < ptr) {
        char temp = *start;
        *start++ = *ptr;
        *ptr-- = temp;
    }
    
    return buffer;
}

/**
 * @brief Convert a signed integer to a string with the given base
 * 
 * @param value Value to convert
 * @param buffer Buffer to store the result
 * @param base Base for conversion (2-16)
 * @param pad_char Character to use for padding
 * @param pad_width Width to pad to (0 for no padding)
 * @param uppercase Use uppercase letters for hex (base 16)
 * @return Pointer to the beginning of the result in the buffer
 */
static char* itoa(
    int64_t value,
    char* buffer,
    int base,
    char pad_char,
    int pad_width,
    bool uppercase
) {
    char* ptr = buffer;
    
    // Handle negative values
    if (value < 0 && base == 10) {
        *ptr++ = '-';
        value = -value;
        if (pad_width > 0) {
            pad_width--;
        }
    }
    
    // Convert the absolute value
    return uitoa((uint64_t)value, ptr, base, pad_char, pad_width, uppercase);
}

/**
 * @brief Format a string and print it to the kernel console
 * 
 * @param fmt Format string
 * @param args Variable arguments
 * @return Number of characters printed
 */
int vkprintf(const char* fmt, va_list args) {
    int count = 0;
    char buffer[128];
    
    while (*fmt) {
        if (*fmt == '%') {
            fmt++;
            
            // Handle format specifiers
            bool left_justify = false;
            bool force_sign = false;
            bool space_for_sign = false;
            bool prefix = false;
            bool zero_pad = false;
            int width = 0;
            int precision = -1;
            bool uppercase = false;
            bool long_flag = false;
            bool long_long_flag = false;
            bool short_flag = false;
            bool char_flag = false;
            
            // Parse flags
            while (true) {
                switch (*fmt) {
                    case '-': left_justify = true; fmt++; continue;
                    case '+': force_sign = true; fmt++; continue;
                    case ' ': space_for_sign = true; fmt++; continue;
                    case '#': prefix = true; fmt++; continue;
                    case '0': zero_pad = true; fmt++; continue;
                    default: break;
                }
                break;
            }
            
            // Parse width
            if (*fmt == '*') {
                width = va_arg(args, int);
                if (width < 0) {
                    left_justify = true;
                    width = -width;
                }
                fmt++;
            } else {
                while (*fmt >= '0' && *fmt <= '9') {
                    width = width * 10 + (*fmt - '0');
                    fmt++;
                }
            }
            
            // Parse precision
            if (*fmt == '.') {
                fmt++;
                precision = 0;
                if (*fmt == '*') {
                    precision = va_arg(args, int);
                    if (precision < 0) {
                        precision = -1;
                    }
                    fmt++;
                } else {
                    while (*fmt >= '0' && *fmt <= '9') {
                        precision = precision * 10 + (*fmt - '0');
                        fmt++;
                    }
                }
            }
            
            // Parse length modifiers
            switch (*fmt) {
                case 'h':
                    fmt++;
                    if (*fmt == 'h') { // hh (char)
                        char_flag = true;
                        fmt++;
                    } else { // h (short)
                        short_flag = true;
                    }
                    break;
                case 'l':
                    fmt++;
                    if (*fmt == 'l') { // ll (long long)
                        long_long_flag = true;
                        fmt++;
                    } else { // l (long)
                        long_flag = true;
                    }
                    break;
                case 'z': // size_t
                case 't': // ptrdiff_t
                    long_flag = true;
                    fmt++;
                    break;
            }
            
            // Parse conversion specifier
            switch (*fmt) {
                case 'd':
                case 'i': {
                    // Signed decimal
                    int64_t value;
                    if (long_long_flag) {
                        value = va_arg(args, int64_t);
                    } else if (long_flag) {
                        value = va_arg(args, long);
                    } else if (short_flag) {
                        value = (short)va_arg(args, int);
                    } else if (char_flag) {
                        value = (char)va_arg(args, int);
                    } else {
                        value = va_arg(args, int);
                    }
                    
                    char pad_char = zero_pad ? '0' : ' ';
                    itoa(value, buffer, 10, pad_char, width, false);
                    kputs(buffer);
                    count += strlen(buffer);
                    break;
                }
                
                case 'u': {
                    // Unsigned decimal
                    uint64_t value;
                    if (long_long_flag) {
                        value = va_arg(args, uint64_t);
                    } else if (long_flag) {
                        value = va_arg(args, unsigned long);
                    } else if (short_flag) {
                        value = (unsigned short)va_arg(args, unsigned int);
                    } else if (char_flag) {
                        value = (unsigned char)va_arg(args, unsigned int);
                    } else {
                        value = va_arg(args, unsigned int);
                    }
                    
                    char pad_char = zero_pad ? '0' : ' ';
                    uitoa(value, buffer, 10, pad_char, width, false);
                    kputs(buffer);
                    count += strlen(buffer);
                    break;
                }
                
                case 'o': {
                    // Octal
                    uint64_t value;
                    if (long_long_flag) {
                        value = va_arg(args, uint64_t);
                    } else if (long_flag) {
                        value = va_arg(args, unsigned long);
                    } else if (short_flag) {
                        value = (unsigned short)va_arg(args, unsigned int);
                    } else if (char_flag) {
                        value = (unsigned char)va_arg(args, unsigned int);
                    } else {
                        value = va_arg(args, unsigned int);
                    }
                    
                    char pad_char = zero_pad ? '0' : ' ';
                    if (prefix && value != 0) {
                        buffer[0] = '0';
                        uitoa(value, buffer + 1, 8, pad_char, width > 0 ? width - 1 : 0, false);
                    } else {
                        uitoa(value, buffer, 8, pad_char, width, false);
                    }
                    kputs(buffer);
                    count += strlen(buffer);
                    break;
                }
                
                case 'x':
                case 'X': {
                    // Hexadecimal
                    uppercase = (*fmt == 'X');
                    uint64_t value;
                    if (long_long_flag) {
                        value = va_arg(args, uint64_t);
                    } else if (long_flag) {
                        value = va_arg(args, unsigned long);
                    } else if (short_flag) {
                        value = (unsigned short)va_arg(args, unsigned int);
                    } else if (char_flag) {
                        value = (unsigned char)va_arg(args, unsigned int);
                    } else {
                        value = va_arg(args, unsigned int);
                    }
                    
                    char pad_char = zero_pad ? '0' : ' ';
                    if (prefix && value != 0) {
                        buffer[0] = '0';
                        buffer[1] = uppercase ? 'X' : 'x';
                        uitoa(value, buffer + 2, 16, pad_char, width > 1 ? width - 2 : 0, uppercase);
                    } else {
                        uitoa(value, buffer, 16, pad_char, width, uppercase);
                    }
                    kputs(buffer);
                    count += strlen(buffer);
                    break;
                }
                
                case 'c': {
                    // Character
                    int ch = va_arg(args, int);
                    kputchar(ch);
                    count++;
                    break;
                }
                
                case 's': {
                    // String
                    const char* str = va_arg(args, const char*);
                    if (str == NULL) {
                        str = "(null)";
                    }
                    
                    size_t len = strlen(str);
                    if (precision >= 0 && (size_t)precision < len) {
                        len = precision;
                    }
                    
                    // Handle width if needed
                    if (width > (int)len && !left_justify) {
                        for (int i = 0; i < width - (int)len; i++) {
                            kputchar(' ');
                            count++;
                        }
                    }
                    
                    // Output the string
                    for (size_t i = 0; i < len; i++) {
                        kputchar(str[i]);
                    }
                    count += len;
                    
                    // Handle left justification
                    if (width > (int)len && left_justify) {
                        for (int i = 0; i < width - (int)len; i++) {
                            kputchar(' ');
                            count++;
                        }
                    }
                    break;
                }
                
                case 'p': {
                    // Pointer
                    void* ptr = va_arg(args, void*);
                    buffer[0] = '0';
                    buffer[1] = 'x';
                    uitoa((uintptr_t)ptr, buffer + 2, 16, '0', sizeof(uintptr_t) * 2, false);
                    kputs(buffer);
                    count += strlen(buffer);
                    break;
                }
                
                case 'n': {
                    // Write number of characters written so far
                    if (long_long_flag) {
                        int64_t* ptr = va_arg(args, int64_t*);
                        *ptr = count;
                    } else if (long_flag) {
                        long* ptr = va_arg(args, long*);
                        *ptr = count;
                    } else if (short_flag) {
                        short* ptr = va_arg(args, short*);
                        *ptr = count;
                    } else if (char_flag) {
                        char* ptr = va_arg(args, char*);
                        *ptr = count;
                    } else {
                        int* ptr = va_arg(args, int*);
                        *ptr = count;
                    }
                    break;
                }
                
                case '%':
                    // Literal %
                    kputchar('%');
                    count++;
                    break;
                
                default:
                    // Unknown format specifier, print it as is
                    kputchar('%');
                    kputchar(*fmt);
                    count += 2;
                    break;
            }
        } else {
            // Regular character
            kputchar(*fmt);
            count++;
        }
        
        // Move to next character
        fmt++;
    }
    
    return count;
}

/**
 * @brief Format a string and print it to the kernel console
 * 
 * @param fmt Format string
 * @param ... Variable arguments
 * @return Number of characters printed
 */
int kprintf(const char* fmt, ...) {
    va_list args;
    va_start(args, fmt);
    int ret = vkprintf(fmt, args);
    va_end(args);
    return ret;
}

/**
 * @brief Format a string and write it to a buffer
 * 
 * @param buffer Buffer to write to
 * @param size Maximum number of characters to write (including null terminator)
 * @param fmt Format string
 * @param ... Variable arguments
 * @return Number of characters that would have been written (excluding null terminator)
 */
int snprintf(char* buffer, size_t size, const char* fmt, ...) {
    va_list args;
    va_start(args, fmt);
    int ret = vsnprintf(buffer, size, fmt, args);
    va_end(args);
    return ret;
}

/**
 * @brief Format a string and write it to a buffer
 * 
 * @param buffer Buffer to write to
 * @param size Maximum number of characters to write (including null terminator)
 * @param fmt Format string
 * @param args Variable arguments
 * @return Number of characters that would have been written (excluding null terminator)
 */
int vsnprintf(char* buffer, size_t size, const char* fmt, va_list args) {
    if (size == 0) {
        return 0;
    }
    
    size_t pos = 0;
    size_t count = 0;
    char temp_buffer[128];
    
    while (*fmt) {
        if (*fmt == '%') {
            fmt++;
            
            // Handle format specifiers
            bool left_justify = false;
            bool force_sign = false;
            bool space_for_sign = false;
            bool prefix = false;
            bool zero_pad = false;
            int width = 0;
            int precision = -1;
            bool uppercase = false;
            bool long_flag = false;
            bool long_long_flag = false;
            bool short_flag = false;
            bool char_flag = false;
            
            // Parse flags
            while (true) {
                switch (*fmt) {
                    case '-': left_justify = true; fmt++; continue;
                    case '+': force_sign = true; fmt++; continue;
                    case ' ': space_for_sign = true; fmt++; continue;
                    case '#': prefix = true; fmt++; continue;
                    case '0': zero_pad = true; fmt++; continue;
                    default: break;
                }
                break;
            }
            
            // Parse width
            if (*fmt == '*') {
                width = va_arg(args, int);
                if (width < 0) {
                    left_justify = true;
                    width = -width;
                }
                fmt++;
            } else {
                while (*fmt >= '0' && *fmt <= '9') {
                    width = width * 10 + (*fmt - '0');
                    fmt++;
                }
            }
            
            // Parse precision
            if (*fmt == '.') {
                fmt++;
                precision = 0;
                if (*fmt == '*') {
                    precision = va_arg(args, int);
                    if (precision < 0) {
                        precision = -1;
                    }
                    fmt++;
                } else {
                    while (*fmt >= '0' && *fmt <= '9') {
                        precision = precision * 10 + (*fmt - '0');
                        fmt++;
                    }
                }
            }
            
            // Parse length modifiers
            switch (*fmt) {
                case 'h':
                    fmt++;
                    if (*fmt == 'h') { // hh (char)
                        char_flag = true;
                        fmt++;
                    } else { // h (short)
                        short_flag = true;
                    }
                    break;
                case 'l':
                    fmt++;
                    if (*fmt == 'l') { // ll (long long)
                        long_long_flag = true;
                        fmt++;
                    } else { // l (long)
                        long_flag = true;
                    }
                    break;
                case 'z': // size_t
                case 't': // ptrdiff_t
                    long_flag = true;
                    fmt++;
                    break;
            }
            
            // Parse conversion specifier
            switch (*fmt) {
                case 'd':
                case 'i': {
                    // Signed decimal
                    int64_t value;
                    if (long_long_flag) {
                        value = va_arg(args, int64_t);
                    } else if (long_flag) {
                        value = va_arg(args, long);
                    } else if (short_flag) {
                        value = (short)va_arg(args, int);
                    } else if (char_flag) {
                        value = (char)va_arg(args, int);
                    } else {
                        value = va_arg(args, int);
                    }
                    
                    char pad_char = zero_pad ? '0' : ' ';
                    itoa(value, temp_buffer, 10, pad_char, width, false);
                    size_t len = strlen(temp_buffer);
                    
                    for (size_t i = 0; i < len; i++) {
                        if (pos < size - 1) {
                            buffer[pos++] = temp_buffer[i];
                        }
                        count++;
                    }
                    break;
                }
                
                case 'u': {
                    // Unsigned decimal
                    uint64_t value;
                    if (long_long_flag) {
                        value = va_arg(args, uint64_t);
                    } else if (long_flag) {
                        value = va_arg(args, unsigned long);
                    } else if (short_flag) {
                        value = (unsigned short)va_arg(args, unsigned int);
                    } else if (char_flag) {
                        value = (unsigned char)va_arg(args, unsigned int);
                    } else {
                        value = va_arg(args, unsigned int);
                    }
                    
                    char pad_char = zero_pad ? '0' : ' ';
                    uitoa(value, temp_buffer, 10, pad_char, width, false);
                    size_t len = strlen(temp_buffer);
                    
                    for (size_t i = 0; i < len; i++) {
                        if (pos < size - 1) {
                            buffer[pos++] = temp_buffer[i];
                        }
                        count++;
                    }
                    break;
                }
                
                case 'o': {
                    // Octal
                    uint64_t value;
                    if (long_long_flag) {
                        value = va_arg(args, uint64_t);
                    } else if (long_flag) {
                        value = va_arg(args, unsigned long);
                    } else if (short_flag) {
                        value = (unsigned short)va_arg(args, unsigned int);
                    } else if (char_flag) {
                        value = (unsigned char)va_arg(args, unsigned int);
                    } else {
                        value = va_arg(args, unsigned int);
                    }
                    
                    char pad_char = zero_pad ? '0' : ' ';
                    if (prefix && value != 0) {
                        temp_buffer[0] = '0';
                        uitoa(value, temp_buffer + 1, 8, pad_char, width > 0 ? width - 1 : 0, false);
                    } else {
                        uitoa(value, temp_buffer, 8, pad_char, width, false);
                    }
                    size_t len = strlen(temp_buffer);
                    
                    for (size_t i = 0; i < len; i++) {
                        if (pos < size - 1) {
                            buffer[pos++] = temp_buffer[i];
                        }
                        count++;
                    }
                    break;
                }
                
                case 'x':
                case 'X': {
                    // Hexadecimal
                    uppercase = (*fmt == 'X');
                    uint64_t value;
                    if (long_long_flag) {
                        value = va_arg(args, uint64_t);
                    } else if (long_flag) {
                        value = va_arg(args, unsigned long);
                    } else if (short_flag) {
                        value = (unsigned short)va_arg(args, unsigned int);
                    } else if (char_flag) {
                        value = (unsigned char)va_arg(args, unsigned int);
                    } else {
                        value = va_arg(args, unsigned int);
                    }
                    
                    char pad_char = zero_pad ? '0' : ' ';
                    if (prefix && value != 0) {
                        temp_buffer[0] = '0';
                        temp_buffer[1] = uppercase ? 'X' : 'x';
                        uitoa(value, temp_buffer + 2, 16, pad_char, width > 1 ? width - 2 : 0, uppercase);
                    } else {
                        uitoa(value, temp_buffer, 16, pad_char, width, uppercase);
                    }
                    size_t len = strlen(temp_buffer);
                    
                    for (size_t i = 0; i < len; i++) {
                        if (pos < size - 1) {
                            buffer[pos++] = temp_buffer[i];
                        }
                        count++;
                    }
                    break;
                }
                
                case 'c': {
                    // Character
                    int ch = va_arg(args, int);
                    if (pos < size - 1) {
                        buffer[pos++] = ch;
                    }
                    count++;
                    break;
                }
                
                case 's': {
                    // String
                    const char* str = va_arg(args, const char*);
                    if (str == NULL) {
                        str = "(null)";
                    }
                    
                    size_t len = strlen(str);
                    if (precision >= 0 && (size_t)precision < len) {
                        len = precision;
                    }
                    
                    // Handle width if needed
                    if (width > (int)len && !left_justify) {
                        for (int i = 0; i < width - (int)len; i++) {
                            if (pos < size - 1) {
                                buffer[pos++] = ' ';
                            }
                            count++;
                        }
                    }
                    
                    // Output the string
                    for (size_t i = 0; i < len; i++) {
                        if (pos < size - 1) {
                            buffer[pos++] = str[i];
                        }
                        count++;
                    }
                    
                    // Handle left justification
                    if (width > (int)len && left_justify) {
                        for (int i = 0; i < width - (int)len; i++) {
                            if (pos < size - 1) {
                                buffer[pos++] = ' ';
                            }
                            count++;
                        }
                    }
                    break;
                }
                
                case 'p': {
                    // Pointer
                    void* ptr = va_arg(args, void*);
                    temp_buffer[0] = '0';
                    temp_buffer[1] = 'x';
                    uitoa((uintptr_t)ptr, temp_buffer + 2, 16, '0', sizeof(uintptr_t) * 2, false);
                    size_t len = strlen(temp_buffer);
                    
                    for (size_t i = 0; i < len; i++) {
                        if (pos < size - 1) {
                            buffer[pos++] = temp_buffer[i];
                        }
                        count++;
                    }
                    break;
                }
                
                case 'n': {
                    // Write number of characters written so far
                    if (long_long_flag) {
                        int64_t* ptr = va_arg(args, int64_t*);
                        *ptr = count;
                    } else if (long_flag) {
                        long* ptr = va_arg(args, long*);
                        *ptr = count;
                    } else if (short_flag) {
                        short* ptr = va_arg(args, short*);
                        *ptr = count;
                    } else if (char_flag) {
                        char* ptr = va_arg(args, char*);
                        *ptr = count;
                    } else {
                        int* ptr = va_arg(args, int*);
                        *ptr = count;
                    }
                    break;
                }
                
                case '%':
                    // Literal %
                    if (pos < size - 1) {
                        buffer[pos++] = '%';
                    }
                    count++;
                    break;
                
                default:
                    // Unknown format specifier, print it as is
                    if (pos < size - 1) {
                        buffer[pos++] = '%';
                    }
                    count++;
                    
                    if (pos < size - 1) {
                        buffer[pos++] = *fmt;
                    }
                    count++;
                    break;
            }
        } else {
            // Regular character
            if (pos < size - 1) {
                buffer[pos++] = *fmt;
            }
            count++;
        }
        
        // Move to next character
        fmt++;
    }
    
    // Null terminate the buffer
    buffer[pos < size ? pos : size - 1] = '\0';
    
    return count;
}
