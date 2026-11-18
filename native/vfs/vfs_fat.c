#include "vfs_internal.h"

#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define DIR_ENTRY_SIZE 32u

enum {
    ATTR_READONLY = 0x01,
    ATTR_HIDDEN = 0x02,
    ATTR_SYSTEM = 0x04,
    ATTR_VOLUME_ID = 0x08,
    ATTR_DIRECTORY = 0x10,
    ATTR_ARCHIVE = 0x20,
    ATTR_LONG_NAME = 0x0F,
    SLOT_FREE = 0x00,
    SLOT_DELETED = 0xE5,
};

static uint16_t u16le(const uint8_t* p) {
    return (uint16_t)p[0] | ((uint16_t)p[1] << 8);
}

static uint32_t u32le(const uint8_t* p) {
    return (uint32_t)p[0] | ((uint32_t)p[1] << 8) | ((uint32_t)p[2] << 16) | ((uint32_t)p[3] << 24);
}

static vfs_status from_vc(vc_status st) {
    switch (st) {
    case VC_OK:
        return VFS_OK;
    case VC_ERR_UNSUPPORTED:
        return VFS_ERR_UNSUPPORTED;
    case VC_ERR_RANGE:
        return VFS_ERR_RANGE;
    case VC_ERR_IO:
        return VFS_ERR_IO;
    default:
        return VFS_ERR_INTERNAL;
    }
}

vfs_status vfs_ctx_read(vfs_ctx* ctx, uint64_t offset, void* buf, size_t len) {
    if (ctx == NULL || ctx->vol == NULL) {
        return VFS_ERR_INTERNAL;
    }
    return from_vc(vc_read(ctx->vol, offset, buf, len));
}

vfs_status vfs_ctx_write(vfs_ctx* ctx, uint64_t offset, const void* buf, size_t len) {
    if (ctx == NULL || ctx->vol == NULL) {
        return VFS_ERR_INTERNAL;
    }
    return from_vc(vc_write(ctx->vol, offset, buf, len));
}

vfs_status vfs_ctx_read_sector(vfs_ctx* ctx, uint32_t lba, void* buf) {
    return vfs_ctx_read(ctx, (uint64_t)lba * ctx->bytes_per_sector, buf, ctx->bytes_per_sector);
}

vfs_status vfs_ctx_write_sector(vfs_ctx* ctx, uint32_t lba, const void* buf) {
    return vfs_ctx_write(ctx, (uint64_t)lba * ctx->bytes_per_sector, buf, ctx->bytes_per_sector);
}

static int boot_is_fat(const uint8_t* boot) {
    if (boot[510] != 0x55 || boot[511] != 0xAA) {
        return 0;
    }
    if (memcmp(boot + 54, "FAT12   ", 8) == 0) {
        return 12;
    }
    if (memcmp(boot + 54, "FAT16   ", 8) == 0 || memcmp(boot + 54, "FAT     ", 8) == 0) {
        return 16;
    }
    if (memcmp(boot + 82, "FAT32   ", 8) == 0) {
        return 32;
    }
    return 0;
}

static void format_dos_name(const uint8_t* raw, char* out, size_t out_len) {
    char base[9];
    char ext[4];
    size_t i;
    for (i = 0; i < 8; i++) {
        base[i] = (char)raw[i];
    }
    base[8] = '\0';
    for (i = 7; i > 0 && (base[i] == ' ' || base[i] == '\0'); i--) {
        base[i] = '\0';
    }
    for (i = 0; i < 3; i++) {
        ext[i] = (char)raw[8 + i];
    }
    ext[3] = '\0';
    for (i = 2; i > 0 && (ext[i] == ' ' || ext[i] == '\0'); i--) {
        ext[i] = '\0';
    }
    while (ext[0] != '\0' && ext[0] == ' ') {
        memmove(ext, ext + 1, sizeof(ext));
    }
    if (ext[0] == '\0') {
        snprintf(out, out_len, "%s", base);
    } else {
        snprintf(out, out_len, "%s.%s", base, ext);
    }
}

