#include "acpi.h"
#include "../../utils.h"
#include "../../io/serial/serial.h"

rsdt_header_t *RSD_T = NULL;

bool acpi_checksum(void *data, uint8_t length){
    uint8_t sum = 0;
    uint8_t *bytes = data;

    for (uint8_t i = 0; i < length; i++){
        sum += bytes[i];
    }

    return sum == 0;
}

bool ACPI_init(){
    for(uint64_t i = 0xE0000; i<0xFFFFF; i++){
        char *sig = (char*)i;
        if(memcmp(sig, "RSD PTR ", 8) == 0){
            if(acpi_checksum(sig, 20)){
                rsdp_t *RSD_P = (rsdp_t*)sig;

                serial_printLN("ACPI found RSD_P");

                rsdt_header_t *RSD_T_a = (rsdt_header_t*)RSD_P->rsdt_address;
                if(memcmp(RSD_T_a->signature, "RSDT", 4) == 0){
                    if(acpi_checksum(RSD_T_a, RSD_T_a->length)){
                        serial_printLN("ACPI found RSD_T");

                        RSD_T = RSD_T_a;
                        return true;
                    }
                }
            }
        }
    }
    return false;
}

rsdt_header_t* ACPI_get_table(const char *name){
    if(RSD_T == NULL){
        serial_printLN("ACPI RSD_T not found or not init");
        return NULL;
    }

    uint32_t *entries = (uint32_t *)((uint8_t *)RSD_T + sizeof(rsdt_header_t));
    uint32_t count = (RSD_T->length - sizeof(rsdt_header_t)) / 4;

    for(uint64_t i = 0; i<count; i++){
        rsdt_header_t *table = (rsdt_header_t*)(uint64_t)entries[i];
        char sig[5];
        for(uint64_t f = 0; f<4; f++){
            sig[f] = table->signature[f];
        }
        sig[4] = '\0';
        serial_printLN(sig);
        if(strcmp(sig, name) == 0){
            return table;
        }
    }

    return NULL;
}
