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
    // Lock interrupts for atomic operation
    cli();
    
    // Check if we have free pages
    if (free_pages == 0) {
        sti();
        return 0;
    }
    
    // Search for a free page in the bitmap
    for (uint64_t i = 0; i < bitmap_size; i++) {
        if (physical_bitmap[i] != ~0ULL) { // Not all bits are set
            for (uint64_t j = 0; j < 64; j++) {
                uint64_t bit = 1ULL << j;
                if (!(physical_bitmap[i] & bit)) {
                    // Found a free page
                    uint64_t page_num = i * 64 + j;
                    if (page_num >= total_pages) {
                        continue; // Out of range
                    }
                    
                    // Mark the page as allocated
                    physical_bitmap[i] |= bit;
                    free_pages--;
                    
                    // Calculate physical address
                    uintptr_t phys_addr = page_num * PAGE_SIZE;
                    
                    sti();
                    return phys_addr;
                }
            }
        }
    }
    
    // No free pages found
    sti();
    return 0;
}

/**
 * @brief Allocate contiguous physical pages
 * 
 * @param count Number of pages to allocate
 * @return Physical address of the first allocated page, or 0 if allocation failed
 */
uintptr_t alloc_physical_pages(size_t count) {
    if (count == 0) {
        return 0;
    }
    
    if (count == 1) {
        return alloc_physical_page();
    }
    
    // Lock interrupts for atomic operation
    cli();
    
    // Check if we have enough free pages
    if (free_pages < count) {
        sti();
        return 0;
    }
    
    // Search for contiguous free pages
    for (uint64_t start_page = 0; start_page <= total_pages - count; start_page++) {
        bool found = true;
        
        // Check if all pages in this range are free
        for (uint64_t offset = 0; offset < count; offset++) {
            uint64_t page_num = start_page + offset;
            if (bitmap_test(page_num)) {
                found = false;
                // Skip to the next page after this allocated one
                start_page = page_num;
                break;
            }
        }
        
        if (found) {
            // Allocate all pages in the range
            for (uint64_t offset = 0; offset < count; offset++) {
                uint64_t page_num = start_page + offset;
                bitmap_set(page_num);
            }
            
            free_pages -= count;
            
            sti();
            return start_page * PAGE_SIZE;
        }
    }
    
    // No contiguous region found
    sti();
    return 0;
}

/**
 * @brief Free a physical page
 * 
 * @param phys_addr Physical address of the page to free
 */
void free_physical_page(uintptr_t phys_addr) {
    // Lock interrupts for atomic operation
    cli();
    
    // Calculate page number
    uint64_t page_num = phys_addr / PAGE_SIZE;
    
    // Bounds check
    if (page_num >= total_pages) {
        sti();
        return;
    }
    
    // Check if the page is already free
    if (!bitmap_test(page_num)) {
        sti();
        return;
    }
    
    // Mark the page as free
    bitmap_clear(page_num);
    free_pages++;
    
    sti();
}

/**
 * @brief Free contiguous physical pages
 * 
 * @param phys_addr Physical address of the first page to free
 * @param count Number of pages to free
 */
void free_physical_pages(uintptr_t phys_addr, size_t count) {
    // Lock interrupts for atomic operation
    cli();
    
    // Free each page in the range
    for (size_t i = 0; i < count; i++) {
        uint64_t page_num = (phys_addr / PAGE_SIZE) + i;
        
        // Bounds check
        if (page_num >= total_pages) {
            continue;
        }
        
        // Check if the page is already free
        if (!bitmap_test(page_num)) {
            continue;
        }
        
        // Mark the page as free
        bitmap_clear(page_num);
        free_pages++;
    }
    
    sti();
}

/**
 * @brief Check if a physical page is allocated
 * 
 * @param phys_addr Physical address to check
 * @return true if the page is allocated, false if it's free
 */
bool is_physical_page_allocated(uintptr_t phys_addr) {
    uint64_t page_num = phys_addr / PAGE_SIZE;
    return bitmap_test(page_num);
}

/**
 * @brief Get the total amount of physical memory in bytes
 * 
 * @return Size of physical memory in bytes
 */
uint64_t get_physical_memory_size(void) {
    return total_memory;
}

/**
 * @brief Get the amount of free physical memory in bytes
 * 
 * @return Size of free physical memory in bytes
 */
uint64_t get_free_physical_memory(void) {
    return free_pages * PAGE_SIZE;
}
