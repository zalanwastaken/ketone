#include "utils.h"

void memcpy(void *src, void *dest, size_t size){
    for(uint64_t i = 0; i<size; i++){
        ((uint8_t*)dest)[i] = ((uint8_t*)src)[i];
    }
}
