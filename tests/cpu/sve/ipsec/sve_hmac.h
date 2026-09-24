/**
 * @file
 *
 * @copyright
 * Copyright 2025 Intel Corporation.
 * SPDX-License-Identifier: Apache-2.0
 *
 * Multi-stream SVE HMAC layer over the sve_sha_kernels.h compression
 * functions. HMAC(K, m) = H(K^opad || H(K^ipad || m)): each stream's
 * message is [K^ipad block][msg blocks] for the inner hash and
 * [K^opad block][inner digest block] for the outer hash. The first block
 * is identical across streams but the chaining state evolves per stream,
 * so 8 HMACs run in parallel on SVE lanes with identical results to
 * serial HMAC (verified against OpenSSL in the standalone verifier).
 */
#ifndef SVE_HMAC_H
#define SVE_HMAC_H

#include "sve_sha_kernels.h"
#include <cstring>

/* SHA-256 家族 HMAC: 8 条流并行。
 * keys: 8 条流的 HMAC 密钥(每条 HMAC_KEY_SIZE 字节, 可各不相同)。
 * msgs: 8 条消息(各 msg_len 字节, msg_len < 2^32/8)。
 * out: out[s] 收到第 s 条流的 32 字节摘要(大端)。 */
static inline void hmac_sha256_x8(uint8_t out[8][32],
                                  const uint8_t *const keys[8], size_t key_len,
                                  const uint8_t *const msgs[8], size_t msg_len)
{
    /* HMAC 密钥块 = 64 字节: key 补零后与 ipad(0x36)/opad(0x5c) 异或 */
    uint8_t ipad_blk[8][64], opad_blk[8][64];
    for (int s = 0; s < 8; ++s) {
        memset(ipad_blk[s], 0x36, 64);
        memset(opad_blk[s], 0x5c, 64);
        for (size_t i = 0; i < key_len; ++i) {
            ipad_blk[s][i] ^= keys[s][i];
            opad_blk[s][i] ^= keys[s][i];
        }
    }

    /* ---- 内层: inner[s] = SHA256(K^ipad[s] || msgs[s]) ----
     * 每流消息 = ipad 块(64B) + msg(msg_len) → padding 后总块数 */
    size_t body = 64 + msg_len;
    size_t padded = ((body + 9 + 63) / 64) * 64;
    uint32_t nblocks = (uint32_t)(padded / 64);

    /* 组装 8 条完整消息(含 padding+长度) */
    uint8_t full[8][ /* padded */ 64 * 32];  /* 支持到 2048B 消息; 测试用 1024 */
    for (int s = 0; s < 8; ++s) {
        memcpy(full[s], ipad_blk[s], 64);
        memcpy(full[s] + 64, msgs[s], msg_len);
        memset(full[s] + body, 0, padded - body);
        full[s][body] = 0x80;
        uint64_t bits = (uint64_t)body * 8;
        for (int i = 0; i < 8; ++i)
            full[s][padded - 1 - i] = (uint8_t)(bits >> (8 * i));
    }

    const uint8_t *mp[8];
    for (int s = 0; s < 8; ++s) mp[s] = full[s];
    uint32_t st[8][8];
    sha256_compress_x8(st, mp, nblocks, sha256_iv);

    /* ---- 外层: out[s] = SHA256(K^opad[s] || inner_digest[s](32B)) ----
     * 消息 = opad 块(64) + 32 字节摘要 → 2 块 */
    for (int s = 0; s < 8; ++s) {
        memcpy(full[s], opad_blk[s], 64);
        for (int w = 0; w < 8; ++w) {
            uint32_t v = st[w][s];
            full[s][64 + w*4 + 0] = (uint8_t)(v >> 24);
            full[s][64 + w*4 + 1] = (uint8_t)(v >> 16);
            full[s][64 + w*4 + 2] = (uint8_t)(v >> 8);
            full[s][64 + w*4 + 3] = (uint8_t)(v);
        }
        /* 96 字节 → padding 到 128 (2 块) */
        memset(full[s] + 96, 0, 128 - 96);
        full[s][96] = 0x80;
        uint64_t bits = 96 * 8;
        for (int i = 0; i < 8; ++i)
            full[s][127 - i] = (uint8_t)(bits >> (8 * i));
    }
    uint32_t st2[8][8];
    sha256_compress_x8(st2, mp, 2, sha256_iv);

    for (int s = 0; s < 8; ++s)
        for (int w = 0; w < 8; ++w) {
            uint32_t v = st2[w][s];
            out[s][w*4 + 0] = (uint8_t)(v >> 24);
            out[s][w*4 + 1] = (uint8_t)(v >> 16);
            out[s][w*4 + 2] = (uint8_t)(v >> 8);
            out[s][w*4 + 3] = (uint8_t)(v);
        }
}

