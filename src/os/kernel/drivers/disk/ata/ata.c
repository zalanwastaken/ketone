#include "ata.h"
#include "../../../io/io.h"
#include "../../../io/serial/serial.h"
#include "../../../mem_manager/manager.h"
#include "../common.h"

// DRIVE CHAN~ ! lol
#define CHANN_DATA          0
#define CHANN_FEATURES      1
#define CHANN_ERR           1
#define CHANN_SECTOR_COUNT  2
#define CHANN_LBA_LOW       3
#define CHANN_LBA_MID       4
#define CHANN_LBA_HIGH      5
#define CHANN_DRIVE         6
#define CHANN_CMD           7
#define CHANN_STATUS        7

#define CHANN_PRIMARY_CTRL   0x3F6
#define CHANN_SECONDARY_CTRL 0x376

#define CMD_ATA_READ_PIO        0x20
#define CMD_ATA_READ_PIO_EXT    0x24
#define CMD_ATA_WRITE_PIO       0x30
#define CMD_ATA_WRITE_PIO_EXT   0x34
#define CMD_ATA_IDENTIFY        0xEC
#define CMD_ATA_IDENTIFY_PACKET 0xA1
#define CMD_ATA_FLUSH           0xE7
#define CMD_ATA_FLUSH_EXT       0xEA

void ata_swap_word(uint16_t word, char *a, char *b){
    *a = (char)(word >> 8);
    *b = (char)(word & 0xFF);
}

uint8_t wait_for_drive(uint32_t chann){

    uint8_t status = inb(chann + CHANN_STATUS);

    while (status & 0x80) {
        status = inb(chann + CHANN_STATUS);
    }

    return status;
}

uint8_t wait_for_data(uint32_t chann){
    uint8_t status = 0;

    for (uint32_t timeout = 0; timeout < 1000000; timeout++) {
        status = inb(chann + CHANN_STATUS);

        if (status & 0x01 || status & 0x08) {
            return status;
        }
    }

    return status;
}

disk_device_t* ATA_identify(uint32_t chann, uint32_t sub){
    disk_device_t *device = (disk_device_t*)kalloc(sizeof(disk_device_t)*1);

    device->chann_main = chann;
    device->chann_sub = sub;

    outb(chann+CHANN_DRIVE, sub);
    outb(chann+CHANN_CMD, CMD_ATA_IDENTIFY);

    uint8_t status = wait_for_data(chann);

    if(status & 0x01){
        serial_print("ATA Identify fail\n");
        device->type = DEVICE_TYPE_NONE;
        return device;
    }

    if (!(status & 0x08)){
        serial_print("ATA Identify: no data\n");
        device->type = DEVICE_TYPE_NONE;
        return device;
    }

    device->type = DEVICE_TYPE_ATA;

    ATA_driver_data *driver_data = kalloc(sizeof(ATA_driver_data)*1);
    device->driver_data = driver_data;

    uint16_t identify[256];

    for (int i = 0; i < 256; i++) {
        identify[i] = inw(chann + 0);
        //serial_print_hexLN(identify[i]);
    }

    uint16_t k = 0;
    for (uint32_t i = 27; i < 46; i++){
        char a, b;
        ata_swap_word(identify[i], &a, &b);
        device->name[k] = a;
        device->name[k+1] = b;
        k += 2;
    }
    device->name[40] = '\0';

    if (identify[83] & (1 << 10)) {
        driver_data->has_LBA48 = true;
    } else {
        driver_data->has_LBA48 = false;
    }

    switch (driver_data->has_LBA48){
    case true:
        device->size =  (((uint64_t)identify[103] << 48) |
                        ((uint64_t)identify[102] << 32) |
                        ((uint64_t)identify[101] << 16) |
                        identify[100])*512;
        break;
    case false:
        device->size = (((uint64_t)identify[61] << 16) |
                        identify[60])*512;
        break;
    }

    return device;
}