static int name_match(const char* a, const char* b) {
    while (*a && *b) {
        unsigned char ca = (unsigned char)*a++;
        unsigned char cb = (unsigned char)*b++;
        if (ca >= 'a' && ca <= 'z') {
            ca = (unsigned char)(ca - 'a' + 'A');
        }
        if (cb >= 'a' && cb <= 'z') {
            cb = (unsigned char)(cb - 'a' + 'A');
        }
        if (ca != cb) {
            return 0;
        }
    }
    return *a == '\0' && *b == '\0';
}

static vfs_status fat_read_bytes(vfs_ctx* ctx, uint8_t* fat, size_t fat_bytes) {
    uint32_t per = (ctx->fat_bits == 32) ? ctx->sectors_per_fat32 : ctx->sectors_per_fat16;
    size_t need = (size_t)per * ctx->bytes_per_sector;
    if (fat_bytes < need) {
        return VFS_ERR_INTERNAL;
    }
    uint8_t* sec = fat;
    for (uint32_t i = 0; i < per; i++) {
        vfs_status st = vfs_ctx_read_sector(ctx, ctx->fat_start_sector + i, sec);
        if (st != VFS_OK) {
            return st;
        }
        sec += ctx->bytes_per_sector;
    }
    return VFS_OK;
}

static uint32_t fat_next(vfs_ctx* ctx, const uint8_t* fat, uint32_t cluster) {
    switch (ctx->fat_bits) {
    case 12: {
        uint32_t offset = cluster + cluster / 2;
        uint16_t ent = (uint16_t)fat[offset] | ((uint16_t)(fat[offset + 1] & 0xFF) << 8);
        if (cluster & 1u) {
            ent >>= 4;
        } else {
            ent &= 0x0FFFu;
        }
        return ent;
    }
    case 16:
        return u16le(fat + cluster * 2u);
    case 32:
        return u32le(fat + cluster * 4u) & 0x0FFFFFFFu;
    default:
        return 0x0FFFFFF8u;
    }
}

static int fat_eoc(vfs_ctx* ctx, uint32_t ent) {
    switch (ctx->fat_bits) {
    case 12:
        return ent >= 0x0FF8u;
    case 16:
        return ent >= 0xFFF8u;
    case 32:
        return ent >= 0x0FFFFFF8u;
    default:
        return 1;
    }
}

static vfs_status parse_dir_data(vfs_ctx* ctx, const uint8_t* data, size_t len, vfs_dirent* entries, size_t cap, size_t* count) {
    size_t n = 0;
    for (size_t i = 0; i + DIR_ENTRY_SIZE <= len; i += DIR_ENTRY_SIZE) {
        const uint8_t* slot = data + i;
        uint8_t first = slot[0];
        if (first == SLOT_FREE) {
            break;
        }
        if (first == SLOT_DELETED) {
            continue;
        }
        uint8_t attr = slot[11];
        if (attr == ATTR_LONG_NAME) {
            continue;
        }
        if (attr & ATTR_VOLUME_ID) {
            continue;
        }
        if (n >= cap) {
            return VFS_ERR_RANGE;
        }
        format_dos_name(slot, entries[n].name, sizeof(entries[n].name));
        if (entries[n].name[0] == '\0') {
            continue;
        }
        entries[n].is_dir = (attr & ATTR_DIRECTORY) ? 1 : 0;
        entries[n].size = u32le(slot + 28);
        n++;
    }
    if (count) {
        *count = n;
    }
    return VFS_OK;
}

