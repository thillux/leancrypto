/* XDRBG with XOF256
 *
 * Copyright (C) 2023 - 2024, Stephan Mueller <smueller@chronox.de>
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

#include "build_bug_on.h"
#include "lc_memcmp_secure.h"
#include "lc_xdrbg.h"
#include "math_helper.h"
#include "timecop.h"
#include "visibility.h"
#include "xdrbg_internal.h"

/*********************************** Helper ***********************************/

static inline uint8_t lc_xdrbg_initially_seeded(
	struct lc_xdrbg_drng_state *state)
{
	return !!(state->status & LC_XDRBG_DRNG_INITIALLY_SEEDED);
}

static inline uint8_t lc_xdrbg_keysize(struct lc_xdrbg_drng_state *state)
{
	return state->status & LC_XDRBG_DRNG_KEYSIZE_MASK;
}


static inline void lc_xdrbg_xof_final(struct lc_hash_ctx *xof_ctx,
				      uint8_t *digest, size_t digest_len)
{
	lc_hash_set_digestsize(xof_ctx, digest_len);
	lc_hash_final(xof_ctx, digest);
}

/* Maximum size of the input data to calculate the encode value */
#define LC_XDRBG_DRNG_ENCODE_LENGTH 84
#define LC_XDRBG_DRNG_ENCODE_N(x) (x * 85)

/*
 * The encoding is based on the XDRBG paper appendix B.2 with the following
 * properties:
 *
 *   * length of the hash is set to be equal to |V|
 */
static void lc_xdrbg_drng_encode(struct lc_hash_ctx *xof_ctx, const uint8_t n,
				 const uint8_t *alpha, size_t alphalen)
{
	uint8_t encode;

	/* Ensure the prerequisite hash size <= 84 holds. */
	BUILD_BUG_ON(LC_XDRBG256_DRNG_KEYSIZE > LC_XDRBG_DRNG_ENCODE_LENGTH);

	/*
	 * Only consider up to 84 left-most bytes of alpha. According to
	 * the XDRBG specification appendix B:
	 *
	 * """
	 * This encoding is efficient and flexible, but does require that the
	 * additional input string is no longer than 84 bytes–a constraint that
	 * seems very easy to manage in practice.
	 *
	 * For example, IPV6 addresses and GUIDs are 16 bytes long, Ethernet
	 * addresses are 12 bytes long, and the most demanding requirement for
	 * unique randomly-generated device identifiers can be met with a
	 * 32-byte random value. This is the encoding we recommend for XDRBG.
	 * """
	 */
	if (alphalen > 84)
		alphalen = 84;

	/* Encode the length. */
	encode = (uint8_t)(n + alphalen);

	/* Insert alpha and encode into the hash context. */
	lc_hash_update(xof_ctx, alpha, alphalen);
	lc_hash_update(xof_ctx, &encode, 1);

#if 0
	/*
	 * The alpha is larger than the allowed size - perform hashing of
	 * alpha together with its size encoding.
	 */
	static const uint8_t byte = 0xff;
	LC_HASH_CTX_ON_STACK(enc_hash_ctx, LC_XDRBG256_DRNG_HASH_TYPE);
	uint8_t encode[LC_XDRBG256_DRNG_KEYSIZE + 1];

	/* Hash alpha with the XOF. */
	lc_hash_init(enc_hash_ctx);
	lc_hash_update(enc_hash_ctx, alpha, alphalen);
	lc_hash_update(enc_hash_ctx, &byte, sizeof(byte));
	xdrbg256_xof_final(enc_hash_ctx, encode, LC_XDRBG256_DRNG_KEYSIZE);
	lc_hash_zero(enc_hash_ctx);

	/* Encode the length */
	encode[LC_XDRBG256_DRNG_KEYSIZE] = (uint8_t)((n * 85) + 84);

	/*
	 * The buffer encode contains the concatentation of
	 * h(alpha) || (n * (hash_length + 1) + hash_length)
	 */
	lc_hash_update(xof_ctx, encode, sizeof(encode));
#endif

	/*
	 * Zeroization of encode is not considered to be necessary as alpha is
	 * considered to be known string.
	 */
}

/*
 * Fast-key-erasure initialization of the XOF context. The caller must
 * securely dispose of the initialized XOF context. Additional data
 * can be squeezed from the state using lc_hash_final.
 *
 * This function initializes the XOF context that can later be used to squeeze
 * random bits out of the XOF context. The initialization happens from the key
 * found in the state. Before any random bits can be created, the first 512
 * output bits that are generated is used to overwrite the key. This implies
 * an automatic backtracking resistance as the next round to generate random
 * numbers uses the already updated key.
 *
 * When this function completes, initialized XOF context can now be used
 * to generate random bits.
 */
static void lc_xdrbg_drng_fke_init_ctx(struct lc_xdrbg_drng_state *state,
				       struct lc_hash_ctx *xof_ctx,
				       const uint8_t *alpha, size_t alphalen)
{
	uint8_t keysize = lc_xdrbg_keysize(state);

	lc_hash_init(xof_ctx);

	/* Insert V' into the XOF */
	lc_hash_update(xof_ctx, state->v, keysize);

	/* Insert alpha into the XOF state together with its encoding. */
	lc_xdrbg_drng_encode(xof_ctx, LC_XDRBG_DRNG_ENCODE_N(2), alpha,
			     alphalen);

	/* Generate the V to store in the state and overwrite V'. */
	lc_xdrbg_xof_final(xof_ctx, state->v, keysize);
}

