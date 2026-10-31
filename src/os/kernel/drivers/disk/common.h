#pragma once

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

typedef enum {
    DISK_NONE,
    DISK_ATA,
    DISK_SATA
}DISK_DEVICE_TYPE_e;

typedef struct{
    char name[41];
    DISK_DEVICE_TYPE_e type;
    uint32_t chann_main; //TODO move this to ATA driver data
    uint32_t chann_sub; //TODO move this to ATA driver data
    uint64_t size;
    void *driver_data;
} disk_device_t;

disk_device_t** get_disk_devices(uint8_t type, size_t *size);
bool read_disk(disk_device_t *disk, uint32_t lba, uint16_t *buff);
bool write_disk(disk_device_t *disk, uint32_t lba, uint16_t *buff);