/* SHA-512 家族 HMAC: 4 条流并行 (128 字节密钥块, 64 字节摘要) */
static inline void hmac_sha512_x4(uint8_t out[4][64],
                                  const uint8_t *const keys[4], size_t key_len,
                                  const uint8_t *const msgs[4], size_t msg_len)
{
    uint8_t ipad_blk[4][128], opad_blk[4][128];
    for (int s = 0; s < 4; ++s) {
        memset(ipad_blk[s], 0x36, 128);
        memset(opad_blk[s], 0x5c, 128);
        for (size_t i = 0; i < key_len; ++i) {
            ipad_blk[s][i] ^= keys[s][i];
            opad_blk[s][i] ^= keys[s][i];
        }
    }

    size_t body = 128 + msg_len;
    size_t padded = ((body + 17 + 127) / 128) * 128;
    uint32_t nblocks = (uint32_t)(padded / 128);

    uint8_t full[4][128 * 16];  /* 支持到 2048B 消息 */
    for (int s = 0; s < 4; ++s) {
        memcpy(full[s], ipad_blk[s], 128);
        memcpy(full[s] + 128, msgs[s], msg_len);
        memset(full[s] + body, 0, padded - body);
        full[s][body] = 0x80;
        uint64_t bits = (uint64_t)body * 8;
        for (int i = 0; i < 8; ++i) full[s][padded - 8 + i] = 0;
        for (int i = 0; i < 8; ++i) full[s][padded - 1 - i] = (uint8_t)(bits >> (8 * i));
    }

    const uint8_t *mp[4];
    for (int s = 0; s < 4; ++s) mp[s] = full[s];
    uint64_t st[8][4];
    sha512_compress_x4(st, mp, nblocks, sha512_iv);

    for (int s = 0; s < 4; ++s) {
        memcpy(full[s], opad_blk[s], 128);
        for (int w = 0; w < 8; ++w)
            for (int k = 0; k < 8; ++k)
                full[s][128 + w*8 + k] = (uint8_t)(st[w][s] >> (56 - 8*k));
        /* 128+64=192 → padding 到 256 (2 块) */
        memset(full[s] + 192, 0, 256 - 192);
        full[s][192] = 0x80;
        uint64_t bits = 192 * 8;
        for (int i = 0; i < 8; ++i) full[s][255 - i] = (uint8_t)(bits >> (8 * i));
    }
    uint64_t st2[8][4];
    sha512_compress_x4(st2, mp, 2, sha512_iv);

    for (int s = 0; s < 4; ++s)
        for (int w = 0; w < 8; ++w)
            for (int k = 0; k < 8; ++k)
                out[s][w*8 + k] = (uint8_t)(st2[w][s] >> (56 - 8*k));
}

