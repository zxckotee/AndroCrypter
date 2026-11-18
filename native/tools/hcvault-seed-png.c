#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "Tcdefs.h"
#include "hcvault/vfs.h"
#include "hcvault/volume.h"

static char* slurp(const char* path, size_t* len) {
    int fd = open(path, O_RDONLY);
    if (fd < 0) {
        return NULL;
    }
    FILE* f = fdopen(fd, "rb");
    fseek(f, 0, SEEK_END);
    long n = ftell(f);
    rewind(f);
    char* buf = malloc((size_t)n);
    if (buf && fread(buf, 1, (size_t)n, f) != (size_t)n) {
        free(buf);
        buf = NULL;
    }
    fclose(f);
    if (len) {
        *len = buf ? (size_t)n : 0;
    }
    return buf;
}

int main(int argc, char** argv) {
    const char* volume = NULL;
    const char* png = NULL;
    const char* pwfile = NULL;
  for (int i = 1; i < argc; i++) {
    if (strcmp(argv[i], "--volume") == 0 && i + 1 < argc) {
      volume = argv[++i];
    } else if (strcmp(argv[i], "--png") == 0 && i + 1 < argc) {
      png = argv[++i];
    } else if (strcmp(argv[i], "--password-file") == 0 && i + 1 < argc) {
      pwfile = argv[++i];
    }
  }
    if (volume == NULL || png == NULL || pwfile == NULL) {
      fprintf(stderr, "Usage: %s --volume PATH --png PATH --password-file PATH\n", argv[0]);
      return 2;
    }
    size_t pwlen;
    char* pw = slurp(pwfile, &pwlen);
    size_t pnglen;
    char* data = slurp(png, &pnglen);
    if (pw == NULL || data == NULL) {
      fprintf(stderr, "read failed\n");
      free(pw);
      free(data);
      return 1;
    }
    int fd = open(volume, O_RDWR);
    if (fd < 0) {
      perror(volume);
      burn(pw, pwlen);
      free(pw);
      free(data);
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
      free(data);
      return 1;
    }
    vfs_ctx* vfs = NULL;
    if (vfs_open(vol, &vfs) != VFS_OK) {
      vc_close(vol);
      close(fd);
      free(data);
      return 1;
    }
    st = (vc_status)vfs_fat_put_root_file(vfs, "HCVAULTF.PNG", data, pnglen);
    vfs_close(vfs);
    vc_flush(vol);
    vc_close(vol);
    close(fd);
    free(data);
    if (st != VFS_OK) {
      fprintf(stderr, "vfs_fat_put_root_file: %d\n", (int)st);
      return 1;
    }
    return 0;
}