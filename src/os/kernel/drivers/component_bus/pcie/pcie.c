#include "pcie.h"

#include "../../../io/serial/serial.h"

mcfg_t *MCFG = NULL;
uint32_t MCFG_entries = 0;

uint64_t pcie_config_addr(uint64_t base, uint8_t bus, uint8_t device, uint8_t function, uint16_t offset){
    return base
        + ((uint64_t)bus      << 20)
        + ((uint64_t)device   << 15)
        + ((uint64_t)function << 12)
        + offset;
}

uint32_t PCIE_read32(uint64_t base, uint8_t bus, uint8_t device, uint8_t function, uint16_t offset){
    uint64_t addr = pcie_config_addr(base, bus, device, function, offset);

    return *(volatile uint32_t*)addr;
}

bool PCIE_init(){
    mcfg_t *MCFG_a = (mcfg_t*)ACPI_get_table("MCFG");
    if(MCFG_a == NULL){
        serial_printLN("PCIE Unable to get MCFG table");
        return false;
    }

    uint32_t entry_count = (MCFG_a->header.length - 44) / sizeof(mcfg_entry_t);
    for(uint64_t i = 0; i<entry_count; i++){
        mcfg_entry_t *MCFG_entry = &(MCFG_a->entries[i]);
        serial_print("ECAM base:");
        serial_print_hexLN(MCFG_entry->base_address);
        serial_print("Start bus:");
        serial_print_hexLN(MCFG_entry->start_bus);
        serial_print("End bus:");
        serial_print_hexLN(MCFG_entry->end_bus);
    }

    MCFG = MCFG_a;
    MCFG_entries = entry_count;

    return true;
}

//! tmp function for testing
void PCIE_print_devices(){
    for(uint64_t i = 0; i<MCFG_entries; i++){
        mcfg_entry_t *MCFG_entry = &(MCFG->entries[i]);

        for(uint64_t bus = MCFG_entry->start_bus; bus<MCFG_entry->end_bus; bus++){
            for(uint16_t device = 0; device<32; device++){
                for(uint8_t function = 0; function<8; function++){
                    uint32_t id = PCIE_read32(MCFG_entry->base_address, bus, device, function, 0);
                    uint16_t vendor_id = id & 0xFFFF;
                    uint16_t device_id = id >> 16;
                    if(vendor_id == 0xFFFF){
                        continue;
                    }
                    serial_print("----");
                    serial_print_hex(bus);
                    serial_print(":");
                    serial_print_hex(device);
                    serial_print(".");
                    serial_print_hex(function);
                    serial_printLN("----");

                    serial_print_hex(vendor_id);
                    serial_print(" as ");
                    serial_printLN(COMPONENT_vendor_name(vendor_id));
                    serial_print_hexLN(device_id);

                    uint32_t class_info = PCIE_read32(MCFG_entry->base_address, bus, device, function, 0x08);
                    uint8_t revision  =  class_info        & 0xFF;
                    uint8_t prog_if   = (class_info >> 8)  & 0xFF;
                    uint8_t subclass  = (class_info >> 16) & 0xFF;
                    uint8_t class_code = (class_info >> 24) & 0xFF;

                    serial_print("Class: ");
                    serial_print_hexLN(class_code);
                    serial_print("Sub class: ");
                    serial_print_hexLN(subclass);
                    serial_print("prog if: ");
                    serial_print_hexLN(prog_if);
                }
            }
        }
    }
}
