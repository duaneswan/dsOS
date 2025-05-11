/**
 * @file memory.h
 * @brief Memory management definitions
 */

#ifndef _MEMORY_H
#define _MEMORY_H

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

/**
 * @brief Physical memory page size
 */
#define PAGE_SIZE 4096

/**
 * @brief Virtual memory addresses
 */
#define KERNEL_VIRTUAL_BASE 0xFFFFFFFF80000000

/**
 * @brief Page table entry flags
 */
#define PTE_PRESENT     (1ULL << 0)  // Page is present
#define PTE_WRITABLE    (1ULL << 1)  // Page is writable
#define PTE_USER        (1ULL << 2)  // Page is accessible from userspace
#define PTE_WRITETHROUGH (1ULL << 3) // Page uses write-through caching
#define PTE_CACHE_DISABLE (1ULL << 4) // Page cache is disabled
#define PTE_ACCESSED    (1ULL << 5)  // Page has been accessed
#define PTE_DIRTY       (1ULL << 6)  // Page has been written to
#define PTE_PAGESIZE    (1ULL << 7)  // Page is a large page (2MB/1GB)
#define PTE_GLOBAL      (1ULL << 8)  // Page is global (no TLB flush on CR3 change)
#define PTE_NX          (1ULL << 63) // No-execute

/**
 * @brief Memory region type
 */
typedef enum {
    MEMORY_FREE,            // Free memory, available for use
    MEMORY_RESERVED,        // Reserved by system, do not use
    MEMORY_ACPI_RECLAIMABLE, // ACPI tables that can be reclaimed
    MEMORY_NVS,             // ACPI NVS memory
    MEMORY_BADRAM,          // Bad RAM, do not use
    MEMORY_KERNEL,          // Used by kernel
    MEMORY_MODULES,         // Used by kernel modules
    MEMORY_FRAMEBUFFER      // Used by framebuffer
} memory_type_t;

/**
 * @brief Memory region descriptor
 */
typedef struct {
    uint64_t base;          // Base physical address
    uint64_t size;          // Size in bytes
    memory_type_t type;     // Type of memory region
} memory_region_t;

/**
 * @brief Page frame allocator interface
 */

/**
 * @brief Allocate a physical page frame
 * 
 * @return Physical address of the allocated page frame, or 0 if out of memory
 */
uintptr_t mm_alloc_page(void);

/**
 * @brief Allocate a contiguous range of physical page frames
 * 
 * @param pages Number of pages to allocate
 * @return Physical address of the first allocated page frame, or 0 if out of memory
 */
uintptr_t mm_alloc_pages(size_t pages);

/**
 * @brief Free a physical page frame
 * 
 * @param addr Physical address of the page frame to free
 */
void mm_free_page(uintptr_t addr);

/**
 * @brief Free a contiguous range of physical page frames
 * 
 * @param addr Physical address of the first page frame to free
 * @param pages Number of pages to free
 */
void mm_free_pages(uintptr_t addr, size_t pages);

/**
 * @brief Get the total number of physical page frames
 * 
 * @return Total number of physical page frames
 */
size_t mm_get_total_pages(void);

/**
 * @brief Get the number of free physical page frames
 * 
 * @return Number of free physical page frames
 */
size_t mm_get_free_pages(void);

/**
 * @brief Virtual memory management interface
 */

/**
 * @brief Map a physical page frame to a virtual address
 * 
 * @param phys Physical address to map
 * @param virt Virtual address to map to
 * @param flags Page table entry flags
 * @return true if mapping was successful, false otherwise
 */
bool vm_map_page(uintptr_t phys, uintptr_t virt, uint64_t flags);

/**
 * @brief Unmap a virtual address
 * 
 * @param virt Virtual address to unmap
 */
void vm_unmap_page(uintptr_t virt);

/**
 * @brief Get the physical address mapped to a virtual address
 * 
 * @param virt Virtual address to lookup
 * @return Physical address mapped to the virtual address, or 0 if not mapped
 */
uintptr_t vm_get_phys(uintptr_t virt);

/**
 * @brief Initialize the memory management subsystem
 * 
 * @param mem_upper Upper memory size in KB from multiboot info
 */
void mm_init(uintptr_t mem_upper);

#endif /* _MEMORY_H */
