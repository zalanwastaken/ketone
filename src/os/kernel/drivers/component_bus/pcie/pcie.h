#pragma once

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

#include "../common.h"
#include "../../acpi/acpi.h"

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

bool PCIE_init();
void PCIE_print_devices();

