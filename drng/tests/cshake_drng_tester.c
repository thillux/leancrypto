/*
 * Copyright (C) 2022 - 2023, Stephan Mueller <smueller@chronox.de>
 *
 * License: see LICENSE file in root directory
 *
 * THIS SOFTWARE IS PROVIDED ``AS IS'' AND ANY EXPRESS OR IMPLIED
 * WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE, ALL OF
 * WHICH ARE HEREBY DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR BE
 * LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT
 * OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR
 * BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF
 * LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE
 * USE OF THIS SOFTWARE, EVEN IF NOT ADVISED OF THE POSSIBILITY OF SUCH
 * DAMAGE.
 */

#include "alignment.h"
#include "build_bug_on.h"
#include "compare.h"
#include "lc_cshake256_drng.h"
#include "visibility.h"

#define LC_CSHAKE_DRNG_SEED_CUSTOMIZATION_STRING "cSHAKE-DRNG seed"
#define LC_CSHAKE_DRNG_CTX_CUSTOMIZATION_STRING "cSHAKE-DRNG generate"

static int cshake_drng_selftest(struct lc_rng_ctx *cshake_ctx)
{
	struct lc_cshake256_drng_state *state = cshake_ctx->rng_state;
	uint8_t seed[] = {
		0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08,
	};
	static const uint8_t exp1[] = {
		0xf8, 0x1c, 0xba, 0x42, 0x6c, 0x6f, 0xd1, 0x04, 0xd2, 0x30,
		0x2f, 0x33, 0x16, 0x2e, 0xea, 0xfe, 0xf6, 0xcc, 0x1a, 0x61,
		0x31, 0x97, 0xac, 0x65, 0xc2, 0x30, 0xe7, 0x0b, 0xe7, 0xe1,
		0xf1, 0xc0, 0xc8, 0x2c, 0x5a, 0x08, 0xbc, 0xd9, 0x53, 0x12,
		0x9f, 0x1b, 0x21, 0x3f, 0xf9, 0x9f, 0x72, 0xb7, 0x76, 0x56,
		0x81, 0xf5, 0x6f, 0x2b, 0xf5, 0x35, 0x6c, 0x9d, 0x0f, 0x0e,
		0xd3, 0xd6, 0x7e, 0xdb, 0xf5, 0x03, 0xbe, 0x33, 0x66, 0x2b,
		0xb8, 0x23, 0xc3, 0xd9, 0x6f, 0xe7, 0xf6, 0xf9, 0xf8, 0xb8,
		0x31, 0x86, 0x70, 0x0c, 0xe4, 0x4f, 0xcb, 0xb2, 0x8e, 0xc3,
		0x98, 0xbf, 0xd2, 0x59, 0xbf, 0x85, 0xc6, 0x2c, 0x63, 0x5c,
		0x29, 0x98, 0xe6, 0x36, 0x25, 0xc7, 0x33, 0x1d, 0xb0, 0xae,
		0x0c, 0x28, 0x83, 0x78, 0x22, 0x95, 0xe9, 0x26, 0x09, 0xf5,
		0x33, 0xd5, 0x56, 0x19, 0x84, 0xa5, 0x93, 0x83, 0x0d, 0xfc,
		0x3b, 0x90, 0x16, 0x80, 0x87, 0x6c, 0x3e, 0xc5, 0xd2, 0xca,
		0x62, 0x08, 0x9a, 0x9c, 0x0a, 0x90, 0x0f, 0xf3, 0x11, 0x24,
		0x75, 0x75, 0x0d, 0x37, 0xdd, 0x37, 0x77, 0x7b, 0x09, 0x5b,
		0x76, 0x6e, 0xc6, 0xc2, 0xd9, 0x83, 0x73, 0x85, 0xfe, 0x5a,
		0xef, 0x36, 0xed, 0x50, 0x07, 0x42, 0x99, 0x51, 0x54, 0x78,
		0xa9, 0xc6, 0x7d, 0xec, 0xd7, 0x60, 0xb1, 0xb1, 0xed, 0x7f,
		0x4f, 0xf6, 0x9f, 0x12, 0x70, 0xd7, 0xb1, 0x2e, 0x11, 0x55,
		0x0d, 0xd7, 0x04, 0xa1, 0xb1, 0x7a, 0x40, 0x33, 0x71, 0x41,
		0xc2, 0x04, 0x4c, 0x79, 0xfb, 0xd8, 0x5a, 0xd9, 0x29, 0x3c,
		0x83, 0xbd, 0xcc, 0xd3, 0x0b, 0xe0, 0x2b, 0x8e, 0xd8, 0x2f,
		0x65, 0x65, 0xea, 0xed, 0xf0, 0x01, 0xa2, 0xd2, 0xe4, 0x5c,
		0x6b, 0xb2, 0x3e, 0x54, 0xfe, 0xde, 0xd1, 0x70, 0x0d, 0xeb,
		0x3c, 0x7f, 0xf1, 0x56, 0x31, 0xe6, 0xff, 0x5e, 0xfa, 0x0d,
		0xd3, 0x55, 0x93, 0x9e, 0xef, 0xa7, 0x71, 0xda, 0xa9, 0x22,
		0xd6, 0xab, 0x02, 0xa4, 0xeb, 0x0a, 0x2e, 0x07, 0x71, 0x8d,
		0x38, 0x5e, 0x67, 0xb7, 0x3e, 0xe9, 0x0b, 0xe9, 0x6e, 0x0a,
		0xb5, 0x9c, 0xeb, 0x54, 0x87, 0x28, 0x72, 0xc1, 0x8a, 0xd3,
		0xd2, 0x8f, 0x03, 0x27, 0x0a, 0xb9

	};
	static const uint8_t byte = 0xff;
	uint8_t act1[sizeof(exp1)] __align(sizeof(uint32_t));
	uint8_t compare1[LC_CSHAKE256_DRNG_MAX_CHUNK];
	uint8_t encode;
	int ret;
	LC_HASH_CTX_ON_STACK(enc_hash_ctx, lc_cshake256);
	LC_HASH_CTX_ON_STACK(cshake_compare, lc_cshake256);

	BUILD_BUG_ON(sizeof(exp1) < sizeof(compare1));

	lc_rng_seed(cshake_ctx, seed, sizeof(seed), NULL, 0);
	lc_rng_generate(cshake_ctx, NULL, 0, act1, sizeof(act1));
	ret = lc_compare(act1, exp1, sizeof(act1), "CSHAKE DRNG");
	lc_rng_zero(cshake_ctx);

	/* Verfy the generation operation with one CSHAKE call */
	/* Prepare the key */
	lc_rng_seed(cshake_ctx, seed, sizeof(seed), NULL, 0);
	lc_cshake_init(cshake_compare,
		       (uint8_t *)LC_CSHAKE_DRNG_SEED_CUSTOMIZATION_STRING,
		       sizeof(LC_CSHAKE_DRNG_SEED_CUSTOMIZATION_STRING) - 1,
		       NULL, 0);
	lc_hash_update(cshake_compare, seed, sizeof(seed));
	encode = 0;
	lc_hash_update(cshake_compare, &encode, sizeof(encode));
	/* Generate data with one CSHAKE call */
	lc_cshake_final(cshake_compare, compare1, LC_CSHAKE256_DRNG_KEYSIZE);
	ret += lc_compare(compare1, state->key, LC_CSHAKE256_DRNG_KEYSIZE,
			  "CSHAKE DRNG initialization verification");

	/* Verify the generate operation */
	/* Use the already generated state from above */
	lc_cshake_init(cshake_compare,
		       (uint8_t *)LC_CSHAKE_DRNG_CTX_CUSTOMIZATION_STRING,
		       sizeof(LC_CSHAKE_DRNG_CTX_CUSTOMIZATION_STRING) - 1,
		       compare1, LC_CSHAKE256_DRNG_KEYSIZE);
	encode = 2 * 85;
	lc_hash_update(cshake_compare, &encode, sizeof(encode));
	lc_cshake_final(cshake_compare, compare1, sizeof(compare1));
	ret += lc_compare(compare1 + LC_CSHAKE256_DRNG_KEYSIZE, exp1,
			  LC_CSHAKE256_DRNG_MAX_CHUNK -
				  LC_CSHAKE256_DRNG_KEYSIZE,
			  "CSHAKE DRNG generate verification");

	lc_rng_zero(cshake_ctx);

	/*
	 * Verify the seeding operation to generate proper state with large
	 * alpha.
	 */
	/* Seed the XDRBG with an alpha > 84 bytes */
	lc_rng_seed(cshake_ctx, seed, sizeof(seed), exp1, sizeof(exp1));
	/* Prepare the state with native SHAKE operations */
	lc_cshake_init(cshake_compare,
		       (uint8_t *)LC_CSHAKE_DRNG_SEED_CUSTOMIZATION_STRING,
		       sizeof(LC_CSHAKE_DRNG_SEED_CUSTOMIZATION_STRING) - 1,
		       NULL, 0);
	lc_hash_update(cshake_compare, seed, sizeof(seed));
	/* Insert SHA3-512 hash of alpha */
	lc_cshake_init(enc_hash_ctx, NULL, 0, NULL, 0);
	lc_hash_update(enc_hash_ctx, exp1, sizeof(exp1));
	lc_hash_update(enc_hash_ctx, &byte, sizeof(byte));
	lc_cshake_final(enc_hash_ctx, act1, LC_CSHAKE256_DRNG_KEYSIZE);
	lc_hash_zero(enc_hash_ctx);
	lc_hash_update(cshake_compare, act1, LC_CSHAKE256_DRNG_KEYSIZE);
	encode = 0 * 85 + 84;
	lc_hash_update(cshake_compare, &encode, sizeof(encode));
	lc_cshake_final(cshake_compare, compare1, LC_CSHAKE256_DRNG_KEYSIZE);
	ret += lc_compare(compare1, state->key, LC_CSHAKE256_DRNG_KEYSIZE,
			  "CSHAKE DRNG state generation with large alpha");

	lc_rng_zero(cshake_ctx);
	lc_hash_zero(cshake_compare);

	return ret;
}

static int cshake_drng_test(void)
{
	struct lc_rng_ctx *cshake_ctx_heap;
	int ret;
	LC_CSHAKE256_DRNG_CTX_ON_STACK(cshake_ctx);

	ret = cshake_drng_selftest(cshake_ctx);

	if (lc_cshake256_drng_alloc(&cshake_ctx_heap))
		return 1;

	ret += cshake_drng_selftest(cshake_ctx_heap);

	lc_rng_zero_free(cshake_ctx_heap);
	return ret;
}

LC_TEST_FUNC(int, main, int argc, char *argv[])
{
	(void)argc;
	(void)argv;

	return cshake_drng_test();
}
