#include "mount_internal.h"

#include <errno.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <time.h>
#include <unistd.h>

static int ensure_parent_dir(const char* path) {
    char buf[PATH_MAX];
    snprintf(buf, sizeof(buf), "%s", path);
    char* slash = strrchr(buf, '/');
    if (slash == NULL) {
        return 0;
    }
    *slash = '\0';
    if (buf[0] == '\0') {
        return 0;
    }
    struct stat st;
    if (stat(buf, &st) == 0) {
        return S_ISDIR(st.st_mode) ? 0 : -1;
    }
    if (ensure_parent_dir(buf) != 0) {
        return -1;
    }
    return mkdir(buf, 0700);
}

hm_status hm_registry_path(char* out, size_t out_len) {
    const char* env = getenv("HCVAULT_MOUNT_REGISTRY");
    if (env != NULL && env[0] != '\0') {
        snprintf(out, out_len, "%s", env);
        return HM_OK;
    }
    const char* runtime = getenv("XDG_RUNTIME_DIR");
    if (runtime != NULL && runtime[0] != '\0') {
        snprintf(out, out_len, "%s/hcvault-mountd-registry.jsonl", runtime);
        return HM_OK;
    }
    const char* home = getenv("HOME");
    if (home == NULL) {
        home = "/tmp";
    }
    snprintf(out, out_len, "%s/.cache/hcvault/mount-registry.jsonl", home);
    return HM_OK;
}

static int json_escape(const char* s, char* out, size_t out_len) {
    if (strchr(s, '"') != NULL || strchr(s, '\\') != NULL) {
        return -1;
    }
    snprintf(out, out_len, "%s", s);
    return 0;
}

hm_status hm_registry_add(
    const char* id, const char* volume_path, const char* mountpoint, const char* fuse_mount,
    const char* loop_dev) {
    if (id == NULL || volume_path == NULL || mountpoint == NULL || fuse_mount == NULL || loop_dev == NULL) {
        return HM_ERR_INVAL;
    }
    char path[PATH_MAX];
    if (hm_registry_path(path, sizeof(path)) != HM_OK) {
        return HM_ERR_INTERNAL;
    }
    if (ensure_parent_dir(path) != 0) {
        return HM_ERR_IO;
    }

    char eid[512], evol[PATH_MAX], emnt[PATH_MAX], efuse[PATH_MAX], eloop[128];
    if (json_escape(id, eid, sizeof(eid)) != 0 || json_escape(volume_path, evol, sizeof(evol)) != 0 ||
        json_escape(mountpoint, emnt, sizeof(emnt)) != 0 || json_escape(fuse_mount, efuse, sizeof(efuse)) != 0 ||
        json_escape(loop_dev, eloop, sizeof(eloop)) != 0) {
        return HM_ERR_INVAL;
    }

    FILE* f = fopen(path, "a");
    if (f == NULL) {
        return HM_ERR_IO;
    }
    fprintf(
        f,
        "{\"id\":\"%s\",\"volume\":\"%s\",\"mountpoint\":\"%s\",\"fuse_mount\":\"%s\",\"loop_dev\":\"%s\",\"ts\":%ld}\n",
        eid, evol, emnt, efuse, eloop, (long)time(NULL));
    fclose(f);
    return HM_OK;
}

static hm_status teardown_entry(const char* mountpoint, const char* fuse_mount, const char* loop_dev) {
    if (mountpoint != NULL && mountpoint[0] != '\0') {
        hm_umount_fs(mountpoint, 1);
    }
    if (loop_dev != NULL && loop_dev[0] != '\0') {
        hm_loop_detach(loop_dev);
    }
    if (fuse_mount != NULL && fuse_mount[0] != '\0') {
        char cmd[PATH_MAX + 40];
        snprintf(cmd, sizeof(cmd), "fusermount3 -u \"%s\" 2>/dev/null", fuse_mount);
        (void)system(cmd);
    }
    return HM_OK;
}

hm_status hm_registry_remove(const char* id) {
    (void)id;
    return HM_ERR_UNSUPPORTED;
}

hm_status hm_registry_cleanup_all(void) {
    char path[PATH_MAX];
    if (hm_registry_path(path, sizeof(path)) != HM_OK) {
        return HM_ERR_INTERNAL;
    }
    FILE* f = fopen(path, "r");
    if (f == NULL) {
        return HM_OK;
    }

    char line[4096];
    while (fgets(line, sizeof(line), f) != NULL) {
        char mountpoint[PATH_MAX], fuse_mount[PATH_MAX], loop_dev[128];
        mountpoint[0] = fuse_mount[0] = loop_dev[0] = '\0';
        const char* p = strstr(line, "\"mountpoint\"");
        if (p != NULL && (p = strchr(p, ':')) != NULL) {
            sscanf(p, ": \"%[^\"]\"", mountpoint);
        }
        p = strstr(line, "\"fuse_mount\"");
        if (p != NULL && (p = strchr(p, ':')) != NULL) {
            sscanf(p, ": \"%[^\"]\"", fuse_mount);
        }
        p = strstr(line, "\"loop_dev\"");
        if (p != NULL && (p = strchr(p, ':')) != NULL) {
            sscanf(p, ": \"%[^\"]\"", loop_dev);
        }
        if (mountpoint[0] != '\0') {
            teardown_entry(mountpoint, fuse_mount, loop_dev);
        }
    }
    fclose(f);
    unlink(path);
    return HM_OK;
}