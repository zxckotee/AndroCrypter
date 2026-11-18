#include "hcvault/volume.h"

#include <errno.h>
#include <fcntl.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>
#include <sys/random.h>
#include <sys/stat.h>
#include <unistd.h>

#include "Pkcs5.h"
#include "vc_internal.h"

vc_status vc_map_header_error(int code) {
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

vc_status vc_random_bytes(void* buf, size_t len) {
    uint8_t* p = buf;
    while (len > 0) {
        ssize_t n = getrandom(p, len, 0);
        if (n < 0) {
            if (errno == EINTR) {
                continue;
            }
            return VC_ERR_INTERNAL;
        }
        p += n;
        len -= (size_t)n;
    }
    return VC_OK;
}

uint64_t vc_file_size(int fd) {
    off_t end = lseek(fd, 0, SEEK_END);
    return end < 0 ? 0 : (uint64_t)end;
}

vc_status vc_build_password(
    Password* out, const char* password, size_t password_len,
    const uint8_t* const* keyfiles, const size_t* keyfile_lens, size_t keyfile_count) {
    memset(out, 0, sizeof(*out));
    if (password_len > MAX_PASSWORD) {
        return VC_ERR_AUTH;
    }
    if (password_len > 0 && password == NULL) {
        return VC_ERR_AUTH;
    }
    if (password_len > 0) {
        memcpy(out->Text, password, password_len);
    }
    size_t len = password_len;
    vc_status st = vc_keyfiles_apply(out->Text, &len, sizeof(out->Text), keyfiles, keyfile_lens, keyfile_count);
    if (st != VC_OK) {
        burn(out, sizeof(*out));
        return st;
    }
    out->Length = (unsigned __int32)len;
    return VC_OK;
}

vc_status vc_try_header(int fd, uint64_t header_offset, Password* password, int prf, int pim, PCRYPTO_INFO* out) {
    unsigned char header[TC_VOLUME_HEADER_EFFECTIVE_SIZE];
    ssize_t got = pread(fd, header, sizeof(header), (off_t)header_offset);
    if (got != (ssize_t)sizeof(header)) {
        return VC_ERR_IO;
    }
    PCRYPTO_INFO crypto = NULL;
    int status = ReadVolumeHeader(FALSE, header, password, prf, pim, &crypto, NULL);
    burn(header, sizeof(header));
    if (status != ERR_SUCCESS || crypto == NULL) {
        if (crypto != NULL) {
            crypto_close(crypto);
        }
        return vc_map_header_error(status);
    }
    *out = crypto;
    return VC_OK;
}

static int prf_from_internal(int pkcs5) {
    switch (pkcs5) {
    case SHA512:
        return VC_PRF_SHA512;
    case WHIRLPOOL:
        return VC_PRF_WHIRLPOOL;
    case SHA256:
        return VC_PRF_SHA256;
    case BLAKE2S:
        return VC_PRF_BLAKE2S;
    case STREEBOG:
        return VC_PRF_STREEBOG;
    default:
        return VC_PRF_AUTO;
    }
}

static int prf_to_internal(int prf) {
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
        return 0;
    }
}

static int aborted(const vc_open_params* params) {
    return params->abort_flag != NULL && *params->abort_flag != 0;
}

typedef struct header_slot {
    uint64_t offset;
    int hidden;
    int backup;
} header_slot;

static size_t plan_slots(const vc_open_params* params, uint64_t file_size, header_slot* slots) {
    size_t n = 0;
    int mode = params->hidden;
    if (mode != VC_HEADER_HIDDEN) {
        slots[n++] = (header_slot){0, 0, 0};
    }
    if (mode != VC_HEADER_NORMAL) {
        slots[n++] = (header_slot){TC_HIDDEN_VOLUME_HEADER_OFFSET, 1, 0};
    }
    if ((params->flags & VC_OPEN_TRY_BACKUP) != 0 && file_size >= 2 * TC_VOLUME_HEADER_GROUP_SIZE) {
        if (mode != VC_HEADER_HIDDEN) {
            slots[n++] = (header_slot){file_size - TC_VOLUME_HEADER_GROUP_SIZE, 0, 1};
        }
        if (mode != VC_HEADER_NORMAL) {
            slots[n++] = (header_slot){file_size - TC_HIDDEN_VOLUME_HEADER_OFFSET, 1, 1};
        }
    }
    return n;
}

