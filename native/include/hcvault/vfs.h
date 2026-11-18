#pragma once

#include <stddef.h>
#include <stdint.h>

#include "hcvault/volume.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef enum vfs_status {
    VFS_OK = 0,
    VFS_ERR_UNSUPPORTED = 1,
    VFS_ERR_IO = 2,
    VFS_ERR_NOT_FOUND = 3,
    VFS_ERR_IS_DIR = 4,
    VFS_ERR_RANGE = 5,
    VFS_ERR_INTERNAL = 6,
} vfs_status;

/** Max DOS 8.3 name we expose: "NAME.EXT" + NUL */
enum { VFS_NAME_MAX = 13 };

typedef struct vfs_dirent {
    char name[VFS_NAME_MAX];
    int is_dir;
    uint64_t size;
} vfs_dirent;

typedef struct vfs_ctx vfs_ctx;

/** Bind a read-only FAT view on a opened VeraCrypt volume. */
vfs_status vfs_open(vc_volume* vol, vfs_ctx** out);
void vfs_close(vfs_ctx* ctx);

/** List directory at POSIX-ish path ("" or "/" for root). */
vfs_status vfs_list_dir(vfs_ctx* ctx, const char* path, vfs_dirent* entries, size_t cap, size_t* count);

/** Read entire file at path into buf (truncates if cap < file size). */
vfs_status vfs_read_file(vfs_ctx* ctx, const char* path, void* buf, size_t cap, size_t* out_len);

#ifdef __cplusplus
}
#endif