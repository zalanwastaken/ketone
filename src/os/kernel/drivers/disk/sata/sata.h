#pragma once

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

#include "../common.h"

typedef struct __attribute__((__packed__)){
    uint8_t  fis_type;
    uint8_t  pmport_c;
    uint8_t  command;
    uint8_t  featurel;

    uint8_t  lba0;
    uint8_t  lba1;
    uint8_t  lba2;
    uint8_t  device;

    uint8_t  lba3;
    uint8_t  lba4;
    uint8_t  lba5;
    uint8_t  featureh;

    uint8_t  countl;
    uint8_t  counth;
    uint8_t  icc;
    uint8_t  control;

    uint8_t  reserved[4];
} FIS_REG_H2D_t;

typedef struct __attribute__((__packed__)){
    uint16_t flags;
    uint16_t prdtl;
    uint32_t prdbc;
    uint32_t ctba;
    uint32_t ctbau;
    uint32_t reserved[4];
} HBA_CMD_HEADER_t;

typedef struct __attribute__((packed)) {
    uint32_t dba;       // Data Base Address (lower 32 bits)
    uint32_t dbau;      // Data Base Address (upper 32 bits)
    uint32_t reserved;
    uint32_t dbc_i;     // Byte count + interrupt flag
} HBA_PRDT_ENTRY_t;

typedef struct __attribute__((__packed__)){
    uint8_t cfis[64];
    uint8_t acmd[16];
    uint8_t reserved[48];
    HBA_PRDT_ENTRY_t prdt_entry[1];
} HBA_CMD_TABLE_t;

typedef struct __attribute__((__packed__)){
    volatile uint32_t clb;       // 0x00
    volatile uint32_t clbu;      // 0x04
    volatile uint32_t fb;        // 0x08
    volatile uint32_t fbu;       // 0x0C
    volatile uint32_t is;        // 0x10
    volatile uint32_t ie;        // 0x14
    volatile uint32_t cmd;       // 0x18
    volatile uint32_t reserved0; // 0x1C
    volatile uint32_t tfd;       // 0x20
    volatile uint32_t sig;       // 0x24
    volatile uint32_t ssts;      // 0x28
    volatile uint32_t sctl;      // 0x2C
    volatile uint32_t serr;      // 0x30
    volatile uint32_t sact;      // 0x34
    volatile uint32_t ci;        // 0x38
    volatile uint32_t sntf;      // 0x3C
    volatile uint32_t fbs;       // 0x40
    volatile uint32_t reserved1[11];
    volatile uint32_t vendor[4];
} HBA_PORT_t;

typedef struct {
    HBA_CMD_HEADER_t *cmd_list;
    FIS_REG_H2D_t *fis;
    HBA_CMD_TABLE_t **cmd_tables;
    size_t command_slots;
} SATA_port_data_t;

typedef struct{
    volatile uint32_t *HBA;
    volatile HBA_PORT_t *port;
    SATA_port_data_t *port_data;
    uint8_t port_num;
} SATA_driver_data_t;

disk_device_t** SATA_get_disks(size_t *size);
bool SATA_read(disk_device_t *disk, uint64_t lba, uint16_t *buff);