static vc_status open_one_credential(
    int fd, const vc_open_params* params, uint64_t file_size, int only_hidden, header_slot* used, PCRYPTO_INFO* out) {
    Password password;
    vc_status st = vc_build_password(
        &password, params->password, params->password_len, params->keyfiles, params->keyfile_lens,
        params->keyfile_count);
    if (st != VC_OK) {
        return st;
    }

    vc_open_params local = *params;
    if (only_hidden) {
        local.hidden = VC_HEADER_HIDDEN;
    }
    header_slot slots[4];
    size_t n = plan_slots(&local, file_size, slots);
    int prf = prf_to_internal(params->prf);
    vc_status last = VC_ERR_AUTH;
    for (size_t i = 0; i < n; i++) {
        if (aborted(params)) {
            last = VC_ERR_CANCELLED;
            break;
        }
        st = vc_try_header(fd, slots[i].offset, &password, prf, params->pim, out);
        if (st == VC_OK) {
            *used = slots[i];
            last = VC_OK;
            break;
        }
        if (st == VC_ERR_IO) {
            last = VC_ERR_IO;
        } else if (st != VC_ERR_AUTH) {
            last = st;
        }
    }
    burn(&password, sizeof(password));
    return last;
}

vc_status vc_open(int fd, const vc_open_params* params, vc_volume** out) {
    if (fd < 0 || params == NULL || out == NULL) {
        return VC_ERR_IO;
    }
    if (params->password == NULL && params->keyfile_count == 0) {
        return VC_ERR_AUTH;
    }

    uint64_t file_size = vc_file_size(fd);
    PCRYPTO_INFO crypto = NULL;
    header_slot used = {0, 0, 0};
    vc_status st = open_one_credential(fd, params, file_size, 0, &used, &crypto);
    if (st != VC_OK) {
        return st;
    }

    PCRYPTO_INFO protect_crypto = NULL;
    header_slot protect_slot = {0, 0, 0};
    if (params->protect_hidden != NULL) {
        if (used.hidden) {
            crypto_close(crypto);
            return VC_ERR_UNSUPPORTED;
        }
        st = open_one_credential(fd, params->protect_hidden, file_size, 1, &protect_slot, &protect_crypto);
        if (st != VC_OK) {
            crypto_close(crypto);
            return st == VC_ERR_AUTH ? VC_ERR_PROTECTED : st;
        }
    }

    vc_volume* vol = calloc(1, sizeof(vc_volume));
    if (vol == NULL) {
        crypto_close(crypto);
        if (protect_crypto != NULL) {
            crypto_close(protect_crypto);
        }
        return VC_ERR_INTERNAL;
    }
    pthread_mutex_init(&vol->write_lock, NULL);

    vol->fd = fd;
    vol->crypto = crypto;
    vol->sector_size = crypto->SectorSize != 0 ? crypto->SectorSize : TC_SECTOR_SIZE_FILE_HOSTED_VOLUME;
    vol->data_base = crypto->EncryptedAreaStart.Value;
    vol->data_size = crypto->VolumeSize.Value;
    if (vol->data_size > crypto->EncryptedAreaLength.Value && crypto->EncryptedAreaLength.Value != 0) {
        vol->data_size = crypto->EncryptedAreaLength.Value;
    }
    vol->hidden = used.hidden;
    vol->used_backup = used.backup;
    vol->prf = prf_from_internal(crypto->pkcs5);
    vol->header_version = crypto->HeaderVersion;
    vol->readonly = (params->flags & VC_OPEN_READONLY) != 0;

    if (protect_crypto != NULL) {
        vol->protect_start = protect_crypto->EncryptedAreaStart.Value;
        vol->protect_end = vol->protect_start + protect_crypto->VolumeSize.Value;
        vol->protect_active = 1;
        crypto_close(protect_crypto);
    }

    /* Best effort: keep key schedules out of swap and core dumps. */
    if (mlock(crypto, sizeof(*crypto)) != 0) {
        /* RLIMIT_MEMLOCK can be tiny on Android; not fatal. */
    }
#ifdef MADV_DONTDUMP
    madvise((void*)((uintptr_t)crypto & ~(uintptr_t)4095), sizeof(*crypto) + 4096, MADV_DONTDUMP);
#endif

    *out = vol;
    return VC_OK;
}

uint64_t vc_data_size(const vc_volume* vol) {
    return vol == NULL ? 0 : vol->data_size;
}

