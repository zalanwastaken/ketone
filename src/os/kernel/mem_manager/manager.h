#pragma once

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

//TODO
//! NOTE: this uses a fuck ton of mem, optimise it !
typedef struct{
    bool isAllocated;
    uint32_t start_block;
    uint32_t end_block;
} allocation_t;

typedef struct{
    uint16_t block_size;
    uint64_t start_addr;
    size_t max_size;
    allocation_t *blocks;
} heap_t;

typedef struct{
    uint64_t start;
    uint64_t next;
    size_t size;
} bump_heap_t;

void* bump_alloc(bump_heap_t **heap, size_t size);
void* bump_alloc_aligned(bump_heap_t **heap, size_t size, size_t align);
void* alloc(heap_t **heap, size_t size);
void* alloc_aligned(heap_t **heap, size_t size, size_t alignment);
void free(heap_t **heap, void *ptr);

void mem_manager_init(size_t size, uint32_t kernel_size_SECTORS, uint32_t *e820_addr, size_t low_heap_size);
void* kalloc(size_t size);
void* low_kalloc(size_t size);
void* kalloc_aligned(size_t size, size_t align);
void* low_kalloc_aligned(size_t size, size_t align);
void kfree(void *ptr);
void low_kfree(void *ptr);
void* k_bump_alloc(size_t size);
void* k_bump_alloc_aligned(size_t size, size_t align);
