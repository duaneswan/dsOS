/**
 * @file string.c
 * @brief String manipulation functions for the kernel
 */

#include "../include/kernel.h"
#include <stddef.h>

/**
 * @brief Copy memory area
 * 
 * @param dest Destination memory area
 * @param src Source memory area
 * @param n Number of bytes to copy
 * @return void* Pointer to destination memory area
 */
void* memcpy(void* dest, const void* src, size_t n) {
    unsigned char* d = (unsigned char*)dest;
    const unsigned char* s = (const unsigned char*)src;
    
    // Handle the case of overlapping memory areas
    if (d == s || n == 0) {
        return dest;
    }
    
    // Copy byte by byte
    while (n--) {
        *d++ = *s++;
    }
    
    return dest;
}

/**
 * @brief Copy memory area with possible overlap
 * 
 * @param dest Destination memory area
 * @param src Source memory area
 * @param n Number of bytes to copy
 * @return void* Pointer to destination memory area
 */
void* memmove(void* dest, const void* src, size_t n) {
    unsigned char* d = (unsigned char*)dest;
    const unsigned char* s = (const unsigned char*)src;
    
    // Handle the case of overlapping memory areas
    if (d == s || n == 0) {
        return dest;
    }
    
    // If dest is after src, copy backwards to avoid overlap issues
    if (d > s && d < s + n) {
        d += n - 1;
        s += n - 1;
        while (n--) {
            *d-- = *s--;
        }
    } else {
        // Otherwise, copy forwards
        while (n--) {
            *d++ = *s++;
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
 * @return void* Pointer to memory area
 */
void* memset(void* s, int c, size_t n) {
    unsigned char* p = (unsigned char*)s;
    
    while (n--) {
        *p++ = (unsigned char)c;
    }
    
    return s;
}

/**
 * @brief Compare two memory areas
 * 
 * @param s1 First memory area
 * @param s2 Second memory area
 * @param n Number of bytes to compare
 * @return int 0 if equal, <0 if s1<s2, >0 if s1>s2
 */
int memcmp(const void* s1, const void* s2, size_t n) {
    const unsigned char* p1 = (const unsigned char*)s1;
    const unsigned char* p2 = (const unsigned char*)s2;
    
    while (n--) {
        if (*p1 != *p2) {
            return *p1 - *p2;
        }
        p1++;
        p2++;
    }
    
    return 0;
}

/**
 * @brief Get the length of a string
 * 
 * @param s String to measure
 * @return size_t Number of characters before the terminating null byte
 */
size_t strlen(const char* s) {
    const char* p = s;
    
    while (*p) {
        p++;
    }
    
    return p - s;
}

/**
 * @brief Copy a string
 * 
 * @param dest Destination string
 * @param src Source string
 * @return char* Pointer to the destination string
 */
char* strcpy(char* dest, const char* src) {
    char* d = dest;
    
    while ((*d++ = *src++) != '\0');
    
    return dest;
}

/**
 * @brief Copy at most n bytes from a string
 * 
 * @param dest Destination string
 * @param src Source string
 * @param n Maximum number of bytes to copy
 * @return char* Pointer to the destination string
 */
char* strncpy(char* dest, const char* src, size_t n) {
    char* d = dest;
    
    // Copy bytes from src
    while (n > 0 && *src) {
        *d++ = *src++;
        n--;
    }
    
    // Fill the rest with zeros
    while (n > 0) {
        *d++ = '\0';
        n--;
    }
    
    return dest;
}

/**
 * @brief Append a string to another
 * 
 * @param dest Destination string
 * @param src Source string
 * @return char* Pointer to the destination string
 */
char* strcat(char* dest, const char* src) {
    char* d = dest;
    
    // Find the end of the destination string
    while (*d) {
        d++;
    }
    
    // Copy the source string
    while ((*d++ = *src++) != '\0');
    
    return dest;
}

/**
 * @brief Append at most n bytes of a string to another
 * 
 * @param dest Destination string
 * @param src Source string
 * @param n Maximum number of bytes to append
 * @return char* Pointer to the destination string
 */
char* strncat(char* dest, const char* src, size_t n) {
    char* d = dest;
    
    // Find the end of the destination string
    while (*d) {
        d++;
    }
    
    // Copy at most n bytes from the source string
    while (n > 0 && *src) {
        *d++ = *src++;
        n--;
    }
    
    // Make sure the string is null-terminated
    *d = '\0';
    
    return dest;
}

/**
 * @brief Compare two strings
 * 
 * @param s1 First string
 * @param s2 Second string
 * @return int 0 if equal, <0 if s1<s2, >0 if s1>s2
 */
int strcmp(const char* s1, const char* s2) {
    while (*s1 && (*s1 == *s2)) {
        s1++;
        s2++;
    }
    
    return *(const unsigned char*)s1 - *(const unsigned char*)s2;
}

/**
 * @brief Compare at most n bytes of two strings
 * 
 * @param s1 First string
 * @param s2 Second string
 * @param n Maximum number of bytes to compare
 * @return int 0 if equal, <0 if s1<s2, >0 if s1>s2
 */
int strncmp(const char* s1, const char* s2, size_t n) {
    if (n == 0) {
        return 0;
    }
    
    while (n-- > 0 && *s1 && (*s1 == *s2)) {
        s1++;
        s2++;
    }
    
    if (n == (size_t)-1) {
        return 0;
    }
    
    return *(const unsigned char*)s1 - *(const unsigned char*)s2;
}

/**
 * @brief Find the first occurrence of a character in a string
 * 
 * @param s String to search
 * @param c Character to find
 * @return char* Pointer to the character, or NULL if not found
 */
char* strchr(const char* s, int c) {
    while (*s) {
        if (*s == (char)c) {
            return (char*)s;
        }
        s++;
    }
    
    // Check for terminating null byte if c is '\0'
    if ((char)c == '\0') {
        return (char*)s;
    }
    
    return NULL;
}

/**
 * @brief Find the last occurrence of a character in a string
 * 
 * @param s String to search
 * @param c Character to find
 * @return char* Pointer to the character, or NULL if not found
 */
char* strrchr(const char* s, int c) {
    const char* found = NULL;
    
    while (*s) {
        if (*s == (char)c) {
            found = s;
        }
        s++;
    }
    
    // Check for terminating null byte if c is '\0'
    if ((char)c == '\0') {
        return (char*)s;
    }
    
    return (char*)found;
}

/**
 * @brief Find the first occurrence of a substring in a string
 * 
 * @param haystack String to search in
 * @param needle Substring to find
 * @return char* Pointer to the beginning of the found substring, or NULL if not found
 */
char* strstr(const char* haystack, const char* needle) {
    size_t needle_len = strlen(needle);
    
    // Empty needle always matches
    if (needle_len == 0) {
        return (char*)haystack;
    }
    
    // Search for the first occurrence of needle in haystack
    while (*haystack) {
        if (*haystack == *needle && strncmp(haystack, needle, needle_len) == 0) {
            return (char*)haystack;
        }
        haystack++;
    }
    
    return NULL;
}

/**
 * @brief Convert a string to uppercase
 * 
 * @param s String to convert
 * @return char* Pointer to the converted string
 */
char* strupr(char* s) {
    char* p = s;
    
    while (*p) {
        if (*p >= 'a' && *p <= 'z') {
            *p -= ('a' - 'A');
        }
        p++;
    }
    
    return s;
}

/**
 * @brief Convert a string to lowercase
 * 
 * @param s String to convert
 * @return char* Pointer to the converted string
 */
char* strlwr(char* s) {
    char* p = s;
    
    while (*p) {
        if (*p >= 'A' && *p <= 'Z') {
            *p += ('a' - 'A');
        }
        p++;
    }
    
    return s;
}

/**
 * @brief Duplicate a string
 * 
 * @param s String to duplicate
 * @return char* Newly allocated copy of the string, or NULL if allocation fails
 */
char* strdup(const char* s) {
    size_t len = strlen(s) + 1; // Include null terminator
    char* new_str = kmalloc(len);
    
    if (new_str) {
        return memcpy(new_str, s, len);
    }
    
    return NULL;
}
