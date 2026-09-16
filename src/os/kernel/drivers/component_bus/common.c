#include "common.h"

const char* COMPONENT_vendor_name(uint16_t vendor_id){
    for (uint64_t i = 0; i < sizeof(vendors) / sizeof(vendors[0]); i++){
        if (vendors[i].id == vendor_id){
            return vendors[i].name;
        }
    }

    return "Unknown";
}