static vfs_status read_root_fat1216(vfs_ctx* ctx, uint8_t** out, size_t* out_len) {
    uint32_t root_sectors = (uint32_t)ctx->root_entry_count * DIR_ENTRY_SIZE;
    root_sectors = (root_sectors + ctx->bytes_per_sector - 1u) / ctx->bytes_per_sector;
    size_t len = (size_t)root_sectors * ctx->bytes_per_sector;
    uint8_t* buf = malloc(len);
    if (buf == NULL) {
        return VFS_ERR_INTERNAL;
    }
    uint32_t lba = ctx->reserved_sectors + ctx->num_fats * ctx->sectors_per_fat16;
    for (uint32_t i = 0; i < root_sectors; i++) {
        vfs_status st = vfs_ctx_read_sector(ctx, lba + i, buf + (size_t)i * ctx->bytes_per_sector);
        if (st != VFS_OK) {
            free(buf);
            return st;
        }
    }
    *out = buf;
    *out_len = len;
    return VFS_OK;
}

static vfs_status read_cluster_chain(vfs_ctx* ctx, const uint8_t* fat, uint32_t start, uint8_t** out, size_t* out_len) {
    if (start < 2u) {
        return VFS_ERR_UNSUPPORTED;
    }
    size_t cap = (size_t)ctx->sectors_per_cluster * ctx->bytes_per_sector * 64u;
    uint8_t* buf = malloc(cap);
    if (buf == NULL) {
        return VFS_ERR_INTERNAL;
    }
    size_t used = 0;
    uint32_t cluster = start;
    for (int guard = 0; guard < 65536; guard++) {
        uint32_t lba = ctx->data_start_sector + (cluster - 2u) * ctx->sectors_per_cluster;
        for (uint32_t s = 0; s < ctx->sectors_per_cluster; s++) {
            if (used + ctx->bytes_per_sector > cap) {
                free(buf);
                return VFS_ERR_RANGE;
            }
            vfs_status st = vfs_ctx_read_sector(ctx, lba + s, buf + used);
            if (st != VFS_OK) {
                free(buf);
                return st;
            }
            used += ctx->bytes_per_sector;
        }
        uint32_t next = fat_next(ctx, fat, cluster);
        if (fat_eoc(ctx, next)) {
            break;
        }
        cluster = next;
    }
    *out = buf;
    *out_len = used;
    return VFS_OK;
}

static uint32_t entry_first_cluster(const vfs_ctx* ctx, const uint8_t* entry) {
    uint32_t lo = u16le(entry + 26);
    if (ctx->fat_bits == 32) {
        uint32_t hi = u16le(entry + 20);
        return (hi << 16) | lo;
    }
    return lo;
}

static vfs_status find_in_dir(vfs_ctx* ctx, const uint8_t* dir, size_t dir_len, const char* name, uint8_t* entry_out) {
    for (size_t i = 0; i + DIR_ENTRY_SIZE <= dir_len; i += DIR_ENTRY_SIZE) {
        const uint8_t* slot = dir + i;
        if (slot[0] == SLOT_FREE) {
            break;
        }
        if (slot[0] == SLOT_DELETED || slot[11] == ATTR_LONG_NAME || (slot[11] & ATTR_VOLUME_ID)) {
            continue;
        }
        char tmp[VFS_NAME_MAX];
        format_dos_name(slot, tmp, sizeof(tmp));
        if (name_match(tmp, name)) {
            memcpy(entry_out, slot, DIR_ENTRY_SIZE);
            return VFS_OK;
        }
    }
    return VFS_ERR_NOT_FOUND;
}

static vfs_status load_dir_at_cluster(vfs_ctx* ctx, const uint8_t* fat, uint32_t cluster, uint8_t** dir, size_t* dir_len) {
    if (ctx->fat_bits == 32 || cluster != 0) {
        uint32_t start = cluster;
        if (start == 0 && ctx->fat_bits == 32) {
            start = ctx->root_cluster;
        }
        return read_cluster_chain(ctx, fat, start, dir, dir_len);
    }
    return read_root_fat1216(ctx, dir, dir_len);
}

