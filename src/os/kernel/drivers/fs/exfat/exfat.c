/*
! to anyone who is reading this
! i wrote this at 2 or 3 something AM
! i have no idea how any of this works or how i wrote it
! soo good luck
! -ZWT
*/

#include "exfat.h"

#include "../../disk/common.h"
#include "../../../mem_manager/manager.h"
#include "../../../io/serial/serial.h"
#include "../mbr/mbr.h"
#include "../../../utils.h"

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

uint32_t EXFAT_get_fat_entry(exfat_instance_t fs, uint32_t cluster){
    uint64_t bytes_per_sector = 1ULL << fs.bytes_per_sector_shift;
    uint64_t fat_byte_offset = (uint64_t)cluster * 4;
    uint64_t fat_sector = fat_byte_offset / bytes_per_sector;
    uint64_t offset = fat_byte_offset % bytes_per_sector;
    uint64_t lba = fs.sector_on_disk_LBA + fs.fat_offset + fat_sector;
    uint8_t *buffer = (uint8_t*)kalloc(bytes_per_sector);

    if(!read_disk(&(fs.disk), lba, (uint16_t*)buffer)){
        kfree(buffer);
        return 0;
    }

    uint32_t next_cluster = read_u32_le(buffer, offset);

    kfree(buffer);
    return next_cluster;
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

exfat_file_entry_t *EXFAT_open_file(exfat_instance_t *fs, const char *path){
    const char *current = path;

    while (*current == '/'){
        current++;
    }

    uint32_t directory_cluster = fs->root_cluster;

    while (*current != '\0') {
        char component[256];
        uint64_t component_length = 0;

        while (current[component_length] != '/' && current[component_length] != '\0'){
            component[component_length] = current[component_length];
            component_length++;
        }

        component[component_length] = '\0';

        const char *next = current + component_length;

        while (*next == '/'){
            next++;
        }

        uint8_t *buffer = (uint8_t*)kalloc((1ULL << fs->bytes_per_sector_shift) * (1ULL << fs->sectors_per_cluster_shift));
        uint64_t bytes_per_sector = 1ULL << fs->bytes_per_sector_shift;
        uint64_t sectors_per_cluster = 1ULL << fs->sectors_per_cluster_shift;
        uint64_t cluster_size = bytes_per_sector * sectors_per_cluster;
        uint32_t cluster = directory_cluster;
        exfat_file_entry_t *found = NULL;

        while (1) {
            uint64_t lba = EXFAT_cluster_to_lba(*fs, cluster);

            for (uint64_t s = 0; s < sectors_per_cluster; s++){
                if (!read_disk(&(fs->disk), lba + s, (uint16_t*)&buffer[s * bytes_per_sector])){
                    kfree(buffer);
                    return NULL;
                }
            }

            for (uint64_t i = 0; i < cluster_size / 32; i++){
                uint8_t *entry = &buffer[i * 32];
                if (entry[0] == 0x00)
                    goto directory_done;

                if (entry[0] != 0x85){
                    continue;
                }

                exfat_file_entry_t *file = (exfat_file_entry_t*)kalloc(sizeof(exfat_file_entry_t));

                file->fs = fs;
                file->attributes = read_u16_le(entry, 0x04);
                uint8_t secondary_count = entry[0x01];

                file->name_length = 0;
                file->first_cluster = 0;
                file->data_length = 0;

                char *filename = (char*)kalloc(256);

                uint64_t name_index = 0;
                for (uint64_t f = 0; f < secondary_count; f++){
                    uint8_t *secondary =
                        &buffer[
                            (i + 1 + f) * 32
                        ];

                    if (secondary[0] == 0xC0) {
                        file->name_length = secondary[0x03];
                        file->name_hash = read_u16_le(secondary, 0x04);

                        file->flags = secondary[0x01];
                        file->valid_data_length = read_u64_le(secondary, 0x08);
                        file->first_cluster = read_u32_le(secondary, 0x14);
                        file->data_length = read_u64_le(secondary, 0x18);
                    }

                    if (secondary[0] == 0xC1) {
                        for (uint64_t k = 0; k < 15; k++){
                            if (name_index >= file->name_length){
                                break;
                            }
                            uint16_t c = read_u16_le(secondary, 0x02 + k * 2);
                            filename[name_index++] =
                                (char)c;
                        }
                    }
                }
                filename[name_index] = '\0';

                if (strcmp(filename, component) == 0) {
                    file->path = filename;
                    found = file;
                    goto directory_done;
                }

                kfree(filename);
                kfree(file);
            }

            uint32_t next_cluster = EXFAT_get_fat_entry(*fs, cluster);
            if (next_cluster >= 0xFFFFFFF8){
                break;
            }
            cluster = next_cluster;
        }

directory_done:
        kfree(buffer);
        if (found == NULL)
            return NULL;
        if (*next == '\0') {
            return found;
        }

        if (!(found->attributes & 0x0010)) {
            kfree(found->path);
            kfree(found);
            return NULL;
        }

        directory_cluster = found->first_cluster;

        kfree(found->path);
        kfree(found);
        current = next;
    }
    return NULL;
}

uint8_t* EXFAT_read_file(exfat_file_entry_t *file, uint64_t *size){
    exfat_instance_t *fs = file->fs;
    *size = file->data_length;

    uint8_t *data = (uint8_t*)kalloc(file->data_length);
    uint8_t *cluster_buffer = (uint8_t*)kalloc((1 << fs->bytes_per_sector_shift) * (1 << fs->sectors_per_cluster_shift));
    uint64_t bytes_per_sector = 1ULL << fs->bytes_per_sector_shift;
    uint64_t sectors_per_cluster = 1ULL << fs->sectors_per_cluster_shift;
    uint64_t cluster_size = bytes_per_sector * sectors_per_cluster;
    uint64_t bytes_read = 0;
    uint32_t cluster = file->first_cluster;

    while (bytes_read < file->data_length){
        uint64_t lba = EXFAT_cluster_to_lba(*fs, cluster);
        for (uint64_t i = 0; i < sectors_per_cluster; i++) {
            read_disk(&(fs->disk), lba + i, (uint16_t*)&cluster_buffer[i * bytes_per_sector]);
        }

        uint64_t bytes_to_copy = file->data_length - bytes_read;
        if (bytes_to_copy > cluster_size){
            bytes_to_copy = cluster_size;
        }

        for (uint64_t i = 0; i < bytes_to_copy; i++) {
            data[bytes_read + i] = cluster_buffer[i];
        }

        bytes_read += bytes_to_copy;
        if (bytes_read >= file->data_length){
            break;
        }

        if (file->flags & 0x02) {
            cluster++;
        } else {
            cluster = EXFAT_get_fat_entry(*fs, cluster);
        }
    }

    kfree(cluster_buffer);
    return data;
}

uint8_t* EXFAT_read_file_ALL(exfat_file_entry_t *file, size_t *size){
    exfat_instance_t *fs = file->fs;
    uint64_t bytes_per_sector = 1ULL << fs->bytes_per_sector_shift;
    uint64_t sectors_per_cluster = 1ULL << fs->sectors_per_cluster_shift;
    uint64_t cluster_size = bytes_per_sector * sectors_per_cluster;
    uint8_t *data = (uint8_t*)kalloc(file->data_length);
    uint8_t *cluster_buffer = (uint8_t*)kalloc(cluster_size);
    uint64_t bytes_read = 0;
    uint32_t cluster = file->first_cluster;

    while (bytes_read < file->data_length) {
        uint64_t lba = EXFAT_cluster_to_lba(*fs, cluster);
        for (uint64_t i = 0; i < sectors_per_cluster; i++) {
            if (!read_disk(&(fs->disk), lba + i, (uint16_t*)&cluster_buffer[i * bytes_per_sector])){

                kfree(cluster_buffer);
                kfree(data);
                return NULL;
            }
        }

        uint64_t remaining = file->data_length - bytes_read;
        uint64_t copy_size = cluster_size;
        if (remaining < copy_size){
            copy_size = remaining;
        }

        for (uint64_t i = 0; i < copy_size; i++){
            data[bytes_read + i] = cluster_buffer[i];
        }
        bytes_read += copy_size;
        if (bytes_read >= file->data_length){
            break;
        }

        if (file->flags & 0x02) {
            cluster++;
        } else {
            cluster = EXFAT_get_fat_entry(*fs, cluster);

            if (cluster >= 0xFFFFFFF8) {
                kfree(cluster_buffer);
                kfree(data);
                return NULL;
            }
        }
    }

    *size = bytes_read;

    kfree(cluster_buffer);
    return data;
}
