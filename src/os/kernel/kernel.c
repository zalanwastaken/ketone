#include <stdint.h>

#include "io/serial/serial.h"
#include "mem_manager/manager.h"
#include "utils.h"
//#include "mem_manager/utils.h"

#include "drivers/disk/ata/ata.h"
#include "drivers/disk/common.h"

uint32_t *kernel_size_SECTORS = (uint32_t*)0x7E00; //? placed here by the bootloader

void init(){
    serial_init();
    serial_print("HALLO FROM THE KERNEL IN 64BIT MODE !\n");
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

    disk_device_t *drive = ATA_identify(ATA_CHANN_PRIMARY, ATA_MASTER);

    if(drive->type != DEVICE_TYPE_NONE){
        serial_printLN(drive->name);
        serial_print_hexLN(drive->hasLBA48);
        uint16_t *buff = (uint16_t*)kalloc(sizeof(uint16_t)*256);
        bool success = ATA_read(drive, 0, &buff);
        if(success){
            for(uint64_t i = 0; i<256; i++){
                serial_print_hexLN(buff[i]);
            }
        }
        kfree(buff);
    }

    kfree(drive);

    halt();
}
