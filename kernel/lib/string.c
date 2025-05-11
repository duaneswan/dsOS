/**
 * @file string.c
 * @brief String manipulation functions
 */

#include "../include/kernel.h"
#include <stddef.h>
#include <stdbool.h>

/**
 * @brief Copy memory area
 * 
 * @param dest Destination buffer
 * @param src Source buffer
 * @param n Number of bytes to copy
 * @return Pointer to destination buffer
 */
void* memcpy(void* dest, const void* src, size_t n) {
    unsigned char* d = (unsigned char*)dest;
    const unsigned char* s = (const unsigned char*)src;
    
    // Fast copy for larger blocks (word-aligned)
    if (n >= 4 && 
        ((uintptr_t)d % 4 == 0) && 
        ((uintptr_t)s % 4 == 0)) {
        
        // Copy 4 bytes at a time for aligned regions
        size_t words = n / 4;
        uint32_t* d32 = (uint32_t*)d;
        const uint32_t* s32 = (const uint32_t*)s;
        
        for (size_t i = 0; i < words; i++) {
            *d32++ = *s32++;
        }
        
        // Update pointers to handle remaining bytes
        d = (unsigned char*)d32;
        s = (const unsigned char*)s32;
        n %= 4;
    }
    
    // Copy remaining bytes or handle unaligned data
    for (size_t i = 0; i < n; i++) {
        *d++ = *s++;
    }
    
    return dest;
}

/**
 * @brief Copy memory area, handling overlaps
 * 
 * @param dest Destination buffer
 * @param src Source buffer
 * @param n Number of bytes to copy
 * @return Pointer to destination buffer
 */
