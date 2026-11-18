#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "Tcdefs.h"
#include "hcvault/vfs.h"
#include "hcvault/volume.h"

static char* slurp(const char* path, size_t* len) {
    FILE* f = fopen(path, "rb");
    if (f == NULL) {
        return NULL;
    }
    fseek(f, 0, SEEK_END);
    long n = ftell(f);
    rewind(f);
    char* buf = malloc((size_t)n + 1);
    if (buf == NULL) {
        fclose(f);
        return NULL;
    }
    size_t got = fread(buf, 1, (size_t)n, f);
    fclose(f);
    buf[got] = '\0';
    if (len) {
        *len = got;
    }
    return buf;
}

int main(int argc, char** argv) {
    const char* volume = NULL;
    const char* pwfile = NULL;
    const char* expect = NULL;
    const char* vpath = "HCVAULTF.PNG";
    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "--volume") == 0 && i + 1 < argc) {
            volume = argv[++i];
        } else if (strcmp(argv[i], "--password-file") == 0 && i + 1 < argc) {
            pwfile = argv[++i];
        } else if (strcmp(argv[i], "--expect-sha256") == 0 && i + 1 < argc) {
            expect = argv[++i];
        } else if (strcmp(argv[i], "--path") == 0 && i + 1 < argc) {
            vpath = argv[++i];
        }
    }
    if (volume == NULL || pwfile == NULL || expect == NULL) {
        fprintf(stderr, "Usage: %s --volume PATH --password-file PATH --expect-sha256 HEX [--path NAME]\n", argv[0]);
        return 2;
    }
    size_t pwlen;
    char* pw = slurp(pwfile, &pwlen);
    if (pw == NULL) {
        return 1;
    }
    int fd = open(volume, O_RDONLY);
    if (fd < 0) {
        perror(volume);
        burn(pw, pwlen);
        free(pw);
        return 1;
    }
    vc_open_params params = {};
    params.password = pw;
    params.password_len = pwlen;
    vc_volume* vol = NULL;
    vc_status st = vc_open(fd, &params, &vol);
    burn(pw, pwlen);
    free(pw);
    if (st != VC_OK) {
      fprintf(stderr, "vc_open: %d\n", (int)st);
      close(fd);
      return 1;
    }
    vfs_ctx* vfs = NULL;
    if (vfs_open(vol, &vfs) != VFS_OK) {
      vc_close(vol);
      close(fd);
      return 1;
    }
    uint8_t buf[1 << 20];
    size_t len = 0;
    if (vfs_read_file(vfs, vpath, buf, sizeof(buf), &len) != VFS_OK) {
      vfs_close(vfs);
      vc_close(vol);
      close(fd);
      return 1;
    }
    vfs_close(vfs);
    vc_close(vol);
    close(fd);

    char tmpl[] = "/tmp/hcvault-dig-XXXXXX";
    int tfd = mkstemp(tmpl);
    if (tfd < 0) {
      return 1;
    }
    if (write(tfd, buf, len) != (ssize_t)len) {
      close(tfd);
      unlink(tmpl);
      return 1;
    }
    close(tfd);
    char cmd[256];
    snprintf(cmd, sizeof(cmd), "sha256sum '%s'", tmpl);
    FILE* pp = popen(cmd, "r");
    char line[128];
    if (pp == NULL || fgets(line, sizeof(line), pp) == NULL) {
      return 1;
    }
    pclose(pp);
    unlink(tmpl);
    char got[65];
    sscanf(line, "%64s", got);
    if (strcasecmp(got, expect) != 0) {
      fprintf(stderr, "sha256 mismatch got=%s expect=%s\n", got, expect);
      return 1;
    }
    printf("vfs-digest: PASS sha256=%s\n", got);
    return 0;
}