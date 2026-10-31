#include "sata.h"

#include "../../component_bus/common.h"
#include "../../../io/serial/serial.h"
#include "../../../mem_manager/manager.h"
#include "../../../utils.h"

#define SATA_SIG    0x00000101
#define ATAPI_SIG   0xEB140101

//#define CMD_SATA_READ_PIO        0x20
//#define CMD_SATA_READ_PIO_EXT    0x24
//#define CMD_SATA_WRITE_PIO       0x30
//#define CMD_SATA_WRITE_PIO_EXT   0x34
#define CMD_SATA_READ_DMA_EXT      0x24
#define CMD_SATA_IDENTIFY           0xEC
#define CMD_SATA_IDENTIFY_PACKET    0xA1
#define CMD_SATA_FLUSH              0xE7
#define CMD_SATA_FLUSH_EXT          0xEA

#define DEF_TIMEOUT 10000000

static int64_t SATA_find_command_slot(volatile HBA_PORT_t *port, size_t slots) {
    uint32_t occupied = port->ci | port->sact;
    for (uint64_t i = 0; i < slots; i++) {
        if (!(occupied & (1u << i)))
            return i;
    }
    return -1;
}

static bool SATA_port_configure(volatile HBA_PORT_t *port, SATA_port_data_t *data, size_t command_slots){
    if (!port || !data || command_slots == 0 || command_slots > 32) {
        return false;
    }

    // Stop command engine
    port->cmd &= ~(1u << 0);

    uint32_t timeout = 10000000;
    while (port->cmd & (1u << 15)) {
        if (!timeout--) {
            return false;
        }
    }

    // Stop FIS reception
    port->cmd &= ~(1u << 4);

    timeout = 10000000;

    while (port->cmd & (1u << 14)) {
        if (!timeout--) {
            return false;
        }
    }

    // Allocate command list
    data->cmd_list = low_kalloc_aligned(1024, 1024);

    // Allocate received FIS buffer
    data->fis = low_kalloc_aligned(256, 256);

    // Allocate command table pointers
    data->cmd_tables = low_kalloc(
        sizeof(HBA_CMD_TABLE_t *) * command_slots
    );

    if (!data->cmd_list || !data->fis || !data->cmd_tables)
        return false;

    memset(data->cmd_list, 0, 1024);
    memset(data->fis, 0, 256);
    memset(data->cmd_tables, 0, sizeof(HBA_CMD_TABLE_t *) * command_slots);

    data->command_slots = command_slots;

    // Allocate individual command tables
    for (size_t i = 0; i < command_slots; i++) {
        data->cmd_tables[i] = low_kalloc_aligned(sizeof(HBA_CMD_TABLE_t), 128);
        if (!data->cmd_tables[i]) {
            return false;
        }
        memset(data->cmd_tables[i], 0, sizeof(HBA_CMD_TABLE_t));
        // Configure command header
        data->cmd_list[i].ctba = (uint32_t)(uintptr_t)data->cmd_tables[i];
        data->cmd_list[i].ctbau = 0;
    }

    // Configure command list address
    port->clb = (uint32_t)(uintptr_t)data->cmd_list;
    port->clbu = 0;

    // Configure received FIS address
    port->fb = (uint32_t)(uintptr_t)data->fis;
    port->fbu = 0;

    // Enable FIS reception
    port->cmd |= (1u << 4);

    // Start command engine
    port->cmd |= (1u << 0);

    return true;
}

