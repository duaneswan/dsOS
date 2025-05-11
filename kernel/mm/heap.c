/**
 * @file heap.c
 * @brief Kernel heap allocator implementation
 */

#include "../include/kernel.h"
#include "../include/memory.h"
#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

// Heap constants
#define HEAP_MAGIC        0x1BADB002   // Magic number for heap integrity checks
#define HEAP_MIN_SIZE     4096         // Minimum size of the heap in bytes
#define HEAP_MIN_ALLOC    16           // Minimum allocation size
#define HEAP_MAX_ALLOC    0x10000000   // Maximum allocation size (256MB)

// Alignment for allocations
#define HEAP_ALIGN        8            // Align all allocations to 8 bytes

/**
 * @brief Heap block header
 */
typedef struct block_header {
    uint32_t magic;               // Magic number for integrity check
    uint32_t flags;               // Block flags (e.g., allocated, free)
    size_t size;                  // Size of the block (excluding header)
    struct block_header* prev;    // Previous block in the list
    struct block_header* next;    // Next block in the list
} block_header_t;

// Block flags
#define BLOCK_FREE      0x00000001     // Block is free
#define BLOCK_LAST      0x00000002     // Last block in the heap

// Heap state
static uintptr_t heap_start = 0;       // Start address of the heap
static uintptr_t heap_end = 0;         // End address of the heap
static size_t heap_size = 0;           // Total size of the heap
static size_t used_space = 0;          // Space used by allocations
static block_header_t* free_list = NULL; // List of free blocks

// Forward declarations
static void* heap_allocate_block(size_t size);
static bool heap_free_block(void* ptr);
static block_header_t* heap_find_free_block(size_t size);
static block_header_t* heap_split_block(block_header_t* block, size_t size);
static bool heap_merge_blocks(block_header_t* block);
static bool heap_check_integrity(void);
static void heap_dump(void);

/**
 * @brief Initialize the kernel heap
 * 
 * @param start Start address of the heap
 * @param size Size of the heap in bytes
 */
void heap_init(uintptr_t start, size_t size) {
    // Ensure the heap is at least the minimum size
    if (size < HEAP_MIN_SIZE) {
        kprintf("Heap: Error - Heap size too small: %zu bytes\n", size);
        return;
    }
    
    // Align the start address
    uintptr_t aligned_start = (start + (HEAP_ALIGN - 1)) & ~(HEAP_ALIGN - 1);
    
    // Adjust the size to account for the alignment
    size -= (aligned_start - start);
    
    // Save the heap parameters
    heap_start = aligned_start;
    heap_size = size;
    heap_end = heap_start + heap_size;
    
    // Create the initial free block
    block_header_t* initial_block = (block_header_t*)heap_start;
    initial_block->magic = HEAP_MAGIC;
    initial_block->flags = BLOCK_FREE | BLOCK_LAST;
    initial_block->size = heap_size - sizeof(block_header_t);
    initial_block->prev = NULL;
    initial_block->next = NULL;
    
    // Initialize the free list
    free_list = initial_block;
    
    // No space used initially
    used_space = 0;
    
    kprintf("Heap: Initialized at %p, size: %zu bytes\n", (void*)heap_start, heap_size);
}

/**
 * @brief Allocate memory from the kernel heap
 * 
 * @param size Size to allocate in bytes
 * @return Pointer to the allocated memory, or NULL if out of memory
 */
void* kmalloc(size_t size) {
    // Check for valid size
    if (size == 0 || size > HEAP_MAX_ALLOC) {
        kprintf("Heap: Error - Invalid allocation size: %zu bytes\n", size);
        return NULL;
    }
    
    // Align the size
    size = (size + (HEAP_ALIGN - 1)) & ~(HEAP_ALIGN - 1);
    
    // Ensure minimum allocation size
    if (size < HEAP_MIN_ALLOC) {
        size = HEAP_MIN_ALLOC;
    }
    
    // Allocate the block
    return heap_allocate_block(size);
}

/**
 * @brief Allocate aligned memory from the kernel heap
 * 
 * @param size Size to allocate in bytes
 * @param alignment Alignment in bytes (must be power of 2)
 * @return Pointer to the allocated memory, or NULL if out of memory
 */
