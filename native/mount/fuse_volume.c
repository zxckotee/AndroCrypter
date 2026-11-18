#define FUSE_USE_VERSION 31

#include "mount_internal.h"

#include "hcvault/volume.h"

#include <errno.h>
#include <limits.h>
#include <stdio.h>
#include <fuse3/fuse.h>
#include <pthread.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

struct fv_ctx {
    struct vc_volume* vol;
    uint64_t size;
    int readonly;
};

static void* fuse_thread_main(void* arg) {
    struct hm_fuse_volume* fv = arg;
    if (fv->fuse != NULL) {
        fuse_loop(fv->fuse);
    }
    fv->running = 0;
    return NULL;
}

static int fv_getattr(const char* path, struct stat* stbuf, struct fuse_file_info* fi) {
    (void)fi;
    struct fv_ctx* ctx = fuse_get_context()->private_data;
    memset(stbuf, 0, sizeof(*stbuf));
    if (strcmp(path, "/") == 0) {
        stbuf->st_mode = S_IFDIR | 0555;
        stbuf->st_nlink = 2;
        return 0;
    }
    if (strcmp(path, "/" HM_PLAIN_IMG) == 0) {
        stbuf->st_mode = S_IFREG | (ctx->readonly ? 0444 : 0644);
        stbuf->st_nlink = 1;
        stbuf->st_size = (off_t)ctx->size;
        stbuf->st_blocks = (blkcnt_t)((ctx->size + 511) / 512);
        stbuf->st_blksize = 4096;
        return 0;
    }
    return -ENOENT;
}

static int fv_readdir(const char* path, void* buf, fuse_fill_dir_t filler, off_t offset, struct fuse_file_info* fi,
                      enum fuse_readdir_flags flags) {
    (void)offset;
    (void)fi;
    (void)flags;
    if (strcmp(path, "/") != 0) {
        return -ENOENT;
    }
    filler(buf, ".", NULL, 0, 0);
    filler(buf, "..", NULL, 0, 0);
    filler(buf, HM_PLAIN_IMG, NULL, 0, 0);
    return 0;
}

static int fv_open(const char* path, struct fuse_file_info* fi) {
    if (strcmp(path, "/" HM_PLAIN_IMG) != 0) {
        return -ENOENT;
    }
    struct fv_ctx* ctx = fuse_get_context()->private_data;
    if (ctx->readonly && (fi->flags & O_ACCMODE) != O_RDONLY) {
        return -EROFS;
    }
    return 0;
}

static int fv_read(const char* path, char* buf, size_t size, off_t offset, struct fuse_file_info* fi) {
    (void)fi;
    if (strcmp(path, "/" HM_PLAIN_IMG) != 0) {
        return -ENOENT;
    }
    struct fv_ctx* ctx = fuse_get_context()->private_data;
    if ((uint64_t)offset >= ctx->size) {
        return 0;
    }
    if ((uint64_t)offset + size > ctx->size) {
        size = (size_t)(ctx->size - (uint64_t)offset);
    }
    vc_status st = vc_read(ctx->vol, (uint64_t)offset, buf, size);
    return st == VC_OK ? (int)size : -EIO;
}

static int fv_write(const char* path, const char* buf, size_t size, off_t offset, struct fuse_file_info* fi) {
    (void)fi;
    if (strcmp(path, "/" HM_PLAIN_IMG) != 0) {
        return -ENOENT;
    }
    struct fv_ctx* ctx = fuse_get_context()->private_data;
    if (ctx->readonly) {
        return -EROFS;
    }
    if ((uint64_t)offset + size > ctx->size) {
        return -EFBIG;
    }
    vc_status st = vc_write(ctx->vol, (uint64_t)offset, buf, size);
    return st == VC_OK ? (int)size : -EIO;
}

static struct fuse_operations fv_ops = {
    .getattr = fv_getattr,
    .readdir = fv_readdir,
    .open = fv_open,
    .read = fv_read,
    .write = fv_write,
};

hm_status hm_fuse_volume_start(struct hm_fuse_volume* fv, const char* mountpoint) {
    if (fv == NULL || mountpoint == NULL || fv->vol == NULL) {
        return HM_ERR_INVAL;
    }
    fv->size = vc_data_size(fv->vol);
    fv->readonly = vc_is_readonly(fv->vol);

    struct fv_ctx ctx = {.vol = fv->vol, .size = fv->size, .readonly = fv->readonly};

    struct fuse_args args = FUSE_ARGS_INIT(0, NULL);
    fuse_opt_add_arg(&args, "hcvault-mountd");
    struct fuse* fuse = fuse_new(&args, &fv_ops, sizeof(fv_ops), &ctx);
    fuse_opt_free_args(&args);
    if (fuse == NULL) {
        return HM_ERR_FUSE;
    }
    if (fuse_mount(fuse, mountpoint) != 0) {
        fuse_destroy(fuse);
        return HM_ERR_FUSE;
    }

    fv->fuse = fuse;
    fv->mountpoint = strdup(mountpoint);
    fv->running = 1;
    if (pthread_create(&fv->thread, NULL, fuse_thread_main, fv) != 0) {
        fuse_unmount(fuse);
        fuse_destroy(fuse);
        free(fv->mountpoint);
        fv->mountpoint = NULL;
        fv->fuse = NULL;
        return HM_ERR_FUSE;
    }
  /* give the session a moment to accept lookups */
    for (int i = 0; i < 50; ++i) {
        char probe[PATH_MAX];
        snprintf(probe, sizeof(probe), "%s/%s", mountpoint, HM_PLAIN_IMG);
        if (access(probe, F_OK) == 0) {
            return HM_OK;
        }
        usleep(20000);
    }
    return HM_OK;
}

void hm_fuse_volume_stop(struct hm_fuse_volume* fv) {
    if (fv == NULL) {
        return;
    }
    if (fv->fuse != NULL) {
        fuse_exit(fv->fuse);
    }
    if (fv->running) {
        pthread_join(fv->thread, NULL);
    }
    if (fv->fuse != NULL) {
        fuse_unmount(fv->fuse);
        fuse_destroy(fv->fuse);
        fv->fuse = NULL;
    }
    free(fv->mountpoint);
    fv->mountpoint = NULL;
    vc_flush(fv->vol);
}