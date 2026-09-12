#include "exfat.h"

#include "../../disk/common.h"
#include "../../../mem_manager/manager.h"
#include "../../../io/serial/serial.h"
#include "../mbr/mbr.h"

uint16_t read_u16_le(uint8_t *buff, uint64_t offset){

    return  ((uint16_t)buff[offset]) |
            ((uint16_t)buff[offset + 1] << 8);
}

uint32_t read_u32_le(uint8_t *buff, uint64_t offset){
    return  ((uint32_t)buff[offset]) |
            ((uint32_t)buff[offset + 1] << 8) |
            ((uint32_t)buff[offset + 2] << 16) |
            ((uint32_t)buff[offset + 3] << 24);
}

uint64_t read_u64_le(uint8_t *buff, uint64_t offset){
    return  ((uint64_t)buff[offset]) |
            ((uint64_t)buff[offset + 1] << 8) |
            ((uint64_t)buff[offset + 2] << 16) |
            ((uint64_t)buff[offset + 3] << 24) |
            ((uint64_t)buff[offset + 4] << 32) |
            ((uint64_t)buff[offset + 5] << 40) |
            ((uint64_t)buff[offset + 6] << 48) |
            ((uint64_t)buff[offset + 7] << 56);
}

uint64_t EXFAT_cluster_to_lba(exfat_instance_t fs, uint32_t cluster){
    uint32_t sectors_per_cluster = 1 << fs.sectors_per_cluster_shift;

    return fs.sector_on_disk_LBA + fs.cluster_heap_offset + ((uint64_t)(cluster - 2) * sectors_per_cluster);
}

exfat_instance_t EXFAT_init(disk_device_t *disk, mbr_partition_t *mbr){
    uint8_t *buff = (uint8_t*)kalloc(sizeof(uint8_t)*512);
    exfat_instance_t ret;
    char exfat_sig[9];
    read_disk(disk, mbr->lba_start, (uint16_t*)buff);
    for(uint64_t i = 3; i<11; i++){
        exfat_sig[i-3] = buff[i];
    }
    exfat_sig[8] = '\0';
    serial_printLN(exfat_sig);

    ret.disk = *disk;
    serial_print("EXFAT init for: ");
    serial_printLN(ret.disk.name);
    ret.partition_offset = read_u64_le(buff, 0x40);
    ret.volume_length = read_u64_le(buff, 0x48);

    ret.fat_offset = read_u32_le(buff, 0x50);
    ret.fat_length = read_u32_le(buff, 0x54);
    ret.cluster_heap_offset = read_u32_le(buff, 0x58);
    ret.cluster_count = read_u32_le(buff, 0x5C);
    ret.root_cluster = read_u32_le(buff, 0x60);
    serial_print("EXFAT root cluster at: ");
    serial_print_hexLN(ret.root_cluster);

    ret.bytes_per_sector_shift = buff[0x6C];
    ret.sectors_per_cluster_shift = buff[0x6D];
    ret.number_of_fats = buff[0x6E];

    ret.sector_on_disk_LBA = mbr->lba_start;

    kfree(buff);
    return ret;
}

uint64_t EXFAT_read_file(exfat_instance_t fs, const char *name){
    uint8_t *root_dir_entries = (uint8_t*)kalloc(sizeof(uint8_t)*512);
    read_disk(&(fs.disk), EXFAT_cluster_to_lba(fs, fs.root_cluster), (uint16_t*)root_dir_entries);

    for(uint64_t i = 0; i<512/32; i++){
        uint8_t *entry = &root_dir_entries[i * 32];

        if (entry[0] == 0x85){
            exfat_file_entry_t file;
            file.attributes = read_u16_le(entry, 0x04);

            uint8_t secondary_count = entry[0x01];
            for (uint64_t f = 0; f < secondary_count; f++){
                uint8_t *secondary = &root_dir_entries[(i + 1 + f) * 32];
                if (secondary[0] == 0xC0){
                    file.name_length = secondary[0x03];
                    file.name_hash = read_u16_le(secondary, 0x04);
                    file.flags = secondary[0x06];
                    file.valid_data_length =
                        read_u64_le(secondary, 0x08);
                    file.first_cluster =
                        read_u32_le(secondary, 0x14);
                    file.data_length =
                        read_u64_le(secondary, 0x18);
                }
                if (secondary[0] == 0xC1) {
                    for(uint16_t k = 2; k<17; k++){
                        serial_write(secondary[k]);
                    }
                }
            }
            serial_write('\n');
        }
    }

    kfree(root_dir_entries);
    return 0;
}
