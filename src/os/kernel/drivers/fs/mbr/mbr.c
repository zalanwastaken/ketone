#include "mbr.h"
#include "../../disk/common.h"
#include "../../../mem_manager/manager.h"
#include "../../../io/serial/serial.h"

#define MBR_PARTITION_TABLE 0x1BE
#define MBR_PARTITION_SIZE  16

int16_t MBR_get_partitions(disk_device_t *disk, mbr_partition_t **partitions){
    uint8_t *mbr = (uint8_t*)kalloc(sizeof(uint8_t)*512);
    if(!read_disk(disk, 0, (uint16_t*)mbr)){
        kfree(mbr);
        return -1;
    }
    if (mbr[510] != 0x55 || mbr[511] != 0xAA){
        kfree(mbr);
        return -1;
    }

    uint16_t partitions_idx = 0;
    for(uint64_t i = 0; i<4; i++){
        uint8_t *entry = &mbr[0x1BE + i * 16];
        uint8_t type = entry[4];

        if(type == 0x00 || type == 0x07){
            mbr_partition_t *partition = (mbr_partition_t*)kalloc(sizeof(mbr_partition_t)*1);
            partition->boot = entry[0];
            partition->type = entry[4];

            partition->lba_start =
                ((uint32_t)entry[8]) |
                ((uint32_t)entry[9] << 8) |
                ((uint32_t)entry[10] << 16) |
                ((uint32_t)entry[11] << 24);

            partition->sectors =
                ((uint32_t)entry[12]) |
                ((uint32_t)entry[13] << 8) |
                ((uint32_t)entry[14] << 16) |
                ((uint32_t)entry[15] << 24);
            partitions[partitions_idx] = partition;
            partitions_idx++;

            if (type == 0x07){
                serial_print("MBR EXFAT: ");
                serial_print_hexLN(partition->type);
                serial_print("MBR EXFAT at: ");
                serial_print_hexLN(partition->lba_start);
                serial_print("MBR EXFAT sectors: ");
                serial_print_hexLN(partition->sectors);
            }
        }
    }

    kfree(mbr);
    return partitions_idx;
}
