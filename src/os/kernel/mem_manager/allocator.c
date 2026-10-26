#include "manager.h"

void* alloc(heap_t **heap, size_t size){
    if(size == 0){
        return NULL;
    }

    uint64_t needed = (size + (*heap)->block_size - 1) / (*heap)->block_size; //? this is a truncating division
    int64_t start = -1; //? used -1 here because start cannot be in -ve so -1 here means no valid start was found
    uint64_t i = 0;

    while ((*heap)->max_size / (*heap)->block_size > i){
        if ((*heap)->blocks[i].isAllocated == false){
            bool free = true;
            for (uint64_t j = 0; j < needed; j++){
                if ((*heap)->blocks[i + j].isAllocated){
                    free = false;
                    break;
                }
            }

            if (free){
                start = i;
                break;
            }
            i++;
        }else{
            start = -1;
            if ((*heap)->blocks[i].end_block > i){
                i = (*heap)->blocks[i].end_block;
            }else{
                i++;
            }
            continue;
        }
        i++;
    }

    if(start == -1){
        return NULL;
    }

    for(uint64_t f = start; f<start+needed; f++){
        (*heap)->blocks[f].isAllocated = true;
        (*heap)->blocks[f].start_block = start;
        (*heap)->blocks[f].end_block = start+needed;
    }

    return (void*)((*heap)->start_addr+(start*(*heap)->block_size));
}

void* alloc_aligned(heap_t **heap, size_t size, size_t alignment){
    if(size == 0){
        return NULL;
    }
    if(alignment == 0){
        alignment = 1;
    }
    if(alignment & (alignment - 1)){
        return NULL; //? alignment must be a power of two
    }

    heap_t *h = *heap;
    uint64_t bs = h->block_size;
    uint64_t total = h->max_size / bs;
    uint64_t needed = (size + bs - 1) / bs;

    if(needed > total){
        return NULL;
    }

    uint64_t i = 0;
    while(i + needed <= total){
        uintptr_t addr = h->start_addr + i * bs;

        //? this block's address isn't aligned, jump to the first block at or after the next aligned address
        if(addr & (alignment - 1)){
            uintptr_t aligned = (addr + alignment - 1) & ~((uintptr_t)alignment - 1);
            i = (aligned - h->start_addr + bs - 1) / bs; //? always > i, so this makes progress
            continue;
        }

        //? check that the whole run is free
        bool run_free = true;
        uint64_t j;
        for(j = 0; j < needed; j++){
            if(h->blocks[i + j].isAllocated){
                run_free = false;
                break;
            }
        }

        if(!run_free){
            //? skip past the allocation we collided with
            uint64_t end = h->blocks[i + j].end_block;
            i = (end > i + j) ? end : i + j + 1;
            continue;
        }

        for(uint64_t f = i; f < i + needed; f++){
            h->blocks[f].isAllocated = true;
            h->blocks[f].start_block = i;
            h->blocks[f].end_block = i + needed;
        }

        return (void*)addr;
    }

    return NULL;
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
