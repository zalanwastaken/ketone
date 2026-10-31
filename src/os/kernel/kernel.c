#include <stdint.h>

#include "io/serial/serial.h"
#include "mem_manager/manager.h"
#include "utils.h"

#include "drivers/disk/common.h"
#include "drivers/fs/mbr/mbr.h"
#include "drivers/fs/exfat/exfat.h"
#include "drivers/acpi/acpi.h"
#include "drivers/component_bus/pcie/pcie.h"
#include "drivers/component_bus/common.h"
#include "drivers/disk/sata/sata.h"

uint32_t *kernel_size_SECTORS = (uint32_t*)0x8F00; //? placed here by the bootloader

void init(){
    serial_init();
    serial_printLN("HALLO FROM THE KETONE KERNEL IN 64BIT MODE !");
    *kernel_size_SECTORS += 4; //? some offset
    serial_print("Kernel size: ");
    serial_print_hexLN((*kernel_size_SECTORS)*512);
    if(!ACPI_init()){
        serial_printLN("ACPI init fail !");
        halt();
    }
    //?              root heap: 1 GiB                                          256 MiB
    mem_manager_init(0x40000000,      *kernel_size_SECTORS, (uint32_t*)0x8F04, 0x10000000);

    uint32_t *kernel_size_SECTORS_new = (uint32_t*)kalloc(sizeof(uint32_t));
    *kernel_size_SECTORS_new = *kernel_size_SECTORS;
    kernel_size_SECTORS = kernel_size_SECTORS_new;

    if(!component_bus_init()){
        serial_printLN("component bus init fail !");
        halt();
    }
}

__attribute__((section(".start")))
void kernel_main(void){
    init();

    size_t size_disk_devices = 0;
    disk_device_t** drives = get_disk_devices(DISK_SATA, &size_disk_devices);
    for(uint64_t i = 0; i<size_disk_devices; i++){
        if(drives[i]->type != DISK_NONE){
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
                        exfat_file_entry_t *hello_file = EXFAT_open_file(&instance, "/ketone/hello");
                        if(hello_file == NULL){
                            continue; //? not found, sad
                        }

                        size_t hello_file_size;
                        uint8_t *buff = EXFAT_read_file_ALL(hello_file, &hello_file_size);
                        for(uint64_t i = 0; i<hello_file_size; i++){
                            serial_write(buff[i]);
                        }
                        serial_write('\n');
                        kfree(buff);
                    }
                }
            }
        }
    }

    /*
    PCIE_init();
    //PCIE_print_devices();
    size_t got = 0;
    component_device_t* PCIE_devices = PCIE_get_devices(&got);
    serial_print_hex64LN(got);
    for(uint16_t i = 0; i<got; i++){
        serial_print_hex64(PCIE_devices[i].bus);
        serial_print(":");
        serial_print_hex64(PCIE_devices[i].device);
        serial_print(".");
        serial_print_hex64(PCIE_devices[i].function);
        serial_print("      ");
        serial_print(COMPONENT_vendor_name(PCIE_devices[i].vendor_id));
        serial_print("      ");
        serial_print_hexLN(PCIE_devices[i].vendor_id);
    }
    */

    //PCIE_print_devices();

    halt();
}
