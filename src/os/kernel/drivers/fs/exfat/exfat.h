#pragma once

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

#include "../mbr/mbr.h"
#include "../../disk/common.h"

typedef struct{
    disk_device_t disk;
    uint64_t sector_on_disk_LBA;

    uint64_t partition_offset;
    uint64_t volume_length;

    uint32_t fat_offset;
    uint32_t fat_length;

    uint32_t cluster_heap_offset;
    uint32_t cluster_count;

    uint32_t root_cluster;

    uint8_t bytes_per_sector_shift;
    uint8_t sectors_per_cluster_shift;
    uint8_t number_of_fats;
} exfat_instance_t;

typedef struct{
    uint16_t attributes;

    uint32_t first_cluster;

    uint64_t valid_data_length;
    uint64_t data_length;

    uint8_t name_length;
    uint16_t name_hash;
    uint8_t flags;
} exfat_file_entry_t;

exfat_instance_t EXFAT_init(disk_device_t *disk, mbr_partition_t *mbr);
uint64_t EXFAT_read_file(exfat_instance_t fs, const char *name);
