#ifndef KALLOC_H
#define KALLOC_H
#include <stdint.h>
#include <stddef.h>

void kalloc_init(uint32_t start_phys);
void* kmalloc(size_t n);
uint32_t kalloc_get_ptr(void);
uint32_t kalloc_get_start(void);
uint32_t kalloc_bytes_used(void);

#endif

