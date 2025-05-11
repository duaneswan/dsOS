/**
 * @file paging.c
 * @brief Virtual memory management implementation
 */

#include "../include/kernel.h"
#include "../include/memory.h"
#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>

// Page map level 4 (top level page table)
static uint64_t* pml4_table = NULL;

// Recursively mapped page tables
#define RECURSIVE_MAPPING_INDEX 510
#define PML4_SELF_REF          0xFFFFFF7FBFDFE000
#define PDP_SELF_REF           0xFFFFFF7FBFC00000
#define PD_SELF_REF            0xFFFFFF7F80000000
#define PT_SELF_REF            0xFFFFFF0000000000

// Page flags
#define PF_PRESENT               0x0001
#define PF_WRITABLE              0x0002
#define PF_USER                  0x0004
#define PF_WRITE_THROUGH         0x0008
#define PF_CACHE_DISABLE         0x0010
#define PF_ACCESSED              0x0020
#define PF_DIRTY                 0x0040
#define PF_LARGE_PAGE            0x0080
#define PF_GLOBAL                0x0100
#define PF_NX                    0x8000000000000000

// Forward declarations
static int map_page_internal(uintptr_t phys_addr, uintptr_t virt_addr, uint64_t flags);
static uint64_t* get_pml4_entry(uintptr_t virt_addr);
static uint64_t* get_pdp_entry(uintptr_t virt_addr);
static uint64_t* get_pd_entry(uintptr_t virt_addr);
static uint64_t* get_pt_entry(uintptr_t virt_addr);

/**
 * @brief Initialize the virtual memory manager
 */
void vm_init(void) {
    // Allocate a page for the PML4 table
    pml4_table = (uint64_t*)KERNEL_PHYSICAL_MAP;
    
    // Map kernel with higher-half mapping
    // The kernel itself is already mapped by the bootloader, 
    // so we just need to set up the appropriate structures
    
    // Set up the recursive mapping in the PML4 table
    pml4_table[RECURSIVE_MAPPING_INDEX] = (uintptr_t)pml4_table | PF_PRESENT | PF_WRITABLE;
    
    // Reload CR3
    uintptr_t cr3;
    __asm__ volatile("mov %%cr3, %0" : "=r"(cr3));
    __asm__ volatile("mov %0, %%cr3" : : "r"(cr3) : "memory");
    
    kprintf("VM: Virtual memory manager initialized\n");
}

/**
 * @brief Map a physical page to a virtual address
 * 
 * @param phys_addr Physical address of the page to map
 * @param virt_addr Virtual address where the page should be mapped
 * @param flags Page table entry flags
 * @return 0 on success, negative value on failure
 */
int map_page(uintptr_t phys_addr, uintptr_t virt_addr, uint64_t flags) {
    // Align addresses to page boundaries
    phys_addr &= PAGE_MASK;
    virt_addr &= PAGE_MASK;
    
    // Add default flags
    uint64_t entry_flags = PF_PRESENT;
    
    // Convert our flags to x86 page flags
    if (flags & PTE_WRITABLE)
        entry_flags |= PF_WRITABLE;
    if (flags & PTE_USER)
        entry_flags |= PF_USER;
    if (flags & PTE_WRITE_THROUGH)
        entry_flags |= PF_WRITE_THROUGH;
    if (flags & PTE_CACHE_DISABLE)
        entry_flags |= PF_CACHE_DISABLE;
    if (flags & PTE_GLOBAL)
        entry_flags |= PF_GLOBAL;
    if (flags & PTE_NX)
        entry_flags |= PF_NX;
    
    // Map the page
    return map_page_internal(phys_addr, virt_addr, entry_flags);
}

/**
 * @brief Internal function to map a physical page to a virtual address
 * 
 * @param phys_addr Physical address of the page to map
 * @param virt_addr Virtual address where the page should be mapped
 * @param flags Page table entry flags
 * @return 0 on success, negative value on failure
 */
