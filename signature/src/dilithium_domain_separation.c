/*
 * Copyright (C) 2024, Stephan Mueller <smueller@chronox.de>
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

#include "dilithium_domain_separation.h"
#include "lc_sha256.h"
#include "lc_sha3.h"
#include "lc_sha512.h"
#include "ret_checkers.h"

/* RFC4055 2.16.840.1.101.3.4.2.1 */
static const uint8_t sha256_oid_der[] = { 0x06, 0x09, 0x60, 0x86, 0x48, 0x01,
					  0x65, 0x03, 0x04, 0x02, 0x01 };

/* RFC4055 2.16.840.1.101.3.4.2.3 */
static const uint8_t sha512_oid_der[] = { 0x06, 0x09, 0x60, 0x86, 0x48, 0x01,
					  0x65, 0x03, 0x04, 0x02, 0x03 };

/* RFC8692 2.16.840.1.101.3.4.2.11 */
static const uint8_t shake128_oid_der[] = { 0x06, 0x09, 0x60, 0x86, 0x48, 0x01,
					    0x65, 0x03, 0x04, 0x02, 0x0B };

/* RFC8692 2.16.840.1.101.3.4.2.11 */
static const uint8_t shake256_oid_der[] = { 0x06, 0x09, 0x60, 0x86, 0x48, 0x01,
					    0x65, 0x03, 0x04, 0x02, 0x0C };

static int dilithium_ph_oids(struct lc_dilithium_ctx *ctx, size_t mlen)
{
	struct lc_hash_ctx *hash_ctx = &ctx->dilithium_hash_ctx;
	int ret = 0;

	/* If no hash is supplied, we have no HashML-DSA */
	if (!ctx->dilithium_prehash_type)
		return 0;

	if (ctx->dilithium_prehash_type == lc_sha256) {
		if (mlen != LC_SHA256_SIZE_DIGEST)
			return -EOPNOTSUPP;
		lc_hash_update(hash_ctx, sha256_oid_der,
			       sizeof(sha256_oid_der));
	} else if (ctx->dilithium_prehash_type == lc_sha512) {
		if (mlen != LC_SHA512_SIZE_DIGEST)
			return -EOPNOTSUPP;
		lc_hash_update(hash_ctx, sha512_oid_der,
			       sizeof(sha512_oid_der));
	} else if (ctx->dilithium_prehash_type == lc_shake128) {
		/* FIPS 204 section 5.4.1 */
		if (mlen != 32)
			return -EOPNOTSUPP;
		lc_hash_update(hash_ctx, shake128_oid_der,
			       sizeof(shake128_oid_der));
	} else if (ctx->dilithium_prehash_type == lc_shake256) {
		/* FIPS 204 section 5.4.1 */
		if (mlen != 64)
			return -EOPNOTSUPP;
		lc_hash_update(hash_ctx, shake256_oid_der,
			       sizeof(shake256_oid_der));
	} else {
		return -EOPNOTSUPP;
	}

	return ret;
}

int dilithium_domain_separation(struct lc_dilithium_ctx *ctx, const uint8_t *m,
				size_t mlen)
{
	uint8_t domainseparation[2];
	struct lc_hash_ctx *hash_ctx;
	int ret = 0;

	hash_ctx = &ctx->dilithium_hash_ctx;

	/* The internal operation skips the domain separation code */
	if (ctx->ml_dsa_internal)
		goto out;

	if (ctx->userctxlen > 255)
		return -EINVAL;

	domainseparation[0] = ctx->dilithium_prehash_type ? 1 : 0;
	domainseparation[1] = (uint8_t)ctx->userctxlen;

	lc_hash_update(hash_ctx, domainseparation, sizeof(domainseparation));
	lc_hash_update(hash_ctx, ctx->userctx, ctx->userctxlen);

	CKINT(dilithium_ph_oids(ctx, mlen));

out:
	lc_hash_update(hash_ctx, m, mlen);
	return ret;
}
