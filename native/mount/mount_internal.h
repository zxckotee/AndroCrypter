#pragma once

#include "hcvault/mount.h"

#include <pthread.h>
#include <sys/types.h>

struct vc_volume;
struct fuse;

#define HM_PLAIN_IMG "plain.img"
#define HM_SOCK_MAGIC 0x48434d54u /* 'HCMT' */

struct hm_fuse_volume {
    struct vc_volume* vol;
    uint64_t size;
    int readonly;
    pthread_t thread;
    struct fuse* fuse;
    char* mountpoint;
    volatile int running;
};

hm_status hm_fuse_volume_start(struct hm_fuse_volume* fv, const char* mountpoint);
void hm_fuse_volume_stop(struct hm_fuse_volume* fv);

hm_status hm_registry_path(char* out, size_t out_len);
hm_status hm_registry_add(
    const char* id, const char* volume_path, const char* mountpoint, const char* fuse_mount,
    const char* loop_dev);
hm_status hm_registry_remove(const char* id);
hm_status hm_registry_cleanup_all(void);

hm_status hm_sockctl_serve(const char* socket_path, uid_t allowed_uid);