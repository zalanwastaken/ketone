#include "common.h"

#include "../../utils.h"
#include "../../io/serial/serial.h"
#include "../../mem_manager/manager.h"

#include "ata/ata.h"
#include "sata/sata.h"

disk_device_t** get_disk_devices(uint8_t type, size_t *size){
    disk_device_t **ret = NULL;

    switch (type){
    case DISK_ATA:
        ret = (disk_device_t**)kalloc(sizeof(disk_device_t*)*4);

        ret[0] = ATA_identify(ATA_CHANN_PRIMARY, ATA_MASTER);
        ret[1] = ATA_identify(ATA_CHANN_PRIMARY, ATA_SLAVE);
        ret[2] = ATA_identify(ATA_CHANN_SECONDARY, ATA_MASTER);
        ret[3] = ATA_identify(ATA_CHANN_SECONDARY, ATA_SLAVE);

        *size = 4;
        break;

    case (DISK_SATA):
            ret = SATA_get_disks(size);
            break;
    case (DISK_NONE):
        *size = 0;
        break;
    }

    return ret;
}

bool read_disk(disk_device_t *disk, uint32_t lba, uint16_t *buff){
    switch (disk->type){
    case DISK_ATA:
        return ATA_read(disk, lba, buff);
        break;
    case DISK_SATA:
        return SATA_read(disk, lba, buff);
        break;
    case DISK_NONE:
        break;
    }

    return false;
}

bool write_disk(disk_device_t *disk, uint32_t lba, uint16_t *buff){
    switch (disk->type){
    case DISK_ATA:
        return ATA_write(disk, lba, buff);
        break;
    case DISK_SATA:
        serial_printLN("SATA write not implemented yet");
        halt();
    case DISK_NONE:
        break;
    }

    return false;
}
