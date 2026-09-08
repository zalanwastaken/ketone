#pragma once

#include <stdint.h>

void serial_init();
void serial_print(const char* s);
void serial_print_hex(uint32_t val);
void serial_print_dec(uint32_t value);
void serial_print_hexLN(uint32_t val);
void serial_print_decLN(uint32_t val);
