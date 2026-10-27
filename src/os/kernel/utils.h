#pragma once

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

void halt();
int strcmp(const char *a, const char *b);
int memcmp(const void *a, const void *b, size_t n);
void memcpy(void *src, void *dest, size_t size);
void memset(void *ptr, uint8_t c, size_t n);
