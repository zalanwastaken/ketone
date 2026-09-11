#pragma once

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

#include "../../disk/common.h"

typedef struct {
    uint8_t boot;
    uint8_t type;

    uint32_t lba_start;
    uint32_t sectors;
} mbr_partition_t;

int16_t MBR_get_partitions(disk_device_t *disk, mbr_partition_t **partitions);
