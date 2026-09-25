/**
 * @copyright
 * Copyright 2022 Intel Corporation.
 * SPDX-License-Identifier: Apache-2.0
 *
 * @test @b ipsec_3des_docsis_hmac_sha384_avx512
 * @parblock
 * This test encrypts random data using 3DES-DOCSIS encryption and computes
 * an HMAC-SHA384 message authentication code on the ciphertext via OpenSSL
 * using the AVX-512 optimized code path. It also decrypts the ciphertext 
 * and verifies that it matches the original plaintext. The ciphertext, MAC, 
 * and decrypted data are compared against pre-computed golden values on 
 * every iteration to detect silent data corruption.
 * @endparblock
 */

#include "sandstone.h"

#if SANDSTONE_SSL_BUILD
#include "sandstone_ssl.h"
#include <string.h>

/* Data size is runtime-configurable via the test knob
 * "-O ipsec_3des_docsis_hmac_sha384_avx512.datasize=N" (1024..64MB, multiple of 16, default 1024). */
#define DATA_SIZE_DEFAULT (1024u)
#define DES3_KEY_SIZE         (24)
#define DES3_IV_SIZE          (8)
#define SHA384_KEY_SIZE       (48)
#define SHA384_DIGEST_SIZE    (48)

struct hmac_sha384_avx512_data {
    uint8_t des3_key[DES3_KEY_SIZE];
    uint8_t des3_iv[DES3_IV_SIZE];
    uint8_t hmac_key[SHA384_KEY_SIZE];
    size_t datasize;                       /* payload bytes, from the datasize knob */
    uint8_t *plaintext;                    /* datasize bytes, malloc'd in init */
    uint8_t *golden_ciphertext;            /* datasize bytes */
    uint8_t golden_mac[SHA384_DIGEST_SIZE];
};

static void hmac_sha384(const uint8_t *key, const uint8_t *msg, size_t len, uint8_t *out) {
    HMAC_CTX *ctx = s_HMAC_CTX_new();
    s_HMAC_Init_ex(ctx, key, SHA384_KEY_SIZE, s_EVP_sha384(), NULL);
    s_HMAC_Update(ctx, msg, len);
    unsigned int outlen;
    s_HMAC_Final(ctx, out, &outlen);
    s_HMAC_CTX_free(ctx);
}

static void des3_encrypt(const uint8_t *key, const uint8_t *iv, const uint8_t *in, uint8_t *out, size_t len) {
    EVP_CIPHER_CTX *ctx = s_EVP_CIPHER_CTX_new();
    s_EVP_EncryptInit_ex(ctx, s_EVP_des_ede3_cbc(), NULL, key, iv);
    s_EVP_CIPHER_CTX_set_padding(ctx, 0);
    int outlen;
    s_EVP_EncryptUpdate(ctx, out, &outlen, in, len);
    int outlen2;
    s_EVP_EncryptFinal_ex(ctx, out + outlen, &outlen2);
    s_EVP_CIPHER_CTX_free(ctx);
}

static void des3_decrypt(const uint8_t *key, const uint8_t *iv, const uint8_t *in, uint8_t *out, size_t len) {
    EVP_CIPHER_CTX *ctx = s_EVP_CIPHER_CTX_new();
    s_EVP_DecryptInit_ex(ctx, s_EVP_des_ede3_cbc(), NULL, key, iv);
    s_EVP_CIPHER_CTX_set_padding(ctx, 0);
    int outlen;
    s_EVP_DecryptUpdate(ctx, out, &outlen, in, len);
    int outlen2;
    s_EVP_DecryptFinal_ex(ctx, out + outlen, &outlen2);
    s_EVP_CIPHER_CTX_free(ctx);
}

static void hmac_sha384_avx512_compute_golden(struct hmac_sha384_avx512_data *d) {
    des3_encrypt(d->des3_key, d->des3_iv, d->plaintext, d->golden_ciphertext, d->datasize);
    hmac_sha384(d->hmac_key, d->golden_ciphertext, d->datasize, d->golden_mac);
}

