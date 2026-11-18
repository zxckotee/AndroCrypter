#include "hcvault/mount.h"

#include <errno.h>
#include <sys/mount.h>
#include <sys/stat.h>
#include <unistd.h>

#ifndef MS_RELATIME
#define MS_RELATIME (1 << 21)
#endif

hm_status hm_mount_fs(const char* dev_path, const char* mountpoint, const char* fstype, unsigned long extra_flags) {
    if (dev_path == NULL || mountpoint == NULL || fstype == NULL) {
        return HM_ERR_INVAL;
    }
    if (!hm_kernel_has_fs(fstype)) {
        return HM_ERR_FS;
    }
    struct stat st;
    if (stat(mountpoint, &st) != 0) {
        if (errno != ENOENT) {
            return HM_ERR_IO;
        }
        if (mkdir(mountpoint, 0755) != 0) {
            return HM_ERR_IO;
        }
    }
    unsigned long flags = MS_NOATIME | MS_NODEV | extra_flags;
    if (mount(dev_path, mountpoint, fstype, flags, NULL) != 0) {
        return HM_ERR_MOUNT;
    }
    return HM_OK;
}

hm_status hm_umount_fs(const char* mountpoint, int lazy) {
    if (mountpoint == NULL) {
        return HM_ERR_INVAL;
    }
    int flags = lazy ? MNT_DETACH : 0;
    if (umount2(mountpoint, flags) != 0) {
        return HM_ERR_MOUNT;
    }
    return HM_OK;
}