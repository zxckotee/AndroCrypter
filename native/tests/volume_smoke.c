#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "hcvault/volume.h"

int main(int argc, char** argv) {
    const char* path = argc > 1 ? argv[1] : "testdata/fixtures/aes-sha512-fat12.hc";
    const char* password = getenv("TEST_VOLUME_PASSWORD");
    if (password == NULL || password[0] == '\0') {
        fprintf(stderr, "TEST_VOLUME_PASSWORD is required\n");
        return 2;
    }

    int fd = open(path, O_RDONLY);
    if (fd < 0) {
        perror("open");
        return 1;
    }

    vc_open_params params = {
        .password = password,
        .password_len = strlen(password),
        .pim = 0,
    };
    vc_volume* vol = NULL;
    vc_status st = vc_open(fd, &params, &vol);
    if (st != VC_OK) {
        fprintf(stderr, "vc_open failed: %d\n", (int)st);
        close(fd);
        return 1;
    }
    printf("opened ok data_size=%llu base=%llu\n", (unsigned long long)vc_data_size(vol), (unsigned long long)vc_encrypted_area_start(vol));

    uint8_t boot[512];
    st = vc_read(vol, 0, boot, sizeof(boot));
    if (st != VC_OK) {
        fprintf(stderr, "vc_read failed\n");
        vc_close(vol);
        close(fd);
        return 1;
    }
    printf("boot sector signature: %02X %02X\n", boot[510], boot[511]);
    vc_close(vol);
    close(fd);
    return (boot[510] == 0x55 && boot[511] == 0xAA) ? 0 : 1;
}
