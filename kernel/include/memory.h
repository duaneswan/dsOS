/**
 * @file memory.h
 * @brief Memory management functions and definitions
 */

#ifndef _MEMORY_H
#define _MEMORY_H

#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>

/**
 * @brief Virtual memory addresses
 */
#define KERNEL_VIRTUAL_BASE 0xFFFFFFFF80000000  // Higher half base address
#define KERNEL_PHYSICAL_MAP 0xFFFF800000000000  // Direct physical memory mapping base

/**
 * @brief Memory page size constants
 */
#define PAGE_SIZE          4096                 // 4 KiB page size
#define PAGE_MASK          0xFFFFFFFFFFFFF000   // Page mask (clear offset bits)
#define PAGE_OFFSET_MASK   0x0000000000000FFF   // Page offset mask
#define LARGE_PAGE_SIZE    (PAGE_SIZE * 512)    // 2 MiB large page size
#define HUGE_PAGE_SIZE     (LARGE_PAGE_SIZE * 512) // 1 GiB huge page size

/**
 * @brief Page table entry flags
 */
#define PTE_PRESENT        (1ULL << 0)          // Page is present
#define PTE_WRITABLE       (1ULL << 1)          // Page is writable
#define PTE_USER           (1ULL << 2)          // Page is accessible from userspace
#define PTE_WRITE_THROUGH  (1ULL << 3)          // Page has write-through caching
#define PTE_CACHE_DISABLE  (1ULL << 4)          // Page has caching disabled
#define PTE_ACCESSED       (1ULL << 5)          // Page has been accessed
#define PTE_DIRTY          (1ULL << 6)          // Page has been written to
#define PTE_LARGE          (1ULL << 7)          // Page is a large page
#define PTE_GLOBAL         (1ULL << 8)          // Page is global (not flushed from TLB)
#define PTE_NX             (1ULL << 63)         // Page is non-executable (if supported)

/**
 * @brief Memory region types
 */
typedef enum {
    MEMORY_REGION_FREE = 0,             // Free memory, available for allocation
    MEMORY_REGION_RESERVED,             // Reserved memory, not available for allocation
    MEMORY_REGION_ACPI_RECLAIMABLE,     // ACPI reclaimable memory
    MEMORY_REGION_NVS,                  // ACPI NVS memory
    MEMORY_REGION_BADRAM,               // Bad RAM, unusable
    MEMORY_REGION_KERNEL,               // Kernel code and data
    MEMORY_REGION_MODULES,              // Kernel modules
    MEMORY_REGION_BOOTLOADER,           // Bootloader data
} memory_region_type_t;

/**
 * @brief Memory region descriptor
 */
typedef struct memory_region {
    uint64_t base_addr;                 // Base physical address
    uint64_t length;                    // Region size in bytes
    memory_region_type_t type;          // Region type
    struct memory_region* next;         // Next region in the list
} memory_region_t;

/**
 * @brief Physical memory manager
 */

/**
 * @brief Initialize the physical memory manager
 * 
 * @param mem_upper Upper memory size in bytes (from bootloader or BIOS)
 */
void mm_init(uintptr_t mem_upper);

/**
 * @brief Allocate a physical page
 * 
 * @return Physical address of the allocated page, or 0 if allocation failed
 */
uintptr_t alloc_physical_page(void);

/**
 * @brief Allocate contiguous physical pages
 * 
 * @param count Number of pages to allocate
 * @return Physical address of the first allocated page, or 0 if allocation failed
 */
uintptr_t alloc_physical_pages(size_t count);

/**
 * @brief Free a physical page
 * 
 * @param phys_addr Physical address of the page to free
 */
void free_physical_page(uintptr_t phys_addr);

/**
 * @brief Free contiguous physical pages
 * 
 * @param phys_addr Physical address of the first page to free
 * @param count Number of pages to free
 */
void free_physical_pages(uintptr_t phys_addr, size_t count);

/**
 * @brief Check if a physical page is allocated
 * 
 * @param phys_addr Physical address to check
 * @return true if the page is allocated, false if it's free
 */
bool is_physical_page_allocated(uintptr_t phys_addr);

