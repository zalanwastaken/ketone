#include <stdint.h>

#include "io/serial/serial.h"
#include "mem_manager/manager.h"
#include "utils.h"
//#include "mem_manager/utils.h"

#include "drivers/disk/common.h"
#include "drivers/fs/mbr/mbr.h"
#include "drivers/fs/exfat/exfat.h"

uint32_t *kernel_size_SECTORS = (uint32_t*)0x7E00; //? placed here by the bootloader

void init(){
    serial_init();
    serial_printLN("HALLO FROM THE KETONE KERNEL IN 64BIT MODE !");
    *kernel_size_SECTORS += 4; //? some offset
    serial_print_hexLN((*kernel_size_SECTORS)*512);
    //               1 GiB
    mem_manager_init(0x40000000, *kernel_size_SECTORS);

    uint32_t *kernel_size_SECTORS_new = (uint32_t*)kalloc(sizeof(uint32_t));
    *kernel_size_SECTORS_new = *kernel_size_SECTORS;
    kernel_size_SECTORS = kernel_size_SECTORS_new;
}

__attribute__((section(".start")))
void kernel_main(void){
    init();

    disk_device_t** drives = get_disk_devices(DEVICE_TYPE_ATA);
    for(uint64_t i = 0; i<4; i++){
        if(drives[i]->type != DEVICE_TYPE_NONE){
            serial_print_hexLN(i);
            serial_printLN(drives[i]->name);
            mbr_partition_t **partitions = (mbr_partition_t**)kalloc(sizeof(mbr_partition_t**)*4); 
            int16_t entries = MBR_get_partitions(drives[i], partitions);
            if(entries == -1){
                kfree(partitions);
            }else{
                serial_print_hexLN(entries);
                for(uint64_t f = 0; f<entries; f++){
                    if(partitions[f]->type == 0x07){
                        exfat_instance_t instance = EXFAT_init(drives[i], partitions[f]);
                        EXFAT_read_file(instance, "");
                    }
                }
            }
        }
    }

    halt();
}
