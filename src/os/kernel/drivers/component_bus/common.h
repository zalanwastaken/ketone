#pragma once

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

#define COMPONENT_DEVICE_TYPE_PCIE 1
#define COMPONENT_DEVICE_TYPE_PCI  2

typedef struct{
    bool is_valid;
    bool is_64_bits;
    bool is_IO;
    bool is_prefetchable;
    uint64_t base;
    size_t bar_size;
} component_device_bar_t;

typedef struct {
    uint8_t bus_type;

    uint8_t  bus;
    uint8_t  device;
    uint8_t  function;

    uint16_t vendor_id;
    uint16_t device_id;

    uint8_t  class_code;
    uint8_t  subclass;
    uint8_t  prog_if;
    uint8_t  revision;

    uint8_t  header_type;

    component_device_bar_t bar[6];

    uint8_t  irq_line;
    uint8_t  irq_pin;

    void    *driver_data;
} component_device_t;

const char* COMPONENT_vendor_name(uint16_t vendor_id);