void* kmalloc_aligned(size_t size, size_t alignment) {
    // Check for valid size and alignment
    if (size == 0 || size > HEAP_MAX_ALLOC || alignment == 0 || (alignment & (alignment - 1)) != 0) {
        kprintf("Heap: Error - Invalid aligned allocation parameters: size=%zu, alignment=%zu\n", size, alignment);
        return NULL;
    }
    
    // Ensure alignment is at least HEAP_ALIGN
    if (alignment < HEAP_ALIGN) {
        alignment = HEAP_ALIGN;
    }
    
    // Allocate extra space to ensure we can align properly
    size_t total_size = size + alignment;
    
    // Allocate a block with extra space
    void* ptr = kmalloc(total_size);
    if (ptr == NULL) {
        return NULL;
    }
    
    // Calculate the aligned address
    uintptr_t addr = (uintptr_t)ptr;
    uintptr_t aligned_addr = (addr + alignment - 1) & ~(alignment - 1);
    
    // If already aligned, return as is
    if (addr == aligned_addr) {
        return ptr;
    }
    
    // Calculate the offset and store it just before the aligned address
    uintptr_t* offset_ptr = (uintptr_t*)(aligned_addr - sizeof(uintptr_t));
    *offset_ptr = aligned_addr - addr;
    
    return (void*)aligned_addr;
}

/**
 * @brief Free memory allocated from the kernel heap
 * 
 * @param ptr Pointer to the memory to free
 */
void kfree(void* ptr) {
    // Check for NULL pointer
    if (ptr == NULL) {
        return;
    }
    
    // Check if this is an aligned allocation
    uintptr_t addr = (uintptr_t)ptr;
    uintptr_t* offset_ptr = (uintptr_t*)(addr - sizeof(uintptr_t));
    
    // Try to find the header
    block_header_t* header = (block_header_t*)(addr - sizeof(block_header_t));
    
    // Check if this appears to be a valid block
    if (header->magic == HEAP_MAGIC && (header->flags & BLOCK_FREE) == 0) {
        // Valid header found, free the block
        heap_free_block(ptr);
        return;
    }
    
    // Check if this is an aligned allocation
    if (addr >= heap_start + sizeof(uintptr_t) && addr < heap_end) {
        uintptr_t offset = *offset_ptr;
        
        // Check if the offset looks valid
        if (offset > 0 && offset < HEAP_MAX_ALLOC) {
            // Try to free the original allocation
            heap_free_block((void*)(addr - offset));
            return;
        }
    }
    
    // Invalid pointer
    kprintf("Heap: Error - Attempt to free invalid pointer: %p\n", ptr);
}

/**
 * @brief Reallocate memory from the kernel heap
 * 
 * @param ptr Pointer to the memory to reallocate
 * @param size New size in bytes
 * @return Pointer to the reallocated memory, or NULL if out of memory
 */
void* krealloc(void* ptr, size_t size) {
    // If ptr is NULL, this is equivalent to kmalloc
    if (ptr == NULL) {
        return kmalloc(size);
    }
    
    // If size is 0, this is equivalent to kfree
    if (size == 0) {
        kfree(ptr);
        return NULL;
    }
    
    // Check for valid size
    if (size > HEAP_MAX_ALLOC) {
        kprintf("Heap: Error - Invalid reallocation size: %zu bytes\n", size);
        return NULL;
    }
    
    // Find the block header
    block_header_t* header = (block_header_t*)((uintptr_t)ptr - sizeof(block_header_t));
    
    // Check if this is a valid block
    if (header->magic != HEAP_MAGIC || (header->flags & BLOCK_FREE) != 0) {
        kprintf("Heap: Error - Attempt to reallocate invalid pointer: %p\n", ptr);
        return NULL;
    }
    
    // If new size is smaller, we can simply shrink the block
    if (size <= header->size) {
        // Align the size
        size = (size + (HEAP_ALIGN - 1)) & ~(HEAP_ALIGN - 1);
        
        // Ensure minimum allocation size
        if (size < HEAP_MIN_ALLOC) {
            size = HEAP_MIN_ALLOC;
        }
        
        // If new size is significantly smaller, split the block
        if (header->size - size >= sizeof(block_header_t) + HEAP_MIN_ALLOC) {
            // Split the block
            block_header_t* new_block = (block_header_t*)((uintptr_t)header + sizeof(block_header_t) + size);
            new_block->magic = HEAP_MAGIC;
            new_block->flags = BLOCK_FREE;
            new_block->size = header->size - size - sizeof(block_header_t);
            new_block->prev = header;
            new_block->next = header->next;
            
            // Update the current block
            header->size = size;
            header->next = new_block;
            
            // If this was the last block, update flags
            if (header->flags & BLOCK_LAST) {
                header->flags &= ~BLOCK_LAST;
                new_block->flags |= BLOCK_LAST;
            } else if (new_block->next) {
                new_block->next->prev = new_block;
            }
            
            // Add the new block to the free list
            new_block->next = free_list;
            if (free_list) {
                free_list->prev = new_block;
            }
            free_list = new_block;
            
            // Update used space
            used_space -= new_block->size + sizeof(block_header_t);
        }
        
        return ptr;
    }
    
    // Allocate a new, larger block
    void* new_ptr = kmalloc(size);
    if (new_ptr == NULL) {
        return NULL;
    }
    
    // Copy the data from the old block to the new block
    memcpy(new_ptr, ptr, header->size);
    
    // Free the old block
    kfree(ptr);
    
    return new_ptr;
}

