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
} vc_status;

typedef struct vc_open_params {
    const char* password;
    size_t password_len;
    int pim;
    const uint8_t* const* keyfiles;
    const size_t* keyfile_lens;
    size_t keyfile_count;
    int hidden; /* 0 = normal, 1 = hidden volume password path */
    volatile int* abort_flag;
} vc_open_params;

typedef struct vc_volume vc_volume;

vc_status vc_open(int fd, const vc_open_params* params, vc_volume** out);
vc_status vc_read(vc_volume* vol, uint64_t offset, void* buf, size_t len);
vc_status vc_write(vc_volume* vol, uint64_t offset, const void* buf, size_t len);
vc_status vc_flush(vc_volume* vol);
void vc_close(vc_volume* vol);
uint64_t vc_data_size(const vc_volume* vol);
uint64_t vc_encrypted_area_start(const vc_volume* vol);

#ifdef __cplusplus
}
#endif