/* SHA-1 HMAC: 8 条流并行 (64B 密钥块, 20 字节摘要) */
static inline void hmac_sha1_x8(uint8_t out[8][20],
                                const uint8_t *const keys[8], size_t key_len,
                                const uint8_t *const msgs[8], size_t msg_len)
{
    uint8_t ipad_blk[8][64], opad_blk[8][64];
    for (int s = 0; s < 8; ++s) {
        memset(ipad_blk[s], 0x36, 64);
        memset(opad_blk[s], 0x5c, 64);
        for (size_t i = 0; i < key_len; ++i) {
            ipad_blk[s][i] ^= keys[s][i];
            opad_blk[s][i] ^= keys[s][i];
        }
    }

    size_t body = 64 + msg_len;
    size_t padded = ((body + 9 + 63) / 64) * 64;
    uint32_t nblocks = (uint32_t)(padded / 64);

    uint8_t full[8][64 * 32];
    for (int s = 0; s < 8; ++s) {
        memcpy(full[s], ipad_blk[s], 64);
        memcpy(full[s] + 64, msgs[s], msg_len);
        memset(full[s] + body, 0, padded - body);
        full[s][body] = 0x80;
        uint64_t bits = (uint64_t)body * 8;
        for (int i = 0; i < 8; ++i)
            full[s][padded - 1 - i] = (uint8_t)(bits >> (8 * i));
    }

    const uint8_t *mp[8];
    for (int s = 0; s < 8; ++s) mp[s] = full[s];
    uint32_t st[5][8];
    sha1_compress_x8(st, mp, nblocks);

    for (int s = 0; s < 8; ++s) {
        memcpy(full[s], opad_blk[s], 64);
        for (int w = 0; w < 5; ++w) {
            uint32_t v = st[w][s];
            full[s][64 + w*4 + 0] = (uint8_t)(v >> 24);
            full[s][64 + w*4 + 1] = (uint8_t)(v >> 16);
            full[s][64 + w*4 + 2] = (uint8_t)(v >> 8);
            full[s][64 + w*4 + 3] = (uint8_t)(v);
        }
        /* 64+20=84 → 128 (2 块) */
        memset(full[s] + 84, 0, 128 - 84);
        full[s][84] = 0x80;
        uint64_t bits = 84 * 8;
        for (int i = 0; i < 8; ++i)
            full[s][127 - i] = (uint8_t)(bits >> (8 * i));
    }
    uint32_t st2[5][8];
    sha1_compress_x8(st2, mp, 2);

    for (int s = 0; s < 8; ++s)
        for (int w = 0; w < 5; ++w) {
            uint32_t v = st2[w][s];
            out[s][w*4 + 0] = (uint8_t)(v >> 24);
            out[s][w*4 + 1] = (uint8_t)(v >> 16);
            out[s][w*4 + 2] = (uint8_t)(v >> 8);
            out[s][w*4 + 3] = (uint8_t)(v);
        }
}


/* SHA-224 HMAC: 与 SHA-256 HMAC 同构但用 sha224_iv, 摘要 7 字(28 字节)。 */
static inline void hmac_sha224_x8(uint8_t out[8][28],
                                  const uint8_t *const keys[8], size_t key_len,
                                  const uint8_t *const msgs[8], size_t msg_len)
{
    uint8_t ipad_blk[8][64], opad_blk[8][64];
    for (int s = 0; s < 8; ++s) {
        memset(ipad_blk[s], 0x36, 64);
        memset(opad_blk[s], 0x5c, 64);
        for (size_t i = 0; i < key_len; ++i) {
            ipad_blk[s][i] ^= keys[s][i];
            opad_blk[s][i] ^= keys[s][i];
        }
    }

    size_t body = 64 + msg_len;
    size_t padded = ((body + 9 + 63) / 64) * 64;
    uint32_t nblocks = (uint32_t)(padded / 64);

    uint8_t full[8][64 * 32];
    for (int s = 0; s < 8; ++s) {
        memcpy(full[s], ipad_blk[s], 64);
        memcpy(full[s] + 64, msgs[s], msg_len);
        memset(full[s] + body, 0, padded - body);
        full[s][body] = 0x80;
        uint64_t bits = (uint64_t)body * 8;
        for (int i = 0; i < 8; ++i)
            full[s][padded - 1 - i] = (uint8_t)(bits >> (8 * i));
    }

    const uint8_t *mp[8];
    for (int s = 0; s < 8; ++s) mp[s] = full[s];
    uint32_t st[8][8];
    sha256_compress_x8(st, mp, nblocks, sha224_iv);

    for (int s = 0; s < 8; ++s) {
        memcpy(full[s], opad_blk[s], 64);
        /* SHA-224 摘要是 7 字(28B) —— 外层消息 = 64+28 = 92 字节, 不是 96! */
        for (int w = 0; w < 7; ++w) {
            uint32_t v = st[w][s];
            full[s][64 + w*4 + 0] = (uint8_t)(v >> 24);
            full[s][64 + w*4 + 1] = (uint8_t)(v >> 16);
            full[s][64 + w*4 + 2] = (uint8_t)(v >> 8);
            full[s][64 + w*4 + 3] = (uint8_t)(v);
        }
        memset(full[s] + 92, 0, 128 - 92);
        full[s][92] = 0x80;
        uint64_t bits = 92 * 8;
        for (int i = 0; i < 8; ++i)
            full[s][127 - i] = (uint8_t)(bits >> (8 * i));
    }
    uint32_t st2[8][8];
    sha256_compress_x8(st2, mp, 2, sha224_iv);

    for (int s = 0; s < 8; ++s)
        for (int w = 0; w < 7; ++w) {
            uint32_t v = st2[w][s];
            out[s][w*4 + 0] = (uint8_t)(v >> 24);
            out[s][w*4 + 1] = (uint8_t)(v >> 16);
            out[s][w*4 + 2] = (uint8_t)(v >> 8);
            out[s][w*4 + 3] = (uint8_t)(v);
        }
}