/**
 * @brief Get the size of an allocated block
 * 
 * @param ptr Pointer to the allocated memory
 * @return Size of the allocated block in bytes, or 0 if invalid
 */
size_t ksize(void* ptr) {
    // Check for NULL pointer
    if (ptr == NULL) {
        return 0;
    }
    
    // Find the block header
    block_header_t* header = (block_header_t*)((uintptr_t)ptr - sizeof(block_header_t));
    
    // Check if this is a valid block
    if (header->magic != HEAP_MAGIC || (header->flags & BLOCK_FREE) != 0) {
        kprintf("Heap: Error - Attempt to get size of invalid pointer: %p\n", ptr);
        return 0;
    }
    
    return header->size;
}

/**
 * @brief Print heap statistics
 */
void heap_stats(void) {
    // Count the number of allocated and free blocks
    size_t allocated_blocks = 0;
    size_t free_blocks = 0;
    size_t free_space = 0;
    
    // Traverse all blocks
    block_header_t* block = (block_header_t*)heap_start;
    while (block) {
        if (block->magic != HEAP_MAGIC) {
            kprintf("Heap: Error - Corrupted heap detected at %p\n", block);
            return;
        }
        
        if (block->flags & BLOCK_FREE) {
            free_blocks++;
            free_space += block->size;
        } else {
            allocated_blocks++;
        }
        
        if (block->flags & BLOCK_LAST) {
            break;
        }
        
        block = (block_header_t*)((uintptr_t)block + sizeof(block_header_t) + block->size);
    }
    
    // Print statistics
    kprintf("Heap Statistics:\n");
    kprintf("  Total Size: %zu bytes\n", heap_size);
    kprintf("  Used Space: %zu bytes (%.2f%%)\n", used_space, (double)used_space / heap_size * 100.0);
    kprintf("  Free Space: %zu bytes (%.2f%%)\n", free_space, (double)free_space / heap_size * 100.0);
    kprintf("  Allocated Blocks: %zu\n", allocated_blocks);
    kprintf("  Free Blocks: %zu\n", free_blocks);
}

/**
 * @brief Allocate a block from the heap
 * 
 * @param size Size to allocate in bytes
 * @return Pointer to the allocated memory, or NULL if out of memory
 */
static void* heap_allocate_block(size_t size) {
    // Check if heap is initialized
    if (heap_start == 0) {
        kprintf("Heap: Error - Heap not initialized\n");
        return NULL;
    }
    
    // Find a suitable free block
    block_header_t* block = heap_find_free_block(size);
    if (block == NULL) {
        kprintf("Heap: Error - Out of memory (requested %zu bytes)\n", size);
        return NULL;
    }
    
    // Remove the block from the free list
    if (block->prev && (block->prev->flags & BLOCK_FREE)) {
        block->prev->next = block->next;
    } else {
        free_list = block->next;
    }
    
    if (block->next && (block->next->flags & BLOCK_FREE)) {
        block->next->prev = block->prev;
    }
    
    // Split the block if it's larger than needed
    if (block->size > size + sizeof(block_header_t) + HEAP_MIN_ALLOC) {
        block = heap_split_block(block, size);
    }
    
    // Mark the block as allocated
    block->flags &= ~BLOCK_FREE;
    
    // Update used space
    used_space += block->size + sizeof(block_header_t);
    
    // Return the pointer to the data area
    return (void*)((uintptr_t)block + sizeof(block_header_t));
}

