#pragma once

#include <pthread.h>
#include <stdint.h>

#include "Crypto.h"
#include "Password.h"
#include "Volumes.h"
#include "hcvault/volume.h"

#define VC_DATA_UNIT 512u
#define VC_IO_CHUNK (1024u * 1024u)

struct vc_volume {
    int fd;
    PCRYPTO_INFO crypto;
    uint64_t data_size;
    uint64_t data_base;
    uint32_t sector_size;
    int readonly;
    int hidden;
    int used_backup;
    int prf;
    int header_version;
    int write_locked;
    uint64_t protect_start; /* absolute file offsets of the protected hidden area */
    uint64_t protect_end;
    int protect_active;
    pthread_mutex_t write_lock;
};

/* Builds the password buffer VeraCrypt derives keys from (password + keyfile pool). */
vc_status vc_build_password(
    Password* out, const char* password, size_t password_len,
    const uint8_t* const* keyfiles, const size_t* keyfile_lens, size_t keyfile_count);

/* Reads and decrypts one volume header at the given file offset. */
vc_status vc_try_header(
    int fd, uint64_t header_offset, Password* password, int prf, int pim, PCRYPTO_INFO* out);

vc_status vc_map_header_error(int code);
uint64_t vc_file_size(int fd);
