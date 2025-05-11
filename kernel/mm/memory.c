/**
 * @file memory.c
 * @brief Physical memory manager implementation
 */

#include "../include/kernel.h"
#include "../include/memory.h"
#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>

// Bitmap to track allocated physical pages
// Each bit represents one physical page
// 1 = allocated, 0 = free
static uint64_t* physical_bitmap = NULL;
static uint64_t bitmap_size = 0;     // Size in uint64_t units
static uint64_t total_pages = 0;     // Total number of physical pages
static uint64_t free_pages = 0;      // Number of free physical pages
static uint64_t total_memory = 0;    // Total physical memory in bytes

// Boot memory allocator state
static uintptr_t boot_alloc_next = 0;
static uintptr_t boot_alloc_end = 0;

// Forward declarations
static void init_physical_bitmap(uintptr_t mem_upper);
static uintptr_t boot_allocate(size_t size, size_t align);

/**
 * @brief Initialize the physical memory manager
 * 
 * @param mem_upper Upper memory size in bytes (from bootloader or BIOS)
 */
void mm_init(uintptr_t mem_upper) {
    // Initialize total memory size
    total_memory = mem_upper;
    
    // Calculate total number of pages
    total_pages = mem_upper / PAGE_SIZE;
    free_pages = total_pages;
    
    // Setup boot allocator for initial allocations
    // Start at 1 MiB (conventional memory end)
    boot_alloc_next = 0x100000;
    boot_alloc_end = mem_upper;
    
    // Initialize the physical bitmap
    init_physical_bitmap(mem_upper);
    
    // Reserve memory up to 1 MiB (conventional memory)
    uint64_t pages_to_reserve = 0x100000 / PAGE_SIZE;
    for (uint64_t i = 0; i < pages_to_reserve; i++) {
        physical_bitmap[i / 64] |= (1ULL << (i % 64));
    }
    free_pages -= pages_to_reserve;
    
    // Reserve memory used by the bitmap itself
    uintptr_t bitmap_start = (uintptr_t)physical_bitmap;
    uintptr_t bitmap_end = bitmap_start + (bitmap_size * sizeof(uint64_t));
    
    for (uintptr_t addr = bitmap_start; addr < bitmap_end; addr += PAGE_SIZE) {
        uint64_t page_num = addr / PAGE_SIZE;
        physical_bitmap[page_num / 64] |= (1ULL << (page_num % 64));
        free_pages--;
    }
    
    kprintf("MM: Physical memory manager initialized\n");
    kprintf("MM: Total memory: %llu MB\n", total_memory / (1024 * 1024));
    kprintf("MM: Total pages: %llu\n", total_pages);
    kprintf("MM: Free pages: %llu\n", free_pages);
}

/**
 * @brief Initialize the physical memory bitmap
 * 
 * @param mem_upper Upper memory size in bytes
 */
static void init_physical_bitmap(uintptr_t mem_upper) {
    // Calculate bitmap size
    bitmap_size = (total_pages + 63) / 64; // Round up to 64-bit units
    
    // Allocate bitmap memory
    physical_bitmap = (uint64_t*)boot_allocate(bitmap_size * sizeof(uint64_t), sizeof(uint64_t));
    
    // Initially mark all pages as free (0)
    for (uint64_t i = 0; i < bitmap_size; i++) {
        physical_bitmap[i] = 0;
    }
}

/**
 * @brief Allocate memory from boot allocator
 * 
 * @param size Size to allocate in bytes
 * @param align Alignment (must be a power of 2)
 * @return Physical address of the allocated memory
 */
static uintptr_t boot_allocate(size_t size, size_t align) {
    // Align address upwards
    uintptr_t addr = (boot_alloc_next + align - 1) & ~(align - 1);
    
    // Check for out of memory
    if (addr + size > boot_alloc_end) {
        panic(PANIC_NORMAL, "Out of memory in boot allocator", __FILE__, __LINE__);
    }
    
    // Update next address
    boot_alloc_next = addr + size;
    
    return addr;
}

/**
 * @brief Set a bit in the physical bitmap
 * 
 * @param page_num Page number to set
 */
static inline void bitmap_set(uint64_t page_num) {
    if (page_num < total_pages) {
        physical_bitmap[page_num / 64] |= (1ULL << (page_num % 64));
    }
}

/**
 * @brief Clear a bit in the physical bitmap
 * 
 * @param page_num Page number to clear
 */
static inline void bitmap_clear(uint64_t page_num) {
    if (page_num < total_pages) {
        physical_bitmap[page_num / 64] &= ~(1ULL << (page_num % 64));
    }
}

/**
 * @brief Test a bit in the physical bitmap
 * 
 * @param page_num Page number to test
 * @return true if the bit is set, false otherwise
 */
static inline bool bitmap_test(uint64_t page_num) {
    if (page_num < total_pages) {
        return (physical_bitmap[page_num / 64] & (1ULL << (page_num % 64))) != 0;
    }
    return true; // Out of range is considered allocated
}

/**
 * @brief Allocate a physical page
 * 
 * @return Physical address of the allocated page, or 0 if allocation failed
 */
uintptr_t alloc_physical_page(void) {
