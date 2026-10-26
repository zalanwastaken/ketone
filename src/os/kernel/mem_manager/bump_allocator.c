#include "manager.h"

void* bump_alloc_aligned(bump_heap_t **heap, size_t size, size_t align){
    if(size == 0){
        return NULL;
    }
    if(align == 0){
        align = 1;
    }
    if(align & (align - 1)){
        return NULL;                      //? alignment must be a power of two
    }

    bump_heap_t *h = *heap;
    uint64_t end  = h->start + h->size;
    uint64_t addr = (h->next + align - 1) & ~((uint64_t)align - 1);

    //? written as subtraction so it can't overflow
    if(addr > end || size > end - addr){
        return NULL;
    }

    h->next = addr + size;
    return (void*)addr;
}

void* bump_alloc(bump_heap_t **heap, size_t size){
    return bump_alloc_aligned(heap, size, 1);
}
