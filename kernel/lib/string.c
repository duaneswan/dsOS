/**
 * @file string.c
 * @brief String manipulation functions
 */

#include "../include/kernel.h"
#include <stdint.h>
#include <stddef.h>

/**
 * @brief Copy memory area
 * 
 * @param dest Destination memory area
 * @param src Source memory area
 * @param n Number of bytes to copy
 * @return Pointer to destination memory area
 */
void* memcpy(void* dest, const void* src, size_t n) {
    uint8_t* d = (uint8_t*)dest;
    const uint8_t* s = (const uint8_t*)src;
    
    // Check for NULL pointers
    if (dest == NULL || src == NULL) {
        return dest;
    }
    
    // Copy byte by byte
    for (size_t i = 0; i < n; i++) {
        d[i] = s[i];
    }
    
    return dest;
}

/**
 * @brief Copy memory area, handling overlap
 * 
 * @param dest Destination memory area
 * @param src Source memory area
 * @param n Number of bytes to copy
 * @return Pointer to destination memory area
 */
void* memmove(void* dest, const void* src, size_t n) {
    uint8_t* d = (uint8_t*)dest;
    const uint8_t* s = (const uint8_t*)src;
    
    // Check for NULL pointers
    if (dest == NULL || src == NULL) {
        return dest;
    }
    
    // If destination is before source, copy forward
    if (d < s) {
        for (size_t i = 0; i < n; i++) {
            d[i] = s[i];
        }
    }
    // If destination is after source, copy backward to handle overlap
    else if (d > s) {
        for (size_t i = n; i > 0; i--) {
            d[i - 1] = s[i - 1];
        }
    }
    
    return dest;
}

/**
 * @brief Fill memory with a constant byte
 * 
 * @param s Memory area to fill
 * @param c Byte to fill with
 * @param n Number of bytes to fill
 * @return Pointer to memory area
 */
void* memset(void* s, int c, size_t n) {
    uint8_t* p = (uint8_t*)s;
    
    // Check for NULL pointer
    if (s == NULL) {
        return s;
    }
    
    // Fill byte by byte
    for (size_t i = 0; i < n; i++) {
        p[i] = (uint8_t)c;
    }
    
    return s;
}

/**
 * @brief Compare memory areas
 * 
 * @param s1 First memory area
 * @param s2 Second memory area
 * @param n Number of bytes to compare
 * @return < 0 if s1 < s2, 0 if s1 == s2, > 0 if s1 > s2
 */
int memcmp(const void* s1, const void* s2, size_t n) {
    const uint8_t* p1 = (const uint8_t*)s1;
    const uint8_t* p2 = (const uint8_t*)s2;
    
    // Check for NULL pointers
    if (s1 == NULL || s2 == NULL) {
        return (s1 == s2) ? 0 : ((s1 == NULL) ? -1 : 1);
    }
    
    // Compare byte by byte
    for (size_t i = 0; i < n; i++) {
        if (p1[i] != p2[i]) {
            return p1[i] - p2[i];
        }
    }
    
    return 0;
}

/**
 * @brief Calculate the length of a string
 * 
 * @param s String to measure
 * @return Length of the string
 */
size_t strlen(const char* s) {
    const char* p = s;
    
    // Check for NULL pointer
    if (s == NULL) {
        return 0;
    }
    
    // Count characters until null terminator
    while (*p) {
        p++;
    }
    
    return (size_t)(p - s);
}

/**
 * @brief Copy a string
 * 
 * @param dest Destination string
 * @param src Source string
 * @return Pointer to destination string
 */
char* strcpy(char* dest, const char* src) {
    char* d = dest;
    
    // Check for NULL pointers
    if (dest == NULL || src == NULL) {
        return dest;
    }
    
    // Copy characters until null terminator
    while ((*d++ = *src++)) {}
    
    return dest;
}

/**
 * @brief Copy a string with length limit
 * 
 * @param dest Destination string
 * @param src Source string
 * @param n Maximum number of characters to copy
 * @return Pointer to destination string
 */
char* strncpy(char* dest, const char* src, size_t n) {
    size_t i;
    
    // Check for NULL pointers
    if (dest == NULL || src == NULL) {
        return dest;
    }
    
    // Copy characters until null terminator or limit
    for (i = 0; i < n && src[i]; i++) {
        dest[i] = src[i];
    }
    
    // Pad with null terminators if needed
    for (; i < n; i++) {
        dest[i] = '\0';
    }
    
    return dest;
}

/**
 * @brief Concatenate strings
 * 
 * @param dest Destination string
 * @param src Source string
 * @return Pointer to destination string
 */
char* strcat(char* dest, const char* src) {
    char* d = dest;
    
    // Check for NULL pointers
    if (dest == NULL || src == NULL) {
        return dest;
    }
    
    // Find the end of the destination string
    while (*d) {
        d++;
    }
    
    // Copy source string to the end of destination
    strcpy(d, src);
    
    return dest;
}

/**
 * @brief Concatenate strings with length limit
 * 
 * @param dest Destination string
 * @param src Source string
 * @param n Maximum number of characters to append
 * @return Pointer to destination string
 */