/**
 * @brief Get the total amount of physical memory in bytes
 * 
 * @return Size of physical memory in bytes
 */
uint64_t get_physical_memory_size(void);

/**
 * @brief Get the amount of free physical memory in bytes
 * 
 * @return Size of free physical memory in bytes
 */
uint64_t get_free_physical_memory(void);

/**
 * @brief Virtual memory manager
 */

/**
 * @brief Initialize the virtual memory manager
 */
void vm_init(void);

/**
 * @brief Map a physical page to a virtual address
 * 
 * @param phys_addr Physical address of the page to map
 * @param virt_addr Virtual address where the page should be mapped
 * @param flags Page table entry flags
 * @return 0 on success, negative value on failure
 */
int map_page(uintptr_t phys_addr, uintptr_t virt_addr, uint64_t flags);

/**
 * @brief Unmap a virtual address
 * 
 * @param virt_addr Virtual address to unmap
 * @return 0 on success, negative value on failure
 */
int unmap_page(uintptr_t virt_addr);

/**
 * @brief Get the physical address for a virtual address
 * 
 * @param virt_addr Virtual address to translate
 * @return Physical address, or 0 if the virtual address is not mapped
 */
uintptr_t virtual_to_physical(uintptr_t virt_addr);

/**
 * @brief Map multiple pages consecutively
 * 
 * @param phys_addr Physical address of the first page to map
 * @param virt_addr Virtual address where the pages should be mapped
 * @param count Number of pages to map
 * @param flags Page table entry flags
 * @return 0 on success, negative value on failure
 */
int map_pages(uintptr_t phys_addr, uintptr_t virt_addr, size_t count, uint64_t flags);

/**
 * @brief Unmap multiple consecutive virtual addresses
 * 
 * @param virt_addr First virtual address to unmap
 * @param count Number of pages to unmap
 * @return 0 on success, negative value on failure
 */
int unmap_pages(uintptr_t virt_addr, size_t count);

/**
 * @brief Check if a virtual address is mapped
 * 
 * @param virt_addr Virtual address to check
 * @return true if the address is mapped, false otherwise
 */
bool is_page_mapped(uintptr_t virt_addr);

/**
 * @brief Kernel heap functions
 */

/**
 * @brief Initialize the kernel heap
 * 
 * @param start Start virtual address of the heap area
 * @param size Size of the heap area in bytes
 */
void heap_init(uintptr_t start, size_t size);

/**
 * @brief Allocate memory from the kernel heap
 * 
 * @param size Size to allocate in bytes
 * @return Pointer to the allocated memory, or NULL if allocation failed
 */
void* kmalloc(size_t size);

/**
 * @brief Allocate aligned memory from the kernel heap
 * 
 * @param size Size to allocate in bytes
 * @param align Alignment boundary (must be a power of 2)
 * @return Pointer to the allocated memory, or NULL if allocation failed
 */
void* kmalloc_aligned(size_t size, size_t align);

/**
 * @brief Allocate zero-initialized memory from the kernel heap
 * 
 * @param size Size to allocate in bytes
 * @return Pointer to the allocated memory, or NULL if allocation failed
 */
void* kzalloc(size_t size);

/**
 * @brief Free memory allocated from the kernel heap
 * 
 * @param ptr Pointer to the memory to free
 */
void kfree(void* ptr);

/**
 * @brief Reallocate memory from the kernel heap
 * 
 * @param ptr Pointer to the memory to reallocate
 * @param size New size in bytes
 * @return Pointer to the reallocated memory, or NULL if reallocation failed
 */
void* krealloc(void* ptr, size_t size);

/**
 * @brief Get the size of an allocated memory block
 * 
 * @param ptr Pointer to the allocated memory
 * @return Size of the allocated block in bytes, or 0 if ptr is NULL
 */
size_t ksize(void* ptr);

/**
 * @brief Get information about heap usage
 * 
 * @param total Pointer where to store total heap size in bytes
 * @param used Pointer where to store used heap size in bytes
 * @param count Pointer where to store number of allocations
 */
void heap_get_info(size_t* total, size_t* used, size_t* count);

#endif /* _MEMORY_H */
