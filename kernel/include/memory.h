/**
 * @file memory.h
 * @brief Memory management definitions
 */

#ifndef _MEMORY_H
#define _MEMORY_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

/**
 * @brief Memory constants
 */
#define PAGE_SIZE           4096     // 4KB page size
#define KERNEL_VIRTUAL_BASE 0xFFFFFFFF80000000 // Higher-half kernel

/**
 * @brief Memory region types
 */
#define MEMORY_FREE         1       // Free memory
#define MEMORY_RESERVED     2       // Reserved memory
#define MEMORY_ACPI         3       // ACPI reclaimable memory
#define MEMORY_NVS          4       // ACPI NVS memory
#define MEMORY_BADRAM       5       // Bad RAM
#define MEMORY_KERNEL       6       // Kernel memory
#define MEMORY_MODULES      7       // Loaded modules

/**
 * @brief Memory region descriptor
 */
typedef struct memory_region {
    uintptr_t base;                // Base physical address
    uintptr_t size;                // Size in bytes
    uint32_t type;                 // Region type
    uint32_t reserved;             // Reserved (for alignment)
} memory_region_t;

/**
 * @brief Initialize memory management
 * 
 * @param mem_upper Upper memory size in KB from multiboot info
 */
void mm_init(uintptr_t mem_upper);

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
 * @brief Page table entry flags
 */
#define PTE_PRESENT       0x001     // Page is present
#define PTE_WRITABLE      0x002     // Page is writable
#define PTE_USER          0x004     // Page is accessible from user mode
#define PTE_PWT           0x008     // Page write-through
#define PTE_PCD           0x010     // Page cache disabled
#define PTE_ACCESSED      0x020     // Page was accessed
#define PTE_DIRTY         0x040     // Page was written to
#define PTE_PS            0x080     // Page size (if set, 2MB or 1GB page)
#define PTE_GLOBAL        0x100     // Page is global (not flushed in TLB)
#define PTE_NX            0x8000000000000000ULL // No execute

/**
 * @brief kernel heap functions 
 */

// Initialize the kernel heap
void heap_init(uintptr_t start, size_t size);

// Allocate memory from the kernel heap
void* kmalloc(size_t size);

// Allocate aligned memory from the kernel heap
void* kmalloc_aligned(size_t size, size_t alignment);

// Free memory allocated from the kernel heap
void kfree(void* ptr);

// Reallocate memory from the kernel heap
void* krealloc(void* ptr, size_t size);

// Get the size of an allocated block
size_t ksize(void* ptr);

// Debug function to print heap statistics
void heap_stats(void);

#endif /* _MEMORY_H */
