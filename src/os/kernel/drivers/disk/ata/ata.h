#pragma once

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

#include "../common.h"

#define ATA_CHANN_PRIMARY       0x1F0
#define ATA_CHANN_SECONDARY     0x170

#define ATA_MASTER      0xA0
#define ATA_SLAVE       0xB0

disk_device_t* ATA_identify(uint32_t chann, uint32_t sub);
bool ATA_read(disk_device_t *disk, uint64_t lba, uint16_t **buff);