/**
 * @brief Free a block in the heap
 * 
 * @param ptr Pointer to the memory to free
 * @return true if the block was freed, false otherwise
 */
static bool heap_free_block(void* ptr) {
    // Find the block header
    block_header_t* block = (block_header_t*)((uintptr_t)ptr - sizeof(block_header_t));
    
    // Check if this is a valid block
    if (block->magic != HEAP_MAGIC) {
        kprintf("Heap: Error - Invalid block magic: %p (%08x)\n", block, block->magic);
        return false;
    }
    
    // Check if the block is already free
    if (block->flags & BLOCK_FREE) {
        kprintf("Heap: Error - Block already free: %p\n", block);
        return false;
    }
    
    // Mark the block as free
    block->flags |= BLOCK_FREE;
    
    // Update used space
    used_space -= block->size + sizeof(block_header_t);
    
    // Add the block to the free list
    block->next = free_list;
    block->prev = NULL;
    if (free_list != NULL) {
        free_list->prev = block;
    }
    free_list = block;
    
    // Try to merge adjacent free blocks
    heap_merge_blocks(block);
    
    return true;
}

/**
 * @brief Find a free block with sufficient size
 * 
 * @param size Size needed in bytes
 * @return Pointer to a suitable free block, or NULL if none found
 */
static block_header_t* heap_find_free_block(size_t size) {
    // Search through the free list
    block_header_t* best_block = NULL;
    size_t best_size = SIZE_MAX;
    
    // Traverse the free list to find the best fit
    block_header_t* block = free_list;
    while (block != NULL) {
        // Check if the block is free and large enough
        if ((block->flags & BLOCK_FREE) && block->size >= size) {
            // Check if this is a better fit than the previous best
            if (block->size < best_size) {
                best_block = block;
                best_size = block->size;
                
                // Perfect fit, stop searching
                if (best_size == size) {
                    break;
                }
            }
        }
        
        block = block->next;
    }
    
    return best_block;
}

/**
 * @brief Split a block into two blocks
 * 
 * @param block Block to split
 * @param size Size of the first part
 * @return Pointer to the first block
 */
static block_header_t* heap_split_block(block_header_t* block, size_t size) {
    // Calculate the address of the new block
    block_header_t* new_block = (block_header_t*)((uintptr_t)block + sizeof(block_header_t) + size);
    
    // Initialize the new block
    new_block->magic = HEAP_MAGIC;
    new_block->flags = BLOCK_FREE;
    new_block->size = block->size - size - sizeof(block_header_t);
    new_block->prev = block;
    new_block->next = block->next;
    
    // Update next block if this wasn't the last block
    if (!(block->flags & BLOCK_LAST)) {
        // Find the next block
        block_header_t* next_block = (block_header_t*)((uintptr_t)block + sizeof(block_header_t) + block->size);
        
        // Update the next block's previous pointer
        if (next_block->magic == HEAP_MAGIC) {
            next_block->prev = new_block;
        }
    } else {
        // Transfer the last flag to the new block
        new_block->flags |= BLOCK_LAST;
        block->flags &= ~BLOCK_LAST;
    }
    
    // Update the block
    block->size = size;
    block->next = new_block;
    
    // Add the new block to the free list
    new_block->next = free_list;
    if (free_list != NULL) {
        free_list->prev = new_block;
    }
    free_list = new_block;
    
    return block;
}

/**
 * @brief Try to merge adjacent free blocks
 * 
 * @param block Block to start merging from
 * @return true if any blocks were merged, false otherwise
 */
