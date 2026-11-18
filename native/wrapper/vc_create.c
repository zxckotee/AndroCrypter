#include "hcvault/volume.h"

#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

#include "Crypto.h"
#include "Password.h"
#include "Volumes.h"
#include "vc_internal.h"

static int prf_to_pkcs5(int prf) {
    switch (prf) {
    case VC_PRF_SHA512:
        return SHA512;
    case VC_PRF_WHIRLPOOL:
        return WHIRLPOOL;
    case VC_PRF_SHA256:
        return SHA256;
    case VC_PRF_BLAKE2S:
        return BLAKE2S;
    case VC_PRF_STREEBOG:
        return STREEBOG;
    default:
        return SHA512;
    }
}

static wchar_t* utf8_to_wide(const char* s, size_t len) {
    wchar_t* w = calloc(len + 1, sizeof(wchar_t));
    if (w == NULL) return NULL;
    for (size_t i = 0; i < len; i++) w[i] = (wchar_t)(unsigned char)s[i];
    return w;
}

vc_status vc_create_file(const char* path, const vc_create_params* params) {
    if (path == NULL || params == NULL) {
        return VC_ERR_IO;
    }
    if (params->volume_size < TC_MIN_VOLUME_SIZE || (params->volume_size % TC_SECTOR_SIZE_FILE_HOSTED_VOLUME) != 0) {
        return VC_ERR_RANGE;
    }

    Password password;
    vc_status st = vc_build_password(
        &password, params->password, params->password_len, params->keyfiles, params->keyfile_lens,
        params->keyfile_count);
    if (st != VC_OK) {
        return st;
    }

    int ea = params->ea != 0 ? params->ea : AES;
    int prf = prf_to_pkcs5(params->prf);
    int mode = XTS;

    uint64_t data_size = params->volume_size - TC_VOLUME_HEADER_GROUP_SIZE;
    unsigned char header[TC_VOLUME_HEADER_EFFECTIVE_SIZE];
    PCRYPTO_INFO crypto = NULL;
    int status = CreateVolumeHeaderInMemory(
        NULL, FALSE, header, ea, mode, &password, prf, params->pim, NULL, &crypto, data_size, 0,
        TC_VOLUME_DATA_OFFSET, data_size, TC_VOLUME_MIN_REQUIRED_PROGRAM_VERSION, 0, TC_SECTOR_SIZE_FILE_HOSTED_VOLUME,
        FALSE);
    if (status != ERR_SUCCESS || crypto == NULL) {
        burn(&password, sizeof(password));
        burn(header, sizeof(header));
        return vc_map_header_error(status);
    }
    crypto_close(crypto);

    int fd = open(path, O_CREAT | O_RDWR | O_TRUNC, 0600);
    if (fd < 0) {
        burn(&password, sizeof(password));
        burn(header, sizeof(header));
        return VC_ERR_IO;
    }
    if (ftruncate(fd, (off_t)params->volume_size) != 0) {
        close(fd);
        unlink(path);
        burn(&password, sizeof(password));
        burn(header, sizeof(header));
        return VC_ERR_IO;
    }

    if (pwrite(fd, header, sizeof(header), 0) != (ssize_t)sizeof(header)) {
        close(fd);
        unlink(path);
        burn(&password, sizeof(password));
        burn(header, sizeof(header));
        return VC_ERR_IO;
    }
    burn(header, sizeof(header));

    if (params->hidden == VC_HEADER_HIDDEN) {
        if (params->hidden_size < TC_MIN_VOLUME_SIZE) {
            close(fd);
            burn(&password, sizeof(password));
            return VC_ERR_RANGE;
        }
        unsigned char hheader[TC_VOLUME_HEADER_EFFECTIVE_SIZE];
        memset(hheader, 0, sizeof(hheader));
        crypto = NULL;
        status = CreateVolumeHeaderInMemory(
            NULL, FALSE, hheader, ea, mode, &password, prf, params->pim, NULL, &crypto, params->hidden_size, 0,
            TC_VOLUME_DATA_OFFSET, params->hidden_size, TC_VOLUME_MIN_REQUIRED_PROGRAM_VERSION, 0,
            TC_SECTOR_SIZE_FILE_HOSTED_VOLUME, FALSE);
        if (status != ERR_SUCCESS || crypto == NULL) {
            close(fd);
            burn(&password, sizeof(password));
            burn(hheader, sizeof(hheader));
            return vc_map_header_error(status);
        }
        crypto_close(crypto);
        if (pwrite(fd, hheader, sizeof(hheader), (off_t)TC_HIDDEN_VOLUME_HEADER_OFFSET) != (ssize_t)sizeof(hheader)) {
            close(fd);
            burn(&password, sizeof(password));
            burn(hheader, sizeof(hheader));
            return VC_ERR_IO;
        }
        burn(hheader, sizeof(hheader));
    }

    burn(&password, sizeof(password));
    fsync(fd);
    close(fd);
    return VC_OK;
}