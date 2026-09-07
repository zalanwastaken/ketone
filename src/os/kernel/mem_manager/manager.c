#include "manager.h"

heap_t *root_heap;

void mem_manager_init(size_t size, uint32_t kernel_size){
    kernel_size *= 512;

    root_heap = (heap_t*)(kernel_size+0x128);
    root_heap->max_size = size;
    root_heap->block_size = 512;
    root_heap->blocks = (allocation_t*)((uintptr_t)root_heap + sizeof(heap_t));
    root_heap->start_addr = (uintptr_t)root_heap->blocks + sizeof(allocation_t) * (size / root_heap->block_size);

    for(uint64_t i = 0; i<size/root_heap->block_size; i++){
        root_heap->blocks[i].isAllocated = false;
        root_heap->blocks[i].start_block = 0;
        root_heap->blocks[i].end_block = 0;
    }
}

void* alloc(heap_t **heap, size_t size){
    if(size == 0){
        return NULL;
    }

    size_t got = 0;
    int64_t start = -1; //? used -1 here because start cannot be in -ve so -1 here means no valid start was found
    uint64_t i = 0;

    while ((*heap)->max_size/(*heap)->block_size > i){
        if((*heap)->blocks[i].isAllocated == false){
            if(start == -1){
                start = i;
            }
            got++;
        }else{
            start = -1;
            got = 0;
        }
        if(got*(*heap)->block_size >= size){
            break;
        }
        i++;

    }
    if(start == -1){
        return NULL;
    }

    for(uint64_t f = start; f<start+got; f++){
        (*heap)->blocks[f].isAllocated = true;
        (*heap)->blocks[f].start_block = start;
        (*heap)->blocks[f].end_block = start+got;
    }

    return (void*)((*heap)->start_addr+(start*(*heap)->block_size));
}

void free(heap_t **heap, void *ptr){
    if(ptr == NULL){
        return;
    }

    uint64_t block = (uint64_t)ptr;
    block -= (*heap)->start_addr;
    block = block/(*heap)->block_size;
    uint64_t end = (*heap)->blocks[block].end_block;

    for(uint64_t i=block; i<end; i++){
        (*heap)->blocks[i].isAllocated = false;
        (*heap)->blocks[i].start_block = 0;
        (*heap)->blocks[i].end_block = 0;
    }
}

void* kalloc(size_t size){
    return alloc(&root_heap, size);
}

void kfree(void *ptr){
    free(&root_heap, ptr);
}
