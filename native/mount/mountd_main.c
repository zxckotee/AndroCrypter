#include "mount_internal.h"

#include "hcvault/volume.h"

#include <ctype.h>
#include <errno.h>
#include <signal.h>
#include <fcntl.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <sys/mount.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <time.h>
#include <unistd.h>

static volatile sig_atomic_t g_mountd_stop;

static void on_stop_signal(int s) {
    (void)s;
    g_mountd_stop = 1;
}

static void usage(FILE* out) {
    fprintf(
        out,
        "Usage:\n"
        "  hcvault-mountd mount --volume PATH --target PATH [--password-fd N] [--fstype TYPE]\n"
        "  hcvault-mountd umount --target PATH\n"
        "  hcvault-mountd cleanup\n"
        "  hcvault-mountd serve --socket PATH [--peer-uid UID]\n"
        "\n"
        "Password is read from --password-fd (default 0). Never pass via argv or environment.\n");
}

static int read_password_fd(int pfd, char** out, size_t* out_len) {
    if (pfd < 0) {
        return -1;
    }
    size_t cap = 256;
    size_t len = 0;
    char* buf = malloc(cap);
    if (buf == NULL) {
        return -1;
    }
    for (;;) {
        ssize_t n = read(pfd, buf + len, cap - len);
        if (n < 0) {
            if (errno == EINTR) {
                continue;
            }
            free(buf);
            return -1;
        }
        if (n == 0) {
            break;
        }
        len += (size_t)n;
        if (len >= cap - 1) {
            cap *= 2;
            if (cap > 4096) {
                free(buf);
                return -1;
            }
            char* nb = realloc(buf, cap);
            if (nb == NULL) {
                free(buf);
                return -1;
            }
            buf = nb;
        }
    }
    while (len > 0 && (buf[len - 1] == '\n' || buf[len - 1] == '\r')) {
        len--;
    }
    buf[len] = '\0';
    *out = buf;
    *out_len = len;
    return 0;
}

static void wipe_password(char* pass, size_t len) {
    if (pass == NULL) {
        return;
    }
    explicit_bzero(pass, len);
    free(pass);
}

static const char* fstype_for(hm_fs_type t) {
    switch (t) {
    case HM_FS_EXT4:
        return "ext4";
    case HM_FS_EXT3:
        return "ext3";
    case HM_FS_EXT2:
        return "ext2";
    case HM_FS_VFAT:
        return "vfat";
    case HM_FS_EXFAT:
        return "exfat";
    case HM_FS_NTFS:
        return "ntfs";
    default:
        return NULL;
    }
}

