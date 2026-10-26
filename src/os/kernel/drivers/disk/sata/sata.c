#include "sata.h"
#include "../../component_bus/common.h"
#include "../../../io/serial/serial.h"

bool SATA_init(){
    size_t devices_size = 0;
    component_device_t *devices = component_bus_get_devicesEX(0x01, 0x06, 0x01, &devices_size);

    for(uint32_t i = 0; i<devices_size; i++){
        if(devices[i].bar[5].is_valid){
            uint64_t ABAR = devices[i].bar[5].base;
            volatile uint32_t *HBA = (volatile uint32_t *)ABAR;

            serial_printLN("ABAR: ");
            serial_print_hexLN(ABAR);

            serial_printLN("BAR valid: ");
            serial_print_hexLN(devices[i].bar[5].is_valid);

            serial_printLN("BAR size: ");
            serial_print_hexLN(devices[i].bar[5].bar_size);

            uint32_t cap = HBA[0];
            uint32_t ghc = HBA[4];
            uint32_t pi  = HBA[0x0C / 4];
            uint32_t vs  = HBA[4];

            serial_print_hexLN(cap);
            //serial_print_hexLN(ghc);
            //serial_print_hexLN(pi);
            serial_print_hexLN(vs);
        }
    }
}
