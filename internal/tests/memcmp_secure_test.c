/*
 * Copyright (C) 2022, Stephan Mueller <smueller@chronox.de>
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

#include <limits.h>
#include <time.h>

#include "binhexbin.h"
#include "lc_hash_drbg.h"
#include "memcmp_secure.h"
#include "memory_support.h"

static int memcmp_secure_tester(void)
{
	struct workspace {
		uint8_t a[65536], b[65536];
	};
	LC_DRBG_HASH_CTX_ON_STACK(drbg);
	uint8_t *ap, *bp;
	time_t now = time(NULL);
	unsigned int i;
	int ret = 1;
	unsigned short rnd = 0, add = 0;
	LC_DECLARE_MEM(ws, struct workspace, sizeof(uint64_t));

	if (lc_rng_seed(drbg, (uint8_t *)&now, sizeof(now), NULL, 0))
		return 1;

	for (i = 0; i < 10000; i++) {
		if (lc_rng_generate(drbg, NULL, 0,
				     (uint8_t *)&rnd, sizeof(rnd)) < 0) {
			printf("error in generating random number\n");
			goto out;
		}

		if (rnd == 0)
			continue;

		if (rnd < (1<<(sizeof(rnd) << 3)) - 4)
			add = i % 4;
		else
			add = 0;

		ap = ws->a + add;
		bp = ws->b + add;

		if (lc_rng_generate(drbg, NULL, 0, ap, rnd) < 0)
			goto out;

		if (lc_rng_generate(drbg, NULL, 0, bp, rnd) < 0)
			goto out;

		if (memcmp_secure(ap, rnd, ap, rnd)) {
			bin2print(ap, rnd, stderr,
				  "Error comparing identical values");
			printf("Offset %u\n", add);
			goto out;
		}

		if (!memcmp_secure(ap, rnd, bp, rnd)) {
			bin2print(ap, rnd, stderr,
				  "Error comparing different values");
			bin2print(bp, rnd, stderr,
				  "Error comparing different values");
			printf("Offset %u\n", add);
			goto out;
		}
	}

	if (!memcmp_secure(ws->a, (rnd == 0) ? (rnd + 1) : (rnd - 1),
			   ws->b, rnd)) {
		bin2print(ws->a, (unsigned short)(rnd + add), stderr,
			  "Error comparing different values of different sizes");
		bin2print(ws->b, rnd, stderr,
			  "Error comparing different values of different sizes");
		goto out;
	}

	ret = 0;

out:
	lc_rng_zero(drbg);
	LC_RELEASE_MEM(ws);
	return ret;
}

int main(int argc, char *argv[])
{
	(void)argc;
	(void)argv;
	return memcmp_secure_tester();
}
