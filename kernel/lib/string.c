/**
 * @file string.c
 * @brief String manipulation functions
 */

#include "../include/kernel.h"
#include <stdint.h>
#include <stddef.h>

/**
 * @brief Copy a block of memory
 * 
 * @param dest Destination pointer
 * @param src Source pointer
 * @param n Number of bytes to copy
 * @return Pointer to destination
 */
void* memcpy(void* dest, const void* src, size_t n) {
    uint8_t* d = (uint8_t*)dest;
    const uint8_t* s = (const uint8_t*)src;
    
    // Check for overlapping memory regions
    if (d == s || n == 0) {
        return dest;
    }
    
    // Handle basic case - non-overlapping memory regions
    if (d < s || d >= (s + n)) {
        // Copy forward
        for (size_t i = 0; i < n; i++) {
            d[i] = s[i];
        }
    } else {
        // Copy backward to handle overlapping regions
        for (size_t i = n; i > 0; i--) {
            d[i-1] = s[i-1];
        }
    }
    
    return dest;
}

/**
 * @brief Move a block of memory, handling overlapping regions
 * 
 * @param dest Destination pointer
 * @param src Source pointer
 * @param n Number of bytes to move
 * @return Pointer to destination
 */
void* memmove(void* dest, const void* src, size_t n) {
    uint8_t* d = (uint8_t*)dest;
    const uint8_t* s = (const uint8_t*)src;
    
    // Check for overlapping memory regions
    if (d == s || n == 0) {
        return dest;
    }
    
    // Handle non-overlapping memory regions (optimized path)
    if (d < s || d >= (s + n)) {
        // Copy forward
        for (size_t i = 0; i < n; i++) {
            d[i] = s[i];
        }
    } else {
        // Copy backward to handle overlapping regions
        for (size_t i = n; i > 0; i--) {
            d[i-1] = s[i-1];
        }
    }
    
    return dest;
}

/**
 * @brief Set a block of memory to a specific value
 * 
 * @param s Pointer to the memory block
 * @param c Value to set (converted to unsigned char)
 * @param n Number of bytes to set
 * @return Pointer to the memory block
 */
void* memset(void* s, int c, size_t n) {
    uint8_t* p = (uint8_t*)s;
    uint8_t value = (uint8_t)c;
    
    for (size_t i = 0; i < n; i++) {
        p[i] = value;
    }
    
    return s;
}

/**
 * @brief Compare two memory blocks
 * 
 * @param s1 First memory block
 * @param s2 Second memory block
 * @param n Number of bytes to compare
 * @return < 0 if s1 < s2, 0 if s1 == s2, > 0 if s1 > s2
 */
int memcmp(const void* s1, const void* s2, size_t n) {
    const uint8_t* p1 = (const uint8_t*)s1;
    const uint8_t* p2 = (const uint8_t*)s2;
    
    for (size_t i = 0; i < n; i++) {
        if (p1[i] != p2[i]) {
            return p1[i] - p2[i];
        }
    }
    
    return 0;
}

/**
 * @brief Get the length of a null-terminated string
 * 
 * @param s String pointer
 * @return Number of characters in the string (excluding null terminator)
 */
size_t strlen(const char* s) {
    size_t len = 0;
    
    while (s[len] != '\0') {
        len++;
    }
    
    return len;
}

/**
 * @brief Copy a null-terminated string
 * 
 * @param dest Destination string pointer
 * @param src Source string pointer
 * @return Pointer to the destination string
 */
char* strcpy(char* dest, const char* src) {
    size_t i = 0;
    
    while (src[i] != '\0') {
        dest[i] = src[i];
        i++;
    }
    
    // Copy null terminator
    dest[i] = '\0';
    
    return dest;
}

/**
 * @brief Copy at most n characters from a null-terminated string
 * 
 * @param dest Destination string pointer
 * @param src Source string pointer
 * @param n Maximum number of characters to copy
 * @return Pointer to the destination string
 */