static int map_page_internal(uintptr_t phys_addr, uintptr_t virt_addr, uint64_t flags) {
    // Check if the page is already mapped
    if (is_page_mapped(virt_addr)) {
        uint64_t* pt_entry = get_pt_entry(virt_addr);
        
        // Update flags if needed
        *pt_entry = (phys_addr & PAGE_MASK) | flags;
        return 0;
    }
    
    // Get or create the page table entries
    uint64_t* pml4_entry = get_pml4_entry(virt_addr);
    if (!(*pml4_entry & PF_PRESENT)) {
        uintptr_t pdp_table_phys = alloc_physical_page();
        if (!pdp_table_phys) {
            return -1; // Out of memory
        }
        
        // Map the new page directory pointer table
        *pml4_entry = pdp_table_phys | PF_PRESENT | PF_WRITABLE | PF_USER;
        
        // Clear the new table
        uint64_t* pdp_table = (uint64_t*)((uintptr_t)PDP_SELF_REF + ((virt_addr >> 39) & 0xFF8));
        for (int i = 0; i < 512; i++) {
            pdp_table[i] = 0;
        }
    }
    
    uint64_t* pdp_entry = get_pdp_entry(virt_addr);
    if (!(*pdp_entry & PF_PRESENT)) {
        uintptr_t pd_table_phys = alloc_physical_page();
        if (!pd_table_phys) {
            return -1; // Out of memory
        }
        
        // Map the new page directory table
        *pdp_entry = pd_table_phys | PF_PRESENT | PF_WRITABLE | PF_USER;
        
        // Clear the new table
        uint64_t* pd_table = (uint64_t*)((uintptr_t)PD_SELF_REF + ((virt_addr >> 30) & 0x3FFFF8));
        for (int i = 0; i < 512; i++) {
            pd_table[i] = 0;
        }
    }
    
    uint64_t* pd_entry = get_pd_entry(virt_addr);
    if (!(*pd_entry & PF_PRESENT)) {
        uintptr_t pt_table_phys = alloc_physical_page();
        if (!pt_table_phys) {
            return -1; // Out of memory
        }
        
        // Map the new page table
        *pd_entry = pt_table_phys | PF_PRESENT | PF_WRITABLE | PF_USER;
        
        // Clear the new table
        uint64_t* pt_table = (uint64_t*)((uintptr_t)PT_SELF_REF + ((virt_addr >> 21) & 0x7FFFFFF8));
        for (int i = 0; i < 512; i++) {
            pt_table[i] = 0;
        }
    }
    
    // Map the page
    uint64_t* pt_entry = get_pt_entry(virt_addr);
    *pt_entry = (phys_addr & PAGE_MASK) | flags;
    
    // Invalidate TLB entry
    __asm__ volatile("invlpg (%0)" : : "r"(virt_addr) : "memory");
    
    return 0;
}

/**
 * @brief Unmap a virtual address
 * 
 * @param virt_addr Virtual address to unmap
 * @return 0 on success, negative value on failure
 */
int unmap_page(uintptr_t virt_addr) {
    // Check if the page is mapped
    if (!is_page_mapped(virt_addr)) {
        return -1; // Page not mapped
    }
    
    // Clear the page table entry
    uint64_t* pt_entry = get_pt_entry(virt_addr);
    *pt_entry = 0;
    
    // Invalidate TLB entry
    __asm__ volatile("invlpg (%0)" : : "r"(virt_addr) : "memory");
    
    return 0;
}

/**
 * @brief Get the physical address for a virtual address
 * 
 * @param virt_addr Virtual address to translate
 * @return Physical address, or 0 if the virtual address is not mapped
 */
uintptr_t virtual_to_physical(uintptr_t virt_addr) {
    // Check if the page is mapped
    if (!is_page_mapped(virt_addr)) {
        return 0; // Page not mapped
    }
    
    // Get the page table entry
    uint64_t* pt_entry = get_pt_entry(virt_addr);
    
    // Combine the physical page address and the page offset
    return (*pt_entry & PAGE_MASK) | (virt_addr & PAGE_OFFSET_MASK);
}

