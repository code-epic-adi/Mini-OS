#include "kalloc.h"
#include <stdint.h>
#include <stddef.h>

static uint32_t heap_start = 0;
static uint32_t heap_ptr = 0;

void kalloc_init(uint32_t start_phys){
    if(start_phys == 0) return;
    heap_start = start_phys;
    heap_ptr = start_phys;
}

/* minimal 8-byte align bump allocator */
void* kmalloc(size_t n){
    if(heap_ptr == 0) return (void*)0;
    /* align to 8 */
    uint32_t cur = (heap_ptr + 7) & ~((uint32_t)7);
    uint32_t next = cur + (uint32_t)n;
    /* naive overflow/limit check is omitted — you can add checks if you like */
    heap_ptr = next;
    return (void*)(uintptr_t)cur;
}

uint32_t kalloc_get_ptr(void){ return heap_ptr; }

uint32_t kalloc_get_start(void){ return heap_start; }

uint32_t kalloc_bytes_used(void){
    if(heap_ptr < heap_start) return 0;
    return heap_ptr - heap_start;
}