char* strncpy(char* dest, const char* src, size_t n) {
    size_t i;
    
    // Copy up to n characters
    for (i = 0; i < n && src[i] != '\0'; i++) {
        dest[i] = src[i];
    }
    
    // Fill remaining characters with null bytes
    for (; i < n; i++) {
        dest[i] = '\0';
    }
    
    return dest;
}

/**
 * @brief Compare two null-terminated strings
 * 
 * @param s1 First string
 * @param s2 Second string
 * @return < 0 if s1 < s2, 0 if s1 == s2, > 0 if s1 > s2
 */
int strcmp(const char* s1, const char* s2) {
    while (*s1 && *s2 && *s1 == *s2) {
        s1++;
        s2++;
    }
    
    return (int)((unsigned char)*s1 - (unsigned char)*s2);
}

/**
 * @brief Compare at most n characters from two null-terminated strings
 * 
 * @param s1 First string
 * @param s2 Second string
 * @param n Maximum number of characters to compare
 * @return < 0 if s1 < s2, 0 if s1 == s2, > 0 if s1 > s2
 */
int strncmp(const char* s1, const char* s2, size_t n) {
    if (n == 0) {
        return 0;
    }
    
    while (--n && *s1 && *s1 == *s2) {
        s1++;
        s2++;
    }
    
    return (int)((unsigned char)*s1 - (unsigned char)*s2);
}

/**
 * @brief Concatenate two null-terminated strings
 * 
 * @param dest Destination string pointer
 * @param src Source string pointer
 * @return Pointer to the destination string
 */
char* strcat(char* dest, const char* src) {
    size_t dest_len = strlen(dest);
    size_t i = 0;
    
    while (src[i] != '\0') {
        dest[dest_len + i] = src[i];
        i++;
    }
    
    // Add null terminator
    dest[dest_len + i] = '\0';
    
    return dest;
}

/**
 * @brief Concatenate at most n characters from a null-terminated string
 * 
 * @param dest Destination string pointer
 * @param src Source string pointer
 * @param n Maximum number of characters to concatenate
 * @return Pointer to the destination string
 */
char* strncat(char* dest, const char* src, size_t n) {
    size_t dest_len = strlen(dest);
    size_t i;
    
    // Copy up to n characters
    for (i = 0; i < n && src[i] != '\0'; i++) {
        dest[dest_len + i] = src[i];
    }
    
    // Add null terminator
    dest[dest_len + i] = '\0';
    
    return dest;
}

/**
 * @brief Find the first occurrence of a character in a null-terminated string
 * 
 * @param s String pointer
 * @param c Character to find
 * @return Pointer to the found character, or NULL if not found
 */
char* strchr(const char* s, int c) {
    while (*s != '\0') {
        if (*s == (char)c) {
            return (char*)s;
        }
        s++;
    }
    
    if ((char)c == '\0') {
        return (char*)s;
    }
    
    return NULL;
}

/**
 * @brief Find the last occurrence of a character in a null-terminated string
 * 
 * @param s String pointer
 * @param c Character to find
 * @return Pointer to the found character, or NULL if not found
 */
char* strrchr(const char* s, int c) {
    const char* last = NULL;
    
    do {
        if (*s == (char)c) {
            last = s;
        }
    } while (*s++);
    
    return (char*)last;
}

/**
 * @brief Find the first occurrence of a substring in a null-terminated string
 * 
 * @param haystack String to search in
 * @param needle Substring to find
 * @return Pointer to the found substring, or NULL if not found
 */
char* strstr(const char* haystack, const char* needle) {
    if (*needle == '\0') {
        return (char*)haystack;
    }
    
    size_t needle_len = strlen(needle);
    
    while (*haystack) {
        if (*haystack == *needle && strncmp(haystack, needle, needle_len) == 0) {
            return (char*)haystack;
        }
        haystack++;
    }
    
    return NULL;
}
