#include "hcvault/mount.h"

#include <errno.h>
#include <fcntl.h>
#include <linux/loop.h>
#include <stdio.h>
#include <string.h>
#include <sys/ioctl.h>
#include <unistd.h>

const char* hm_status_str(hm_status st) {
    switch (st) {
    case HM_OK:
        return "ok";
    case HM_ERR_INVAL:
        return "invalid";
    case HM_ERR_IO:
        return "io";
    case HM_ERR_NOMEM:
        return "nomem";
    case HM_ERR_FUSE:
        return "fuse";
    case HM_ERR_LOOP:
        return "loop";
    case HM_ERR_FS:
        return "fs";
    case HM_ERR_MOUNT:
        return "mount";
    case HM_ERR_BUSY:
        return "busy";
    case HM_ERR_PERM:
        return "perm";
    case HM_ERR_UNSUPPORTED:
        return "unsupported";
    case HM_ERR_INTERNAL:
        return "internal";
    default:
        return "unknown";
    }
}

static hm_status loop_configure(int loop_fd, int back_fd, const char* backing_path, int readonly) {
    struct loop_config config;
    memset(&config, 0, sizeof(config));
    config.fd = (uint32_t)back_fd;
    config.block_size = 512;
    config.info.lo_device = 0;
    if (readonly) {
        config.info.lo_flags |= LO_FLAGS_READ_ONLY;
    }
    strncpy((char*)config.info.lo_file_name, backing_path, LO_NAME_SIZE - 1);
    if (ioctl(loop_fd, LOOP_CONFIGURE, &config) == 0) {
        return HM_OK;
    }
    if (errno != EINVAL && errno != ENOTTY) {
        return HM_ERR_LOOP;
    }

    if (ioctl(loop_fd, LOOP_CLR_FD) < 0 && errno != ENXIO) {
        return HM_ERR_LOOP;
    }
    if (ioctl(loop_fd, LOOP_SET_FD, back_fd) < 0) {
        return HM_ERR_LOOP;
    }

    struct loop_info64 info;
    memset(&info, 0, sizeof(info));
    if (readonly) {
        info.lo_flags |= LO_FLAGS_READ_ONLY;
    }
    strncpy((char*)info.lo_file_name, backing_path, LO_NAME_SIZE - 1);
    if (ioctl(loop_fd, LOOP_SET_STATUS64, &info) < 0) {
        ioctl(loop_fd, LOOP_CLR_FD);
        return HM_ERR_LOOP;
    }
    return HM_OK;
}

hm_status hm_loop_attach(const char* backing_path, int readonly, char* loop_dev_out, size_t loop_dev_len) {
    if (backing_path == NULL || loop_dev_out == NULL || loop_dev_len < 8) {
        return HM_ERR_INVAL;
    }

    int ctl = open("/dev/loop-control", O_RDWR | O_CLOEXEC);
    if (ctl < 0) {
        return HM_ERR_LOOP;
    }

    int devnr = ioctl(ctl, LOOP_CTL_GET_FREE);
    close(ctl);
    if (devnr < 0) {
        return HM_ERR_LOOP;
    }

    if (snprintf(loop_dev_out, loop_dev_len, "/dev/loop%d", devnr) >= (int)loop_dev_len) {
        return HM_ERR_INVAL;
    }

    int loop_fd = open(loop_dev_out, O_RDWR | O_CLOEXEC);
    if (loop_fd < 0) {
        return HM_ERR_LOOP;
    }

    int flags = readonly ? O_RDONLY : O_RDWR;
    int back_fd = open(backing_path, flags | O_CLOEXEC);
    if (back_fd < 0) {
        close(loop_fd);
        return HM_ERR_IO;
    }

    hm_status st = loop_configure(loop_fd, back_fd, backing_path, readonly);
    close(back_fd);
    close(loop_fd);
    return st;
}

hm_status hm_loop_detach(const char* loop_dev) {
    if (loop_dev == NULL) {
        return HM_ERR_INVAL;
    }
    int loop_fd = open(loop_dev, O_RDWR | O_CLOEXEC);
    if (loop_fd < 0) {
        return HM_ERR_LOOP;
    }
    int rc = ioctl(loop_fd, LOOP_CLR_FD);
    close(loop_fd);
    return rc == 0 ? HM_OK : HM_ERR_LOOP;
}