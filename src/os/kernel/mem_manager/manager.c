#include "manager.h"
#include "../io/serial/serial.h"
#include "../utils.h"

#define MAPPED_LIMIT 0x100000000ULL

#define KERNEL_START 0x120000

#define E820_MAX_ENTRIES 64
#define E820_USABLE       1
#define E820_RESERVED     2
#define E820_ACPI_RECLAIM 3
#define E820_ACPI_NVS     4
#define E820_BAD_MEMORY   5

typedef struct __attribute__((packed)){
    uint64_t base;
    uint64_t length;
    uint32_t type;
    uint32_t attributes;
} e820_entry;

typedef struct __attribute__((packed)){
    uint16_t count;
    e820_entry entries[E820_MAX_ENTRIES];
} e820_map;

static heap_t *root_heap = NULL;
static heap_t *low_heap = NULL;
static e820_map *memory_map = NULL;

bool e820_find_region(uint32_t type, uint64_t min_size, uint64_t min_addr, uint64_t max_addr, uint64_t align, uint64_t *out_base, uint64_t *out_size){
    if (memory_map == NULL){
        return false;
    }
    if (align == 0){
        align = 1;
    }
    //* must be a power of two
    if (align & (align - 1)){
        return false;
    }

    for (uint16_t i = 0; i < memory_map->count; i++) {
        e820_entry *e = &memory_map->entries[i];
        if (e->type != type){
            continue;
        }

        uint64_t start = e->base > min_addr ? e->base : min_addr;

        //? guard against a bogus entry whose base + length wraps around
        uint64_t end = (e->length > UINT64_MAX - e->base)? UINT64_MAX: e->base + e->length;
        if (end > max_addr){
            end = max_addr;
        }

        //? align the start up, guarding against overflow
        if (start > UINT64_MAX - (align - 1)){
            continue;
        }
        start = (start + align - 1) & ~(align - 1);

        if (end <= start){
            continue;
        }
        if (end - start < min_size){
            continue;
        }
        if (out_base){
            *out_base = start;
        }
        if (out_size){
            *out_size = end - start;
        }
        return true;
    }

    return false;
}

static bool low_heap_init(size_t low_heap_size){
    uint64_t base = 0;
    size_t size = 0;

    if(!e820_find_region(E820_USABLE, low_heap_size, 0x0, UINT32_MAX, 0x1000, &base, &size)){
        return false;
    }

    low_heap = (heap_t*)base;
    low_heap->block_size = 512;
    low_heap->blocks = (allocation_t*)(base + sizeof(heap_t));

    //* each block costs 512 data bytes + one table entry; leave 4095 for page alignment
    uint64_t nblocks = (low_heap_size - sizeof(heap_t) - 0xFFF) / (512 + sizeof(allocation_t));

    low_heap->max_size = nblocks * 512;
    low_heap->start_addr = ((uintptr_t)low_heap->blocks + nblocks * sizeof(allocation_t) + 0xFFF) & ~(uintptr_t)0xFFF;


    serial_print("MEMORY MAN: low region at: ");
    serial_print_hex64LN(low_heap->start_addr);

    for(uint64_t i = 0; i < nblocks; i++){
        low_heap->blocks[i].end_block   = 0;
        low_heap->blocks[i].start_block = 0;
        low_heap->blocks[i].isAllocated = false;
    }
    return true;
}

void mem_manager_init(size_t size, uint32_t kernel_size_SECTORS, uint32_t *e820_addr, size_t low_heap_size){
    kernel_size_SECTORS *= 512;
    memory_map = (e820_map*)e820_addr;

    uint64_t kernel_end = KERNEL_START + (uint64_t)kernel_size_SECTORS;
    uint64_t table_bytes = sizeof(allocation_t) * (size / 512);
    uint64_t total = sizeof(heap_t) + table_bytes + 0xFFF + size;

    e820_entry *chosen = NULL;
    uint64_t root_heap_start = 0;

    for (uint16_t i = 0; i < memory_map->count; i++) {
        e820_entry *e = &memory_map->entries[i];
        if (e->type != E820_USABLE){
            continue;
        }

        uint64_t start = e->base > kernel_end ? e->base : kernel_end;
        start = (start + 0xFFF) & ~0xFFFULL;
        uint64_t end = e->base + e->length;
        if (end > MAPPED_LIMIT){
            end = MAPPED_LIMIT;
        }

        if (end <= start || end - start < total){
            continue;
        }

        root_heap_start = start;
        chosen = e;
        break;
    }

    if (chosen == NULL) {
        serial_printLN("MEMORY MAN: Unable to find usable memory");
        halt();
    }
    serial_print("MEMORY MAN: region at: ");
    serial_print_hex64LN(root_heap_start);

    root_heap = (heap_t*)root_heap_start;
    root_heap->max_size = size;
    root_heap->block_size = 512;
    root_heap->blocks = (allocation_t*)((uintptr_t)root_heap + sizeof(heap_t));

    uintptr_t table_end = (uintptr_t)root_heap->blocks + table_bytes;
    root_heap->start_addr = (table_end + 0xFFF) & ~(uintptr_t)0xFFF;

    // ---- update the map: reserve everything up to the end of the heap ----
    uint64_t heap_end  = root_heap->start_addr + size;           // page aligned
    uint64_t orig_end  = chosen->base + chosen->length;          // unclamped original end

    chosen->type   = E820_RESERVED;
    chosen->length = heap_end - chosen->base;

    if (orig_end > heap_end && memory_map->count < E820_MAX_ENTRIES) {
        e820_entry *rest = &memory_map->entries[memory_map->count];
        rest->base = heap_end;
        rest->length = orig_end - heap_end;
        rest->type = E820_USABLE;
        rest->attributes = 1;
        memory_map->count++;
    }

    for (uint64_t i = 0; i < size / root_heap->block_size; i++) {
        root_heap->blocks[i].isAllocated = false;
        root_heap->blocks[i].start_block = 0;
        root_heap->blocks[i].end_block = 0;
    }

    e820_map *map_buff = (e820_map*)kalloc(sizeof(e820_map));
    if (map_buff == NULL){
        serial_printLN("MEMORY MAN: map copy alloc failed");
        halt();
    }
    memcpy(memory_map, map_buff, sizeof(e820_map));
    memory_map = map_buff;

    if (!low_heap_init(low_heap_size)) {
        serial_printLN("MEMORY MAN: Unable to setup low heap");
        halt();
    }
}

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

void* kalloc(size_t size){
    return alloc(&root_heap, size);
}

void* low_kalloc(size_t size){
    return alloc(&low_heap, size);
}

void* kalloc_aligned(size_t size, size_t align){
    return alloc_aligned(&root_heap, size, align);
}

void* low_kalloc_aligned(size_t size, size_t align){
    return alloc_aligned(&low_heap, size,  align);
}

void kfree(void *ptr){
    free(&root_heap, ptr);
}

void low_kfree(void *ptr){
    free(&low_heap, ptr);
}
