#include <stdint.h>

#define PAGE_PRESENT 0x001
#define PAGE_RW      0x002
#define PAGE_USER    0x004

#define PAGE_SIZE    4096
#define NUM_TABLES   8      // 8 * 4 MiB = 32 MiB identity-mapped

__attribute__((aligned(4096)))
static uint32_t page_directory[1024];

__attribute__((aligned(4096)))
static uint32_t page_tables[NUM_TABLES][1024];

void paging_init(void){
    // clear directory
    for(int i=0;i<1024;i++) page_directory[i] = 0;

    uint32_t flags = PAGE_PRESENT | PAGE_RW;

    // identity-map first 32 MiB
    for(int t=0;t<NUM_TABLES;t++){
        uint32_t pt_phys = (uint32_t)page_tables[t];
        page_directory[t] = pt_phys | flags;   // PDE for 4 MiB chunk

        for(int i=0;i<1024;i++){
            uint32_t page_index = t*1024 + i;          // page number
            uint32_t addr       = page_index * PAGE_SIZE; // physical addr
            page_tables[t][i] = addr | flags;          // PTE
        }
    }

    // load directory into CR3
    __asm__ volatile("mov %0, %%cr3" :: "r"(page_directory));

    // enable paging bit in CR0
    uint32_t cr0;
    __asm__ volatile("mov %%cr0, %0" : "=r"(cr0));
    cr0 |= 0x80000000u; // set PG bit
    __asm__ volatile("mov %0, %%cr0" :: "r"(cr0));
}
