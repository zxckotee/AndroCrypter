#include <string.h>

#include "Crc.h"
#include "hcvault/volume.h"
#include "vc_internal.h"

/* Mirrors Keyfiles.h (not included: it pulls in Windows-only declarations). */
#define POOL_MAX 128
#define POOL_LEGACY 64
#define KEYFILE_READ_LIMIT (1024 * 1024)

static void pool_feed(uint8_t* pool, uint32_t pool_size, const uint8_t* data, size_t len) {
    uint32_t crc = 0xffffffff;
    uint32_t write_pos = 0;
    size_t total = 0;
    for (size_t i = 0; i < len; i++) {
        crc = UPDC32(data[i], crc);
        pool[write_pos++] += (uint8_t)(crc >> 24);
        pool[write_pos++] += (uint8_t)(crc >> 16);
        pool[write_pos++] += (uint8_t)(crc >> 8);
        pool[write_pos++] += (uint8_t)crc;
        if (write_pos >= pool_size) {
            write_pos = 0;
        }
        if (++total >= KEYFILE_READ_LIMIT) {
            break;
        }
    }
}

vc_status vc_keyfiles_apply(
    uint8_t* password_text, size_t* password_len, size_t password_cap,
    const uint8_t* const* keyfiles, const size_t* keyfile_lens, size_t keyfile_count) {
    if (keyfile_count == 0) {
        return VC_OK;
    }
    if (password_text == NULL || password_len == NULL || keyfiles == NULL || keyfile_lens == NULL) {
        return VC_ERR_KEYFILE;
    }
    if (password_cap < POOL_MAX || *password_len > POOL_MAX) {
        return VC_ERR_KEYFILE;
    }

    uint32_t pool_size = *password_len <= MAX_LEGACY_PASSWORD ? POOL_LEGACY : POOL_MAX;
    uint8_t pool[POOL_MAX];
    memset(pool, 0, sizeof(pool));

    for (size_t i = 0; i < keyfile_count; i++) {
        if (keyfiles[i] == NULL || keyfile_lens[i] == 0) {
            burn(pool, sizeof(pool));
            return VC_ERR_KEYFILE;
        }
        pool_feed(pool, pool_size, keyfiles[i], keyfile_lens[i]);
    }

    for (size_t i = 0; i < pool_size; i++) {
        if (i < *password_len) {
            password_text[i] = (uint8_t)(password_text[i] + pool[i]);
        } else {
            password_text[i] = pool[i];
        }
    }
    if (*password_len < pool_size) {
        *password_len = pool_size;
    }
    burn(pool, sizeof(pool));
    return VC_OK;
}
