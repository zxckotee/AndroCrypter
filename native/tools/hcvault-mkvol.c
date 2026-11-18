#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "Tcdefs.h"
#include "hcvault/volume.h"

static void usage(const char* argv0) {
    fprintf(stderr, "Usage: %s --size BYTES --password-file PATH [-o PATH] [--pim N] [--prf auto|sha512|sha256|whirlpool|blake2s|streebog] [--hidden SIZE]\n", argv0);
}

static int parse_prf(const char* s) {
    if (strcmp(s, "auto") == 0 || strcmp(s, "sha512") == 0) return VC_PRF_SHA512;
    if (strcmp(s, "sha256") == 0) return VC_PRF_SHA256;
    if (strcmp(s, "whirlpool") == 0) return VC_PRF_WHIRLPOOL;
    if (strcmp(s, "blake2s") == 0) return VC_PRF_BLAKE2S;
    if (strcmp(s, "streebog") == 0) return VC_PRF_STREEBOG;
    return -1;
}

static char* slurp_file(const char* path, size_t* len) {
    FILE* f = fopen(path, "rb");
    if (f == NULL) return NULL;
    if (fseek(f, 0, SEEK_END) != 0) {
        fclose(f);
        return NULL;
    }
    long n = ftell(f);
    if (n < 0) {
        fclose(f);
        return NULL;
    }
    rewind(f);
    char* buf = malloc((size_t)n + 1);
    if (buf == NULL) {
        fclose(f);
        return NULL;
    }
    if (fread(buf, 1, (size_t)n, f) != (size_t)n) {
        free(buf);
        fclose(f);
        return NULL;
    }
    fclose(f);
    buf[n] = '\0';
    if (len) *len = (size_t)n;
    return buf;
}

int main(int argc, char** argv) {
    const char* out = "volume.hc";
    int do_format = 0;
    const char* pwfile = NULL;
    uint64_t size = 3u * 1024u * 1024u;
    int pim = 0;
    int prf = VC_PRF_SHA512;
    uint64_t hidden = 0;

    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "--size") == 0 && i + 1 < argc) {
            size = strtoull(argv[++i], NULL, 0);
        } else if (strcmp(argv[i], "-o") == 0 && i + 1 < argc) {
            out = argv[++i];
        } else if (strcmp(argv[i], "--password-file") == 0 && i + 1 < argc) {
            pwfile = argv[++i];
        } else if (strcmp(argv[i], "--pim") == 0 && i + 1 < argc) {
            pim = atoi(argv[++i]);
        } else if (strcmp(argv[i], "--prf") == 0 && i + 1 < argc) {
            prf = parse_prf(argv[++i]);
            if (prf < 0) return usage(argv[0]), 2;
        } else if (strcmp(argv[i], "--hidden") == 0 && i + 1 < argc) {
            hidden = strtoull(argv[++i], NULL, 0);
        } else if (strcmp(argv[i], "--format") == 0 && i + 1 < argc) {
            if (strcmp(argv[++i], "fat12") != 0) {
                fprintf(stderr, "only --format fat12 supported\n");
                return 2;
            }
            do_format = 1;
        } else if (strcmp(argv[i], "-h") == 0 || strcmp(argv[i], "--help") == 0) {
            return usage(argv[0]), 0;
        } else {
            return usage(argv[0]), 2;
        }
    }
    if (pwfile == NULL) {
        fprintf(stderr, "password file required\n");
        return 2;
    }
    size_t pwlen = 0;
    char* pw = slurp_file(pwfile, &pwlen);
    if (pw == NULL) {
        fprintf(stderr, "cannot read password file: %s\n", pwfile);
        return 1;
    }

    vc_create_params params = {
        .password = pw,
        .password_len = pwlen,
        .pim = pim,
        .keyfiles = NULL,
        .keyfile_lens = NULL,
        .keyfile_count = 0,
        .prf = prf,
        .ea = 0,
        .hidden = hidden > 0 ? VC_HEADER_HIDDEN : VC_HEADER_NORMAL,
        .volume_size = size,
        .hidden_size = hidden,
    };

    vc_status st = vc_create_file(out, &params);
    if (st != VC_OK) {
        burn(pw, pwlen);
        free(pw);
        fprintf(stderr, "create failed: %d\n", (int)st);
        return 1;
    }
    if (do_format) {
        int fd = open(out, O_RDWR);
        if (fd < 0) {
            fprintf(stderr, "open for format: %s\n", strerror(errno));
            return 1;
        }
        vc_open_params op = {};
        op.password = pw;
        op.password_len = pwlen;
        op.pim = pim;
        op.prf = prf;
        vc_volume* vol = NULL;
        st = vc_open(fd, &op, &vol);
        if (st != VC_OK) {
            close(fd);
            fprintf(stderr, "open for format failed: %d\n", (int)st);
            return 1;
        }
        st = vc_format_fat12(vol);
        vc_close(vol);
        close(fd);
        if (st != VC_OK) {
            fprintf(stderr, "format failed: %d\n", (int)st);
            return 1;
        }
    }
    printf("%s\n", out);
    burn(pw, pwlen);
    free(pw);
    return 0;
}