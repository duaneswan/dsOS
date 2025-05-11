/**
 * @file heap.c
 * @brief Kernel heap allocator implementation
 */

#include "../include/kernel.h"
#include "../include/memory.h"
#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>

// Heap control block and free block structure
typedef struct heap_block {
    size_t size;                 // Size of this block in bytes (including header)
    bool free;                   // Whether this block is free
    struct heap_block* next;     // Next block in the list
    struct heap_block* prev;     // Previous block in the list
} heap_block_t;

// Heap statistics
static size_t heap_size = 0;     // Total heap size
static size_t heap_used = 0;     // Used heap memory
static size_t alloc_count = 0;   // Number of active allocations

// Heap control
static uintptr_t heap_start = 0; // Start address of the heap
static uintptr_t heap_end = 0;   // End address of the heap
static heap_block_t* free_list = NULL; // Start of the free list

// Magic values for checking heap integrity
#define HEAP_MAGIC          0x4845415042524B00ULL // "HEAPBRK\0"
#define HEAP_BLOCK_MAGIC    0x424C4F434B00ULL     // "BLOCK\0\0"

// Forward declarations
static heap_block_t* find_free_block(size_t size, size_t align);
static void split_block(heap_block_t* block, size_t size);
static bool merge_adjacent_blocks(heap_block_t* block);
static bool is_block_free(heap_block_t* block);

/**
 * @brief Initialize the kernel heap
 * 
 * @param start Start virtual address of the heap area
 * @param size Size of the heap area in bytes
 */
void heap_init(uintptr_t start, size_t size) {
    // Align start address to page boundary
    start = (start + PAGE_SIZE - 1) & PAGE_MASK;
    
    // Save heap parameters
    heap_start = start;
    heap_size = size;
    heap_end = start + size;
    heap_used = 0;
    alloc_count = 0;
    
    // Initialize the free list with a single block covering the entire heap
    free_list = (heap_block_t*)start;
    free_list->size = size;
    free_list->free = true;
    free_list->next = NULL;
    free_list->prev = NULL;
    
    kprintf("HEAP: Kernel heap initialized at 0x%lx, size %lu KB\n", 
            heap_start, heap_size / 1024);
}

/**
 * @brief Allocate memory from the kernel heap
 * 
 * @param size Size to allocate in bytes
 * @return Pointer to the allocated memory, or NULL if allocation failed
 */
void* kmalloc(size_t size) {
    // Handle 0-size allocations
    if (size == 0) {
        return NULL;
    }
    
    // Calculate required size with header
    size_t required_size = size + sizeof(heap_block_t);
    
    // Align to 8-byte boundary
    required_size = (required_size + 7) & ~7;
    
    // Find a suitable free block
    heap_block_t* block = find_free_block(required_size, sizeof(void*));
    if (!block) {
        // Out of memory
        return NULL;
    }
    
    // Split the block if it's much larger than needed
    if (block->size > required_size + sizeof(heap_block_t) + 16) {
        split_block(block, required_size);
    }
    
    // Mark the block as used
    block->free = false;
    
    // Update statistics
    heap_used += block->size;
    alloc_count++;
    
    // Return pointer to the data area
    return (void*)((uintptr_t)block + sizeof(heap_block_t));
}

/**
 * @brief Allocate aligned memory from the kernel heap
 * 
 * @param size Size to allocate in bytes
 * @param align Alignment boundary (must be a power of 2)
 * @return Pointer to the allocated memory, or NULL if allocation failed
 */
