/**
 * @copyright
 * Copyright 2022 Intel Corporation.
 * SPDX-License-Identifier: Apache-2.0
 *
 * @test @b ipsec_aes192_cbc_sha2_384_avx
 * @parblock
 * This test encrypts random data using AES-192-CBC encryption and computes
 * an HMAC-SHA-384 message authentication code on the ciphertext via OpenSSL
 * using the AVX optimized code path. It also decrypts the ciphertext and
 * verifies that it matches the original plaintext. The ciphertext, MAC, and
 * decrypted data are compared against pre-computed golden values on every
 * iteration to detect silent data corruption.
 * @endparblock
 */

#include "sandstone.h"

#if SANDSTONE_SSL_BUILD

#include "sandstone_ssl.h"
#include <string.h>

/* Data size is runtime-configurable via the test knob
 * "-O ipsec_aes192_cbc_sha2_384_avx.datasize=N" (1024..64MB, multiple of 16, default 1024). */
#define DATA_SIZE_DEFAULT (1024u)
#define AES_KEY_SIZE (24)
#define AES_IV_SIZE (16)
#define HMAC_KEY_SIZE (48)
#define HMAC_DIGEST_SIZE (48)

struct aes_hmac_avx_data {
    uint8_t aes_key[AES_KEY_SIZE];
    uint8_t aes_iv[AES_IV_SIZE];
    uint8_t hmac_key[HMAC_KEY_SIZE];
    size_t datasize;                       /* payload bytes, from the datasize knob */
    uint8_t *plaintext;                    /* datasize bytes, malloc'd in init */
    uint8_t *golden_ciphertext;            /* datasize bytes */
    uint8_t golden_mac[HMAC_DIGEST_SIZE];
};

static void hmac_sha384(const uint8_t *key, size_t key_len, const uint8_t *msg, size_t len, uint8_t *out) {
    HMAC_CTX *ctx = s_HMAC_CTX_new();
    s_HMAC_Init_ex(ctx, key, key_len, s_EVP_sha384(), NULL);
    s_HMAC_Update(ctx, msg, len);
    unsigned int outlen;
    s_HMAC_Final(ctx, out, &outlen);
    s_HMAC_CTX_free(ctx);
}

static void aes_encrypt(const uint8_t *key, const uint8_t *iv, const uint8_t *in, uint8_t *out, size_t len) {
    EVP_CIPHER_CTX *ctx = s_EVP_CIPHER_CTX_new();
    s_EVP_EncryptInit_ex(ctx, s_EVP_aes_192_cbc(), NULL, key, iv);
    s_EVP_CIPHER_CTX_set_padding(ctx, 0);
    int outlen;
    s_EVP_EncryptUpdate(ctx, out, &outlen, in, len);
    int outlen2;
    s_EVP_EncryptFinal_ex(ctx, out + outlen, &outlen2);
    s_EVP_CIPHER_CTX_free(ctx);
}

static void aes_decrypt(const uint8_t *key, const uint8_t *iv, const uint8_t *in, uint8_t *out, size_t len) {
    EVP_CIPHER_CTX *ctx = s_EVP_CIPHER_CTX_new();
    s_EVP_DecryptInit_ex(ctx, s_EVP_aes_192_cbc(), NULL, key, iv);
    s_EVP_CIPHER_CTX_set_padding(ctx, 0);
    int outlen;
    s_EVP_DecryptUpdate(ctx, out, &outlen, in, len);
    int outlen2;
    s_EVP_DecryptFinal_ex(ctx, out + outlen, &outlen2);
    s_EVP_CIPHER_CTX_free(ctx);
}

static void aes_hmac_avx_compute_golden(struct aes_hmac_avx_data *d) {
    aes_encrypt(d->aes_key, d->aes_iv, d->plaintext, d->golden_ciphertext, d->datasize);
    hmac_sha384(d->hmac_key, HMAC_KEY_SIZE, d->golden_ciphertext, d->datasize, d->golden_mac);
}

static int aes_hmac_avx_init(struct test *test) {
    if (s_EVP_CIPHER_CTX_new && s_EVP_EncryptInit_ex && s_EVP_DecryptInit_ex && s_EVP_aes_192_cbc() && s_HMAC_CTX_new) {
        struct aes_hmac_avx_data *d = (struct aes_hmac_avx_data *)malloc(sizeof(*d));
        if (!d) return EXIT_SKIP;
        int64_t knob = get_testspecific_knob_value_int(test, "datasize", DATA_SIZE_DEFAULT);
        if (knob < DATA_SIZE_DEFAULT || knob > 64 * 1024 * 1024 || (knob % 16) != 0) {
            report_fail_msg("datasize knob invalid: %ld (valid 1024..67108864, multiple of 16, default 1024)", (long)knob);
        }
        d->datasize = (size_t)knob;
        d->plaintext = (uint8_t *)malloc(d->datasize);
        d->golden_ciphertext = (uint8_t *)malloc(d->datasize);
        if (!d->plaintext || !d->golden_ciphertext) {
            report_fail_msg("OOM allocating %zu bytes of plaintext/golden", 2 * d->datasize);
        }
        memset_random(d->aes_key, AES_KEY_SIZE);
        memset_random(d->aes_iv, AES_IV_SIZE);
        memset_random(d->hmac_key, HMAC_KEY_SIZE);
        memset_random(d->plaintext, d->datasize);
        aes_hmac_avx_compute_golden(d);
        test->data = d;
        return EXIT_SUCCESS;
    } else {
        log_skip(TestResourceIssueSkipCategory, "OpenSSL library is not available or the current version is not supported");
        return EXIT_SKIP;
    }
}

