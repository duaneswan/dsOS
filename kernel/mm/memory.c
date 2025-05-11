/**
 * @file memory.c
 * @brief Memory management implementation
 */

#include "../include/kernel.h"
#include "../include/memory.h"
#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

// Definitions for page frame allocator
#define MAX_MEMORY_REGIONS 64
#define BITMAP_ENTRY_SIZE 64
#define BITMAP_ALIGN_MASK (BITMAP_ENTRY_SIZE - 1)

// Memory map information
static memory_region_t memory_map[MAX_MEMORY_REGIONS];
static size_t memory_map_entries = 0;
static uint64_t total_memory = 0;

// Bitmap allocator state
static uint64_t* page_bitmap = NULL;
static size_t page_bitmap_entries = 0;
static size_t total_pages = 0;
static size_t used_pages = 0;
static uintptr_t managed_memory_start = 0x100000; // Start at 1MB

// Current page directory pointer table
static uint64_t* current_pml4 = NULL;

// Forward declarations
static void setup_page_bitmap(void);
static void map_kernel_memory(void);

/**
 * @brief Set a bit in the page bitmap
 * 
 * @param index Bit index to set
 */
static inline void bitmap_set(size_t index) {
    size_t entry = index / BITMAP_ENTRY_SIZE;
    size_t bit = index % BITMAP_ENTRY_SIZE;
    page_bitmap[entry] |= (1ULL << bit);
}

/**
 * @brief Clear a bit in the page bitmap
 * 
 * @param index Bit index to clear
 */
static inline void bitmap_clear(size_t index) {
    size_t entry = index / BITMAP_ENTRY_SIZE;
    size_t bit = index % BITMAP_ENTRY_SIZE;
    page_bitmap[entry] &= ~(1ULL << bit);
}

/**
 * @brief Test a bit in the page bitmap
 * 
 * @param index Bit index to test
 * @return true if the bit is set, false otherwise
 */
static inline bool bitmap_test(size_t index) {
    size_t entry = index / BITMAP_ENTRY_SIZE;
    size_t bit = index % BITMAP_ENTRY_SIZE;
    return (page_bitmap[entry] & (1ULL << bit)) != 0;
}

/**
 * @brief Find the first free bit in the page bitmap
 * 
 * @return Index of the first free bit, or (size_t)-1 if no free bits found
 */
static size_t bitmap_find_free(void) {
    for (size_t i = 0; i < page_bitmap_entries; i++) {
        if (page_bitmap[i] != ~0ULL) {
            for (size_t j = 0; j < BITMAP_ENTRY_SIZE; j++) {
                if ((page_bitmap[i] & (1ULL << j)) == 0) {
                    return i * BITMAP_ENTRY_SIZE + j;
                }
            }
        }
    }
    return (size_t)-1;
}

/**
 * @brief Find the first n contiguous free bits in the page bitmap
 * 
 * @param n Number of contiguous free bits to find
 * @return Index of the first free bit, or (size_t)-1 if not enough contiguous free bits found
 */
static size_t bitmap_find_free_contiguous(size_t n) {
    if (n == 0) return (size_t)-1;
    if (n == 1) return bitmap_find_free();
    
    size_t start = 0;
    size_t count = 0;
    
    for (size_t i = 0; i < total_pages; i++) {
        if (!bitmap_test(i)) {
            if (count == 0) start = i;
            count++;
            if (count >= n) return start;
        } else {
            count = 0;
        }
    }
    
    return (size_t)-1;
}

/**
 * @brief Initialize memory management
 * 
 * @param mem_upper Upper memory size in KB from multiboot info
 */
void mm_init(uintptr_t mem_upper) {
    // For now, just use the upper memory value to determine the total memory
    // In a real implementation, we would parse the memory map from multiboot
    total_memory = mem_upper * 1024;
    
    // Calculate the number of pages
    total_pages = total_memory / PAGE_SIZE;
    
    // Set up the initial memory map with one entry for the whole memory
    memory_map[0].base = managed_memory_start;
    memory_map[0].size = total_memory - managed_memory_start;
    memory_map[0].type = MEMORY_FREE;
    memory_map_entries = 1;
    
    // Set up the page bitmap
    setup_page_bitmap();
    
    // Reserve the first 1MB of memory (already done by starting managed memory at 1MB)
    
    // Mark the kernel and the page bitmap as used
    extern uint8_t _kernel_start[], _kernel_end[];
    uintptr_t kernel_start = (uintptr_t)_kernel_start;
    uintptr_t kernel_end = (uintptr_t)_kernel_end;
    uintptr_t kernel_size = kernel_end - kernel_start;
    
    // Mark kernel memory as used
    for (uintptr_t addr = kernel_start - KERNEL_VIRTUAL_BASE;
         addr < kernel_end - KERNEL_VIRTUAL_BASE;
         addr += PAGE_SIZE) {
        size_t page = addr / PAGE_SIZE;
        bitmap_set(page);
        used_pages++;
    }
    
    // Mark page bitmap memory as used (calculated as bitmap size in pages)
    size_t bitmap_size = (page_bitmap_entries * sizeof(uint64_t) + PAGE_SIZE - 1) / PAGE_SIZE;
    for (size_t i = 0; i < bitmap_size; i++) {
        size_t page = ((uintptr_t)page_bitmap - KERNEL_VIRTUAL_BASE) / PAGE_SIZE + i;
        bitmap_set(page);
        used_pages++;
    }
    
    kprintf("MM: Total memory: %llu MB\n", total_memory / (1024 * 1024));
    kprintf("MM: Total pages: %lu\n", total_pages);
    kprintf("MM: Used pages: %lu\n", used_pages);
    kprintf("MM: Free pages: %lu\n", total_pages - used_pages);
    kprintf("MM: Kernel size: %lu KB\n", kernel_size / 1024);
    
    // Ensure current page tables are properly set up for virtual memory
    map_kernel_memory();
}

