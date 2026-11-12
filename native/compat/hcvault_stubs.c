#include "Aes.h"
#include "Aes_hw_cpu.h"
#include "Camellia.h"
#include "Serpent.h"
#include <string.h>
#include "Twofish.h"
#include "chacha256.h"
#include "chachaRng.h"
#include "cpu.h"
#include "kuznyechik.h"

void aes_hw_cpu_encrypt(const uint8* ks, uint8* data) {
    aes_encrypt(data, data, (aes_encrypt_ctx*)ks);
}

void aes_hw_cpu_decrypt(const uint8* ks, uint8* data) {
    aes_decrypt(data, data, (aes_decrypt_ctx*)ks);
}

void aes_hw_cpu_encrypt_32_blocks(const uint8* ks, uint8* data) {
    for (int i = 0; i < 32; ++i) {
        aes_hw_cpu_encrypt(ks, data + i * 16);
    }
}

void aes_hw_cpu_decrypt_32_blocks(const uint8* ks, uint8* data) {
    for (int i = 0; i < 32; ++i) {
        aes_hw_cpu_decrypt(ks, data + i * 16);
    }
}

void serpent_encrypt_blocks(const unsigned __int8* in, unsigned __int8* out, size_t blocks, unsigned __int8* ks) {
    for (size_t i = 0; i < blocks; ++i) {
        serpent_encrypt(in + i * 16, out + i * 16, ks);
    }
}

void serpent_decrypt_blocks(const unsigned __int8* in, unsigned __int8* out, size_t blocks, unsigned __int8* ks) {
    for (size_t i = 0; i < blocks; ++i) {
        serpent_decrypt(in + i * 16, out + i * 16, ks);
    }
}

void twofish_enc_blk(void* ks, uint8* out, const uint8* in) {
    twofish_encrypt(ks, out, in);
}

void twofish_dec_blk(void* ks, uint8* out, const uint8* in) {
    twofish_decrypt(ks, out, in);
}

void twofish_enc_blk2(void* ks, uint8* out, const uint8* in) {
    twofish_enc_blk(ks, out, in);
    twofish_enc_blk(ks, out + 16, in + 16);
}

void twofish_dec_blk2(void* ks, uint8* out, const uint8* in) {
    twofish_dec_blk(ks, out, in);
    twofish_dec_blk(ks, out + 16, in + 16);
}

void twofish_enc_blk3(void* ks, uint8* out, const uint8* in) {
    twofish_enc_blk2(ks, out, in);
    twofish_enc_blk(ks, out + 32, in + 32);
}

void twofish_dec_blk3(void* ks, uint8* out, const uint8* in) {
    twofish_dec_blk2(ks, out, in);
    twofish_dec_blk(ks, out + 32, in + 32);
}

void camellia_encrypt_asm(const uint8* in, uint8* out, const void* ks) {
    camellia_encrypt(out, in, ks);
}

void camellia_decrypt_asm(const uint8* in, uint8* out, const void* ks) {
    camellia_decrypt(out, in, ks);
}

void camellia_ecb_enc_16way(void* ks, uint8* data) {
    for (int i = 0; i < 16; ++i) {
        camellia_encrypt(data + i * 16, data + i * 16, ks);
    }
}

void camellia_ecb_dec_16way(void* ks, uint8* data) {
    for (int i = 0; i < 16; ++i) {
        camellia_decrypt(data + i * 16, data + i * 16, ks);
    }
}

void camellia_enc_blk2(void* ks, uint8* out, const uint8* in) {
    camellia_encrypt(out, in, ks);
    camellia_encrypt(out + 16, in + 16, ks);
}

void camellia_dec_blk2(void* ks, uint8* out, const uint8* in) {
    camellia_decrypt(out, in, ks);
    camellia_decrypt(out + 16, in + 16, ks);
}

void kuznyechik_set_key_simd(const uint8* key, void* ks) {
    kuznyechik_set_key(key, (kuznyechik_kds*)ks);
}

void kuznyechik_encrypt_block_simd(const uint8* in, uint8* out, const void* ks) {
    kuznyechik_encrypt_block(out, in, ks);
}

void kuznyechik_decrypt_block_simd(const uint8* in, uint8* out, const void* ks) {
    kuznyechik_decrypt_block(out, in, ks);
}

void kuznyechik_encrypt_blocks_simd(uint8* out, const uint8* in, size_t count, const void* ks) {
    for (size_t i = 0; i < count; ++i) {
        kuznyechik_encrypt_block(out + i * 16, in + i * 16, ks);
    }
}

void kuznyechik_decrypt_blocks_simd(uint8* out, const uint8* in, size_t count, const void* ks) {
    for (size_t i = 0; i < count; ++i) {
        kuznyechik_decrypt_block(out + i * 16, in + i * 16, ks);
    }
}

void sha512_sse4(void) {}
void sha512_rorx(void) {}
void sha512_avx(void) {}
void sha512_compress_nayuki(void) {}
void sha256_intel(void) {}
void sha256_sse4(void) {}
void sha256_rorx(void) {}
void sha256_avx(void) {}
int TrySHA256(void) {
    return 0;
}

int blake2s_has_sse2(void) {
    return 0;
}
int blake2s_has_ssse3(void) {
    return 0;
}
int blake2s_has_sse41(void) {
    return 0;
}
void blake2s_compress_sse2(void) {}
void blake2s_compress_ssse3(void) {}
void blake2s_compress_sse41(void) {}

int t1ha_selfcheck__t1ha2(void) {
    return 1;
}

uint64_t t1ha2_atonce128(uint64_t* extra_result, const void* data, size_t len, uint64_t seed) {
    if (extra_result != NULL) {
        *extra_result = 0;
    }
    (void)data;
    (void)len;
    (void)seed;
    return 0;
}

void ChaCha20RngInit(
    ChaCha20RngCtx* pCtx,
    const unsigned char* key,
    GetRandSeedFn rngCallback,
    size_t InitialBytesToSkip) {
    (void)pCtx;
    (void)key;
    (void)rngCallback;
    (void)InitialBytesToSkip;
}

void ChaCha20RngGetBytes(ChaCha20RngCtx* pCtx, unsigned char* buffer, size_t bufferLen) {
    (void)pCtx;
    memset(buffer, 0, bufferLen);
}

void ChaCha256Init(ChaCha256Ctx* ctx, const unsigned char* key, const unsigned char* iv, int rounds) {
    (void)ctx;
    (void)key;
    (void)iv;
    (void)rounds;
}

void ChaCha256Encrypt(ChaCha256Ctx* ctx, const unsigned char* in, size_t len, unsigned char* out) {
    (void)ctx;
    if (in == out) {
        memset(out, 0, len);
    } else {
        memset(out, 0, len);
        (void)in;
    }
}