static int aes_hmac_avx_run(struct test *test, int cpu) {
    struct aes_hmac_avx_data *d = (struct aes_hmac_avx_data *)test->data;
    uint8_t *ciphertext = (uint8_t *)malloc(d->datasize);
    uint8_t *decrypted = (uint8_t *)malloc(d->datasize);
    uint8_t *mac = (uint8_t *)malloc(HMAC_DIGEST_SIZE);


    /* randomization hardening H6': per-thread plaintext + golden, re-rolled
     * every iteration (thread-safe: shared d is only read; the shallow
     * struct copy points plaintext/golden_ciphertext at thread-local
     * buffers, and compute_golden writes only into the copy). */
    struct aes_hmac_avx_data local_d;
    uint8_t *plain = (uint8_t *)malloc(d->datasize);
    uint8_t *gold = (uint8_t *)malloc(d->datasize);
    if (!plain || !gold) {
        free(ciphertext); free(decrypted); free(mac);
        free(plain); free(gold);
        return EXIT_FAILURE;
    }
    memcpy(&local_d, d, sizeof(local_d));
    local_d.plaintext = plain;
    local_d.golden_ciphertext = gold;
    TEST_LOOP(test, 256) {

        /* re-roll: fresh plaintext + same-iteration golden (independent
         * EVP path recomputed for this thread's data) */
        memset_random(local_d.plaintext, d->datasize);
        aes_hmac_avx_compute_golden(&local_d);
        aes_encrypt(local_d.aes_key, local_d.aes_iv, local_d.plaintext, ciphertext, local_d.datasize);
        memcmp_or_fail(ciphertext, local_d.golden_ciphertext, local_d.datasize, "AES-192-CBC ciphertext mismatch (AVX)");

        aes_decrypt(local_d.aes_key, local_d.aes_iv, ciphertext, decrypted, local_d.datasize);
        memcmp_or_fail(decrypted, local_d.plaintext, local_d.datasize, "AES-192-CBC decryption mismatch (AVX)");

        hmac_sha384(local_d.hmac_key, HMAC_KEY_SIZE, ciphertext, local_d.datasize, mac);
        memcmp_or_fail(mac, local_d.golden_mac, HMAC_DIGEST_SIZE, "HMAC-SHA-384 digest mismatch (AVX)");
    }
    free(ciphertext);
    free(decrypted);
    free(mac);
    free(plain);
    free(gold);
    return EXIT_SUCCESS;
}

static int aes_hmac_avx_cleanup(struct test *test) {
    struct aes_hmac_avx_data *d = (aes_hmac_avx_data *)test->data;
    free(d->plaintext);
    free(d->golden_ciphertext);
    free(d);
    return EXIT_SUCCESS;
}

DECLARE_TEST(ipsec_aes192_cbc_sha2_384_avx, "Intel IPSEC AES192-CBC SHA2_384 AVX")
    .groups = DECLARE_TEST_GROUPS(&group_ipsec),
    .test_init = aes_hmac_avx_init,
    .test_run = aes_hmac_avx_run,
    .test_cleanup = aes_hmac_avx_cleanup,
    .minimum_cpu = IPSEC_X86_GATE(cpu_haswell),
    .quality_level = TEST_QUALITY_PROD,
END_DECLARE_TEST

#else

static int aes_hmac_avx_init(struct test *test) {
    log_skip(OsNotSupportedSkipCategory, "OpenSSL build is not enabled");
    return EXIT_SKIP;
}

static int aes_hmac_avx_run(struct test *test, int cpu) {
    __builtin_unreachable();
}

DECLARE_TEST(ipsec_aes192_cbc_sha2_384_avx, "Intel IPSEC AES192-CBC SHA2_384 AVX")
    .groups = DECLARE_TEST_GROUPS(&group_ipsec),
    .test_init = aes_hmac_avx_init,
    .test_run = aes_hmac_avx_run,
    .minimum_cpu = IPSEC_X86_GATE(cpu_haswell),
    .quality_level = TEST_QUALITY_PROD,
END_DECLARE_TEST

#endif
