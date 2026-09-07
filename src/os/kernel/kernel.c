#include <stdint.h>
#include "io/serial/serial.h"
#include "mem_manager/manager.h"

uint32_t *kernel_size_SECTORS = (uint32_t*)0x7E00;

void init(){
    serial_init();
    serial_print("HALLO FROM THE KERNEL IN 64BIT MODE !\n");
    serial_print_hexLN((*kernel_size_SECTORS)*512);
    mem_manager_init(0x40000000, *kernel_size_SECTORS);
}

__attribute__((section(".start")))
void kernel_main(void){
    init();

    asm volatile("hang:\n"
    "jmp hang");
}
