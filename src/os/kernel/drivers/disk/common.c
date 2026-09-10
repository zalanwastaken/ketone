#include "common.h"
#include "../../mem_manager/manager.h"

#include "ata/ata.h"

disk_device_t** get_disk_devices(uint8_t type){
    disk_device_t **ret = NULL;

    switch (type){
    case DEVICE_TYPE_ATA:
        ret = (disk_device_t**)kalloc(sizeof(disk_device_t*)*4);

        ret[0] = ATA_identify(ATA_CHANN_PRIMARY, ATA_MASTER);
        ret[1] = ATA_identify(ATA_CHANN_PRIMARY, ATA_SLAVE);
        ret[2] = ATA_identify(ATA_CHANN_SECONDARY, ATA_MASTER);
        ret[3] = ATA_identify(ATA_CHANN_SECONDARY, ATA_SLAVE);
        break;
    default:
        break;
    }

    return ret;
}

bool read_disk(disk_device_t *disk, uint32_t lba, uint16_t *buff){
    switch (disk->type){
    case DEVICE_TYPE_ATA:
        return ATA_read(disk, lba, buff);
        break;
    case DEVICE_TYPE_NONE:
        break;
    }

    return false;
}

bool write_disk(disk_device_t *disk, uint32_t lba, uint16_t *buff){
    switch (disk->type){
    case DEVICE_TYPE_ATA:
        return ATA_write(disk, lba, buff);
        break;
    case DEVICE_TYPE_NONE:
        break;
    }

    return false;
}