char* strncat(char* dest, const char* src, size_t n) {
    char* d = dest;
    size_t i;
    
    // Check for NULL pointers
    if (dest == NULL || src == NULL) {
        return dest;
    }
    
    // Find the end of the destination string
    while (*d) {
        d++;
    }
    
    // Copy source string to the end of destination with limit
    for (i = 0; i < n && src[i]; i++) {
        d[i] = src[i];
    }
    
    // Ensure null termination
    d[i] = '\0';
    
    return dest;
}

/**
 * @brief Compare strings
 * 
 * @param s1 First string
 * @param s2 Second string
 * @return < 0 if s1 < s2, 0 if s1 == s2, > 0 if s1 > s2
 */
int strcmp(const char* s1, const char* s2) {
    // Check for NULL pointers
    if (s1 == NULL || s2 == NULL) {
        return (s1 == s2) ? 0 : ((s1 == NULL) ? -1 : 1);
    }
    
    // Compare characters until mismatch or null terminator
    while (*s1 && (*s1 == *s2)) {
        s1++;
        s2++;
    }
    
    return *(const unsigned char*)s1 - *(const unsigned char*)s2;
}

/**
 * @brief Compare strings with length limit
 * 
 * @param s1 First string
 * @param s2 Second string
 * @param n Maximum number of characters to compare
 * @return < 0 if s1 < s2, 0 if s1 == s2, > 0 if s1 > s2
 */
int strncmp(const char* s1, const char* s2, size_t n) {
    // Check for NULL pointers
    if (s1 == NULL || s2 == NULL) {
        return (s1 == s2) ? 0 : ((s1 == NULL) ? -1 : 1);
    }
    
    // Special case for n=0
    if (n == 0) {
        return 0;
    }
    
    // Compare characters until mismatch, null terminator, or limit
    while (--n && *s1 && (*s1 == *s2)) {
        s1++;
        s2++;
    }
    
    return *(const unsigned char*)s1 - *(const unsigned char*)s2;
}

/**
 * @brief Find first occurrence of character in string
 * 
 * @param s String to search
 * @param c Character to find
 * @return Pointer to first occurrence, or NULL if not found
 */
char* strchr(const char* s, int c) {
    // Check for NULL pointer
    if (s == NULL) {
        return NULL;
    }
    
    // Search for character until null terminator
    while (*s && *s != (char)c) {
        s++;
    }
    
    // Return pointer if found, NULL otherwise
    return (*s == (char)c) ? (char*)s : NULL;
}

/**
 * @brief Find last occurrence of character in string
 * 
 * @param s String to search
 * @param c Character to find
 * @return Pointer to last occurrence, or NULL if not found
 */
char* strrchr(const char* s, int c) {
    const char* last = NULL;
    
    // Check for NULL pointer
    if (s == NULL) {
        return NULL;
    }
    
    // Search for character until null terminator
    while (*s) {
        if (*s == (char)c) {
            last = s;
        }
        s++;
    }
    
    // Check for character in null terminator
    if ((char)c == '\0') {
        return (char*)s;
    }
    
    // Return pointer if found, NULL otherwise
    return (char*)last;
}

/**
 * @brief Find substring in string
 * 
 * @param haystack String to search in
 * @param needle Substring to find
 * @return Pointer to first occurrence, or NULL if not found
 */
char* strstr(const char* haystack, const char* needle) {
    size_t needle_len;
    
    // Check for NULL pointers
    if (haystack == NULL || needle == NULL) {
        return NULL;
    }
    
    // Special case for empty needle
    if (*needle == '\0') {
        return (char*)haystack;
    }
    
    // Get needle length
    needle_len = strlen(needle);
    
    // Search for needle in haystack
    while (*haystack) {
        if (strncmp(haystack, needle, needle_len) == 0) {
            return (char*)haystack;
        }
        haystack++;
    }
    
    return NULL;
}

/**
 * @brief Convert string to uppercase
 * 
 * @param s String to convert
 * @return Pointer to converted string
 */
char* strupr(char* s) {
    char* p = s;
    
    // Check for NULL pointer
    if (s == NULL) {
        return NULL;
    }
    
    // Convert each character to uppercase
    while (*p) {
        if (*p >= 'a' && *p <= 'z') {
            *p = *p - 'a' + 'A';
        }
        p++;
    }
    
    return s;
}

/**
 * @brief Convert string to lowercase
 * 
 * @param s String to convert
 * @return Pointer to converted string
 */
char* strlwr(char* s) {
    char* p = s;
    
    // Check for NULL pointer
    if (s == NULL) {
        return NULL;
    }
    
    // Convert each character to lowercase
    while (*p) {
        if (*p >= 'A' && *p <= 'Z') {
            *p = *p - 'A' + 'a';
        }
        p++;
    }
    
    return s;
}

/**
 * @brief Duplicate a string
 * 
 * @param s String to duplicate
 * @return Pointer to new string, or NULL if allocation fails
 */
char* strdup(const char* s) {
    size_t len;
    char* dup;
    
    // Check for NULL pointer
    if (s == NULL) {
        return NULL;
    }
    
    // Get string length
    len = strlen(s) + 1;
    
    // Allocate memory for duplicate
    dup = (char*)kmalloc(len);
    if (dup == NULL) {
        return NULL;
    }
    
    // Copy string to duplicate
    memcpy(dup, s, len);
    
    return dup;
}