/* SHA-384 HMAC: SHA-512 内核 + sha384_iv, 摘要 6 字(48 字节)。 */
static inline void hmac_sha384_x4(uint8_t out[4][48],
                                  const uint8_t *const keys[4], size_t key_len,
                                  const uint8_t *const msgs[4], size_t msg_len)
{
    uint8_t ipad_blk[4][128], opad_blk[4][128];
    for (int s = 0; s < 4; ++s) {
        memset(ipad_blk[s], 0x36, 128);
        memset(opad_blk[s], 0x5c, 128);
        for (size_t i = 0; i < key_len; ++i) {
            ipad_blk[s][i] ^= keys[s][i];
            opad_blk[s][i] ^= keys[s][i];
        }
    }

    size_t body = 128 + msg_len;
    size_t padded = ((body + 17 + 127) / 128) * 128;
    uint32_t nblocks = (uint32_t)(padded / 128);

    uint8_t full[4][128 * 16];
    for (int s = 0; s < 4; ++s) {
        memcpy(full[s], ipad_blk[s], 128);
        memcpy(full[s] + 128, msgs[s], msg_len);
        memset(full[s] + body, 0, padded - body);
        full[s][body] = 0x80;
        uint64_t bits = (uint64_t)body * 8;
        for (int i = 0; i < 8; ++i) full[s][padded - 8 + i] = 0;
        for (int i = 0; i < 8; ++i) full[s][padded - 1 - i] = (uint8_t)(bits >> (8 * i));
    }

    const uint8_t *mp[4];
    for (int s = 0; s < 4; ++s) mp[s] = full[s];
    uint64_t st[8][4];
    sha512_compress_x4(st, mp, nblocks, sha384_iv);

    for (int s = 0; s < 4; ++s) {
        memcpy(full[s], opad_blk[s], 128);
        /* SHA-384 摘要是 6 字(48B) —— 外层消息 = 128+48 = 176 字节, 不是 192! */
        for (int w = 0; w < 6; ++w)
            for (int k = 0; k < 8; ++k)
                full[s][128 + w*8 + k] = (uint8_t)(st[w][s] >> (56 - 8*k));
        memset(full[s] + 176, 0, 256 - 176);
        full[s][176] = 0x80;
        uint64_t bits = 176 * 8;
        for (int i = 0; i < 8; ++i) full[s][255 - i] = (uint8_t)(bits >> (8 * i));
    }
    uint64_t st2[8][4];
    sha512_compress_x4(st2, mp, 2, sha384_iv);

    for (int s = 0; s < 4; ++s)
        for (int w = 0; w < 6; ++w)
            for (int k = 0; k < 8; ++k)
                out[s][w*8 + k] = (uint8_t)(st2[w][s] >> (56 - 8*k));
}

#endif /* SVE_HMAC_H */

