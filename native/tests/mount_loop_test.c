#include "hcvault/mount.h"

#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mount.h>
#include <unistd.h>

static int fail(const char* msg) {
    fprintf(stderr, "mount_loop_test: %s\n", msg);
    return 1;
}

int main(int argc, char** argv) {
    if (argc != 3) {
        fprintf(stderr, "usage: %s <ext4.img> <mountpoint>\n", argv[0]);
        return 2;
    }
    const char* img = argv[1];
    const char* mnt = argv[2];

    if (!hm_kernel_has_fs("ext4")) {
        fprintf(stderr, "mount_loop_test: SKIP no kernel ext4\n");
        return 0;
    }

    char loop[64];
    hm_status st = hm_loop_attach(img, 0, loop, sizeof(loop));
    if (st != HM_OK) {
        return fail("loop attach");
    }

    hm_fs_type fs = HM_FS_UNKNOWN;
    char kname[32];
    st = hm_fsprobe_path(loop, &fs, kname, sizeof(kname));
    if (st != HM_OK || fs != HM_FS_EXT4) {
        hm_loop_detach(loop);
        return fail("fsprobe ext4");
    }

    st = hm_mount_fs(loop, mnt, "ext4", MS_RDONLY);
    if (st != HM_OK) {
        hm_loop_detach(loop);
        return fail("mount ext4");
    }

    char marker[PATH_MAX];
    snprintf(marker, sizeof(marker), "%s/lost+found", mnt);
    if (access(marker, F_OK) != 0) {
        hm_umount_fs(mnt, 0);
        hm_loop_detach(loop);
        return fail("lost+found missing");
    }

    hm_umount_fs(mnt, 0);
    hm_loop_detach(loop);
    printf("mount_loop_test ok\n");
    return 0;
}