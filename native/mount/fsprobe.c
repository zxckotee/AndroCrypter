#include "hcvault/mount.h"

#include <ctype.h>
#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>

static const char* proc_filesystems_path(void) {
    const char* env = getenv("HCVAULT_PROC_FILESYSTEMS");
    return (env != NULL && env[0] != '\0') ? env : "/proc/filesystems";
}

int hm_kernel_has_fs(const char* fstype) {
    if (fstype == NULL || fstype[0] == '\0') {
        return 0;
    }
    FILE* f = fopen(proc_filesystems_path(), "r");
    if (f == NULL) {
        return 0;
    }
    char line[128];
    int found = 0;
    while (fgets(line, sizeof(line), f) != NULL) {
        char* p = line;
        while (*p != '\0' && isspace((unsigned char)*p)) {
            p++;
        }
        if (*p == '\0') {
            continue;
        }
        /* skip "nodev" prefix if present */
        if (strncmp(p, "nodev", 5) == 0) {
            p += 5;
            while (*p != '\0' && isspace((unsigned char)*p)) {
                p++;
            }
        }
        size_t n = strlen(p);
        while (n > 0 && isspace((unsigned char)p[n - 1])) {
            p[--n] = '\0';
        }
        if (strcmp(p, fstype) == 0) {
            found = 1;
            break;
        }
    }
    fclose(f);
    return found;
}

const char* hm_fs_type_name(hm_fs_type t) {
    switch (t) {
    case HM_FS_EXT2:
        return "ext2";
    case HM_FS_EXT3:
        return "ext3";
    case HM_FS_EXT4:
        return "ext4";
    case HM_FS_VFAT:
        return "vfat";
    case HM_FS_EXFAT:
        return "exfat";
    case HM_FS_NTFS:
        return "ntfs";
    default:
        return "unknown";
    }
}

static hm_fs_type probe_magic(const uint8_t* buf, size_t len) {
    if (len >= 512) {
        if (buf[510] == 0x55 && buf[511] == 0xAA) {
            if (len >= 90 && memcmp(buf + 82, "FAT32   ", 8) == 0) {
                return HM_FS_VFAT;
            }
            if (len >= 90 && memcmp(buf + 54, "FAT12   ", 8) == 0) {
                return HM_FS_VFAT;
            }
            if (len >= 90 && memcmp(buf + 54, "FAT16   ", 8) == 0) {
                return HM_FS_VFAT;
            }
        }
    }
    if (len >= 1082) {
        uint16_t magic = (uint16_t)buf[1080] | ((uint16_t)buf[1081] << 8);
        if (magic == 0xEF53) {
            uint32_t feat = (uint32_t)buf[1124] | ((uint32_t)buf[1125] << 8) | ((uint32_t)buf[1126] << 16) |
                            ((uint32_t)buf[1127] << 24);
            if (feat & (1u << 7)) {
                return HM_FS_EXT3;
            }
            if (feat & (1u << 5)) {
                return HM_FS_EXT4;
            }
            return HM_FS_EXT2;
        }
    }
    if (len >= 11 && memcmp(buf, "EXFAT   ", 8) == 0) {
        return HM_FS_EXFAT;
    }
    if (len >= 8 && memcmp(buf + 3, "NTFS    ", 8) == 0) {
        return HM_FS_NTFS;
    }
    return HM_FS_UNKNOWN;
}

static void kernel_name_for(hm_fs_type t, char* out, size_t out_len) {
    const char* name = hm_fs_type_name(t);
    if (t == HM_FS_VFAT) {
        name = "vfat";
    }
    snprintf(out, out_len, "%s", name);
}

hm_status hm_fsprobe_path(const char* path, hm_fs_type* fs_out, char* kernel_name_out, size_t kernel_name_len) {
    if (path == NULL || fs_out == NULL) {
        return HM_ERR_INVAL;
    }
    int fd = open(path, O_RDONLY | O_CLOEXEC);
    if (fd < 0) {
        return HM_ERR_IO;
    }
    uint8_t buf[2048];
    ssize_t got = pread(fd, buf, sizeof(buf), 0);
    close(fd);
    if (got < 512) {
        return HM_ERR_IO;
    }

    hm_fs_type t = probe_magic(buf, (size_t)got);
    *fs_out = t;
    if (kernel_name_out != NULL && kernel_name_len > 0) {
        if (t == HM_FS_UNKNOWN) {
            kernel_name_out[0] = '\0';
        } else {
            kernel_name_for(t, kernel_name_out, kernel_name_len);
        }
    }
    return HM_OK;
}