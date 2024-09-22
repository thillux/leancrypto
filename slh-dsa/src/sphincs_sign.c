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
/*
 * This code is derived in parts from the code distribution provided with
 * https://github.com/sphincs/sphincsplus
 *
 * That code is released under Public Domain
 * (https://creativecommons.org/share-your-work/public-domain/cc0/).
 */

#include "cpufeatures.h"
#include "lc_rng.h"
#include "lc_memcmp_secure.h"
#include "small_stack_support.h"
#include "sphincs_type.h"
#include "sphincs_address.h"
#include "sphincs_fors.h"
#include "sphincs_hash.h"
#include "sphincs_internal.h"
#include "sphincs_merkle.h"
#include "sphincs_thash.h"
#include "sphincs_utils.h"
#include "sphincs_wots.h"
#include "ret_checkers.h"
#include "visibility.h"

#include "avx2/sphincs_fors_avx2.h"
#include "avx2/sphincs_merkle_avx2.h"
#include "avx2/sphincs_wots_avx2.h"

struct lc_sphincs_func_ctx {
	merkle_sign_f merkle_sign;
	merkle_gen_root_f merkle_gen_root;
	fors_sign_f fors_sign;
	fors_pk_from_sig_f fors_pk_from_sig;
	wots_pk_from_sig_f wots_pk_from_sig;
	chain_lengths_f chain_lengths;
};

static void lc_sphincs_get_ctx(struct lc_sphincs_func_ctx *ctx)
{
	enum lc_cpu_features feat = lc_cpu_feature_available();

#ifdef LC_HOST_X86_64
	if (feat & LC_CPU_FEATURE_INTEL_AVX2) {
		ctx->merkle_sign = sphincs_merkle_sign_avx2;
		ctx->merkle_gen_root = sphincs_merkle_gen_root_avx2;
		ctx->fors_sign = fors_sign_avx2;
		ctx->fors_pk_from_sig = fors_pk_from_sig_avx2;
		ctx->wots_pk_from_sig = wots_pk_from_sig_avx2;
		ctx->chain_lengths = chain_lengths_avx2;
	} else
#endif /* LC_HOST_X86_64 */
	{
		ctx->merkle_sign = sphincs_merkle_sign_c;
		ctx->merkle_gen_root = sphincs_merkle_gen_root_c;
		ctx->fors_sign = fors_sign_c;
		ctx->fors_pk_from_sig = fors_pk_from_sig_c;
		ctx->wots_pk_from_sig = wots_pk_from_sig_c;
		ctx->chain_lengths = chain_lengths_c;
	}
}

static void lc_sphincs_keypair_from_seed_internal(struct lc_sphincs_pk *pk,
						  struct lc_sphincs_sk *sk)
{
	struct lc_sphincs_func_ctx f_ctx;
	spx_ctx ctx;

	lc_sphincs_get_ctx(&f_ctx);

	/* Initialize SK_SEED, SK_PRF and PUB_SEED from seed. */
	memcpy(pk, sk->pk_seed, sizeof(sk->pk_seed));

	ctx.pub_seed = pk->pk_seed;
	ctx.sk_seed = sk->sk_seed;

	/* Compute root node of the top-most subtree. */
	f_ctx.merkle_gen_root(sk->pk_root, &ctx);

	memcpy(pk->pk_root, sk->pk_root, sizeof(pk->pk_root));
}

/*
 * Generates an LC_SPX key pair given a seed of length
 * Format sk: [SK_SEED || SK_PRF || PUB_SEED || root]
 * Format pk: [PUB_SEED || root]
 */
LC_INTERFACE_FUNCTION(int, lc_sphincs_keypair_from_seed,
		      struct lc_sphincs_pk *pk, struct lc_sphincs_sk *sk,
		      const uint8_t *seed, size_t seedlen)
{
	int ret = 0;

	CKNULL(pk, -EINVAL);
	CKNULL(sk, -EINVAL);

	if (seedlen != LC_SPX_SEEDBYTES)
		return -EINVAL;

	/* Initialize SK_SEED, SK_PRF and PUB_SEED from seed. */
	memcpy(sk, seed, LC_SPX_SEEDBYTES);

	lc_sphincs_keypair_from_seed_internal(pk, sk);

out:
	return ret;
}