uint64_t vc_encrypted_area_start(const vc_volume* vol) {
    return vol == NULL ? 0 : vol->data_base;
}

int vc_is_readonly(const vc_volume* vol) {
    return vol == NULL ? 1 : (vol->readonly || vol->write_locked);
}

vc_status vc_get_info(const vc_volume* vol, vc_info* out) {
    if (vol == NULL || out == NULL) {
        return VC_ERR_IO;
    }
    memset(out, 0, sizeof(*out));
    out->data_size = vol->data_size;
    out->data_offset = vol->data_base;
    out->sector_size = vol->sector_size;
    out->prf = vol->prf;
    out->hidden = vol->hidden;
    out->readonly = vc_is_readonly(vol);
    out->header_version = vol->header_version;
    out->used_backup_header = vol->used_backup;
    out->hidden_protected = vol->protect_active;

    wchar_t wname[64];
    EAGetName(wname, sizeof(wname) / sizeof(wname[0]), vol->crypto->ea, 0);
    size_t i = 0;
    for (; wname[i] != 0 && i + 1 < sizeof(out->cipher_name); i++) {
        out->cipher_name[i] = (char)wname[i];
    }
    out->cipher_name[i] = '\0';

    const wchar_t* prf_name = HashGetName(vol->crypto->pkcs5);
    for (i = 0; prf_name != NULL && prf_name[i] != 0 && i + 1 < sizeof(out->prf_name); i++) {
        out->prf_name[i] = (char)prf_name[i];
    }
    out->prf_name[i] = '\0';
    return VC_OK;
}

static vc_status check_range(const vc_volume* vol, uint64_t offset, size_t len) {
    if (offset > vol->data_size || len > vol->data_size - offset) {
        return VC_ERR_RANGE;
    }
    return VC_OK;
}

static void decrypt_units(vc_volume* vol, uint8_t* buf, uint64_t abs_aligned, size_t bytes) {
    UINT64_STRUCT unit;
    unit.Value = abs_aligned / VC_DATA_UNIT;
    DecryptDataUnitsCurrentThreadEx(buf, &unit, (TC_LARGEST_COMPILER_UINT)(bytes / VC_DATA_UNIT), vol->crypto);
}

static void encrypt_units(vc_volume* vol, uint8_t* buf, uint64_t abs_aligned, size_t bytes) {
    UINT64_STRUCT unit;
    unit.Value = abs_aligned / VC_DATA_UNIT;
    EncryptDataUnitsCurrentThreadEx(buf, &unit, (TC_LARGEST_COMPILER_UINT)(bytes / VC_DATA_UNIT), vol->crypto);
}

static vc_status pread_full(int fd, void* buf, size_t len, uint64_t off) {
    uint8_t* p = buf;
    while (len > 0) {
        ssize_t n = pread(fd, p, len, (off_t)off);
        if (n < 0) {
            if (errno == EINTR) {
                continue;
            }
            return VC_ERR_IO;
        }
        if (n == 0) {
            return VC_ERR_IO;
        }
        p += n;
        off += (uint64_t)n;
        len -= (size_t)n;
    }
    return VC_OK;
}

static vc_status pwrite_full(int fd, const void* buf, size_t len, uint64_t off) {
    const uint8_t* p = buf;
    while (len > 0) {
        ssize_t n = pwrite(fd, p, len, (off_t)off);
        if (n < 0) {
            if (errno == EINTR) {
                continue;
            }
            return errno == ENOSPC ? VC_ERR_NOSPACE : VC_ERR_IO;
        }
        p += n;
        off += (uint64_t)n;
        len -= (size_t)n;
    }
    return VC_OK;
}