/**
 * @brief Set up the page bitmap
 */
static void setup_page_bitmap(void) {
    // Calculate the size of the bitmap in uint64_t entries
    page_bitmap_entries = (total_pages + BITMAP_ENTRY_SIZE - 1) / BITMAP_ENTRY_SIZE;
    
    // Allocate memory for the bitmap at a fixed address for now
    // In a real implementation, we would allocate this memory more carefully
    page_bitmap = (uint64_t*)(KERNEL_VIRTUAL_BASE + 0x400000); // At 4MB mark
    
    // Clear the bitmap
    for (size_t i = 0; i < page_bitmap_entries; i++) {
        page_bitmap[i] = 0;
    }
    
    // Mark the memory below managed_memory_start as used
    for (size_t i = 0; i < managed_memory_start / PAGE_SIZE; i++) {
        bitmap_set(i);
        used_pages++;
    }
}

/**
 * @brief Map kernel memory in virtual address space
 */
static void map_kernel_memory(void) {
    // Get the current page table from CR3
    uint64_t cr3;
    asm volatile("mov %%cr3, %0" : "=r"(cr3));
    
    // Use the current page tables
    current_pml4 = (uint64_t*)(cr3 + KERNEL_VIRTUAL_BASE);
    
    // In a real implementation, we would set up proper page tables for the kernel
    // For now, we'll just use what was set up in boot.asm
}

/**
 * @brief Allocate a physical page frame
 * 
 * @return Physical address of the allocated page frame, or 0 if out of memory
 */
uintptr_t mm_alloc_page(void) {
    size_t page = bitmap_find_free();
    if (page == (size_t)-1) {
        // Out of memory
        return 0;
    }
    
    bitmap_set(page);
    used_pages++;
    
    return page * PAGE_SIZE;
}

/**
 * @brief Allocate a contiguous range of physical page frames
 * 
 * @param pages Number of pages to allocate
 * @return Physical address of the first allocated page frame, or 0 if out of memory
 */
uintptr_t mm_alloc_pages(size_t pages) {
    size_t page = bitmap_find_free_contiguous(pages);
    if (page == (size_t)-1) {
        // Not enough contiguous memory
        return 0;
    }
    
    // Mark all pages as used
    for (size_t i = 0; i < pages; i++) {
        bitmap_set(page + i);
    }
    
    used_pages += pages;
    return page * PAGE_SIZE;
}

/**
 * @brief Free a physical page frame
 * 
 * @param addr Physical address of the page frame to free
 */
void mm_free_page(uintptr_t addr) {
    // Ignore addresses below the managed memory start
    if (addr < managed_memory_start) {
        return;
    }
    
    size_t page = addr / PAGE_SIZE;
    if (page >= total_pages) {
        // Invalid address
        return;
    }
    
    bitmap_clear(page);
    used_pages--;
}

/**
 * @brief Free a contiguous range of physical page frames
 * 
 * @param addr Physical address of the first page frame to free
 * @param pages Number of pages to free
 */
void mm_free_pages(uintptr_t addr, size_t pages) {
    for (size_t i = 0; i < pages; i++) {
        mm_free_page(addr + i * PAGE_SIZE);
    }
}

/**
 * @brief Get the total number of physical page frames
 * 
 * @return Total number of physical page frames
 */
size_t mm_get_total_pages(void) {
    return total_pages;
}

/**
 * @brief Get the number of free physical page frames
 * 
 * @return Number of free physical page frames
 */
size_t mm_get_free_pages(void) {
    return total_pages - used_pages;
}

/**
 * @brief Map a physical page frame to a virtual address
 * 
 * @param phys Physical address to map
 * @param virt Virtual address to map to
 * @param flags Page table entry flags
 * @return true if mapping was successful, false otherwise
 */
bool vm_map_page(uintptr_t phys, uintptr_t virt, uint64_t flags) {
    // In a real implementation, we would set up proper page tables
    // For now, we'll just return true for addresses that are likely already mapped
    if (virt >= KERNEL_VIRTUAL_BASE) {
        return true;
    }
    
    return false;
}

/**
 * @brief Unmap a virtual address
 * 
 * @param virt Virtual address to unmap
 */
void vm_unmap_page(uintptr_t virt) {
    // In a real implementation, we would clear the page table entry
}

/**
 * @brief Get the physical address mapped to a virtual address
 * 
 * @param virt Virtual address to lookup
 * @return Physical address mapped to the virtual address, or 0 if not mapped
 */
uintptr_t vm_get_phys(uintptr_t virt) {
    // In a real implementation, we would look up the page table
    if (virt >= KERNEL_VIRTUAL_BASE) {
        return virt - KERNEL_VIRTUAL_BASE;
    }
    
    return 0;
}
