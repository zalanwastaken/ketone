#include "utils.h"

void halt(){
    asm volatile(
        "hang:\n"
        "nop\n"
        "jmp hang"
    );
}

int strcmp(const char *a, const char *b){
    while (*a && (*a == *b)){
        a++;
        b++;
    }

    return (unsigned char)*a - (unsigned char)*b;
}

int memcmp(const void *a, const void *b, size_t n){
    const unsigned char *x = a;
    const unsigned char *y = b;

    for (size_t i = 0; i < n; i++) {
        if (x[i] != y[i]){
            return x[i] - y[i];
        }
    }

    return 0;
}