void* kmalloc_aligned(size_t size, size_t align) {
    // Handle 0-size allocations
    if (size == 0) {
        return NULL;
    }
    
    // Ensure alignment is a power of 2
    if ((align & (align - 1)) != 0) {
        // Not a power of 2
        return NULL;
    }
    
    // Calculate required size with header
    size_t required_size = size + sizeof(heap_block_t) + align;
    
    // Align to 8-byte boundary
    required_size = (required_size + 7) & ~7;
    
    // Find a suitable free block
    heap_block_t* block = find_free_block(required_size, align);
    if (!block) {
        // Out of memory
        return NULL;
    }
    
    // Calculate aligned address
    uintptr_t data_addr = (uintptr_t)block + sizeof(heap_block_t);
    uintptr_t aligned_addr = (data_addr + align - 1) & ~(align - 1);
    size_t padding = aligned_addr - data_addr;
    
    // If there's padding, we need to create a new block
    if (padding > 0) {
        // We need enough space for a new block header + the aligned data
        if (padding >= sizeof(heap_block_t)) {
            // Create a new block for the padding
            heap_block_t* padding_block = (heap_block_t*)((uintptr_t)block + sizeof(heap_block_t) + padding - sizeof(heap_block_t));
            padding_block->size = block->size - padding;
            padding_block->free = false;
            padding_block->next = block->next;
            padding_block->prev = block;
            
            if (block->next) {
                block->next->prev = padding_block;
            }
            
            block->next = padding_block;
            block->size = padding;
            
            block = padding_block;
        }
    }
    
    // Split the block if it's much larger than needed
    if (block->size > size + sizeof(heap_block_t) + 16) {
        split_block(block, size + sizeof(heap_block_t));
    }
    
    // Mark the block as used
    block->free = false;
    
    // Update statistics
    heap_used += block->size;
    alloc_count++;
    
    // Return pointer to the data area
    return (void*)((uintptr_t)block + sizeof(heap_block_t));
}

/**
 * @brief Allocate zero-initialized memory from the kernel heap
 * 
 * @param size Size to allocate in bytes
 * @return Pointer to the allocated memory, or NULL if allocation failed
 */
void* kzalloc(size_t size) {
    void* ptr = kmalloc(size);
    if (ptr) {
        memset(ptr, 0, size);
    }
    return ptr;
}

/**
 * @brief Free memory allocated from the kernel heap
 * 
 * @param ptr Pointer to the memory to free
 */
void kfree(void* ptr) {
    // Handle NULL pointer
    if (!ptr) {
        return;
    }
    
    // Get the block header
    heap_block_t* block = (heap_block_t*)((uintptr_t)ptr - sizeof(heap_block_t));
    
    // Validate the block is within the heap
    if ((uintptr_t)block < heap_start || (uintptr_t)block >= heap_end) {
        panic(PANIC_NORMAL, "Invalid free: ptr outside heap range", __FILE__, __LINE__);
    }
    
    // Check if the block is already free
    if (block->free) {
        panic(PANIC_NORMAL, "Double free detected", __FILE__, __LINE__);
    }
    
    // Update statistics
    heap_used -= block->size;
    alloc_count--;
    
    // Mark the block as free
    block->free = true;
    
    // Try to merge with adjacent blocks
    merge_adjacent_blocks(block);
}

/**
 * @brief Reallocate memory from the kernel heap
 * 
 * @param ptr Pointer to the memory to reallocate
 * @param size New size in bytes
 * @return Pointer to the reallocated memory, or NULL if reallocation failed
 */
void* krealloc(void* ptr, size_t size) {
    // Handle NULL pointer (equivalent to kmalloc)
    if (!ptr) {
        return kmalloc(size);
    }
    
    // Handle 0-size (equivalent to kfree)
    if (size == 0) {
        kfree(ptr);
        return NULL;
    }
    
    // Get the block header
    heap_block_t* block = (heap_block_t*)((uintptr_t)ptr - sizeof(heap_block_t));
    
    // Calculate current data size
    size_t current_size = block->size - sizeof(heap_block_t);
    
    // If new size is smaller or the same, we can use the existing block
    if (size <= current_size) {
        // If the block is much larger than needed, split it
        size_t required_size = size + sizeof(heap_block_t);
        required_size = (required_size + 7) & ~7;
        
        if (block->size > required_size + sizeof(heap_block_t) + 16) {
            split_block(block, required_size);
            heap_used -= (block->size - required_size);
        }
        
        return ptr;
    }
    
    // Try to merge with the next block if it's free
    if (block->next && block->next->free && 
        block->size + block->next->size >= size + sizeof(heap_block_t)) {
        
        // Merge the blocks
        block->size += block->next->size;
        block->next = block->next->next;
        if (block->next) {
            block->next->prev = block;
        }
        
        // If the merged block is much larger than needed, split it
        size_t required_size = size + sizeof(heap_block_t);
        required_size = (required_size + 7) & ~7;
        
        if (block->size > required_size + sizeof(heap_block_t) + 16) {
            split_block(block, required_size);
        }
        
        return ptr;
    }
    
    // Allocate a new, larger block
    void* new_ptr = kmalloc(size);
    if (!new_ptr) {
        return NULL;
    }
    
    // Copy the old data
    memcpy(new_ptr, ptr, current_size);
    
    // Free the old block
    kfree(ptr);
    
    return new_ptr;
}