static int cmd_mount(int argc, char** argv) {
    const char* volume = NULL;
    const char* target = NULL;
  const char* fstype_want = "ext4";
    int password_fd = 0;

    for (int i = 2; i < argc; ++i) {
        if (strcmp(argv[i], "--volume") == 0 && i + 1 < argc) {
            volume = argv[++i];
        } else if (strcmp(argv[i], "--target") == 0 && i + 1 < argc) {
            target = argv[++i];
        } else if (strcmp(argv[i], "--password-fd") == 0 && i + 1 < argc) {
            password_fd = atoi(argv[++i]);
        } else if (strcmp(argv[i], "--fstype") == 0 && i + 1 < argc) {
            fstype_want = argv[++i];
        } else {
            fprintf(stderr, "unknown option: %s\n", argv[i]);
            return 2;
        }
    }
    if (volume == NULL || target == NULL) {
        usage(stderr);
        return 2;
    }

    char* password = NULL;
    size_t password_len = 0;
    if (read_password_fd(password_fd, &password, &password_len) != 0) {
        fprintf(stderr, "failed to read password from fd %d\n", password_fd);
        return 1;
    }

    int vfd = open(volume, O_RDWR | O_CLOEXEC);
    if (vfd < 0) {
        vfd = open(volume, O_RDONLY | O_CLOEXEC);
    }
    if (vfd < 0) {
        perror("open volume");
        wipe_password(password, password_len);
        return 1;
    }

    vc_open_params params = {
        .password = password,
        .password_len = password_len,
        .pim = 0,
        .hidden = VC_HEADER_AUTO,
    };
    vc_volume* vol = NULL;
    vc_status vst = vc_open(vfd, &params, &vol);
    wipe_password(password, password_len);
    if (vst != VC_OK) {
        fprintf(stderr, "vc_open failed: %d\n", (int)vst);
        close(vfd);
        return 1;
    }

    char fuse_tpl[] = "/tmp/hcvault-fuse-XXXXXX";
    char* fuse_mount = mkdtemp(fuse_tpl);
    if (fuse_mount == NULL) {
        perror("mkdtemp");
        vc_close(vol);
        close(vfd);
        return 1;
    }

    struct hm_fuse_volume fv = {.vol = vol, .fuse = NULL, .mountpoint = NULL};
    hm_status st = hm_fuse_volume_start(&fv, fuse_mount);
    if (st != HM_OK) {
        fprintf(stderr, "FUSE start failed: %s\n", hm_status_str(st));
        rmdir(fuse_mount);
        vc_close(vol);
        close(vfd);
        return 1;
    }

    char plain_path[PATH_MAX];
    snprintf(plain_path, sizeof(plain_path), "%s/%s", fuse_mount, HM_PLAIN_IMG);

    char loop_dev[64];
    st = hm_loop_attach(plain_path, vc_is_readonly(vol), loop_dev, sizeof(loop_dev));
    if (st != HM_OK) {
        fprintf(stderr, "loop attach failed: %s\n", hm_status_str(st));
        hm_fuse_volume_stop(&fv);
        rmdir(fuse_mount);
        vc_close(vol);
        close(vfd);
        return 1;
    }

    hm_fs_type detected = HM_FS_UNKNOWN;
    char kernel_name[32];
    st = hm_fsprobe_path(loop_dev, &detected, kernel_name, sizeof(kernel_name));
    if (st != HM_OK) {
        fprintf(stderr, "fsprobe failed: %s\n", hm_status_str(st));
        hm_loop_detach(loop_dev);
        hm_fuse_volume_stop(&fv);
        rmdir(fuse_mount);
        vc_close(vol);
        close(vfd);
        return 1;
    }

    const char* mount_fstype = fstype_for(detected);
    if (mount_fstype == NULL) {
        fprintf(stderr, "unsupported inner filesystem (magic probe)\n");
        hm_loop_detach(loop_dev);
        hm_fuse_volume_stop(&fv);
        rmdir(fuse_mount);
        vc_close(vol);
        close(vfd);
        return 1;
    }
    if (strcmp(mount_fstype, fstype_want) != 0 && detected != HM_FS_UNKNOWN) {
        fprintf(
            stderr, "inner fs is %s but --fstype %s was requested\n", mount_fstype, fstype_want);
    }
    if (!hm_kernel_has_fs(mount_fstype)) {
        fprintf(stderr, "kernel lacks driver for %s (see /proc/filesystems)\n", mount_fstype);
        hm_loop_detach(loop_dev);
        hm_fuse_volume_stop(&fv);
        rmdir(fuse_mount);
        vc_close(vol);
        close(vfd);
        return 1;
    }

    unsigned long extra = vc_is_readonly(vol) ? MS_RDONLY : 0;
    st = hm_mount_fs(loop_dev, target, mount_fstype, extra);
    if (st != HM_OK) {
        fprintf(stderr, "mount failed: %s\n", hm_status_str(st));
        hm_loop_detach(loop_dev);
        hm_fuse_volume_stop(&fv);
        rmdir(fuse_mount);
        vc_close(vol);
        close(vfd);
        return 1;
    }

    char id[64];
    snprintf(id, sizeof(id), "m-%ld-%d", (long)time(NULL), getpid());
    hm_registry_add(id, volume, target, fuse_mount, loop_dev);

    printf(
        "mounted %s at %s (loop %s, fuse %s, fstype %s)\n", volume, target, loop_dev, fuse_mount,
        mount_fstype);
    printf("id=%s\n", id);

    pid_t child = fork();
    if (child < 0) {
        perror("fork");
        hm_umount_fs(target, 0);
        hm_loop_detach(loop_dev);
        hm_fuse_volume_stop(&fv);
        vc_close(vol);
        close(vfd);
        return 1;
    }
    if (child > 0) {
        return 0;
    }

    if (setsid() < 0) {
        _exit(1);
    }

    g_mountd_stop = 0;
    signal(SIGTERM, on_stop_signal);
    signal(SIGINT, on_stop_signal);

    while (!g_mountd_stop) {
        pause();
    }

    hm_umount_fs(target, 0);
    hm_loop_detach(loop_dev);
    hm_fuse_volume_stop(&fv);
    vc_close(vol);
    close(vfd);
    rmdir(fuse_mount);
    _exit(0);
}

static int cmd_umount(int argc, char** argv) {
    const char* target = NULL;
    for (int i = 2; i < argc; ++i) {
        if (strcmp(argv[i], "--target") == 0 && i + 1 < argc) {
            target = argv[++i];
        }
    }
    if (target == NULL) {
        usage(stderr);
        return 2;
    }
    hm_status st = hm_umount_fs(target, 0);
    if (st != HM_OK) {
        fprintf(stderr, "umount failed: %s\n", hm_status_str(st));
        return 1;
    }
    printf("unmounted %s\n", target);
    return 0;
}

static int cmd_cleanup(void) {
    hm_status st = hm_registry_cleanup_all();
    if (st != HM_OK) {
        fprintf(stderr, "cleanup failed: %s\n", hm_status_str(st));
        return 1;
    }
    printf("registry cleanup done\n");
    return 0;
}

static int cmd_serve(int argc, char** argv) {
    const char* socket_path = NULL;
    uid_t peer_uid = (uid_t)-1;
    for (int i = 2; i < argc; ++i) {
        if (strcmp(argv[i], "--socket") == 0 && i + 1 < argc) {
            socket_path = argv[++i];
        } else if (strcmp(argv[i], "--peer-uid") == 0 && i + 1 < argc) {
            peer_uid = (uid_t)strtoul(argv[++i], NULL, 10);
        }
    }
    if (socket_path == NULL) {
        usage(stderr);
        return 2;
    }
    return hm_sockctl_serve(socket_path, peer_uid) == HM_OK ? 0 : 1;
}

int main(int argc, char** argv) {
    if (argc < 2) {
        usage(stderr);
        return 2;
    }
    if (strcmp(argv[1], "mount") == 0) {
        return cmd_mount(argc, argv);
    }
    if (strcmp(argv[1], "umount") == 0) {
        return cmd_umount(argc, argv);
    }
    if (strcmp(argv[1], "cleanup") == 0) {
        return cmd_cleanup();
    }
    if (strcmp(argv[1], "serve") == 0) {
        return cmd_serve(argc, argv);
    }
    usage(stderr);
    return 2;
}