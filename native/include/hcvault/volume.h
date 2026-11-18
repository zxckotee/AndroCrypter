#pragma once

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef enum vc_status {
    VC_OK = 0,
    VC_ERR_AUTH = 1,
    VC_ERR_CANCELLED = 2,
    VC_ERR_UNSUPPORTED = 3,
    VC_ERR_IO = 4,
    VC_ERR_INTERNAL = 5,
    VC_ERR_READONLY = 6,
    VC_ERR_PROTECTED = 7,
    VC_ERR_RANGE = 8,
    VC_ERR_KEYFILE = 9,
    VC_ERR_EXISTS = 10,
    VC_ERR_NOSPACE = 11,
} vc_status;

/* Key derivation PRF. VC_PRF_AUTO tries every supported PRF. */
typedef enum vc_prf {
    VC_PRF_AUTO = 0,
    VC_PRF_SHA512 = 1,
    VC_PRF_WHIRLPOOL = 2,
    VC_PRF_SHA256 = 3,
    VC_PRF_BLAKE2S = 4,
    VC_PRF_STREEBOG = 5,
} vc_prf;

/* Which header slot(s) to try (vc_open_params.hidden). */
enum {
    VC_HEADER_AUTO = 0,   /* normal header, then hidden header */
    VC_HEADER_HIDDEN = 1, /* hidden volume header only */
    VC_HEADER_NORMAL = 2, /* normal header only */
};

/* vc_open_params.flags */
enum {
    VC_OPEN_READONLY = 1u << 0,
    VC_OPEN_TRY_BACKUP = 1u << 1, /* fall back to the embedded backup headers */
};

typedef struct vc_open_params {
    const char* password;
    size_t password_len;
    int pim;
    const uint8_t* const* keyfiles;
    const size_t* keyfile_lens;
    size_t keyfile_count;
    int hidden; /* VC_HEADER_* */
    volatile int* abort_flag;
    int prf;                                     /* vc_prf, 0 = auto-detect */
    unsigned flags;                              /* VC_OPEN_* */
    const struct vc_open_params* protect_hidden; /* credentials of the hidden volume to protect while writing to the outer one */
} vc_open_params;

typedef struct vc_volume vc_volume;

typedef struct vc_info {
    uint64_t data_size;
    uint64_t data_offset;
    uint32_t sector_size;
    int prf;
    int hidden;
    int readonly;
    int header_version;
    int used_backup_header;
    int hidden_protected;
    char cipher_name[64];
    char prf_name[32];
} vc_info;

vc_status vc_open(int fd, const vc_open_params* params, vc_volume** out);
vc_status vc_read(vc_volume* vol, uint64_t offset, void* buf, size_t len);
vc_status vc_write(vc_volume* vol, uint64_t offset, const void* buf, size_t len);
vc_status vc_flush(vc_volume* vol);
void vc_close(vc_volume* vol);
uint64_t vc_data_size(const vc_volume* vol);
uint64_t vc_encrypted_area_start(const vc_volume* vol);
vc_status vc_get_info(const vc_volume* vol, vc_info* out);
int vc_is_readonly(const vc_volume* vol);

/* VeraCrypt keyfile pool applied to a password buffer (exposed for tests and creation code). */
vc_status vc_keyfiles_apply(
    uint8_t* password_text, size_t* password_len, size_t password_cap,
    const uint8_t* const* keyfiles, const size_t* keyfile_lens, size_t keyfile_count);

/* Cryptographically secure random bytes (getrandom). */
vc_status vc_random_bytes(void* buf, size_t len);

typedef struct vc_create_params {
    const char* password;
    size_t password_len;
    int pim;
    const uint8_t* const* keyfiles;
    const size_t* keyfile_lens;
    size_t keyfile_count;
    int prf;            /* vc_prf, 0 = SHA-512 default */
    int ea;             /* 0 = AES */
    int hidden;         /* VC_HEADER_* */
    uint64_t volume_size; /* total container size in bytes (multiple of 512) */
    uint64_t hidden_size; /* inner hidden volume size when creating hidden header */
} vc_create_params;

/* Creates a new file-hosted VeraCrypt volume (header at 0; optional hidden header). */
vc_status vc_create_file(const char* path, const vc_create_params* params);

/* Quick FAT12 layout in the decrypted data area (host/tests; not full mkfs). */
vc_status vc_format_fat12(vc_volume* vol);

#ifdef __cplusplus
}
#endif
