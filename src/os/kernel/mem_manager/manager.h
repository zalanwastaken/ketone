#pragma once

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

typedef struct{ //! NOTE TO SELF: this uses a fuck ton of mem, optimise it !
    bool isAllocated;
    uint32_t start_block;
    uint32_t end_block;
} allocation_t;

typedef struct{
    uint16_t block_size;
    uint64_t start_addr;
    uint64_t max_size;
    allocation_t *blocks;
} heap_t;

extern heap_t *root_heap;

void mem_manager_init(size_t size, uint32_t kernel_size_SECTORS);
void* alloc(heap_t **heap, size_t size);
void free(heap_t **heap, void *ptr);
void* kalloc(size_t size);
void kfree(void *ptr);
