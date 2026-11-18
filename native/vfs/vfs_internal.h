#pragma once

#include <stdint.h>

#include "hcvault/volume.h"
#include "hcvault/vfs.h"

struct vfs_ctx {
    vc_volume* vol;
    int fat_bits;
    uint16_t bytes_per_sector;
    uint8_t sectors_per_cluster;
    uint16_t reserved_sectors;
    uint8_t num_fats;
    uint16_t root_entry_count;
    uint16_t sectors_per_fat16;
    uint32_t sectors_per_fat32;
    uint32_t total_sectors;
    uint32_t root_cluster;
    uint32_t fat_start_sector;
    uint32_t data_start_sector;
    uint32_t cluster_count;
};

vfs_status vfs_ctx_read(vfs_ctx* ctx, uint64_t offset, void* buf, size_t len);
vfs_status vfs_ctx_write(vfs_ctx* ctx, uint64_t offset, const void* buf, size_t len);
vfs_status vfs_ctx_read_sector(vfs_ctx* ctx, uint32_t lba, void* buf);
vfs_status vfs_ctx_write_sector(vfs_ctx* ctx, uint32_t lba, const void* buf);