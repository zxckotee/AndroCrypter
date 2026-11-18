#pragma once

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef enum hm_status {
    HM_OK = 0,
    HM_ERR_INVAL = 1,
    HM_ERR_IO = 2,
    HM_ERR_NOMEM = 3,
    HM_ERR_FUSE = 4,
    HM_ERR_LOOP = 5,
    HM_ERR_FS = 6,
    HM_ERR_MOUNT = 7,
    HM_ERR_BUSY = 8,
    HM_ERR_PERM = 9,
    HM_ERR_UNSUPPORTED = 10,
    HM_ERR_INTERNAL = 11,
} hm_status;

typedef enum hm_fs_type {
    HM_FS_UNKNOWN = 0,
    HM_FS_EXT2,
    HM_FS_EXT3,
    HM_FS_EXT4,
    HM_FS_VFAT,
    HM_FS_EXFAT,
    HM_FS_NTFS,
} hm_fs_type;

struct vc_volume;

/* Backing image path (e.g. FUSE plain.img) -> /dev/loopN in loop_dev_out. */
hm_status hm_loop_attach(const char* backing_path, int readonly, char* loop_dev_out, size_t loop_dev_len);

hm_status hm_loop_detach(const char* loop_dev);

hm_status hm_fsprobe_path(const char* path, hm_fs_type* fs_out, char* kernel_name_out, size_t kernel_name_len);

int hm_kernel_has_fs(const char* fstype);

hm_status hm_mount_fs(const char* dev_path, const char* mountpoint, const char* fstype, unsigned long extra_flags);

hm_status hm_umount_fs(const char* mountpoint, int lazy);

const char* hm_status_str(hm_status st);

const char* hm_fs_type_name(hm_fs_type t);

#ifdef __cplusplus
}
#endif