/**
 * @brief Get the size of an allocated memory block
 * 
 * @param ptr Pointer to the allocated memory
 * @return Size of the allocated block in bytes, or 0 if ptr is NULL
 */
size_t ksize(void* ptr) {
    // Handle NULL pointer
    if (!ptr) {
        return 0;
    }
    
    // Get the block header
    heap_block_t* block = (heap_block_t*)((uintptr_t)ptr - sizeof(heap_block_t));
    
    // Validate the block is within the heap
    if ((uintptr_t)block < heap_start || (uintptr_t)block >= heap_end) {
        return 0;
    }
    
    // Return the data size
    return block->size - sizeof(heap_block_t);
}

/**
 * @brief Get information about heap usage
 * 
 * @param total Pointer where to store total heap size in bytes
 * @param used Pointer where to store used heap size in bytes
 * @param count Pointer where to store number of allocations
 */
void heap_get_info(size_t* total, size_t* used, size_t* count) {
    if (total) *total = heap_size;
    if (used) *used = heap_used;
    if (count) *count = alloc_count;
}

/**
 * @brief Find a free block of the given size
 * 
 * @param size Size needed in bytes
 * @param align Alignment requirement
 * @return Pointer to a suitable free block, or NULL if none found
 */
static heap_block_t* find_free_block(size_t size, size_t align) {
    // First-fit strategy: use the first block that fits
    heap_block_t* block = free_list;
    
    while (block) {
        if (block->free && block->size >= size) {
            // Check alignment if needed
            if (align > 1) {
                uintptr_t data_addr = (uintptr_t)block + sizeof(heap_block_t);
                uintptr_t aligned_addr = (data_addr + align - 1) & ~(align - 1);
                size_t padding = aligned_addr - data_addr;
                
                // Check if there's enough space with alignment
                if (block->size >= size + padding) {
                    return block;
                }
            } else {
                return block;
            }
        }
        block = block->next;
    }
    
    return NULL;
}

/**
 * @brief Split a block into two
 * 
 * @param block Block to split
 * @param size Size of the first part
 */
static void split_block(heap_block_t* block, size_t size) {
    // Calculate the new block position
    heap_block_t* new_block = (heap_block_t*)((uintptr_t)block + size);
    
    // Setup the new block
    new_block->size = block->size - size;
    new_block->free = true;
    new_block->next = block->next;
    new_block->prev = block;
    
    // Update the links
    if (block->next) {
        block->next->prev = new_block;
    }
    block->next = new_block;
    
    // Update the original block's size
    block->size = size;
}

/**
 * @brief Try to merge a free block with adjacent free blocks
 * 
 * @param block Block to merge
 * @return true if a merge occurred, false otherwise
 */
static bool merge_adjacent_blocks(heap_block_t* block) {
    bool merged = false;
    
    // Try to merge with the next block
    if (block->next && block->next->free) {
        block->size += block->next->size;
        block->next = block->next->next;
        if (block->next) {
            block->next->prev = block;
        }
        merged = true;
    }
    
    // Try to merge with the previous block
    if (block->prev && block->prev->free) {
        block->prev->size += block->size;
        block->prev->next = block->next;
        if (block->next) {
            block->next->prev = block->prev;
        }
        merged = true;
        
        // The merged block is now the previous one
        block = block->prev;
    }
    
    return merged;
}
