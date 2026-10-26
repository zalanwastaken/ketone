#include "manager.h"
#include "../io/serial/serial.h"
#include "../utils.h"
#include "../drivers/acpi/acpi.h"


#define MAPPED_LIMIT 0x100000000ULL

#define KERNEL_START 0x120000

#define E820_MAX_ENTRIES 64
#define E820_USABLE       1
#define E820_RESERVED     2
#define E820_ACPI_RECLAIM 3
#define E820_ACPI_NVS     4
#define E820_BAD_MEMORY   5

#define FOUR_GIB 0x100000000ULL
#define MAX_RANGES (E820_MAX_ENTRIES + 8)

typedef struct {
    uint64_t base_address;
    uint16_t segment_group;
    uint8_t  start_bus;
    uint8_t  end_bus;
    uint32_t reserved;
} __attribute__((packed)) mcfg_entry_t;

typedef struct {
    rsdt_header_t header;
    uint8_t reserved[8];
    mcfg_entry_t entries[];
} __attribute__((packed)) mcfg_t;

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

typedef struct{
    uint64_t base;
    uint64_t end;
}range_t;

static range_t ranges[MAX_RANGES];
static int range_count;

static heap_t *root_heap = NULL;
static heap_t *low_heap = NULL;
static bump_heap_t *bump_heap = NULL;
static e820_map *memory_map = NULL;

static bool e820_find_region(uint32_t type, uint64_t min_size, uint64_t min_addr, uint64_t max_addr, uint64_t align, uint64_t *out_base, uint64_t *out_size){
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

static void range_add(uint64_t base, uint64_t end){
    if (base >= FOUR_GIB || end <= base || range_count >= MAX_RANGES) return;
    if (end > FOUR_GIB) end = FOUR_GIB;
    ranges[range_count++] = (range_t){ base, end };
}

static bool find_mmio_hole(uint64_t *out_base, uint64_t *out_end){
    range_count = 0;

    // 1. every E820 entry, regardless of type
    for (uint16_t i = 0; i < memory_map->count; i++) {
        e820_entry *e = &memory_map->entries[i];
        range_add(e->base, e->base + e->length);
    }

    // 2. fixed devices that E820 usually doesn't list
    range_add(0xFEC00000, 0xFEC01000);   // I/O APIC
    range_add(0xFED00000, 0xFED00400);   // HPET
    range_add(0xFEE00000, 0xFEE01000);   // local APIC
    range_add(0xFFC00000, FOUR_GIB);     // firmware flash
    mcfg_t *mcfg = (mcfg_t*)ACPI_get_table("MCFG");
    uint32_t n = (mcfg->header.length - sizeof(rsdt_header_t)) / sizeof(mcfg_entry_t);
    mcfg_entry_t *e = (mcfg_entry_t*)((uint8_t*)mcfg + sizeof(rsdt_header_t));

    for (uint32_t i = 0; i < n; i++) {
        uint64_t base = e[i].base_address + ((uint64_t)e[i].start_bus << 20);
        uint64_t end  = e[i].base_address + (((uint64_t)e[i].end_bus + 1) << 20);
        range_add(base, end);

        serial_print("ECAM seg ");  serial_print_hexLN(e[i].segment_group);
        serial_print_hex64LN(base);
        serial_print_hex64LN(end);
    }

    // 3. insertion sort by base (at most ~70 entries)
    for (int i = 1; i < range_count; i++) {
        range_t key = ranges[i];
        int j = i - 1;
        while (j >= 0 && ranges[j].base > key.base) { ranges[j + 1] = ranges[j]; j--; }
        ranges[j + 1] = key;
    }

    // 4. walk and track the largest gap
    uint64_t cursor = 0, best_base = 0, best_size = 0;
    for (int i = 0; i < range_count; i++) {
        if (ranges[i].base > cursor && ranges[i].base - cursor > best_size) {
            best_base = cursor;
            best_size = ranges[i].base - cursor;
        }
        if (ranges[i].end > cursor) cursor = ranges[i].end;
    }
    if (FOUR_GIB > cursor && FOUR_GIB - cursor > best_size) {
        best_base = cursor;
        best_size = FOUR_GIB - cursor;
    }

    if (best_size == 0) return false;
    *out_base = best_base;
    *out_end  = best_base + best_size;
    return true;
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

    uint64_t start_hole = 0;
    uint64_t end_hole = 0;
    bool success = find_mmio_hole(&start_hole, &end_hole);
    if(success){
        serial_print("MMIO hole start: ");
        serial_print_hex64LN(start_hole);
        serial_print("MMIO hole end: ");
        serial_print_hex64LN(end_hole);
    }else{
        serial_printLN("MEMORY MAN: unable to find MMIO hole");
        halt();
    }

    bump_heap = (bump_heap_t*)kalloc(sizeof(bump_heap_t));

    bump_heap->start = start_hole;
    bump_heap->size = end_hole-start_hole;
    bump_heap->next = start_hole;
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

void* k_bump_alloc(size_t size){
    return bump_alloc(&bump_heap, size);
}

void* k_bump_alloc_aligned(size_t size, size_t align){
    bump_alloc_aligned(&bump_heap, size, align);
}
