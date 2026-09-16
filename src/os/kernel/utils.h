#pragma once

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

void halt();
int strcmp(const char *a, const char *b);
int memcmp(const void *a, const void *b, size_t n);
