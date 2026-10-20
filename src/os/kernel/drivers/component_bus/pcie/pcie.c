#include "pcie.h"
#include "../common.h"

#include "../../../io/serial/serial.h"
#include "../../../mem_manager/manager.h"
#include "../../../mem_manager/utils.h"

static mcfg_t *MCFG = NULL;
static uint32_t MCFG_entries = 0;

static inline uint64_t pcie_config_addr(uint64_t base, uint8_t bus, uint8_t device, uint8_t function, uint16_t offset){
    return base
        + ((uint64_t)bus      << 20)
        + ((uint64_t)device   << 15)
        + ((uint64_t)function << 12)
        + offset;
}

static inline uint32_t PCIE_read32(uint64_t base, uint8_t bus, uint8_t device, uint8_t function, uint16_t offset){
    uint64_t addr = pcie_config_addr(base, bus, device, function, offset);

    return *(volatile uint32_t*)addr;
}

static inline void PCIE_write32(uint64_t base, uint8_t bus, uint8_t device, uint8_t function, uint16_t offset, uint32_t value) {
    uint64_t addr = pcie_config_addr(base, bus, device, function, offset);

    *(volatile uint32_t *)addr = value;
}

static inline uint16_t PCIE_read16(uint64_t base, uint8_t bus, uint8_t device, uint8_t function, uint16_t offset){
    volatile uint16_t *addr = (volatile uint16_t *)pcie_config_addr(base, bus, device, function, offset);

    return *addr;
}

static inline void PCIE_write16(uint64_t base, uint8_t bus, uint8_t device, uint8_t function, uint16_t offset, uint16_t value){
    volatile uint16_t *addr = (volatile uint16_t *)pcie_config_addr(base, bus, device, function, offset);

    *addr = value;
}