vc_status vc_read(vc_volume* vol, uint64_t offset, void* buf, size_t len) {
    if (vol == NULL || (buf == NULL && len > 0)) {
        return VC_ERR_IO;
    }
    if (len == 0) {
        return VC_OK;
    }
    vc_status st = check_range(vol, offset, len);
    if (st != VC_OK) {
        return st;
    }

    uint8_t* out = buf;
    uint64_t abs = vol->data_base + offset;
    uint64_t end = abs + len;
    uint64_t pos = abs & ~(uint64_t)(VC_DATA_UNIT - 1);
    uint64_t aligned_end = (end + VC_DATA_UNIT - 1) & ~(uint64_t)(VC_DATA_UNIT - 1);

    size_t cap = aligned_end - pos < VC_IO_CHUNK ? (size_t)(aligned_end - pos) : VC_IO_CHUNK;
    uint8_t* tmp = malloc(cap);
    if (tmp == NULL) {
        return VC_ERR_INTERNAL;
    }
    while (pos < aligned_end) {
        size_t chunk = aligned_end - pos < cap ? (size_t)(aligned_end - pos) : cap;
        st = pread_full(vol->fd, tmp, chunk, pos);
        if (st != VC_OK) {
            break;
        }
        decrypt_units(vol, tmp, pos, chunk);
        uint64_t copy_from = abs > pos ? abs : pos;
        uint64_t copy_to = end < pos + chunk ? end : pos + chunk;
        if (copy_to > copy_from) {
            memcpy(out, tmp + (copy_from - pos), (size_t)(copy_to - copy_from));
            out += copy_to - copy_from;
        }
        pos += chunk;
    }
    free(tmp);
    return st;
}

static int overlaps_protected(const vc_volume* vol, uint64_t abs, uint64_t end) {
    return vol->protect_active && abs < vol->protect_end && end > vol->protect_start;
}

vc_status vc_write(vc_volume* vol, uint64_t offset, const void* buf, size_t len) {
    if (vol == NULL || (buf == NULL && len > 0)) {
        return VC_ERR_IO;
    }
    if (len == 0) {
        return VC_OK;
    }
    if (vol->readonly || vol->write_locked) {
        return VC_ERR_READONLY;
    }
    vc_status st = check_range(vol, offset, len);
    if (st != VC_OK) {
        return st;
    }

    const uint8_t* in = buf;
    uint64_t abs = vol->data_base + offset;
    uint64_t end = abs + len;
    if (overlaps_protected(vol, abs, end)) {
        vol->write_locked = 1;
        return VC_ERR_PROTECTED;
    }

    uint64_t pos = abs & ~(uint64_t)(VC_DATA_UNIT - 1);
    uint64_t aligned_end = (end + VC_DATA_UNIT - 1) & ~(uint64_t)(VC_DATA_UNIT - 1);
    size_t cap = aligned_end - pos < VC_IO_CHUNK ? (size_t)(aligned_end - pos) : VC_IO_CHUNK;
    uint8_t* tmp = malloc(cap);
    if (tmp == NULL) {
        return VC_ERR_INTERNAL;
    }

    pthread_mutex_lock(&vol->write_lock);
    while (pos < aligned_end && st == VC_OK) {
        size_t chunk = aligned_end - pos < cap ? (size_t)(aligned_end - pos) : cap;
        uint64_t chunk_end = pos + chunk;
        int head_partial = abs > pos;
        int tail_partial = end < chunk_end;

        if (head_partial || tail_partial) {
            /* Read-modify-write only the boundary units; interior is fully overwritten. */
            if (head_partial) {
                st = pread_full(vol->fd, tmp, VC_DATA_UNIT, pos);
                if (st != VC_OK) {
                    break;
                }
                decrypt_units(vol, tmp, pos, VC_DATA_UNIT);
            }
            if (tail_partial && !(head_partial && chunk == VC_DATA_UNIT)) {
                st = pread_full(vol->fd, tmp + chunk - VC_DATA_UNIT, VC_DATA_UNIT, chunk_end - VC_DATA_UNIT);
                if (st != VC_OK) {
                    break;
                }
                decrypt_units(vol, tmp + chunk - VC_DATA_UNIT, chunk_end - VC_DATA_UNIT, VC_DATA_UNIT);
            }
        }

        uint64_t copy_from = abs > pos ? abs : pos;
        uint64_t copy_to = end < chunk_end ? end : chunk_end;
        memcpy(tmp + (copy_from - pos), in, (size_t)(copy_to - copy_from));
        in += copy_to - copy_from;

        encrypt_units(vol, tmp, pos, chunk);
        st = pwrite_full(vol->fd, tmp, chunk, pos);
        pos = chunk_end;
    }
    pthread_mutex_unlock(&vol->write_lock);
    free(tmp);
    return st;
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
        munlock(vol->crypto, sizeof(*vol->crypto));
        crypto_close(vol->crypto);
    }
    pthread_mutex_destroy(&vol->write_lock);
    burn(vol, sizeof(*vol));
    free(vol);
}