static bool SATA_execute_command(volatile HBA_PORT_t *port, SATA_port_data_t *data, FIS_REG_H2D_t *fis, void *buffer, size_t buffer_size, bool write){
    if (!port || !data || !fis) {
        return false;
    }

    if (!buffer || buffer_size == 0 || buffer_size > 0x400000) {
        return false;
    }

    // Find available command slot
    int slot = SATA_find_command_slot(port, data->command_slots);

    if (slot < 0) {
        return false;
    }

    // Wait for device readiness
    uint32_t timeout = 10000000;
    while (port->tfd & ((1u << 7) | (1u << 3))) {
        if (!timeout--) {
            return false;
        }
    }

    // Get command resources
    HBA_CMD_HEADER_t *header = &data->cmd_list[slot];
    HBA_CMD_TABLE_t *table = data->cmd_tables[slot];

    // Clear command table
    memset(table, 0, sizeof(HBA_CMD_TABLE_t));

    // Copy command FIS
    memcpy(fis, table->cfis, sizeof(FIS_REG_H2D_t));

    // Configure command header
    header->flags = 5 | (write ? (1u << 6) : 0);
    header->prdtl = 1;
    header->prdbc = 0;

    // Configure PRDT
    HBA_PRDT_ENTRY_t *prdt = &table->prdt_entry[0];
    uintptr_t dma_address = (uintptr_t)buffer;

    if ((dma_address & 3) || dma_address > UINT32_MAX) {
        return false;
    }

    prdt->dba = (uint32_t)dma_address;
    prdt->dbau = 0;
    prdt->reserved = 0;
    prdt->dbc_i = (uint32_t)(buffer_size - 1);

    // Clear previous interrupt/error status
    port->is = 0xFFFFFFFF;
    port->serr = 0xFFFFFFFF;

    // Ensure memory writes are visible before issuing DMA
    asm volatile("" ::: "memory");

    // Issue command
    port->ci = 1u << slot;

    // Wait for completion
    timeout = 10000000;

    while (port->ci & (1u << slot)) {
        if (port->is & (1u << 30)) {
            return false;
        }
        if (!timeout--) {
            return false;
        }
    }

    // Check final task-file status
    if (port->tfd & 1u) {
        return false;
    }

    if (port->is & (1u << 30)) {
        return false;
    }

    return true;
}

static void SATA_get_model(uint16_t *identify, char *model) {
    for (int i = 0; i < 20; i++) {
        model[i * 2]     = identify[27 + i] >> 8;
        model[i * 2 + 1] = identify[27 + i] & 0xFF;
    }
    model[40] = '\0';
}