bool ATA_read(disk_device_t *disk, uint64_t lba, uint16_t *buff){
    ATA_driver_data *driver_data = (ATA_driver_data*)(disk->driver_data);
    switch (driver_data->has_LBA48){
    case true:
        outb(disk->chann_main + CHANN_DRIVE, disk->chann_sub | 0x40);
        // High bytes first
        outb(disk->chann_main + CHANN_SECTOR_COUNT, 0);
        outb(disk->chann_main + CHANN_LBA_LOW,  (lba >> 24) & 0xFF);
        outb(disk->chann_main + CHANN_LBA_MID,  (lba >> 32) & 0xFF);
        outb(disk->chann_main + CHANN_LBA_HIGH, (lba >> 40) & 0xFF);

        // Low bytes
        outb(disk->chann_main + CHANN_SECTOR_COUNT, 1);
        outb(disk->chann_main + CHANN_LBA_LOW,  lba & 0xFF);
        outb(disk->chann_main + CHANN_LBA_MID,  (lba >> 8) & 0xFF);
        outb(disk->chann_main + CHANN_LBA_HIGH, (lba >> 16) & 0xFF);

        outb(disk->chann_main + CHANN_CMD, CMD_ATA_READ_PIO_EXT);
        break;
    case false:
        outb(disk->chann_main + CHANN_DRIVE, disk->chann_sub | 0x40 | ((lba >> 24) & 0x0F));

        outb(disk->chann_main + CHANN_SECTOR_COUNT, 1);
        outb(disk->chann_main + CHANN_LBA_LOW,  lba & 0xFF);
        outb(disk->chann_main + CHANN_LBA_MID,  (lba >> 8) & 0xFF);
        outb(disk->chann_main + CHANN_LBA_HIGH, (lba >> 16) & 0xFF);

        outb(disk->chann_main + CHANN_CMD, CMD_ATA_READ_PIO);
        break;
    }

    uint8_t status = wait_for_data(disk->chann_main);

    if (status & 0x01) {
        serial_printLN("ATA read device error");
        return false;
    }

    if (!(status & 0x08)) {
        serial_printLN("ATA read device no data");
        return false;
    }

    for (int i = 0; i < 256; i++) {
        buff[i] = inw(disk->chann_main + CHANN_DATA);
    }

    return true;
}

bool ATA_write(disk_device_t *disk, uint32_t lba, uint16_t *buff){
    ATA_driver_data *driver_data = (ATA_driver_data*)(disk->driver_data);
    switch (driver_data->has_LBA48){
    case true:
        outb(disk->chann_main + CHANN_DRIVE, disk->chann_sub | 0x40);
        // High bytes
        outb(disk->chann_main + CHANN_SECTOR_COUNT, 0);
        outb(disk->chann_main + CHANN_LBA_LOW,  (lba >> 24) & 0xFF);
        outb(disk->chann_main + CHANN_LBA_MID,  (lba >> 32) & 0xFF);
        outb(disk->chann_main + CHANN_LBA_HIGH, (lba >> 40) & 0xFF);

        // Low bytes
        outb(disk->chann_main + CHANN_SECTOR_COUNT, 1);
        outb(disk->chann_main + CHANN_LBA_LOW,  lba & 0xFF);
        outb(disk->chann_main + CHANN_LBA_MID,  (lba >> 8) & 0xFF);
        outb(disk->chann_main + CHANN_LBA_HIGH, (lba >> 16) & 0xFF);

        outb(disk->chann_main + CHANN_CMD, CMD_ATA_WRITE_PIO_EXT);
        break;
    case false:
        outb(disk->chann_main + CHANN_DRIVE, disk->chann_sub | 0x40 | ((lba >> 24) & 0x0F));
        outb(disk->chann_main + CHANN_SECTOR_COUNT, 1);
        outb(disk->chann_main + CHANN_LBA_LOW, lba & 0xFF);
        outb(disk->chann_main + CHANN_LBA_MID, (lba >> 8) & 0xFF);
        outb(disk->chann_main + CHANN_LBA_HIGH, (lba >> 16) & 0xFF);

        outb(disk->chann_main + CHANN_CMD, CMD_ATA_WRITE_PIO);
        break;
    }

    uint8_t status = wait_for_data(disk->chann_main);

    if (status & 0x01) {
        serial_printLN("ATA write device error");
        return false;
    }

    if (!(status & 0x08)) {
        serial_printLN("ATA write device no data");
        return false;
    }

    for (int i = 0; i < 256; i++) {
        outw(disk->chann_main + CHANN_DATA, buff[i]);
    }

    return true;
}