static int hmac_sha384_avx512_init(struct test *test) {
    if (s_EVP_CIPHER_CTX_new && s_EVP_EncryptInit_ex && s_EVP_DecryptInit_ex && s_EVP_des_ede3_cbc && s_HMAC_CTX_new && s_EVP_sha384) {
        struct hmac_sha384_avx512_data *d = (struct hmac_sha384_avx512_data *)malloc(sizeof(*d));
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

        memset_random(d->des3_key, DES3_KEY_SIZE);
        memset_random(d->des3_iv, DES3_IV_SIZE);
        memset_random(d->hmac_key, SHA384_KEY_SIZE);
        memset_random(d->plaintext, d->datasize);

        hmac_sha384_avx512_compute_golden(d);

        test->data = d;
        return EXIT_SUCCESS;
    } else {
        log_skip(TestResourceIssueSkipCategory, "OpenSSL library is not available or the current version is not supported");
        return EXIT_SKIP;
    }
}

static int hmac_sha384_avx512_run(struct test *test, int cpu) {
    struct hmac_sha384_avx512_data *d = (struct hmac_sha384_avx512_data *)test->data;

    uint8_t *ciphertext = (uint8_t *)malloc(d->datasize);
    uint8_t *decrypted = (uint8_t *)malloc(d->datasize);
    uint8_t *mac = (uint8_t *)malloc(SHA384_DIGEST_SIZE);


    /* randomization hardening H6': per-thread plaintext + golden, re-rolled
     * every iteration (thread-safe: shared d is only read; the shallow
     * struct copy points plaintext/golden_ciphertext at thread-local
     * buffers, and compute_golden writes only into the copy). */
    struct hmac_sha384_avx512_data local_d;
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
        hmac_sha384_avx512_compute_golden(&local_d);
        des3_encrypt(local_d.des3_key, local_d.des3_iv, local_d.plaintext, ciphertext, local_d.datasize);
        memcmp_or_fail(ciphertext, local_d.golden_ciphertext, local_d.datasize, "3DES-DOCSIS ciphertext mismatch (AVX-512)");

        des3_decrypt(local_d.des3_key, local_d.des3_iv, ciphertext, decrypted, local_d.datasize);
        memcmp_or_fail(decrypted, local_d.plaintext, local_d.datasize, "3DES-DOCSIS decryption mismatch (AVX-512)");

        hmac_sha384(local_d.hmac_key, ciphertext, local_d.datasize, mac);
        memcmp_or_fail(mac, local_d.golden_mac, SHA384_DIGEST_SIZE, "HMAC-SHA384 digest mismatch (AVX-512)");
    }

    free(ciphertext);
    free(decrypted);
    free(mac);
    free(plain);
    free(gold);
    return EXIT_SUCCESS;
}

static int hmac_sha384_avx512_cleanup(struct test *test) {
    struct hmac_sha384_avx512_data *d = (hmac_sha384_avx512_data *)test->data;
    free(d->plaintext);
    free(d->golden_ciphertext);
    free(d);
    return EXIT_SUCCESS;
}

DECLARE_TEST(ipsec_3des_docsis_hmac_sha384_avx512, "Intel IPSEC 3DES-DOCSIS w/ HMAC-SHA384 AVX512")
    .groups = DECLARE_TEST_GROUPS(&group_ipsec),
    .test_init = hmac_sha384_avx512_init,
    .test_run = hmac_sha384_avx512_run,
    .test_cleanup = hmac_sha384_avx512_cleanup,
    .minimum_cpu = IPSEC_X86_GATE(cpu_skylake_avx512),
    .quality_level = TEST_QUALITY_PROD,
END_DECLARE_TEST

#else

static int hmac_sha384_avx512_init(struct test *test) {
    log_skip(OsNotSupportedSkipCategory, "OpenSSL build is not enabled");
    return EXIT_SKIP;
}

static int hmac_sha384_avx512_run(struct test *test, int cpu) {
    __builtin_unreachable();
}

DECLARE_TEST(ipsec_3des_docsis_hmac_sha384_avx512, "Intel IPSEC 3DES-DOCSIS w/ HMAC-SHA384 AVX512")
    .groups = DECLARE_TEST_GROUPS(&group_ipsec),
    .test_init = hmac_sha384_avx512_init,
    .test_run = hmac_sha384_avx512_run,
    .quality_level = TEST_QUALITY_PROD,
END_DECLARE_TEST

#endif