/********************************** XDRB256 ***********************************/

/*
 * Generating random bits is performed by initializing a transient XOF state
 * with the key found in state. The initialization implies that the key in
 * the state variable is already updated before random bits are generated.
 *
 * The random bits are generated by performing a XOF final operation. The
 * generation operation is chunked to ensure that the fast-key-erasure updates
 * the key when large quantities of random bits are generated.
 *
 * This function implements the following functions from Algorithm 2  of the
 * XDRBG specification:
 *
 *   * GENERATE
 */
static int lc_xdrbg_drng_generate(void *_state, const uint8_t *alpha,
				  size_t alphalen, uint8_t *out, size_t outlen)
{
	struct lc_xdrbg_drng_state *state = _state;

	if (!state)
		return -EINVAL;

	LC_HASH_CTX_ON_STACK(xof_ctx, state->xof);

	while (outlen) {
		size_t todo = min_size(outlen, state->chunksize);

		/*
		 * Instantiate XOF with V', and alpha with its encoding,
		 * and generate V.
		 */
		lc_xdrbg_drng_fke_init_ctx(state, xof_ctx, alpha, alphalen);

		/* Generate the requested amount of output bits */
		lc_xdrbg_xof_final(xof_ctx, out, todo);

		/* Timecop: out is not sensitive for side channels. */
		unpoison(out, todo);

		out += todo;
		outlen -= todo;
	}

	/* V is already in place. */

	/* Clear the XOF state which is not needed any more. */
	lc_hash_zero(xof_ctx);

	return 0;
}

/*
 * The DRNG is seeded by initializing a fast-key-erasure XOF context and add
 * the key into the XOF state. The XOF final operation replaces the key in
 * state.
 *
 * This function implements the following functions from Algorithm 2 of the
 * XDRBG specification:
 *
 *  * INSTANTIATE: The state is empty (either freshly allocated or zeroized with
 *                 lc_xdrbg_drng_zero). In particular state->state has
 *		   LC_XDRBG256_DRNG_INITIALLY_SEEDED unset.
 *
 *  * RESEED: The state contains a working XDRBG state that was seeded before.
 *            In this case, state->state has LC_XDRBG256_DRNG_INITIALLY_SEEDED
 *	      set.
 */
static int lc_xdrbg_drng_seed(void *_state, const uint8_t *seed, size_t seedlen,
			      const uint8_t *alpha, size_t alphalen)
{
	static int tested = 0;
	struct lc_xdrbg_drng_state *state = _state;
	uint8_t keysize, initially_seeded;

	/* Timecop: Seed is sensitive. */
	poison(seed, seedlen);

	if (!state)
		return -EINVAL;

	LC_HASH_CTX_ON_STACK(xof_ctx, state->xof);

	initially_seeded = lc_xdrbg_initially_seeded(state);
	keysize = lc_xdrbg_keysize(state);

	if (keysize == LC_XDRBG256_DRNG_KEYSIZE)
		xdrbg256_drng_selftest(&tested, "XOF DRNG");
	else
		xdrbg128_drng_selftest(&tested, "Ascon DRNG");

	lc_hash_init(xof_ctx);

	/*
	 * During reseeding, insert V' into the XOF state. During initial
	 * seeding, V' does not yet exist and thus is not considered.
	 */
	if (initially_seeded)
		lc_hash_update(xof_ctx, state->v, keysize);
	else
		state->status |= LC_XDRBG_DRNG_INITIALLY_SEEDED;

	/* Insert the seed data into the XOF state. */
	lc_hash_update(xof_ctx, seed, seedlen);

	/* Insert alpha into the XOF state together with its encoding. */
	lc_xdrbg_drng_encode(xof_ctx,
			     LC_XDRBG_DRNG_ENCODE_N(initially_seeded), alpha,
			     alphalen);

	/* Generate the V to store in the state and overwrite V'. */
	lc_xdrbg_xof_final(xof_ctx, state->v, keysize);

	/* Clear the XOF state which is not needed any more. */
	lc_hash_zero(xof_ctx);

	return 0;
}

static void lc_xdrbg_drng_zero(void *_state)
{
	struct lc_xdrbg_drng_state *state = _state;

	if (!state)
		return;

	state->status &= (uint8_t)~LC_XDRBG_DRNG_INITIALLY_SEEDED;
	lc_memset_secure(state->v, 0, lc_xdrbg_keysize(state));
}

static const struct lc_rng _lc_xdrbg_drng = {
	.generate = lc_xdrbg_drng_generate,
	.seed = lc_xdrbg_drng_seed,
	.zero = lc_xdrbg_drng_zero,
};
LC_INTERFACE_SYMBOL(const struct lc_rng *, lc_xdrbg_drng) = &_lc_xdrbg_drng;
