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

#include "lc_ascon_hash.h"
#include "build_bug_on.h"
#include "compare.h"
#include "visibility.h"

#include "ascon_avx512.h"
#include "ascon_arm_neon.h"
#include "ascon_c.h"

#define LC_EXEC_ONE_TEST(ascon_impl)                                           \
	if (ascon_impl)                                                        \
	ret += _ascon_sqeeze_more_tester(ascon_impl, #ascon_impl)

static int _ascon_sqeeze_more_tester(const struct lc_hash *ascon,
				     const char *name)
{
	static const uint8_t msg[] = { 0x00, 0x01, 0x02, 0x03 };
	static const uint8_t exp[] = {
		0x66, 0xFB, 0x74, 0x17, 0x47, 0x82, 0xAF, 0xED,
		0x89, 0x84, 0x78, 0xAA, 0x72, 0x90, 0x58, 0xD5,
		0xC3, 0x0A, 0xF1, 0x9A, 0xF2, 0xF5, 0xD4, 0xE1,
		0xCE, 0x65, 0xCD, 0x32, 0x05, 0x94, 0xEF, 0x66
	};
	uint8_t act[sizeof(exp)];
	size_t i, len = sizeof(exp);
	uint8_t *act_p;
	int ret;
	LC_HASH_CTX_ON_STACK(ctx, ascon);

	printf("hash ctx %s (%s implementation) len %lu\n", name,
	       ascon == lc_ascon_xof_c ? "C" : "accelerated",
	       LC_HASH_CTX_SIZE(ascon));

	for (i = 1; i <= sizeof(exp); i++) {
		act_p = act;
		lc_hash_init(ctx);
		lc_hash_update(ctx, msg, sizeof(msg));
		lc_hash_set_digestsize(ctx, i);

		for (len = sizeof(exp); len > 0;
		     len -= lc_hash_digestsize(ctx),
		     act_p += lc_hash_digestsize(ctx)) {
			if (len < lc_hash_digestsize(ctx))
				lc_hash_set_digestsize(ctx, len);

			lc_hash_final(ctx, act_p);
		}
		ret = lc_compare(act, exp, sizeof(act), "Ascon sqeeze more");
		lc_hash_zero(ctx);

		if (ret) {
			printf("round %zu\n", i);
			return ret;
		}
	}

	return ret;
}

static int ascon_sqeeze_more_tester(void)
{
	int ret = 0;

	LC_EXEC_ONE_TEST(lc_ascon_xof);
	LC_EXEC_ONE_TEST(lc_ascon_xof_c);
	LC_EXEC_ONE_TEST(lc_ascon_xof_avx512);
	LC_EXEC_ONE_TEST(lc_ascon_xof_arm_neon);

	return ret;
}

LC_TEST_FUNC(int, main, int argc, char *argv[])
{
	(void)argc;
	(void)argv;
	return ascon_sqeeze_more_tester();
}