static bool PCIE_remap_BAR(component_device_t *device, uint8_t bar_IDX, uint64_t newBase){
    if(bar_IDX >= 6){
        return false;
    }
    if(!device->bar[bar_IDX].is_valid){
        return false;
    }

    PCIE_driver_data *data = device->driver_data;
    uint8_t offset = 0x10 + bar_IDX * 4;
    uint16_t command = PCIE_read16(data->MCFG_entry.base_address, device->bus, device->device, device->function, 0x04);
    uint32_t BAR_data = PCIE_read32(data->MCFG_entry.base_address, device->bus, device->device, device->function, offset);
    uint32_t BAR_data_HIGH = 0;

    uint32_t newLow = 0;
    if(device->bar[bar_IDX].is_IO){
        if(newBase > UINT32_MAX || (newBase & 3u) || newBase+device->bar[bar_IDX].bar_size > UINT32_MAX){
            serial_printLN("PCIE BAR REMAP err: base too large");
            return false;
        }

        newLow = (BAR_data & 3u) | ((uint32_t)newBase & ~3u);
    }else{
        uint64_t align_mask = device->bar[bar_IDX].bar_size - 1;
        if(device->bar[bar_IDX].bar_size == 0){
            serial_printLN("PCIE BAR REMAP err: size = 0");
            return false;
        }
        if((newBase & align_mask)){
            serial_printLN("PCIE BAR REMAP err: unaligned base");
            return false;
        }
        if(!device->bar[bar_IDX].is_64_bits && newBase > UINT32_MAX || newBase+device->bar[bar_IDX].bar_size > UINT32_MAX){
            serial_printLN("PCIE BAR REMAP err: base too large");
            return false;
        }

        newLow = (BAR_data & 0xFu) | ((uint32_t)newBase & ~0xFu);
    }

    PCIE_write16(data->MCFG_entry.base_address, device->bus, device->device, device->function, 0x04, command & ~(uint16_t)3u); //? disable IO and mem decoding
    if(device->bar[bar_IDX].is_64_bits){
        BAR_data_HIGH = PCIE_read32(data->MCFG_entry.base_address, device->bus, device->device,device->function, offset+4);
        PCIE_write32(data->MCFG_entry.base_address, device->bus, device->device, device->function, offset + 4, (uint32_t)(newBase >> 32));
    }
    PCIE_write32(data->MCFG_entry.base_address, device->bus, device->device, device->function, offset, newLow);

    uint32_t checkLow = PCIE_read32(data->MCFG_entry.base_address, device->bus, device->device, device->function, offset);
    uint32_t checkHigh = 0;

    if(device->bar[bar_IDX].is_64_bits){
        checkHigh = PCIE_read32(data->MCFG_entry.base_address, device->bus, device->device, device->function, offset + 4);
    }

    uint64_t actual;
    if(device->bar[bar_IDX].is_IO){
        actual = checkLow & ~3u;
    }else if(device->bar[bar_IDX].is_64_bits){
        actual = ((uint64_t)checkHigh << 32 | (checkLow & ~0xFu));
    }else{
        actual = checkLow & ~0xFu;
    }

    if(actual == newBase){
        device->bar[bar_IDX].base = newBase;
    }else{
        if(device->bar[bar_IDX].is_64_bits){
            PCIE_write32(data->MCFG_entry.base_address, device->bus, device->device, device->function, offset + 4, BAR_data_HIGH);
        }
        PCIE_write32(data->MCFG_entry.base_address, device->bus, device->device, device->function, offset, BAR_data);

        PCIE_write16(data->MCFG_entry.base_address, device->bus, device->device, device->function, 0x04, command);

        serial_printLN("PCIE BAR REMAP err: base not set");
        return false;
    }

    device->bar[bar_IDX].base = newBase;
    PCIE_write16(data->MCFG_entry.base_address, device->bus, device->device, device->function, 0x04, command);

    return true;
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

component_device_t* PCIE_get_devices(size_t *devices_size){
    uint32_t devices_IDX = 0;
    *devices_size = 16;
    component_device_t* devices = kalloc(sizeof(component_device_t)*(*devices_size));

    for(uint64_t i = 0; i<MCFG_entries; i++){
        mcfg_entry_t *MCFG_entry = &(MCFG->entries[i]);

        for(uint64_t bus = MCFG_entry->start_bus; bus<=MCFG_entry->end_bus; bus++){
            for(uint16_t device = 0; device<32; device++){
                for(uint8_t function = 0; function<8; function++){
                    bool last_BAR_was_64 = false;
                    uint32_t id = PCIE_read32(MCFG_entry->base_address, bus, device, function, 0);
                    uint16_t vendor_id = id & 0xFFFF;
                    if(vendor_id == 0xFFFF){
                        continue;
                    }

                    for(uint16_t BAR = 0; BAR<6; BAR++){
                        uint8_t offset = 0x10 + BAR * 4;
                        uint32_t BAR_data = PCIE_read32(MCFG_entry->base_address, bus, device, function, offset);

                        bool is_io = (BAR_data & 1u) != 0;
                        bool is_64 = !is_io && (((BAR_data >> 1) & 3u) == 2u);
                        bool is_vaild = true;
                        uint64_t size = 0;
                        uint64_t base = 0;

                        uint32_t BAR_data_HIGH = 0;

                        if(last_BAR_was_64){
                            last_BAR_was_64 = false;
                            is_vaild = false;
                            is_64 = false;
                            goto done;
                        }
                        if(is_64){
                            BAR_data_HIGH = PCIE_read32(MCFG_entry->base_address, bus, device, function, offset+4);
                        }

                        uint16_t command = PCIE_read16(MCFG_entry->base_address, bus, device, function, 0x04);
                        PCIE_write16(MCFG_entry->base_address, bus, device, function, 0x04, command & ~3u); //? disable IO and mem decoding

                        PCIE_write32(MCFG_entry->base_address, bus, device, function, offset, 0xFFFFFFFFu);
                        if(is_64){
                            PCIE_write32(MCFG_entry->base_address, bus, device, function, offset+4, 0xFFFFFFFFu);
                        }

                        uint32_t mask = PCIE_read32(MCFG_entry->base_address, bus, device, function, offset);
                        uint32_t mask_HIGH = 0;
                        if(is_64){
                            mask_HIGH = PCIE_read32(MCFG_entry->base_address, bus, device, function, offset+4);

                            PCIE_write32(MCFG_entry->base_address, bus, device, function, offset+4, BAR_data_HIGH);
                        }
                        PCIE_write32(MCFG_entry->base_address, bus, device, function, offset, BAR_data);

                        PCIE_write16(MCFG_entry->base_address, bus, device, function, 0x04, command); //? enable previosly disabled stuff

                        if (is_io) {
                            uint32_t mask_c = mask & ~3u;
                            size = ~mask_c + 1u;
                            base = BAR_data & ~3u;

                            if (!mask_c || !size){
                                is_vaild = false;
                            }

                            goto done;
                        }

                        if (((BAR_data >> 1) & 3u) == 3u){
                            is_vaild = false;
                            goto done;
                        }

                        if(is_64){
                            uint64_t mask_c = ((uint64_t)mask_HIGH << 32) | (mask & ~0xFULL);
                            size = ~mask_c + 1ULL;
                            base = ((uint64_t)BAR_data_HIGH << 32) | (BAR_data & ~0xFULL);

                            if(!mask_c || !size){
                                is_vaild = false;
                            }
                        }else{
                            uint32_t mask_c = mask & ~0xFu;
                            size = ~mask_c + 1u;
                            base = BAR_data & ~0xFu;

                            if(!mask_c || !size){
                                is_vaild = false;
                            }
                        }

                        done:
                        if(devices_IDX >= *devices_size){
                            component_device_t *buff = kalloc(sizeof(component_device_t)*((*devices_size)+8));
                            memcpy(devices, buff, sizeof(component_device_t)*(*devices_size));
                            *devices_size += 8;
                            kfree(devices);
                            devices = buff;
                        }
                        devices[devices_IDX].bar[BAR].bar_size = size;
                        devices[devices_IDX].bar[BAR].is_64_bits = is_64;
                        devices[devices_IDX].bar[BAR].is_IO = is_io;
                        devices[devices_IDX].bar[BAR].is_prefetchable = (BAR_data & 8u) != 0;
                        devices[devices_IDX].bar[BAR].is_valid = is_vaild;
                        devices[devices_IDX].bar[BAR].base = base;

                        if(is_64){
                            last_BAR_was_64 = true;
                        }
                    }
                    uint32_t class_info = PCIE_read32(MCFG_entry->base_address, bus, device, function, 0x08);
                    uint8_t revision  =  class_info        & 0xFF;
                    uint8_t prog_if   = (class_info >> 8)  & 0xFF;
                    uint8_t subclass  = (class_info >> 16) & 0xFF;
                    uint8_t class_code = (class_info >> 24) & 0xFF;
                    uint16_t device_id = id >> 16;

                    devices[devices_IDX].bus = bus;
                    devices[devices_IDX].device = device;
                    devices[devices_IDX].function = function;
                    devices[devices_IDX].bus_type = COMPONENT_DEVICE_TYPE_PCIE;
                    devices[devices_IDX].class_code = class_code;
                    devices[devices_IDX].prog_if = prog_if;
                    devices[devices_IDX].revision = revision;
                    devices[devices_IDX].subclass = subclass;
                    devices[devices_IDX].device_id = device_id;
                    PCIE_driver_data *driver_data = (PCIE_driver_data*)kalloc(sizeof(PCIE_driver_data));
                    memcpy(MCFG_entry, &(driver_data->MCFG_entry), sizeof(mcfg_entry_t));
                    devices[devices_IDX].driver_data = driver_data;
                    devices_IDX++;
                }
            }
        }
    }

    if(devices_IDX < (*devices_size)){
        component_device_t* buff = kalloc(sizeof(component_device_t)*(devices_IDX));
        memcpy(devices, buff, sizeof(component_device_t)*(devices_IDX));
        kfree(devices);
        devices = buff;
        *devices_size = devices_IDX;
    }

    for(uint32_t i = 0; i<(*devices_size); i++){
        for(uint32_t f = 0; f<6; f++){
            if(!devices[i].bar[f].is_valid || devices[i].bar[f].bar_size == 0){
                //serial_printLN("invaild BAR");
                continue;
            }
            void *BAR_buff = kalloc_aligned(devices[i].bar[f].bar_size, devices[i].bar[f].bar_size);
            if(BAR_buff == NULL){
                serial_print("BAR size: ");
                serial_print_hex64LN(devices[i].bar[f].bar_size);
                serial_printLN("cant find memory");
                continue;
            }
            uint64_t old = devices[i].bar[f].base;
            bool remapped = PCIE_remap_BAR(&(devices[i]), f, (uint64_t)BAR_buff);
            uint64_t new = devices[i].bar[f].base;
            if(remapped){
                serial_printLN("BAR remapped");
                serial_print_hex64(old);
                serial_print(" TO ");
                serial_print_hex64LN(new);
                serial_printLN("----");
            }else{
                serial_printLN("BAR remap fail");
                kfree(BAR_buff);
            }
        }
    }

    return devices;
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
