#include "utils.h"

void halt(){
    asm volatile(
        "hang:\n"
        "nop\n"
        "jmp hang"
    );
}
