#pragma once

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

#define DEVICE_TYPE_NONE    0
#define DEVICE_TYPE_ATA     1

typedef struct{
    char name[41];
    uint8_t type;
    uint32_t chann_main;
    uint32_t chann_sub;
    uint64_t size;
    bool hasLBA48;
} disk_device_t;
