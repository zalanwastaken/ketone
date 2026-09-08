#include <stdint.h>

#include "io/serial/serial.h"
#include "mem_manager/manager.h"
#include "mem_manager/utils.h"

uint32_t *kernel_size_SECTORS = (uint32_t*)0x7E00;

void init(){
    serial_init();
    serial_print("HALLO FROM THE KERNEL IN 64BIT MODE !\n");
    *kernel_size_SECTORS += 4; //? some offset
    serial_print_hexLN((*kernel_size_SECTORS)*512);
    mem_manager_init(0x40000000, *kernel_size_SECTORS);

    uint32_t *kernel_size_SECTORS_new = (uint32_t*)kalloc(sizeof(uint32_t));
    *kernel_size_SECTORS_new = *kernel_size_SECTORS;
    kernel_size_SECTORS = kernel_size_SECTORS_new;

    for(uint64_t i = i; i<6000; i++){
        if(i%1000 == 0){
            serial_print_decLN(i);
        }
        kalloc(0x1000);
    }
}

__attribute__((section(".start")))
void kernel_main(void){
    init();

    asm volatile("hang:\n"
    "jmp hang");
}
