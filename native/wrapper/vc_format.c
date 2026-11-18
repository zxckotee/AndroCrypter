#include "hcvault/volume.h"

#include <string.h>

#include "vc_internal.h"

/* Minimal FAT12 (≈1.44M layout) for containers ≥ 260 KiB data area — matches test fixture style. */
vc_status vc_format_fat12(vc_volume* vol) {
    if (vol == NULL || vol->readonly) {
        return VC_ERR_READONLY;
    }
    uint64_t size = vol->data_size;
    if (size < 260 * 1024) {
        return VC_ERR_RANGE;
    }

    uint16_t cluster_size = 512u;
    uint16_t reserved = 1;
    uint8_t fats = 1;
    uint16_t root_entries = 512;
    uint32_t total_sectors = (uint32_t)(size / 512);
    uint16_t sectors_per_fat = (uint16_t)((root_entries * 32 + cluster_size - 1) / cluster_size + total_sectors / cluster_size + fats) / fats;
    if (sectors_per_fat < 9) {
        sectors_per_fat = 9;
    }

    uint8_t bpb[512];
    memset(bpb, 0, sizeof(bpb));
    bpb[0] = 0xEB;
    bpb[1] = 0x3C;
    bpb[2] = 0x90;
    memcpy(bpb + 3, "MSWIN4.1", 8);
    *(uint16_t*)(bpb + 11) = 512;
    *(uint8_t*)(bpb + 13) = 1;
    *(uint16_t*)(bpb + 14) = reserved;
    *(uint8_t*)(bpb + 16) = fats;
    *(uint16_t*)(bpb + 17) = root_entries;
    *(uint16_t*)(bpb + 19) = (uint16_t)(total_sectors <= 0xFFFF ? total_sectors : 0);
    *(uint8_t*)(bpb + 21) = 0xF9; /* FAT12 */
    *(uint16_t*)(bpb + 22) = cluster_size;
    *(uint16_t*)(bpb + 24) = 1;
    *(uint32_t*)(bpb + 28) = total_sectors;
    bpb[36] = 0x80;
    bpb[38] = 0x29;
    memcpy(bpb + 39, "HCVAULT123", 11);
    memcpy(bpb + 43, "FAT12   ", 8);
    bpb[510] = 0x55;
    bpb[511] = 0xAA;

    vc_status st = vc_write(vol, 0, bpb, sizeof(bpb));
    if (st != VC_OK) {
        return st;
    }
    uint8_t zero[512];
    memset(zero, 0, sizeof(zero));
    for (uint32_t s = 1; s < reserved + fats * sectors_per_fat + 32; s++) {
        st = vc_write(vol, (uint64_t)s * 512, zero, sizeof(zero));
        if (st != VC_OK) {
            return st;
        }
    }
    return vc_flush(vol);
}