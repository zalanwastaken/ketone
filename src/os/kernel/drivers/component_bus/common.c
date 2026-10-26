#include "common.h"
#include "pcie/pcie.h"
#include "../../mem_manager/manager.h"
#include "../../utils.h"
#include "../../io/serial/serial.h"

typedef struct{
    uint16_t id;
    const char *name;
}component_vendor_t;

#define COMPONENT_VENDOR(id, name) \
    { id, name }

static const component_vendor_t vendors[] = {
    COMPONENT_VENDOR(0x8086, "Intel"),
    COMPONENT_VENDOR(0x1022, "AMD"),
    COMPONENT_VENDOR(0x10DE, "NVIDIA"),
    COMPONENT_VENDOR(0x1002, "ATI"),
    COMPONENT_VENDOR(0x10EC, "Realtek"),
    COMPONENT_VENDOR(0x14E4, "Broadcom"),
    COMPONENT_VENDOR(0x1AF4, "Red Hat"),
    COMPONENT_VENDOR(0x1234, "QEMU"),
    COMPONENT_VENDOR(0x80EE, "VirtualBox"),
    COMPONENT_VENDOR(0x15AD, "VMware"),
    COMPONENT_VENDOR(0x1013, "Cirrus Logic"),

    COMPONENT_VENDOR(0x1106, "VIA"),
    COMPONENT_VENDOR(0x11AB, "Marvell"),
    COMPONENT_VENDOR(0x1B21, "ASMedia"),
    COMPONENT_VENDOR(0x1B4B, "Marvell Technology"),
    COMPONENT_VENDOR(0x19E5, "Huawei"),
    COMPONENT_VENDOR(0x17CB, "Qualcomm"),
    COMPONENT_VENDOR(0x14C3, "MediaTek"),
    COMPONENT_VENDOR(0x1D6B, "Linux Foundation"),

    COMPONENT_VENDOR(0x144D, "Samsung"),
    COMPONENT_VENDOR(0x1344, "Micron"),
    COMPONENT_VENDOR(0x1C5C, "SK hynix"),
    COMPONENT_VENDOR(0x1B96, "Western Digital"),
    COMPONENT_VENDOR(0x1C58, "Western Digital"),
    COMPONENT_VENDOR(0x1179, "Toshiba"),
    COMPONENT_VENDOR(0x104C, "Texas Instruments"),
    COMPONENT_VENDOR(0x10B5, "PLX Technology"),

    COMPONENT_VENDOR(0x1033, "NEC"),
    COMPONENT_VENDOR(0x1043, "ASUSTeK"),
    COMPONENT_VENDOR(0x1028, "Dell"),
    COMPONENT_VENDOR(0x103C, "HP"),
    COMPONENT_VENDOR(0x107B, "Gateway"),
    COMPONENT_VENDOR(0x1025, "Acer"),
    COMPONENT_VENDOR(0x17AA, "Lenovo"),
    COMPONENT_VENDOR(0x10CF, "Fujitsu"),

    COMPONENT_VENDOR(0x13B5, "ARM"),
    COMPONENT_VENDOR(0x1AE0, "Google"),
    COMPONENT_VENDOR(0x1A03, "ASPEED"),
    COMPONENT_VENDOR(0x1B36, "Red Hat/QEMU"),
    COMPONENT_VENDOR(0x1D0F, "Amazon"),
    COMPONENT_VENDOR(0x1DA8, "Ampere Computing"),

    COMPONENT_VENDOR(0x1969, "Atheros"),
    COMPONENT_VENDOR(0x168C, "Qualcomm Atheros"),
    COMPONENT_VENDOR(0x14F1, "Conexant"),
    COMPONENT_VENDOR(0x14F2, "RDC Semiconductor"),
    COMPONENT_VENDOR(0x10B7, "3Com"),
    COMPONENT_VENDOR(0x10DF, "Emulex"),
    COMPONENT_VENDOR(0x1077, "QLogic"),
    COMPONENT_VENDOR(0x8087, "Intel"),
};

const char* COMPONENT_vendor_name(uint16_t vendor_id){
    for (uint64_t i = 0; i < sizeof(vendors) / sizeof(vendors[0]); i++){
        if (vendors[i].id == vendor_id){
            return vendors[i].name;
        }
    }

    return "Unknown";
}

static component_device_t *devices = NULL;
static size_t devices_got = 0;

bool component_bus_init(){
    if(!PCIE_init()){
        return false;
    }

    devices = PCIE_get_devices(&devices_got);

    return true;
}

component_device_t* component_bus_get_devices(uint8_t class, uint8_t subclass, size_t *ret_size){
    *ret_size = 16;
    component_device_t *ret = kalloc(sizeof(component_device_t)*(*ret_size));
    size_t ret_IDX = 0;
    for(uint32_t i = 0; i<devices_got; i++){
        if(devices[i].class_code == class && devices[i].subclass == subclass){
            if(ret_IDX >= *ret_size){
                component_device_t *buff = kalloc(sizeof(component_device_t)*(*ret_size+16));
                memcpy(ret, buff, sizeof(component_device_t)*(*ret_size));
                *ret_size += 16;
                kfree(ret);
                ret = buff;
            }
            ret[ret_IDX] = devices[i];
            ret_IDX++;
        }
    }

    if(*ret_size > ret_IDX){
        component_device_t *buff = kalloc(sizeof(component_device_t)*ret_IDX);
        memcpy(ret, buff, sizeof(component_device_t)*ret_IDX);
        kfree(ret);
        *ret_size = ret_IDX;
        ret = buff;
    }

    return ret;
}

component_device_t* component_bus_get_devicesEX(uint8_t class, uint8_t subclass, uint8_t prog_if, size_t *ret_size){
    *ret_size = 16;
    component_device_t *ret = kalloc(sizeof(component_device_t)*(*ret_size));
    size_t ret_IDX = 0;
    for(uint32_t i = 0; i<devices_got; i++){
        if(devices[i].class_code == class && devices[i].subclass == subclass && devices[i].prog_if == prog_if){
            if(ret_IDX >= *ret_size){
                component_device_t *buff = kalloc(sizeof(component_device_t)*(*ret_size+16));
                memcpy(ret, buff, sizeof(component_device_t)*(*ret_size));
                *ret_size += 16;
                kfree(ret);
                ret = buff;
            }
            ret[ret_IDX] = devices[i];
            ret_IDX++;
        }
    }

    if(*ret_size > ret_IDX){
        component_device_t *buff = kalloc(sizeof(component_device_t)*ret_IDX);
        memcpy(ret, buff, sizeof(component_device_t)*ret_IDX);
        kfree(ret);
        *ret_size = ret_IDX;
        ret = buff;
    }

    return ret;
}