disk_device_t** SATA_get_disks(size_t *size){
    size_t devices_size = 0;
    component_device_t *devices = component_bus_get_devicesEX(0x01, 0x06, 0x01, &devices_size);
    *size = 16;
    size_t ret_IDX = 0;
    disk_device_t **ret = kalloc(sizeof(disk_device_t*)*(*size));

    for(uint32_t i = 0; i<devices_size; i++){
        if(devices[i].bar[5].is_valid){
            uint64_t ABAR = devices[i].bar[5].base;
            volatile uint32_t *HBA = (volatile uint32_t *)ABAR;

            serial_print("SATA: ABAR: ");
            serial_print_hexLN(ABAR);

            serial_print("SATA: BAR valid: ");
            serial_print_hexLN(devices[i].bar[5].is_valid);

            serial_print("SATA: BAR size: ");
            serial_print_hexLN(devices[i].bar[5].bar_size);
            uint32_t cap = HBA[0];
            uint32_t ghc = HBA[1];
            uint32_t pi  = HBA[3];
            uint32_t vs  = HBA[4];
            uint32_t ports_total = (cap & 0x1F) + 1;
            for(uint8_t f = 0; f<ports_total; f++){
                if(!(pi & (1u << f))){
                    continue;
                }
                volatile HBA_PORT_t *port = (volatile HBA_PORT_t*)((uintptr_t)HBA + 0x100 + f * 0x80);

                uint32_t ssts = port->ssts;
                uint32_t det = ssts & 0x0F;
                uint32_t ipm = (ssts >> 8) & 0x0F;

                if(det == 3 && ipm == 1){
                    uint32_t sig = port->sig;
                    if(sig == ATAPI_SIG){
                        serial_printLN("SATA: ATAPI is not supported :(");
                        continue;
                    }
                    if(sig != SATA_SIG){
                        serial_printLN("SATA: unknown device");
                        continue;
                    }

                    void *id_buff = low_kalloc_aligned(512, 512);
                    SATA_port_data_t *port_data = (SATA_port_data_t*)kalloc(sizeof(SATA_port_data_t));
                    if (!id_buff || !port_data) {
                        serial_printLN("SATA: failed to allocate memory for ID");
                        halt();
                    }

                    FIS_REG_H2D_t fis_to_send; //! breaks with = {0} init idk why so therefore the memset below
                    memset(&fis_to_send, 0, sizeof(FIS_REG_H2D_t));
                    fis_to_send.fis_type = 0x27;
                    fis_to_send.pmport_c = 1 << 7;
                    fis_to_send.command = CMD_SATA_IDENTIFY;

                    uint64_t slot = ((cap >> 8) & 0x1F) + 1;
                    if (!SATA_port_configure(port, port_data, slot)) {
                        serial_printLN("SATA: failed to configure port");
                        halt();
                    }
                    char *name = (char*)kalloc(sizeof(char)*41);
                    if(SATA_execute_command(port, port_data, &fis_to_send, id_buff, 512, false)) {
                        serial_printLN("SATA: sent command");
                        SATA_get_model(id_buff, name);
                    }else {
                        serial_printLN("SATA: failed to send command");
                        halt();
                    }

                    disk_device_t *dev = kalloc(sizeof(disk_device_t));
                    dev->type = DISK_SATA;
                    SATA_driver_data_t *data = kalloc(sizeof(SATA_driver_data_t));
                    data->HBA = HBA;
                    data->port = port;
                    data->port_data = port_data;
                    data->port_num = f;
                    dev->driver_data = data;
                    memcpy(name, dev->name, sizeof(char)*41);

                    kfree(name);
                    low_kfree(id_buff);

                    serial_print("SATA: fount SATA device !: ");
                    serial_print_hex(sig);
                    serial_print("  ");
                    serial_printLN(dev->name);

                    if(ret_IDX >= *size){
                        disk_device_t **buff = kalloc(sizeof(disk_device_t*)*(*size+16));
                        memcpy(ret, buff, sizeof(disk_device_t*)*(*size));
                        kfree(ret);
                        ret = buff;

                        *size += 16;
                    }
                    ret[ret_IDX] = dev;
                    ret_IDX++;
                }
            }
        }
    }

    if(*size > ret_IDX){
        disk_device_t **buff = kalloc(sizeof(disk_device_t*)*(ret_IDX));
        memcpy(ret, buff, sizeof(disk_device_t*)*ret_IDX);
        kfree(ret);
        ret = buff;
        *size = ret_IDX;
    }

    return ret;
}

bool SATA_read(disk_device_t *disk, uint64_t lba, uint16_t *buff) {
    if (!disk || !buff) {
        return false;
    }

    // Allocate a DMA-compatible sector buffer.
    uint16_t *dma_buffer = low_kalloc_aligned(512, 512);
    if (!dma_buffer) {
        return false;
    }

    memset(dma_buffer, 0, 512);

    FIS_REG_H2D_t fis;
    memset(&fis, 0, sizeof(FIS_REG_H2D_t));

    fis.fis_type = 0x27;
    fis.pmport_c = 0x80;
    fis.command = CMD_SATA_READ_DMA_EXT;

    fis.lba0 = (uint8_t)(lba);
    fis.lba1 = (uint8_t)(lba >> 8);
    fis.lba2 = (uint8_t)(lba >> 16);
    fis.lba3 = (uint8_t)(lba >> 24);
    fis.lba4 = (uint8_t)(lba >> 32);
    fis.lba5 = (uint8_t)(lba >> 40);

    fis.device = 1 << 6; // LBA mode

    fis.countl = 1;
    fis.counth = 0;

    SATA_driver_data_t *data = (SATA_driver_data_t*)disk->driver_data;

    bool success = SATA_execute_command(data->port, data->port_data, &fis, dma_buffer, 512,false);

    if (success) {
        memcpy(dma_buffer, buff, 512);
    }

    low_kfree(dma_buffer);
    return success;
}