/**
 * @brief Map multiple pages consecutively
 * 
 * @param phys_addr Physical address of the first page to map
 * @param virt_addr Virtual address where the pages should be mapped
 * @param count Number of pages to map
 * @param flags Page table entry flags
 * @return 0 on success, negative value on failure
 */
int map_pages(uintptr_t phys_addr, uintptr_t virt_addr, size_t count, uint64_t flags) {
    // Align addresses to page boundaries
    phys_addr &= PAGE_MASK;
    virt_addr &= PAGE_MASK;
    
    // Map each page
    for (size_t i = 0; i < count; i++) {
        int result = map_page(phys_addr, virt_addr, flags);
        if (result != 0) {
            // Failed to map a page, try to clean up
            for (size_t j = 0; j < i; j++) {
                unmap_page(virt_addr - (j * PAGE_SIZE));
            }
            return result;
        }
        
        phys_addr += PAGE_SIZE;
        virt_addr += PAGE_SIZE;
    }
    
    return 0;
}

/**
 * @brief Unmap multiple consecutive virtual addresses
 * 
 * @param virt_addr First virtual address to unmap
 * @param count Number of pages to unmap
 * @return 0 on success, negative value on failure
 */
int unmap_pages(uintptr_t virt_addr, size_t count) {
    // Align address to page boundary
    virt_addr &= PAGE_MASK;
    
    // Unmap each page
    for (size_t i = 0; i < count; i++) {
        unmap_page(virt_addr);
        virt_addr += PAGE_SIZE;
    }
    
    return 0;
}

/**
 * @brief Check if a virtual address is mapped
 * 
 * @param virt_addr Virtual address to check
 * @return true if the address is mapped, false otherwise
 */
bool is_page_mapped(uintptr_t virt_addr) {
    // Get the page table entries
    uint64_t* pml4_entry = get_pml4_entry(virt_addr);
    if (!(*pml4_entry & PF_PRESENT)) {
        return false;
    }
    
    uint64_t* pdp_entry = get_pdp_entry(virt_addr);
    if (!(*pdp_entry & PF_PRESENT)) {
        return false;
    }
    
    uint64_t* pd_entry = get_pd_entry(virt_addr);
    if (!(*pd_entry & PF_PRESENT)) {
        return false;
    }
    
    uint64_t* pt_entry = get_pt_entry(virt_addr);
    return (*pt_entry & PF_PRESENT) != 0;
}

/**
 * @brief Get the PML4 entry for a virtual address
 * 
 * @param virt_addr Virtual address
 * @return Pointer to the PML4 entry
 */
static uint64_t* get_pml4_entry(uintptr_t virt_addr) {
    return &pml4_table[(virt_addr >> 39) & 0x1FF];
}

/**
 * @brief Get the PDP entry for a virtual address
 * 
 * @param virt_addr Virtual address
 * @return Pointer to the PDP entry
 */
static uint64_t* get_pdp_entry(uintptr_t virt_addr) {
    uint64_t* pdp_table = (uint64_t*)((uintptr_t)PDP_SELF_REF + ((virt_addr >> 39) & 0xFF8));
    return &pdp_table[(virt_addr >> 30) & 0x1FF];
}

/**
 * @brief Get the PD entry for a virtual address
 * 
 * @param virt_addr Virtual address
 * @return Pointer to the PD entry
 */
static uint64_t* get_pd_entry(uintptr_t virt_addr) {
    uint64_t* pd_table = (uint64_t*)((uintptr_t)PD_SELF_REF + ((virt_addr >> 30) & 0x3FFFF8));
    return &pd_table[(virt_addr >> 21) & 0x1FF];
}

/**
 * @brief Get the PT entry for a virtual address
 * 
 * @param virt_addr Virtual address
 * @return Pointer to the PT entry
 */
static uint64_t* get_pt_entry(uintptr_t virt_addr) {
    uint64_t* pt_table = (uint64_t*)((uintptr_t)PT_SELF_REF + ((virt_addr >> 21) & 0x7FFFFFF8));
    return &pt_table[(virt_addr >> 12) & 0x1FF];
}
