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
 * https://github.com/pq-crystals/kyber
 *
 * That code is released under Public Domain
 * (https://creativecommons.org/share-your-work/public-domain/cc0/).
 */

#include "kyber_kdf.h"
#include "kyber_kem.h"
#include "kyber_verify.h"
#include "lc_hash.h"
#include "lc_kyber.h"
#include "lc_sha3.h"
#include "ret_checkers.h"
#include "small_stack_support.h"
#include "visibility.h"

int _lc_kyber_keypair(
	struct lc_kyber_pk *pk, struct lc_kyber_sk *sk,
	struct lc_rng_ctx *rng_ctx,
	int (*indcpa_keypair_f)(uint8_t pk[LC_KYBER_INDCPA_PUBLICKEYBYTES],
				uint8_t sk[LC_KYBER_INDCPA_SECRETKEYBYTES],
				struct lc_rng_ctx *rng_ctx))
{
	int ret;

	if (!pk || !sk || !rng_ctx)
		return -EINVAL;

	ret = indcpa_keypair_f(pk->pk, sk->sk, rng_ctx);
	if (ret)
		return ret;

	memcpy(&sk->sk[LC_KYBER_INDCPA_SECRETKEYBYTES], pk->pk,
	       LC_KYBER_INDCPA_PUBLICKEYBYTES);

	lc_hash(lc_sha3_256, pk->pk, LC_KYBER_PUBLICKEYBYTES,
		sk->sk + LC_KYBER_SECRETKEYBYTES - 2 * LC_KYBER_SYMBYTES);

	/* Value z for pseudo-random output on reject */
	return lc_rng_generate(rng_ctx, NULL, 0,
			       sk->sk + LC_KYBER_SECRETKEYBYTES -
				       LC_KYBER_SYMBYTES,
			       LC_KYBER_SYMBYTES);
}

int _lc_kyber_enc(
	struct lc_kyber_ct *ct, uint8_t ss[LC_KYBER_SSBYTES],
	const struct lc_kyber_pk *pk, struct lc_rng_ctx *rng_ctx,
	int (*indcpa_enc_f)(uint8_t c[LC_KYBER_INDCPA_BYTES],
			    const uint8_t m[LC_KYBER_INDCPA_MSGBYTES],
			    const uint8_t pk[LC_KYBER_INDCPA_PUBLICKEYBYTES],
			    const uint8_t coins[LC_KYBER_SYMBYTES]))
{
	uint8_t buf[2 * LC_KYBER_SYMBYTES];
	/* Will contain key, coins */
	uint8_t kr[2 * LC_KYBER_SYMBYTES];
	int ret;

	if (!ct || !ss || !pk || !rng_ctx)
		return -EINVAL;

	CKINT(lc_rng_generate(rng_ctx, NULL, 0, buf, LC_KYBER_SYMBYTES));

	/* Multitarget countermeasure for coins + contributory KEM */
	lc_hash(lc_sha3_256, pk->pk, LC_KYBER_PUBLICKEYBYTES,
		buf + LC_KYBER_SYMBYTES);
	lc_hash(lc_sha3_512, buf, sizeof(buf), kr);

	/* coins are in kr+KYBER_SYMBYTES */
	CKINT(indcpa_enc_f(ct->ct, buf, pk->pk, kr + LC_KYBER_SYMBYTES));

	memcpy(ss, kr, LC_KYBER_SSBYTES);

out:
	lc_memset_secure(buf, 0, sizeof(buf));
	lc_memset_secure(kr, 0, sizeof(kr));
	return ret;
}

int _lc_kyber_enc_kdf(
	struct lc_kyber_ct *ct, uint8_t *ss, size_t ss_len,
	const struct lc_kyber_pk *pk, struct lc_rng_ctx *rng_ctx,
	int (*indcpa_enc_f)(uint8_t c[LC_KYBER_INDCPA_BYTES],
			    const uint8_t m[LC_KYBER_INDCPA_MSGBYTES],
			    const uint8_t pk[LC_KYBER_INDCPA_PUBLICKEYBYTES],
			    const uint8_t coins[LC_KYBER_SYMBYTES]))
{
	uint8_t ss_internal[2 * LC_KYBER_SYMBYTES];
	int ret;

	CKINT(_lc_kyber_enc(ct, ss_internal, pk, rng_ctx, indcpa_enc_f));

	/* calculate KDF from CT */
	lc_hash(lc_sha3_256, ct->ct, LC_KYBER_CIPHERTEXTBYTES,
		ss_internal + LC_KYBER_SYMBYTES);
	/* hash concatenation of pre-k and H(c) to k */
	lc_shake(lc_shake256, ss_internal, sizeof(ss_internal), ss, ss_len);

out:
	lc_memset_secure(ss_internal, 0, sizeof(ss_internal));
	return ret;
}

int _lc_kyber_dec(
	uint8_t ss[LC_KYBER_SSBYTES], const struct lc_kyber_ct *ct,
	const struct lc_kyber_sk *sk,
	int (*indcpa_dec_f)(uint8_t m[LC_KYBER_INDCPA_MSGBYTES],
			    const uint8_t c[LC_KYBER_INDCPA_BYTES],
			    const uint8_t sk[LC_KYBER_INDCPA_SECRETKEYBYTES]),
	int (*indcpa_enc_f)(uint8_t c[LC_KYBER_INDCPA_BYTES],
			    const uint8_t m[LC_KYBER_INDCPA_MSGBYTES],
			    const uint8_t pk[LC_KYBER_INDCPA_PUBLICKEYBYTES],
			    const uint8_t coins[LC_KYBER_SYMBYTES]))
{
	struct workspace {
		uint8_t buf[2 * LC_KYBER_SYMBYTES];
		/* Will contain key, coins */
		uint8_t kr[2 * LC_KYBER_SYMBYTES];
		uint8_t cmp[LC_KYBER_CIPHERTEXTBYTES];
	};
	const uint8_t *pk;
	uint8_t fail;
	int ret;
	LC_DECLARE_MEM(ws, struct workspace, sizeof(uint64_t));

	if (!ss || !ct || !sk) {
		ret = -EINVAL;
		goto out;
	}

	pk = sk->sk + LC_KYBER_INDCPA_SECRETKEYBYTES;

	CKINT(indcpa_dec_f(ws->buf, ct->ct, sk->sk));

	/* Multitarget countermeasure for coins + contributory KEM */
	memcpy(&ws->buf[LC_KYBER_SYMBYTES],
	       &sk->sk[LC_KYBER_SECRETKEYBYTES - 2 * LC_KYBER_SYMBYTES],
	       LC_KYBER_SYMBYTES);
	lc_hash(lc_sha3_512, ws->buf, sizeof(ws->buf), ws->kr);

	/* coins are in kr + KYBER_SYMBYTES */
	CKINT(indcpa_enc_f(ws->cmp, ws->buf, pk, ws->kr + LC_KYBER_SYMBYTES));

	fail = verify(ct->ct, ws->cmp, LC_KYBER_CIPHERTEXTBYTES);

	/* Compute rejection key */
	kyber_shake256_rkprf(
		ss, sk->sk + LC_KYBER_SECRETKEYBYTES - LC_KYBER_SYMBYTES,
		ct->ct);

	/* Copy true key to return buffer if fail is false */
	cmov(ss, ws->kr, LC_KYBER_SSBYTES, !fail);

out:
	LC_RELEASE_MEM(ws);
	return ret;
}

int _lc_kyber_dec_kdf(
	uint8_t *ss, size_t ss_len, const struct lc_kyber_ct *ct,
	const struct lc_kyber_sk *sk,
	int (*indcpa_dec_f)(uint8_t m[LC_KYBER_INDCPA_MSGBYTES],
			    const uint8_t c[LC_KYBER_INDCPA_BYTES],
			    const uint8_t sk[LC_KYBER_INDCPA_SECRETKEYBYTES]),
	int (*indcpa_enc_f)(uint8_t c[LC_KYBER_INDCPA_BYTES],
			    const uint8_t m[LC_KYBER_INDCPA_MSGBYTES],
			    const uint8_t pk[LC_KYBER_INDCPA_PUBLICKEYBYTES],
			    const uint8_t coins[LC_KYBER_SYMBYTES]))
{
	uint8_t ss_internal[2 * LC_KYBER_SYMBYTES];
	int ret;

	CKINT(_lc_kyber_dec(ss_internal, ct, sk, indcpa_dec_f, indcpa_enc_f));

	/* calculate KDF from CT */
	lc_hash(lc_sha3_256, ct->ct, LC_KYBER_CIPHERTEXTBYTES,
		ss_internal + LC_KYBER_SYMBYTES);

	/* hash concatenation of pre-k and H(c) to k */
	lc_shake(lc_shake256, ss_internal, sizeof(ss_internal), ss, ss_len);

out:
	lc_memset_secure(ss_internal, 0, sizeof(ss_internal));
	return ret;
}