/*
 * Generates an LC_SPX key pair.
 * Format sk: [SK_SEED || SK_PRF || PUB_SEED || root]
 * Format pk: [PUB_SEED || root]
 */

LC_INTERFACE_FUNCTION(int, lc_sphincs_keypair, struct lc_sphincs_pk *pk,
		      struct lc_sphincs_sk *sk, struct lc_rng_ctx *rng_ctx)
{
	int ret;

	CKNULL(pk, -EINVAL);
	CKNULL(sk, -EINVAL);

	CKINT(lc_rng_generate(rng_ctx, NULL, 0, (uint8_t *)sk,
			      LC_SPX_SEEDBYTES));
	lc_sphincs_keypair_from_seed_internal(pk, sk);

out:
	return ret;
}

/**
 * Returns an array containing a detached signature.
 */
LC_INTERFACE_FUNCTION(int, lc_sphincs_sign, struct lc_sphincs_sig *sig,
		      const uint8_t *m, size_t mlen,
		      const struct lc_sphincs_sk *sk,
		      struct lc_rng_ctx *rng_ctx)
{
	struct workspace {
		uint8_t optrand[LC_SPX_N];
		uint8_t root[LC_SPX_N];
		uint32_t wots_addr[8];
		uint32_t tree_addr[8];
		uint8_t mhash[LC_SPX_FORS_MSG_BYTES];
	};
	uint32_t i;
	uint64_t tree;
	uint32_t idx_leaf;
	struct lc_sphincs_func_ctx f_ctx;
	spx_ctx ctx;
	const uint8_t *sk_prf = sk->sk_prf;
	const uint8_t *pk = sk->pk_seed;
	uint8_t *wots_sig = sig->sight;
	int ret = 0;
	LC_DECLARE_MEM(ws, struct workspace, sizeof(uint64_t));

	CKNULL(sig, -EINVAL);
	CKNULL(sk, -EINVAL);

	lc_sphincs_get_ctx(&f_ctx);

	ctx.sk_seed = sk->sk_seed;
	ctx.pub_seed = pk;

	set_type(ws->wots_addr, LC_SPX_ADDR_TYPE_WOTS);
	set_type(ws->tree_addr, LC_SPX_ADDR_TYPE_HASHTREE);

	if (rng_ctx) {
		CKINT(lc_rng_generate(rng_ctx, NULL, 0, ws->optrand,
				      sizeof(ws->optrand)));
	} else {
		memset(ws->optrand, 0, sizeof(ws->optrand));
	}

	/* Compute the digest randomization value. */
	gen_message_random(sig->r, sk_prf, ws->optrand, m, mlen);

	/* Derive the message digest and leaf index from R, PK and M. */
	hash_message(ws->mhash, &tree, &idx_leaf, sig->r, pk, m, mlen);

	set_tree_addr(ws->wots_addr, tree);
	set_keypair_addr(ws->wots_addr, idx_leaf);

	/* Sign the message hash using FORS. */
	f_ctx.fors_sign(sig->sigfors, ws->root, ws->mhash, &ctx, ws->wots_addr);

	for (i = 0; i < LC_SPX_D; i++) {
		set_layer_addr(ws->tree_addr, i);
		set_tree_addr(ws->tree_addr, tree);

		copy_subtree_addr(ws->wots_addr, ws->tree_addr);
		set_keypair_addr(ws->wots_addr, idx_leaf);

		f_ctx.merkle_sign(wots_sig, ws->root, &ctx, ws->wots_addr,
				  ws->tree_addr, idx_leaf);
		wots_sig += LC_SPX_WOTS_BYTES + LC_SPX_TREE_HEIGHT * LC_SPX_N;

		/* Update the indices for the next layer. */
		idx_leaf = (tree & ((1 << LC_SPX_TREE_HEIGHT) - 1));
		tree = tree >> LC_SPX_TREE_HEIGHT;
	}

out:
	if (ret && sig)
		lc_memset_secure(sig, 0, sizeof(struct lc_sphincs_sig));
	LC_RELEASE_MEM(ws);
	return ret;
}

