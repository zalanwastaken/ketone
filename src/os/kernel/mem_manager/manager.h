#pragma once

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

typedef struct{
    bool isAllocated;
    uint64_t start_block;
    uint64_t end_block;
} allocation_t;

typedef struct{
    uint16_t block_size;
    uint64_t start_addr;
    uint64_t max_size;
    allocation_t *blocks;
} heap_t;

void mem_manager_init(size_t size, uint32_t kernel_size);
void* alloc(heap_t **heap, size_t size);
void* kalloc(size_t size);
void kfree(void *ptr);