void* memmove(void* dest, const void* src, size_t n) {
    unsigned char* d = (unsigned char*)dest;
    const unsigned char* s = (const unsigned char*)src;
    
    // Check for overlap
    if (d == s || n == 0) {
        return dest;  // Nothing to do
    }
    
    // Copy backwards if destination is after source and overlapping
    if (d > s && d < s + n) {
        // Copy from end to avoid overwriting source data
        for (size_t i = n; i > 0; i--) {
            d[i-1] = s[i-1];
        }
    } else {
        // Normal copy (non-overlapping or dest before src)
        for (size_t i = 0; i < n; i++) {
            d[i] = s[i];
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
 * @return Pointer to filled memory area
 */
void* memset(void* s, int c, size_t n) {
    unsigned char* p = (unsigned char*)s;
    unsigned char byte = (unsigned char)c;
    
    // Fast fill for larger blocks (word-aligned)
    if (n >= 4 && ((uintptr_t)p % 4 == 0)) {
        // Create a word with the byte pattern repeated
        uint32_t word = byte | (byte << 8) | (byte << 16) | (byte << 24);
        uint32_t* p32 = (uint32_t*)p;
        
        // Fill 4 bytes at a time
        size_t words = n / 4;
        for (size_t i = 0; i < words; i++) {
            *p32++ = word;
        }
        
        // Update pointer to handle remaining bytes
        p = (unsigned char*)p32;
        n %= 4;
    }
    
    // Fill remaining bytes or handle unaligned data
    for (size_t i = 0; i < n; i++) {
        *p++ = byte;
    }
    
    return s;
}

/**
 * @brief Compare two memory areas
 * 
 * @param s1 First memory area
 * @param s2 Second memory area
 * @param n Number of bytes to compare
 * @return 0 if equal, negative if s1 < s2, positive if s1 > s2
 */
int memcmp(const void* s1, const void* s2, size_t n) {
    const unsigned char* p1 = (const unsigned char*)s1;
    const unsigned char* p2 = (const unsigned char*)s2;
    
    for (size_t i = 0; i < n; i++) {
        if (p1[i] != p2[i]) {
            return p1[i] - p2[i];
        }
    }
    
    return 0;  // Equal
}

/**
 * @brief Locate a byte in a memory area
 * 
 * @param s Memory area to search
 * @param c Byte to search for
 * @param n Number of bytes to search
 * @return Pointer to the byte, or NULL if not found
 */
void* memchr(const void* s, int c, size_t n) {
    const unsigned char* p = (const unsigned char*)s;
    unsigned char byte = (unsigned char)c;
    
    for (size_t i = 0; i < n; i++) {
        if (p[i] == byte) {
            return (void*)(p + i);
        }
    }
    
    return NULL;  // Not found
}

/**
 * @brief Get the length of a string
 * 
 * @param s String to measure
 * @return Length of string
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
 * @param dest Destination buffer
 * @param src Source string
 * @return Pointer to destination buffer
 */
char* strcpy(char* dest, const char* src) {
    char* d = dest;
    
    while ((*d++ = *src++));
    
    return dest;
}

/**
 * @brief Copy a string with length limit
 * 
 * @param dest Destination buffer
 * @param src Source string
 * @param n Maximum number of bytes to copy
 * @return Pointer to destination buffer
 */
char* strncpy(char* dest, const char* src, size_t n) {
    char* d = dest;
    size_t i;
    
    // Copy from src
    for (i = 0; i < n && src[i] != '\0'; i++) {
        d[i] = src[i];
    }
    
    // Pad with zeros
    for (; i < n; i++) {
        d[i] = '\0';
    }
    
    return dest;
}

/**
 * @brief Concatenate two strings
 * 
 * @param dest Destination buffer
 * @param src Source string
 * @return Pointer to destination buffer
 */
char* strcat(char* dest, const char* src) {
    char* d = dest;
    
    // Find the end of dest
    while (*d) {
        d++;
    }
    
    // Copy src to the end of dest
    while ((*d++ = *src++));
    
    return dest;
}

/**
 * @brief Concatenate two strings with length limit
 * 
 * @param dest Destination buffer
 * @param src Source string
 * @param n Maximum number of bytes to copy from src
 * @return Pointer to destination buffer
 */
char* strncat(char* dest, const char* src, size_t n) {
    char* d = dest;
    
    // Find the end of dest
    while (*d) {
        d++;
    }
    
    // Copy at most n bytes from src
    size_t i;
    for (i = 0; i < n && src[i] != '\0'; i++) {
        d[i] = src[i];
    }
    
    // Ensure null termination
    d[i] = '\0';
    
    return dest;
}

/**
 * @brief Compare two strings
 * 
 * @param s1 First string
 * @param s2 Second string
 * @return 0 if equal, negative if s1 < s2, positive if s1 > s2
 */
int strcmp(const char* s1, const char* s2) {
    while (*s1 && (*s1 == *s2)) {
        s1++;
        s2++;
    }
    
    return (unsigned char)*s1 - (unsigned char)*s2;
}

/**
 * @brief Compare two strings with length limit
 * 
 * @param s1 First string
 * @param s2 Second string
 * @param n Maximum number of bytes to compare
 * @return 0 if equal, negative if s1 < s2, positive if s1 > s2
 */
int strncmp(const char* s1, const char* s2, size_t n) {
    if (n == 0) {
        return 0;
    }
    
    while (--n && *s1 && (*s1 == *s2)) {
        s1++;
        s2++;
    }
    
    return (unsigned char)*s1 - (unsigned char)*s2;
}

/**
 * @brief Locate character in a string
 * 
 * @param s String to search
 * @param c Character to search for
 * @return Pointer to the character, or NULL if not found
 */
char* strchr(const char* s, int c) {
    char ch = (char)c;
    
    while (*s && *s != ch) {
        s++;
    }
    
    return (*s == ch) ? (char*)s : NULL;
}

/**
 * @brief Locate character in a string, searching from the end
 * 
 * @param s String to search
 * @param c Character to search for
 * @return Pointer to the character, or NULL if not found
 */
char* strrchr(const char* s, int c) {
    char ch = (char)c;
    const char* found = NULL;
    
    while (*s) {
        if (*s == ch) {
            found = s;
        }
        s++;
    }
    
    // Check for NULL terminator case
    if (ch == '\0') {
        found = s;
    }
    
    return (char*)found;
}

/**
 * @brief Locate a substring
 * 
 * @param haystack String to search in
 * @param needle Substring to search for
 * @return Pointer to the beginning of needle in haystack, or NULL if not found
 */
char* strstr(const char* haystack, const char* needle) {
    // Empty needle edge case
    if (*needle == '\0') {
        return (char*)haystack;
    }
    
    // For each character in haystack
    while (*haystack) {
        // Check if this could be a match
        if (*haystack == *needle) {
            const char* h = haystack;
            const char* n = needle;
            
            // Check if the rest matches
            do {
                // Reached end of needle - match found
                if (*++n == '\0') {
                    return (char*)haystack;
                }
                
                // Reached end of haystack - no match
                if (*++h == '\0') {
                    return NULL;
                }
                
            } while (*h == *n);
        }
        
        haystack++;
    }
    
    return NULL;  // No match found
}

/**
 * @brief Convert string to uppercase
 * 
 * @param s String to convert
 * @return Pointer to the string
 */
char* strupr(char* s) {
    char* p = s;
    
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
 * @return Pointer to the string
 */
char* strlwr(char* s) {
    char* p = s;
    
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
 * @return Pointer to new copy of the string, or NULL if allocation fails
 */
char* strdup(const char* s) {
    size_t len = strlen(s) + 1;  // Include null terminator
    char* new_str = kmalloc(len);
    
    if (new_str == NULL) {
        return NULL;
    }
    
    return memcpy(new_str, s, len);
}

/**
 * @brief Extract token from string
 * 
 * @param str String to tokenize, or NULL to continue from previous call
 * @param delim String of delimiter characters
 * @param saveptr Pointer to save tokenizing state
 * @return Pointer to next token, or NULL if no more tokens
 */
char* strtok_r(char* str, const char* delim, char** saveptr) {
    char* token;
    
    if (str == NULL) {
        str = *saveptr;
    }
    
    // Skip leading delimiters
    str += strspn(str, delim);
    if (*str == '\0') {
        *saveptr = str;
        return NULL;
    }
    
    // Find end of token
    token = str;
    str = strpbrk(token, delim);
    if (str == NULL) {
        // No more delimiters
        *saveptr = strchr(token, '\0');
    } else {
        // Terminate token and update saveptr
        *str = '\0';
        *saveptr = str + 1;
    }
    
    return token;
}

/**
 * @brief Simplified strtok implementation
 * 
 * @param str String to tokenize, or NULL to continue from previous call
 * @param delim String of delimiter characters
 * @return Pointer to next token, or NULL if no more tokens
 */
char* strtok(char* str, const char* delim) {
    static char* last;
    return strtok_r(str, delim, &last);
}

/**
 * @brief Get length of a string segment with characters not in a set
 * 
 * @param str String to check
 * @param accept Set of allowed characters
 * @return Length of initial segment not containing any characters from reject
 */
size_t strcspn(const char* str, const char* reject) {
    size_t count = 0;
    
    while (*str) {
        if (strchr(reject, *str)) {
            return count;
        }
        str++;
        count++;
    }
    
    return count;
}

/**
 * @brief Get length of a string segment with characters only in a set
 * 
 * @param str String to check
 * @param accept Set of allowed characters
 * @return Length of initial segment containing only characters from accept
 */
size_t strspn(const char* str, const char* accept) {
    size_t count = 0;
    
    while (*str && strchr(accept, *str)) {
        str++;
        count++;
    }
    
    return count;
}

/**
 * @brief Find first occurrence of any character from a set
 * 
 * @param str String to search
 * @param accept Set of characters to search for
 * @return Pointer to first matching character, or NULL if none found
 */
char* strpbrk(const char* str, const char* accept) {
    while (*str) {
        if (strchr(accept, *str)) {
            return (char*)str;
        }
        str++;
    }
    
    return NULL;
}
