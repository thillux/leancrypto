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
/*
 * This code is derived in parts from the code distribution provided with
 * https://github.com/kokke/tiny-AES-c
 *
 * This is free and unencumbered software released into the public domain.
 */

#include "aes_aesni.h"
#include "aes_c.h"
#include "aes_internal.h"
#include "lc_aes.h"
#include "compare.h"
#include "ret_checkers.h"
#include "visibility.h"

#define LC_EXEC_ONE_TEST(aes_impl)					       \
	if (aes_impl)							       \
		ret += test_decrypt_cbc(aes_impl, #aes_impl)

static const uint8_t key256[] = {
	0x60, 0x3d, 0xeb, 0x10, 0x15, 0xca, 0x71, 0xbe,
	0x2b, 0x73, 0xae, 0xf0, 0x85, 0x7d, 0x77, 0x81,
	0x1f, 0x35, 0x2c, 0x07, 0x3b, 0x61, 0x08, 0xd7,
	0x2d, 0x98, 0x10, 0xa3, 0x09, 0x14, 0xdf, 0xf4
};
static const uint8_t in256[]  = {
	0xf5, 0x8c, 0x4c, 0x04, 0xd6, 0xe5, 0xf1, 0xba,
	0x77, 0x9e, 0xab, 0xfb, 0x5f, 0x7b, 0xfb, 0xd6,
	0x9c, 0xfc, 0x4e, 0x96, 0x7e, 0xdb, 0x80, 0x8d,
	0x67, 0x9f, 0x77, 0x7b, 0xc6, 0x70, 0x2c, 0x7d,
	0x39, 0xf2, 0x33, 0x69, 0xa9, 0xd9, 0xba, 0xcf,
	0xa5, 0x30, 0xe2, 0x63, 0x04, 0x23, 0x14, 0x61,
	0xb2, 0xeb, 0x05, 0xe2, 0xc3, 0x9b, 0xe9, 0xfc,
	0xda, 0x6c, 0x19, 0x07, 0x8c, 0x6a, 0x9d, 0x1b
};
static const uint8_t key192[] = {
	0x8e, 0x73, 0xb0, 0xf7, 0xda, 0x0e, 0x64, 0x52,
	0xc8, 0x10, 0xf3, 0x2b, 0x80, 0x90, 0x79, 0xe5,
	0x62, 0xf8, 0xea, 0xd2, 0x52, 0x2c, 0x6b, 0x7b
};
static const uint8_t in192[]  = {
	0x4f, 0x02, 0x1d, 0xb2, 0x43, 0xbc, 0x63, 0x3d,
	0x71, 0x78, 0x18, 0x3a, 0x9f, 0xa0, 0x71, 0xe8,
	0xb4, 0xd9, 0xad, 0xa9, 0xad, 0x7d, 0xed, 0xf4,
	0xe5, 0xe7, 0x38, 0x76, 0x3f, 0x69, 0x14, 0x5a,
	0x57, 0x1b, 0x24, 0x20, 0x12, 0xfb, 0x7a, 0xe0,
	0x7f, 0xa9, 0xba, 0xac, 0x3d, 0xf1, 0x02, 0xe0,
	0x08, 0xb0, 0xe2, 0x79, 0x88, 0x59, 0x88, 0x81,
	0xd9, 0x20, 0xa9, 0xe6, 0x4f, 0x56, 0x15, 0xcd
};
static const uint8_t key128[] = {
	0x2b, 0x7e, 0x15, 0x16, 0x28, 0xae, 0xd2, 0xa6,
	0xab, 0xf7, 0x15, 0x88, 0x09, 0xcf, 0x4f, 0x3c
};
static const uint8_t in128[]  = {
	0x76, 0x49, 0xab, 0xac, 0x81, 0x19, 0xb2, 0x46,
	0xce, 0xe9, 0x8e, 0x9b, 0x12, 0xe9, 0x19, 0x7d,
	0x50, 0x86, 0xcb, 0x9b, 0x50, 0x72, 0x19, 0xee,
	0x95, 0xdb, 0x11, 0x3a, 0x91, 0x76, 0x78, 0xb2,
	0x73, 0xbe, 0xd6, 0xb8, 0xe3, 0xc1, 0x74, 0x3b,
	0x71, 0x16, 0xe6, 0x9e, 0x22, 0x22, 0x95, 0x16,
	0x3f, 0xf1, 0xca, 0xa1, 0x68, 0x1f, 0xac, 0x09,
	0x12, 0x0e, 0xca, 0x30, 0x75, 0x86, 0xe1, 0xa7
};

static int test_decrypt_cbc_one(struct lc_sym_ctx *ctx,
				const uint8_t *key, size_t keylen, uint8_t *in)
{
	static const uint8_t iv[]  = {
		0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07,
		0x08, 0x09, 0x0a, 0x0b, 0x0c, 0x0d, 0x0e, 0x0f
	};
	static const uint8_t out[] = {
		0x6b, 0xc1, 0xbe, 0xe2, 0x2e, 0x40, 0x9f, 0x96,
		0xe9, 0x3d, 0x7e, 0x11, 0x73, 0x93, 0x17, 0x2a,
		0xae, 0x2d, 0x8a, 0x57, 0x1e, 0x03, 0xac, 0x9c,
		0x9e, 0xb7, 0x6f, 0xac, 0x45, 0xaf, 0x8e, 0x51,
		0x30, 0xc8, 0x1c, 0x46, 0xa3, 0x5c, 0xe4, 0x11,
		0xe5, 0xfb, 0xc1, 0x19, 0x1a, 0x0a, 0x52, 0xef,
		0xf6, 0x9f, 0x24, 0x45, 0xdf, 0x4f, 0x9b, 0x17,
		0xad, 0x2b, 0x41, 0x7b, 0xe6, 0x6c, 0x37, 0x10
	};
	int ret;

	lc_sym_init(ctx);
	CKINT(lc_sym_setkey(ctx, key, keylen));
	CKINT(lc_sym_setiv(ctx, iv, sizeof(iv)));
	lc_sym_decrypt(ctx, in, in, sizeof(out));
	ret = lc_compare(in, out, sizeof(out), "AES-CBC decrypt");

out:
	lc_sym_zero(ctx);
	return ret;
}

static int test_decrypt_cbc(const struct lc_sym *aes, const char *name)
{
	struct lc_sym_ctx *aes_cbc_heap;
	uint8_t in[sizeof(in256)];
	int ret;
	LC_SYM_CTX_ON_STACK(aes_cbc, aes);

	printf("AES CBC ctx %s (%s implementation) len %lu\n", name,
	       aes == lc_aes_cbc_c ? "C" : "accelerated", LC_SYM_CTX_SIZE(aes));

	memcpy(in, in256, sizeof(in256));
	ret = test_decrypt_cbc_one(aes_cbc, key256, sizeof(key256), in);
	lc_sym_zero(aes_cbc);

	memcpy(in, in192, sizeof(in192));
	ret += test_decrypt_cbc_one(aes_cbc, key192, sizeof(key192), in);
	lc_sym_zero(aes_cbc);

	memcpy(in, in128, sizeof(in128));
	ret += test_decrypt_cbc_one(aes_cbc, key128, sizeof(key128), in);
	lc_sym_zero(aes_cbc);

	if (lc_sym_alloc(aes, &aes_cbc_heap))
		return ret + 1;
	memcpy(in, in256, sizeof(in256));
	ret += test_decrypt_cbc_one(aes_cbc_heap, key256, sizeof(key256), in);
	lc_sym_zero_free(aes_cbc_heap);

	return ret;
}

LC_TEST_FUNC(int, main, int argc, char *argv[])
{
	int ret = 0;

	(void)argc;
	(void)argv;

	LC_EXEC_ONE_TEST(lc_aes_cbc);
	LC_EXEC_ONE_TEST(lc_aes_cbc_aesni);
	LC_EXEC_ONE_TEST(lc_aes_cbc_c);

	return ret;
}