static vfs_status resolve_path(vfs_ctx* ctx, const uint8_t* fat, const char* path, uint8_t* entry_out) {
    if (path == NULL) {
        path = "/";
    }
    while (*path == '/') {
        path++;
    }
    if (*path == '\0') {
        return VFS_ERR_IS_DIR;
    }

    char work[512];
    if (strlen(path) >= sizeof(work)) {
        return VFS_ERR_RANGE;
    }
    memcpy(work, path, strlen(path) + 1);

    char* save = NULL;
    char* tok = strtok_r(work, "/", &save);
    char* next_tok;
    uint32_t dir_cluster = 0;
    uint8_t* dir_buf = NULL;
    size_t dir_len = 0;
    vfs_status st;

    if (ctx->fat_bits == 32) {
        dir_cluster = ctx->root_cluster;
    }
    st = load_dir_at_cluster(ctx, fat, dir_cluster, &dir_buf, &dir_len);
    if (st != VFS_OK) {
        return st;
    }

    uint8_t entry[DIR_ENTRY_SIZE];
    while (tok != NULL) {
        next_tok = strtok_r(NULL, "/", &save);
        st = find_in_dir(ctx, dir_buf, dir_len, tok, entry);
        if (st != VFS_OK) {
            free(dir_buf);
            return st;
        }
        if (next_tok != NULL) {
            if ((entry[11] & ATTR_DIRECTORY) == 0) {
                free(dir_buf);
                return VFS_ERR_NOT_FOUND;
            }
            uint32_t clus = entry_first_cluster(ctx, entry);
            free(dir_buf);
            dir_buf = NULL;
            st = load_dir_at_cluster(ctx, fat, clus, &dir_buf, &dir_len);
            if (st != VFS_OK) {
                return st;
            }
        } else {
            memcpy(entry_out, entry, DIR_ENTRY_SIZE);
            free(dir_buf);
            return VFS_OK;
        }
        tok = next_tok;
    }
    free(dir_buf);
    return VFS_ERR_NOT_FOUND;
}

vfs_status vfs_open(vc_volume* vol, vfs_ctx** out) {
    if (vol == NULL || out == NULL) {
        return VFS_ERR_INTERNAL;
    }
    uint8_t boot[512];
    vfs_status st = from_vc(vc_read(vol, 0, boot, sizeof(boot)));
    if (st != VFS_OK) {
        return st;
    }
    int bits = boot_is_fat(boot);
    if (bits == 0) {
        uint16_t bps = u16le(boot + 11);
        uint16_t root_ent = u16le(boot + 17);
        uint16_t spf = u16le(boot + 22);
        uint32_t total = u16le(boot + 19);
        if (total == 0) {
            total = u32le(boot + 32);
        }
        uint32_t data_sects = total - (uint32_t)u16le(boot + 14) - (uint32_t)boot[16] * spf -
                              ((root_ent * DIR_ENTRY_SIZE + bps - 1u) / bps);
        uint32_t clusters = data_sects / boot[13];
        bits = (clusters < 4085u) ? 12 : 16;
    }

    vfs_ctx* ctx = calloc(1, sizeof(*ctx));
    if (ctx == NULL) {
        return VFS_ERR_INTERNAL;
    }
    ctx->vol = vol;
    ctx->fat_bits = bits;
    ctx->bytes_per_sector = u16le(boot + 11);
    ctx->sectors_per_cluster = boot[13];
    ctx->reserved_sectors = u16le(boot + 14);
    ctx->num_fats = boot[16];
    ctx->root_entry_count = u16le(boot + 17);
    ctx->sectors_per_fat16 = u16le(boot + 22);
    ctx->sectors_per_fat32 = u32le(boot + 36);
    ctx->total_sectors = u16le(boot + 19);
    if (ctx->total_sectors == 0) {
        ctx->total_sectors = u32le(boot + 32);
    }
    ctx->root_cluster = u32le(boot + 44);
    ctx->fat_start_sector = ctx->reserved_sectors;

    if (bits == 32) {
        ctx->data_start_sector = ctx->reserved_sectors + ctx->num_fats * ctx->sectors_per_fat32;
        ctx->cluster_count = ctx->total_sectors - ctx->data_start_sector;
        ctx->cluster_count /= ctx->sectors_per_cluster;
    } else {
        uint32_t root_sectors =
            (ctx->root_entry_count * DIR_ENTRY_SIZE + ctx->bytes_per_sector - 1u) / ctx->bytes_per_sector;
        ctx->data_start_sector = ctx->reserved_sectors + ctx->num_fats * ctx->sectors_per_fat16 + root_sectors;
        uint32_t data_sects = ctx->total_sectors - ctx->data_start_sector;
        ctx->cluster_count = data_sects / ctx->sectors_per_cluster;
    }

    if (ctx->bytes_per_sector < 512 || ctx->sectors_per_cluster == 0 || ctx->num_fats == 0) {
        free(ctx);
        return VFS_ERR_UNSUPPORTED;
    }

    *out = ctx;
    return VFS_OK;
}