/**
 * Verifies a detached signature and message under a given public key.
 */
LC_INTERFACE_FUNCTION(int, lc_sphincs_verify, const struct lc_sphincs_sig *sig,
		      const uint8_t *m, size_t mlen,
		      const struct lc_sphincs_pk *pk)
{
	struct workspace {
		uint8_t root[LC_SPX_N];
		uint8_t leaf[LC_SPX_N];
		uint32_t wots_addr[8];
		uint32_t tree_addr[8];
		uint32_t wots_pk_addr[8];
		uint8_t wots_pk[LC_SPX_WOTS_BYTES];
		uint8_t mhash[LC_SPX_FORS_MSG_BYTES];
	};
	unsigned int i;
	uint64_t tree;
	uint32_t idx_leaf;
	struct lc_sphincs_func_ctx f_ctx;
	spx_ctx ctx;
	const uint8_t *pub_root = pk->pk_root;
	const uint8_t *wots_sig = sig->sight;
	int ret = 0;
	LC_DECLARE_MEM(ws, struct workspace, sizeof(uint64_t));

	CKNULL(sig, -EINVAL);
	CKNULL(pk, -EINVAL);

	lc_sphincs_get_ctx(&f_ctx);

	ctx.pub_seed = pk->pk_seed;

	set_type(ws->wots_addr, LC_SPX_ADDR_TYPE_WOTS);
	set_type(ws->tree_addr, LC_SPX_ADDR_TYPE_HASHTREE);
	set_type(ws->wots_pk_addr, LC_SPX_ADDR_TYPE_WOTSPK);

	/* Derive the message digest and leaf index from R || PK || M. */
	/* The additional LC_SPX_N is a result of the hash domain separator. */
	hash_message(ws->mhash, &tree, &idx_leaf, sig->r, pk->pk_seed, m, mlen);

	/* Layer correctly defaults to 0, so no need to set_layer_addr */
	set_tree_addr(ws->wots_addr, tree);
	set_keypair_addr(ws->wots_addr, idx_leaf);

	f_ctx.fors_pk_from_sig(ws->root, sig->sigfors, ws->mhash, &ctx,
			       ws->wots_addr);

	/* For each subtree.. */
	for (i = 0; i < LC_SPX_D; i++) {
		set_layer_addr(ws->tree_addr, i);
		set_tree_addr(ws->tree_addr, tree);

		copy_subtree_addr(ws->wots_addr, ws->tree_addr);
		set_keypair_addr(ws->wots_addr, idx_leaf);

		copy_keypair_addr(ws->wots_pk_addr, ws->wots_addr);

		/*
		 * The WOTS public key is only correct if the signature was
		 * correct. Initially, root is the FORS pk, but on subsequent
		 * iterations it is the root of the subtree below the currently
		 * processed subtree.
		 */
		f_ctx.wots_pk_from_sig(ws->wots_pk, wots_sig, ws->root, &ctx,
				       ws->wots_addr);
		wots_sig += LC_SPX_WOTS_BYTES;

		/* Compute the leaf node using the WOTS public key. */
		thash(ws->leaf, ws->wots_pk, LC_SPX_WOTS_LEN, pk->pk_seed,
		      ws->wots_pk_addr);

		/* Compute the root node of this subtree. */
		compute_root(ws->root, ws->leaf, idx_leaf, 0, wots_sig,
			     LC_SPX_TREE_HEIGHT, pk->pk_seed, ws->tree_addr);
		wots_sig += LC_SPX_TREE_HEIGHT * LC_SPX_N;

		/* Update the indices for the next layer. */
		idx_leaf = (tree & ((1 << LC_SPX_TREE_HEIGHT) - 1));
		tree = tree >> LC_SPX_TREE_HEIGHT;
	}

	/* Check if the root node equals the root node in the public key. */
	if (lc_memcmp_secure(ws->root, sizeof(ws->root), pub_root, LC_SPX_N))
		ret = -EBADMSG;

out:
	LC_RELEASE_MEM(ws);
	return ret;
}