static bool heap_merge_blocks(block_header_t* block) {
    bool merged = false;
    
    // Check if we can merge with the next block
    while (!(block->flags & BLOCK_LAST)) {
        // Get the next physical block (not linked list next)
        block_header_t* next = (block_header_t*)((uintptr_t)block + sizeof(block_header_t) + block->size);
        
        // Check if the next block is valid and free
        if (next->magic == HEAP_MAGIC && (next->flags & BLOCK_FREE)) {
            // Remove next block from the free list
            if (next->prev && (next->prev->flags & BLOCK_FREE)) {
                next->prev->next = next->next;
            } else {
                free_list = next->next;
            }
            
            if (next->next && (next->next->flags & BLOCK_FREE)) {
                next->next->prev = next->prev;
            }
            
            // Merge the blocks
            if (next->flags & BLOCK_LAST) {
                block->flags |= BLOCK_LAST;
            }
            block->size += sizeof(block_header_t) + next->size;
            
            // Ensure we don't leave a dangling reference
            next->magic = 0;
            
            merged = true;
        } else {
            // Can't merge, stop
            break;
        }
    }
    
    // Check if we can merge with the previous block
    while (block->prev != NULL && (block->prev->flags & BLOCK_FREE)) {
        block_header_t* prev = block->prev;
        
        // Check if these are physically adjacent blocks
        if ((uintptr_t)prev + sizeof(block_header_t) + prev->size == (uintptr_t)block) {
            // Remove block from the free list
            if (block->prev && (block->prev->flags & BLOCK_FREE)) {
                block->prev->next = block->next;
            } else {
                free_list = block->next;
            }
            
            if (block->next && (block->next->flags & BLOCK_FREE)) {
                block->next->prev = block->prev;
            }
            
            // Merge the blocks
            if (block->flags & BLOCK_LAST) {
                prev->flags |= BLOCK_LAST;
            }
            prev->size += sizeof(block_header_t) + block->size;
            prev->next = block->next;
            
            // Ensure we don't leave a dangling reference
            block->magic = 0;
            
            // Continue with prev block
            block = prev;
            
            merged = true;
        } else {
            // Can't merge, stop
            break;
        }
    }
    
    return merged;
}

/**
 * @brief Check heap integrity
 * 
 * @return true if the heap is intact, false otherwise
 */
static bool heap_check_integrity(void) {
    if (heap_start == 0 || heap_size == 0) {
        return false;
    }
    
    block_header_t* block = (block_header_t*)heap_start;
    bool found_last = false;
    
    while (!found_last) {
        // Check block magic
        if (block->magic != HEAP_MAGIC) {
            kprintf("Heap: Error - Invalid block magic at %p (%08x)\n", block, block->magic);
            return false;
        }
        
        // Check if we've reached the last block
        found_last = (block->flags & BLOCK_LAST) != 0;
        
        // Make sure we're still within the heap bounds
        if ((uintptr_t)block + sizeof(block_header_t) + block->size > heap_end) {
            kprintf("Heap: Error - Block extends beyond heap bounds at %p\n", block);
            return false;
        }
        
        // Move to the next physical block
        if (!found_last) {
            block = (block_header_t*)((uintptr_t)block + sizeof(block_header_t) + block->size);
        }
    }
    
    return true;
}

/**
 * @brief Dump the heap state for debugging
 */
static void heap_dump(void) {
    kprintf("Heap Dump:\n");
    kprintf("  Start: %p\n", (void*)heap_start);
    kprintf("  End: %p\n", (void*)heap_end);
    kprintf("  Size: %zu bytes\n", heap_size);
    kprintf("  Used: %zu bytes\n", used_space);
    
    // Traverse all blocks
    block_header_t* block = (block_header_t*)heap_start;
    size_t block_idx = 0;
    
    while (block) {
        kprintf("  Block %zu at %p:\n", block_idx++, block);
        kprintf("    Magic: %08x\n", block->magic);
        kprintf("    Flags: %08x (%s%s)\n", block->flags,
                (block->flags & BLOCK_FREE) ? "FREE" : "USED",
                (block->flags & BLOCK_LAST) ? " LAST" : "");
        kprintf("    Size: %zu bytes\n", block->size);
        kprintf("    Prev: %p\n", block->prev);
        kprintf("    Next: %p\n", block->next);
        
        if (block->flags & BLOCK_LAST) {
            break;
        }
        
        block = (block_header_t*)((uintptr_t)block + sizeof(block_header_t) + block->size);
    }
}