void vfs_close(vfs_ctx* ctx) {
    free(ctx);
}

vfs_status vfs_list_dir(vfs_ctx* ctx, const char* path, vfs_dirent* entries, size_t cap, size_t* count) {
    if (ctx == NULL || entries == NULL || cap == 0) {
        return VFS_ERR_INTERNAL;
    }
    if (path == NULL) {
        path = "/";
    }
    while (*path == '/') {
        path++;
    }

    uint32_t fat_bytes =
        ((ctx->fat_bits == 32) ? ctx->sectors_per_fat32 : ctx->sectors_per_fat16) * ctx->bytes_per_sector;
    uint8_t* fat = malloc(fat_bytes);
    if (fat == NULL) {
        return VFS_ERR_INTERNAL;
    }
    vfs_status st = fat_read_bytes(ctx, fat, fat_bytes);
    if (st != VFS_OK) {
        free(fat);
        return st;
    }

    uint32_t dir_cluster = 0;
    if (*path != '\0') {
        uint8_t entry[DIR_ENTRY_SIZE];
        st = resolve_path(ctx, fat, path, entry);
        if (st != VFS_OK) {
            free(fat);
            return st;
        }
        if ((entry[11] & ATTR_DIRECTORY) == 0) {
            free(fat);
            return VFS_ERR_IS_DIR;
        }
        dir_cluster = entry_first_cluster(ctx, entry);
    } else if (ctx->fat_bits == 32) {
        dir_cluster = ctx->root_cluster;
    }

    uint8_t* dir = NULL;
    size_t dir_len = 0;
    st = load_dir_at_cluster(ctx, fat, dir_cluster, &dir, &dir_len);
    free(fat);
    if (st != VFS_OK) {
        return st;
    }
    st = parse_dir_data(ctx, dir, dir_len, entries, cap, count);
    free(dir);
    return st;
}

vfs_status vfs_read_file(vfs_ctx* ctx, const char* path, void* buf, size_t cap, size_t* out_len) {
    if (ctx == NULL || path == NULL || buf == NULL) {
        return VFS_ERR_INTERNAL;
    }
    uint32_t fat_bytes =
        ((ctx->fat_bits == 32) ? ctx->sectors_per_fat32 : ctx->sectors_per_fat16) * ctx->bytes_per_sector;
    uint8_t* fat = malloc(fat_bytes);
    if (fat == NULL) {
        return VFS_ERR_INTERNAL;
    }
    vfs_status st = fat_read_bytes(ctx, fat, fat_bytes);
    if (st != VFS_OK) {
        free(fat);
        return st;
    }

    uint8_t entry[DIR_ENTRY_SIZE];
    st = resolve_path(ctx, fat, path, entry);
    if (st != VFS_OK) {
        free(fat);
        return st;
    }
    if (entry[11] & ATTR_DIRECTORY) {
        free(fat);
        return VFS_ERR_IS_DIR;
    }
    uint64_t file_size = u32le(entry + 28);
    uint32_t cluster = entry_first_cluster(ctx, entry);
    uint8_t* data = NULL;
    size_t data_len = 0;
    st = read_cluster_chain(ctx, fat, cluster, &data, &data_len);
    free(fat);
    if (st != VFS_OK) {
        return st;
    }
    size_t copy = file_size;
    if (copy > data_len) {
        copy = data_len;
    }
    if (copy > cap) {
        copy = cap;
    }
    memcpy(buf, data, copy);
    if (out_len) {
        *out_len = copy;
    }
    free(data);
    return VFS_OK;
}

