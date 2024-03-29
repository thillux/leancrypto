/*
 * Copyright (C) 2020 - 2024, Stephan Mueller <smueller@chronox.de>
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
#include "compare.h"
#include "lc_sha3.h"
#include "visibility.h"

#include "sha3_c.h"
#include "sha3_arm_asm.h"
#include "sha3_arm_ce.h"
#include "sha3_arm_neon.h"
#include "sha3_avx2.h"
#include "sha3_avx512.h"
#include "sha3_riscv_asm.h"

#define LC_EXEC_ONE_TEST(sha3_impl)                                            \
	if (sha3_impl)                                                         \
	ret += _keccak_tester(sha3_impl, #sha3_impl)

static int _keccak_tester(const struct lc_hash *sha3_512, const char *name)
{
	static const uint8_t msg[] = { 0x82, 0xD9, 0x19 };
	static const uint8_t exp_zero[] = {
		0xe7, 0xdd, 0xe1, 0x40, 0x79, 0x8f, 0x25, 0xf1, 0x8a, 0x47,
		0xc0, 0x33, 0xf9, 0xcc, 0xd5, 0x84, 0xee, 0xa9, 0x5a, 0xa6,
		0x1e, 0x26, 0x98, 0xd5, 0x4d, 0x49, 0x80, 0x6f, 0x30, 0x47,
		0x15, 0xbd, 0x57, 0xd0, 0x53, 0x62, 0x05, 0x4e, 0x28, 0x8b,
		0xd4, 0x6f, 0x8e, 0x7f, 0x2d, 0xa4, 0x97, 0xff, 0xc4, 0x47,
		0x46, 0xa4, 0xa0, 0xe5, 0xfe, 0x90, 0x76, 0x2e, 0x19, 0xd6,
		0x0c, 0xda, 0x5b, 0x8c, 0x9c, 0x05, 0x19, 0x1b, 0xf7, 0xa6,
		0x30, 0xad, 0x64, 0xfc, 0x8f, 0xd0, 0xb7, 0x5a, 0x93, 0x30,
		0x35, 0xd6, 0x17, 0x23, 0x3f, 0xa9, 0x5a, 0xeb, 0x03, 0x21,
		0x71, 0x0d, 0x26, 0xe6, 0xa6, 0xa9, 0x5f, 0x55, 0xcf, 0xdb,
		0x16, 0x7c, 0xa5, 0x81, 0x26, 0xc8, 0x47, 0x03, 0xcd, 0x31,
		0xb8, 0x43, 0x9f, 0x56, 0xa5, 0x11, 0x1a, 0x2f, 0xf2, 0x01,
		0x61, 0xae, 0xd9, 0x21, 0x5a, 0x63, 0xe5, 0x05, 0xf2, 0x70,
		0xc9, 0x8c, 0xf2, 0xfe, 0xbe, 0x64, 0x11, 0x66, 0xc4, 0x7b,
		0x95, 0x70, 0x36, 0x61, 0xcb, 0x0e, 0xd0, 0x4f, 0x55, 0x5a,
		0x7c, 0xb8, 0xc8, 0x32, 0xcf, 0x1c, 0x8a, 0xe8, 0x3e, 0x8c,
		0x14, 0x26, 0x3a, 0xae, 0x22, 0x79, 0x0c, 0x94, 0xe4, 0x09,
		0xc5, 0xa2, 0x24, 0xf9, 0x41, 0x18, 0xc2, 0x65, 0x04, 0xe7,
		0x26, 0x35, 0xf5, 0x16, 0x3b, 0xa1, 0x30, 0x7f, 0xe9, 0x44,
		0xf6, 0x75, 0x49, 0xa2, 0xec, 0x5c, 0x7b, 0xff, 0xf1, 0xea
	};
	static const uint8_t exp_filled[] = {
		0xc6, 0xaf, 0xcf, 0xa9, 0xab, 0x72, 0x50, 0x86, 0xf0, 0xc5,
		0x45, 0x9e, 0x10, 0xbd, 0xac, 0x72, 0x84, 0x8d, 0x45, 0x00,
		0x64, 0x24, 0xab, 0x3c, 0x9f, 0x98, 0xeb, 0xb9, 0xd2, 0x62,
		0x65, 0x3e, 0x51, 0xd4, 0x26, 0x5b, 0x16, 0xb1, 0xdd, 0x45,
		0xf7, 0x4a, 0x8c, 0xf6, 0x16, 0x42, 0x14, 0xbe, 0x8d, 0x77,
		0x4a, 0xe0, 0x49, 0x0e, 0xbf, 0xbc, 0x4a, 0x5f, 0xa6, 0x47,
		0x45, 0xb0, 0x8e, 0xbf, 0x24, 0xf1, 0xa5, 0x55, 0x86, 0x32,
		0x51, 0x7e, 0x46, 0x75, 0xba, 0xc0, 0x82, 0x74, 0x1b, 0x4b,
		0x9d, 0x3c, 0x75, 0xe1, 0x54, 0x76, 0x50, 0x81, 0x4d, 0xdf,
		0xb8, 0x0d, 0x2c, 0xcf, 0x05, 0xe3, 0x05, 0x93, 0xa0, 0xf3,
		0x33, 0x6e, 0xee, 0xc4, 0x3c, 0xc2, 0xe3, 0x17, 0xda, 0xff,
		0x40, 0x1a, 0x4a, 0x81, 0xb1, 0x28, 0x0a, 0xb4, 0x0a, 0x48,
		0xd1, 0xf9, 0x73, 0xd4, 0x87, 0xc7, 0xa8, 0xec, 0x6e, 0x2e,
		0xe5, 0x08, 0xe1, 0x80, 0x45, 0xab, 0xa4, 0xe6, 0x39, 0x9f,
		0xc8, 0xe6, 0x49, 0xc7, 0x75, 0xd1, 0x6c, 0xa2, 0x8c, 0xa4,
		0xb0, 0xff, 0xcc, 0x7b, 0x68, 0x37, 0x55, 0xf1, 0xd4, 0x37,
		0x34, 0x53, 0xe2, 0xbe, 0x2b, 0x44, 0x5b, 0x61, 0x7b, 0x7b,
		0xa6, 0xea, 0xd3, 0x43, 0x64, 0x7d, 0x93, 0xdc, 0xe9, 0xed,
		0x65, 0xad, 0x3a, 0xfc, 0xca, 0x29, 0x29, 0x2c, 0xcf, 0xd4,
		0x6b, 0x63, 0xa1, 0x14, 0x90, 0x06, 0x55, 0xc9, 0x5a, 0x3e
	};
	uint64_t state[LC_SHA3_STATE_WORDS] __align(LC_HASH_COMMON_ALIGNMENT);
	uint8_t act[LC_SHA3_STATE_SIZE];
	unsigned int i;
	int ret;

	printf("Keccak permutation %s (%s implementation) len\n", name,
	       sha3_512 == lc_sha3_512_c ? "C" : "accelerated");

	memset(state, 0, sizeof(state));
	lc_sponge(sha3_512, state, 0);
	lc_sponge_extract_bytes(sha3_512, state, act, 0, sizeof(act));
	ret = lc_compare(act, exp_zero, LC_SHA3_STATE_SIZE,
			 "Keccak on zero state");

	memset(state, 0, sizeof(state));
	for (i = 0; i < LC_SHA3_STATE_SIZE - sizeof(msg) + 1;
	     i += sizeof(msg)) {
		lc_sponge_add_bytes(sha3_512, state, msg, i, sizeof(msg));
		lc_sponge(sha3_512, state, 0);
	}
	lc_sponge_extract_bytes(sha3_512, state, act, 0, sizeof(act));
	ret += lc_compare(act, exp_filled, LC_SHA3_STATE_SIZE,
			  "Keccak on filled state");

	return ret;
}

static int keccak_tester(void)
{
	int ret = 0;

	LC_EXEC_ONE_TEST(lc_sha3_512);
	LC_EXEC_ONE_TEST(lc_sha3_512_c);
	LC_EXEC_ONE_TEST(lc_sha3_512_arm_asm);
	LC_EXEC_ONE_TEST(lc_sha3_512_arm_ce);
	LC_EXEC_ONE_TEST(lc_sha3_512_arm_neon);
	LC_EXEC_ONE_TEST(lc_sha3_512_avx2);
	LC_EXEC_ONE_TEST(lc_sha3_512_avx512);
	LC_EXEC_ONE_TEST(lc_sha3_512_riscv_asm);

	return ret;
}

LC_TEST_FUNC(int, main, int argc, char *argv[])
{
	(void)argc;
	(void)argv;
	return keccak_tester();
}
