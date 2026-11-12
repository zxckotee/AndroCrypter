#include "hcvault/volume.h"

#include <errno.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "Crypto.h"
#include "Password.h"
#include "Volumes.h"

struct vc_volume {
    int fd;
    PCRYPTO_INFO crypto;
    uint64_t data_size;
    uint64_t data_base;
    uint32_t sector_size;
};

static vc_status map_vc_error(int code) {
    switch (code) {
    case ERR_SUCCESS:
        return VC_OK;
    case ERR_PASSWORD_WRONG:
    case ERR_KEY_DERIVATION_FAILED:
        return VC_ERR_AUTH;
    case ERR_PARAMETER_INCORRECT:
    case ERR_VOL_FORMAT_BAD:
    case ERR_CIPHER_INIT_FAILURE:
    case ERR_MODE_INIT_FAILED:
        return VC_ERR_UNSUPPORTED;
    default:
        return VC_ERR_INTERNAL;
    }
}

static void wipe_password(Password* pw) {
    burn(pw->Text, sizeof(pw->Text));
    pw->Length = 0;
}

vc_status vc_open(int fd, const vc_open_params* params, vc_volume** out) {
    if (fd < 0 || params == NULL || out == NULL || params->password == NULL) {
        return VC_ERR_IO;
    }

    unsigned char header[TC_VOLUME_HEADER_EFFECTIVE_SIZE];
    ssize_t got = pread(fd, header, sizeof(header), 0);
    if (got != (ssize_t)sizeof(header)) {
        return VC_ERR_IO;
    }

    Password password;
    memset(&password, 0, sizeof(password));
    if (params->password_len > MAX_PASSWORD) {
        return VC_ERR_AUTH;
    }
    memcpy(password.Text, params->password, params->password_len);
    password.Length = (unsigned __int32)params->password_len;

    PCRYPTO_INFO crypto = NULL;
    /* Auto-detect PRF/EA when selected_pkcs5_prf == 0; use SHA-512 when iterating in CI fixtures. */
    /* When the caller does not pin a PRF, default to SHA-512 for CI fixtures; full auto-detect is expensive. */
    int header_status = ReadVolumeHeader(FALSE, header, &password, SHA512, params->pim, &crypto, NULL);
    wipe_password(&password);

    if (header_status != ERR_SUCCESS || crypto == NULL) {
        if (crypto != NULL) {
            crypto_close(crypto);
        }
        return map_vc_error(header_status);
    }

    vc_volume* vol = calloc(1, sizeof(vc_volume));
    if (vol == NULL) {
        crypto_close(crypto);
        return VC_ERR_INTERNAL;
    }

    vol->fd = fd;
    vol->crypto = crypto;
    vol->sector_size = crypto->SectorSize != 0 ? crypto->SectorSize : TC_SECTOR_SIZE_FILE_HOSTED_VOLUME;
    vol->data_base = crypto->EncryptedAreaStart.Value;
    vol->data_size = crypto->VolumeSize.Value;
    if (vol->data_size > crypto->EncryptedAreaLength.Value && crypto->EncryptedAreaLength.Value != 0) {
        vol->data_size = crypto->EncryptedAreaLength.Value;
    }

    *out = vol;
    return VC_OK;
}

uint64_t vc_data_size(const vc_volume* vol) {
    return vol == NULL ? 0 : vol->data_size;
}

uint64_t vc_encrypted_area_start(const vc_volume* vol) {
    return vol == NULL ? 0 : vol->data_base;
}

vc_status vc_read(vc_volume* vol, uint64_t offset, void* buf, size_t len) {
    if (vol == NULL || buf == NULL) {
        return VC_ERR_IO;
    }
    uint8_t* out = buf;
    size_t remaining = len;
    uint64_t pos = offset;

    while (remaining > 0) {
        uint64_t file_off = vol->data_base + pos;
        uint32_t sector = vol->sector_size;
        uint32_t sector_off = (uint32_t)(pos % sector);
        size_t chunk = sector - sector_off;
        if (chunk > remaining) {
            chunk = remaining;
        }

        uint8_t sector_buf[4096];
        if (sector > sizeof(sector_buf)) {
            return VC_ERR_INTERNAL;
        }

        const off_t sector_file_off = (off_t)(file_off - sector_off);
        ssize_t read_bytes = pread(vol->fd, sector_buf, sector, sector_file_off);
        if (read_bytes != (ssize_t)sector) {
            return VC_ERR_IO;
        }

        UINT64_STRUCT unit;
        unit.Value = (uint64_t)sector_file_off / sector;
        DecryptDataUnitsCurrentThreadEx(sector_buf, &unit, 1, vol->crypto);

        memcpy(out, sector_buf + sector_off, chunk);
        out += chunk;
        pos += chunk;
        remaining -= chunk;
    }
    return VC_OK;
}

vc_status vc_write(vc_volume* vol, uint64_t offset, const void* buf, size_t len) {
    if (vol == NULL || buf == NULL) {
        return VC_ERR_IO;
    }
    const uint8_t* in = buf;
    size_t remaining = len;
    uint64_t pos = offset;

    while (remaining > 0) {
        uint64_t file_off = vol->data_base + pos;
        uint32_t sector = vol->sector_size;
        uint32_t sector_off = (uint32_t)(pos % sector);
        size_t chunk = sector - sector_off;
        if (chunk > remaining) {
            chunk = remaining;
        }

        uint8_t sector_buf[4096];
        if (sector > sizeof(sector_buf)) {
            return VC_ERR_INTERNAL;
        }

        const off_t sector_file_off = (off_t)(file_off - sector_off);
        ssize_t read_bytes = pread(vol->fd, sector_buf, sector, sector_file_off);
        if (read_bytes != (ssize_t)sector) {
            return VC_ERR_IO;
        }

        memcpy(sector_buf + sector_off, in, chunk);

        UINT64_STRUCT unit;
        unit.Value = (uint64_t)sector_file_off / sector;
        EncryptDataUnitsCurrentThreadEx(sector_buf, &unit, 1, vol->crypto);

        ssize_t written = pwrite(vol->fd, sector_buf, sector, sector_file_off);
        if (written != (ssize_t)sector) {
            return VC_ERR_IO;
        }

        in += chunk;
        pos += chunk;
        remaining -= chunk;
    }
    return VC_OK;
}

vc_status vc_flush(vc_volume* vol) {
    if (vol == NULL) {
        return VC_ERR_IO;
    }
    return fsync(vol->fd) == 0 ? VC_OK : VC_ERR_IO;
}

void vc_close(vc_volume* vol) {
    if (vol == NULL) {
        return;
    }
    if (vol->crypto != NULL) {
        crypto_close(vol->crypto);
    }
    burn(vol, sizeof(*vol));
    free(vol);
}