static void to_83(const char* basename, uint8_t ent[DIR_ENTRY_SIZE]) {
    memset(ent, 0, DIR_ENTRY_SIZE);
    char base[9];
    char ext[4];
    strncpy(base, basename, 8);
    base[8] = '\0';
    ext[0] = '\0';
    const char* dot = strrchr(basename, '.');
    if (dot != NULL && dot[1] != '\0') {
        strncpy(ext, dot + 1, 3);
    }
    for (size_t i = 0; i < 8; i++) {
        char c = i < strlen(base) ? base[i] : ' ';
        if (c >= 'a' && c <= 'z') {
            c = (char)(c - 'a' + 'A');
        }
        ent[i] = (uint8_t)c;
    }
    for (size_t i = 0; i < 3; i++) {
        char c = i < strlen(ext) ? ext[i] : ' ';
        if (c >= 'a' && c <= 'z') {
            c = (char)(c - 'a' + 'A');
        }
        ent[8 + i] = (uint8_t)c;
    }
    ent[11] = ATTR_ARCHIVE;
}

vfs_status vfs_fat_put_root_file(vfs_ctx* ctx, const char* basename, const void* data, size_t len) {
    if (ctx == NULL || basename == NULL || (data == NULL && len > 0)) {
        return VFS_ERR_INTERNAL;
    }
    if (ctx->fat_bits == 32) {
        return VFS_ERR_UNSUPPORTED;
    }

    uint32_t fat_bytes = ctx->sectors_per_fat16 * ctx->bytes_per_sector;
    uint8_t* fat = calloc(1, fat_bytes);
    if (fat == NULL) {
        return VFS_ERR_INTERNAL;
    }
    vfs_status st = fat_read_bytes(ctx, fat, fat_bytes);
    if (st != VFS_OK) {
        free(fat);
        return st;
    }

    uint32_t root_sector =
        ctx->reserved_sectors + ctx->num_fats * ctx->sectors_per_fat16;
    uint32_t root_sectors =
        (ctx->root_entry_count * DIR_ENTRY_SIZE + ctx->bytes_per_sector - 1) / ctx->bytes_per_sector;

    int slot = -1;
    uint8_t sector[512];
    for (uint32_t s = 0; s < root_sectors && slot < 0; s++) {
        st = vfs_ctx_read_sector(ctx, root_sector + s, sector);
        if (st != VFS_OK) {
            free(fat);
            return st;
        }
        for (uint32_t i = 0; i < ctx->bytes_per_sector / DIR_ENTRY_SIZE; i++) {
            uint8_t tag = sector[i * DIR_ENTRY_SIZE];
            if (tag == SLOT_FREE || tag == SLOT_DELETED) {
                slot = (int)(s * (ctx->bytes_per_sector / DIR_ENTRY_SIZE) + i);
                break;
            }
        }
    }
    if (slot < 0) {
        free(fat);
        return VFS_ERR_RANGE;
    }

    uint32_t cluster = 2;
    uint32_t clusters_needed =
        (uint32_t)((len + (uint64_t)ctx->sectors_per_cluster * ctx->bytes_per_sector - 1) /
                   ((uint64_t)ctx->sectors_per_cluster * ctx->bytes_per_sector));
    if (clusters_needed == 0) {
        clusters_needed = 1;
    }
    if (cluster + clusters_needed > ctx->cluster_count) {
        free(fat);
        return VFS_ERR_RANGE;
    }

    for (uint32_t c = 0; c < clusters_needed; c++) {
        uint32_t cl = cluster + c;
        uint16_t eoc = (ctx->fat_bits == 12) ? 0x0FF8 : 0xFFF8;
        if (c + 1 < clusters_needed) {
            eoc = (uint16_t)(cl + 1);
        }
        if (ctx->fat_bits == 12) {
            uint32_t offset = cl + cl / 2;
            uint16_t entry = (uint16_t)fat[offset] | ((uint16_t)(fat[offset + 1] & 0xFF) << 8);
            if (cl & 1u) {
                entry = (uint16_t)((entry & 0x0F00) | (eoc & 0x0FFF));
            } else {
                entry = (uint16_t)((entry & 0xF000) | (eoc & 0x0FFF));
            }
            fat[offset] = (uint8_t)(entry & 0xFF);
            fat[offset + 1] = (uint8_t)((entry >> 8) & 0xFF);
        } else {
            fat[cl * 2] = (uint8_t)(eoc & 0xFF);
            fat[cl * 2 + 1] = (uint8_t)((eoc >> 8) & 0xFF);
        }
    }

    for (uint32_t f = 0; f < ctx->num_fats; f++) {
        uint32_t base = (ctx->fat_start_sector + f * ctx->sectors_per_fat16) * ctx->bytes_per_sector;
        for (uint32_t i = 0; i < ctx->sectors_per_fat16; i++) {
            st = vfs_ctx_write(ctx, base + (uint64_t)i * ctx->bytes_per_sector, fat + i * ctx->bytes_per_sector,
                              ctx->bytes_per_sector);
            if (st != VFS_OK) {
                free(fat);
                return st;
            }
        }
    }
    free(fat);

    size_t written = 0;
    for (uint32_t c = 0; c < clusters_needed && written < len; c++) {
        uint32_t cl = cluster + c;
        uint64_t off =
            ((uint64_t)(ctx->data_start_sector) + (uint64_t)(cl - 2) * ctx->sectors_per_cluster) *
            ctx->bytes_per_sector;
        size_t chunk = (size_t)ctx->sectors_per_cluster * ctx->bytes_per_sector;
        if (written + chunk > len) {
            memset(sector, 0, sizeof(sector));
            memcpy(sector, (const uint8_t*)data + written, len - written);
            st = vfs_ctx_write(ctx, off, sector, chunk);
            written = len;
        } else {
            st = vfs_ctx_write(ctx, off, (const uint8_t*)data + written, chunk);
            written += chunk;
        }
        if (st != VFS_OK) {
            return st;
        }
    }

    uint8_t ent[DIR_ENTRY_SIZE];
    to_83(basename, ent);
    ent[26] = (uint8_t)(cluster & 0xFF);
    ent[27] = (uint8_t)((cluster >> 8) & 0xFF);
    ent[28] = (uint8_t)(len & 0xFF);
    ent[29] = (uint8_t)((len >> 8) & 0xFF);
    ent[30] = (uint8_t)((len >> 16) & 0xFF);
    ent[31] = (uint8_t)((len >> 24) & 0xFF);

    uint32_t slot_sector = root_sector + (uint32_t)slot / (ctx->bytes_per_sector / DIR_ENTRY_SIZE);
    uint32_t slot_off = ((uint32_t)slot % (ctx->bytes_per_sector / DIR_ENTRY_SIZE)) * DIR_ENTRY_SIZE;
    st = vfs_ctx_read_sector(ctx, slot_sector, sector);
    if (st != VFS_OK) {
        return st;
    }
    memcpy(sector + slot_off, ent, DIR_ENTRY_SIZE);
    return vfs_ctx_write_sector(ctx, slot_sector, sector);
}