#pragma once

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

typedef struct{
    char     signature[8];
    uint8_t  checksum;
    char     oem_id[6];
    uint8_t  revision;
    uint32_t rsdt_address;

    //* ACPI 2.0+
    //? not used in this driver because QEMU uses SeaBios which is too old(sad)
    uint32_t length;
    uint64_t xsdt_address;
    uint8_t  extended_checksum;
    uint8_t  reserved[3];
} __attribute__((packed)) rsdp_t;

typedef struct{
    char signature[4];

    uint32_t length;
    uint8_t revision;
    uint8_t checksum;

    char oem_id[6];
    char oem_tableID[8];

    uint32_t oem_revision;
    uint32_t creatorID;
    uint32_t creator_revision;
} __attribute__ ((packed)) rsdt_header_t;

bool ACPI_init();
rsdt_header_t* ACPI_get_table(const char *name);
