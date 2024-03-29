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

#include "alignment.h"
#include "ascon_internal.h"
#include "bitshift.h"
#include "build_bug_on.h"
#include "lc_ascon_aead.h"
#include "lc_memcmp_secure.h"
#include "math_helper.h"
#include "ret_checkers.h"
#include "visibility.h"
#include "xor.h"

static void lc_ascon_zero(struct lc_ascon_cryptor *ascon)
{
	uint64_t *state_mem = ascon->state;

	lc_memset_secure((uint8_t *)state_mem, 0, ascon->statesize);
	lc_memset_secure((uint8_t *)ascon->key, 0, sizeof(ascon->key));
	ascon->keylen = 0;
	ascon->rate_offset = 0;
	ascon->roundb = 0;

	/* Do not touch ascon->statesize! */
}

/**
 * @brief Set the key for the encryption or decryption operation
 *
 * @param state [in] Ascon crypt cipher handle
 * @param key [in] Buffer with key
 * @param keylen [in] Length of key buffer
 * @param nonce [in] Nonce vector to be used
 * @param noncelen [in] Length of nonce vector
 *
 * The algorithm supports a key of arbitrary size. The only requirement is that
 * the same key is used for decryption as for encryption.
 */
static int lc_ascon_setkey(void *state, const uint8_t *key, size_t keylen,
			   const uint8_t *nonce, size_t noncelen)
{
	struct lc_ascon_cryptor *ascon = state;
	const struct lc_hash *hash = ascon->hash;
	uint64_t *state_mem = ascon->state;
	int ret;

	if (noncelen != 16)
		return -EINVAL;

	lc_ascon_zero(ascon);

	/*
	 * Add (IV || key || Nonce) to rate section and first part of capacity
	 * section of state.
	 */
	ret = lc_ak_setiv(ascon, keylen);
	if (ret < 0)
		return ret;

	/* lc_ak_setiv did not identify the key */
	if (!ret) {
		ret = lc_ascon_ascon_setiv(ascon, keylen);
		if (ret < 0)
			return ret;
	}

	/* lc_ascon_ascon_setiv also did not take the data */
	if (!ret)
		return -EINVAL;

	memcpy(ascon->key, key, keylen);

	/* Insert key past the IV. */
	lc_sponge_add_bytes(hash, state_mem, key,
			    (unsigned int)(sizeof(uint64_t)),
			    (unsigned int)keylen);

	/* Insert nonce past the key. */
	lc_sponge_add_bytes(hash, state_mem, nonce,
			    (unsigned int)(sizeof(uint64_t) + keylen),
			    (unsigned int)noncelen);

	/* Sponge permutation */
	lc_sponge(hash, state_mem, 12);

	/* XOR key to last part of capacity */
	lc_sponge_add_bytes(hash, state_mem, key,
			    (unsigned int)(ascon->statesize - keylen),
			    (unsigned int)keylen);

	return 0;
}

/*
 * This function adds the padding byte with which the AAD as well as the
 * plaintext is appended with.
 */
static void lc_ascon_add_padbyte(struct lc_ascon_cryptor *ascon, size_t offset)
{
	const struct lc_hash *hash = ascon->hash;
	static const uint8_t pad_data = 0x80;
	uint64_t *state_mem = ascon->state;

	/*
	 * The data was exactly a multiple of the rate -> permute before adding
	 * the padding byte.
	 */
	if (offset == hash->rate)
		offset = 0;

	lc_sponge_add_bytes(hash, state_mem, &pad_data, (unsigned int)offset,
			    1);
}

/* Insert the AAD into the sponge state. */
static void lc_ascon_aad(struct lc_ascon_cryptor *ascon, const uint8_t *aad,
			 size_t aadlen)
{
	const struct lc_hash *hash = ascon->hash;
	uint64_t *state_mem = ascon->state;
	static const uint8_t pad_trail = 0x01;

	if (!aadlen)
		return;

	/* Authenticated Data - Insert into rate section of the state */
	while (aadlen >= hash->rate) {
		lc_sponge_add_bytes(hash, state_mem, aad, 0, hash->rate);

		aadlen -= hash->rate;
		aad += hash->rate;

		lc_sponge(hash, state_mem, ascon->roundb);
	}

	lc_sponge_add_bytes(hash, state_mem, aad, 0, (unsigned int)aadlen);
	lc_ascon_add_padbyte(ascon, aadlen);

	lc_sponge(hash, state_mem, ascon->roundb);

	/* Add pad_trail bit */
	lc_sponge_add_bytes(hash, state_mem, &pad_trail, ascon->statesize - 1,
			    1);
}

/* Handle the finalization phase of the Ascon algorithm. */
static void lc_ascon_finalization(struct lc_ascon_cryptor *ascon, uint8_t *tag,
				  size_t taglen)
{
	const struct lc_hash *hash = ascon->hash;
	uint64_t *state_mem = ascon->state;
	uint8_t tag_offset = ascon->statesize - (uint8_t)taglen;

	/* Finalization - Insert key into capacity */
	lc_sponge_add_bytes(hash, state_mem, ascon->key, hash->rate,
			    ascon->keylen);

	/* Sponge permutation */
	lc_sponge(hash, state_mem, 12);

	/* Finalization - Insert key into capacity */
	lc_sponge_add_bytes(hash, state_mem, ascon->key, tag_offset,
			    (unsigned int)taglen);

	/* Finalization - Extract tag from capacity */
	lc_sponge_extract_bytes(hash, state_mem, tag, tag_offset,
				(unsigned int)taglen);
}

/* Plaintext - Insert into sponge state and extract the ciphertext */
static void lc_ascon_enc_update(struct lc_ascon_cryptor *ascon,
				const uint8_t *plaintext, uint8_t *ciphertext,
				size_t datalen)
{
	const struct lc_hash *hash = ascon->hash;
	uint64_t *state_mem = ascon->state;
	size_t todo = 0;

	while (datalen) {
		todo = min_size(datalen, hash->rate - ascon->rate_offset);

		lc_sponge_add_bytes(hash, state_mem, plaintext,
				    ascon->rate_offset, (unsigned int)todo);

		lc_sponge_extract_bytes(hash, state_mem, ciphertext,
					ascon->rate_offset, (unsigned int)todo);

		datalen -= todo;

		/* Apply Sponge for all rounds other than the last one */
		if (datalen) {
			plaintext += todo;
			ciphertext += todo;
			ascon->rate_offset = 0;

		} else {
			ascon->rate_offset += (uint8_t)todo;
		}

		lc_sponge(hash, state_mem, ascon->roundb);
	}
}

static void lc_ascon_enc_final(struct lc_ascon_cryptor *ascon, uint8_t *tag,
			       size_t taglen)
{
	/*
	 * Tag size can be at most the key size which in turn is smaller than
	 * the capacity. Thus, all bits of the tag (a) are always affected by
	 * the key, and (b) affected by the capacity.
	 *
	 * Note, this code allows small tag sizes, including zero tag sizes.
	 * It is supported here, but the decryption side requires 16 bytes
	 * tag length as a minimum.
	 */
	if (taglen > ascon->keylen)
		return;

	lc_ascon_add_padbyte(ascon, ascon->rate_offset);

	/* Finalization */
	lc_ascon_finalization(ascon, tag, taglen);
}

/* Complete one-shot encryption */
static void lc_ascon_encrypt(void *state, const uint8_t *plaintext,
			     uint8_t *ciphertext, size_t datalen,
			     const uint8_t *aad, size_t aadlen, uint8_t *tag,
			     size_t taglen)
{
	struct lc_ascon_cryptor *ascon = state;

	/* Authenticated Data */
	lc_ascon_aad(ascon, aad, aadlen);

	/* Plaintext - Insert into rate */
	lc_ascon_enc_update(ascon, plaintext, ciphertext, datalen);

	/* Finalize operation and get authentication tag */
	lc_ascon_enc_final(ascon, tag, taglen);
}

/* Ciphertext - Insert into sponge state and extract the plaintext */
static void lc_ascon_dec_update(struct lc_ascon_cryptor *ascon,
				const uint8_t *ciphertext, uint8_t *plaintext,
				size_t datalen)
{
	const struct lc_hash *hash = ascon->hash;
	uint8_t tmp_pt[136] __align(sizeof(uint64_t)) = { 0 };
	uint64_t *state_mem = ascon->state;
	uint8_t *pt_p = plaintext;
	size_t todo = 0;
	int zero_tmp = 0;

	/* If we have an in-place cipher operation, we need a tmp-buffer */
	if (plaintext == ciphertext) {
		pt_p = tmp_pt;
		zero_tmp = 1;
	}

	while (datalen) {
		todo = min_size(datalen, hash->rate - ascon->rate_offset);
		lc_sponge_extract_bytes(hash, state_mem, pt_p,
					ascon->rate_offset, (unsigned int)todo);

		datalen -= todo;

		/*
		 * Replace state with ciphertext and apply Sponge for all
		 * rounds other than the last one.
		 */
		if (datalen) {
			lc_sponge_newstate(hash, state_mem, ciphertext,
					   ascon->rate_offset, todo);

			/*
			 * Perform XOR operation here to ensure decryption in
			 * place.
			 */
			if (!zero_tmp) {
				xor_64(pt_p, ciphertext, todo);
				pt_p += todo;
			} else {
				xor_64_3(plaintext, pt_p, ciphertext, todo);
				plaintext += todo;
			}

			ciphertext += todo;
			ascon->rate_offset = 0;
		} else {
			if (!zero_tmp) {
				xor_64(pt_p, ciphertext, todo);
				lc_sponge_add_bytes(hash, state_mem, pt_p,
						    ascon->rate_offset,
						    (unsigned int)todo);
			} else {
				xor_64_3(plaintext, pt_p, ciphertext, todo);
				lc_sponge_add_bytes(hash, state_mem, plaintext,
						    ascon->rate_offset,
						    (unsigned int)todo);
			}
			ascon->rate_offset += (uint8_t)todo;
		}

		lc_sponge(hash, ascon->state, ascon->roundb);

	}

	if (zero_tmp)
		lc_memset_secure(tmp_pt, 0, sizeof(tmp_pt));
}

/* Perform the authentication as the last step of the decryption operation */
static int lc_ascon_dec_final(struct lc_ascon_cryptor *ak, const uint8_t *tag,
			      size_t taglen)
{
	uint8_t calctag[64] __align(sizeof(uint64_t));
	int ret;

	BUILD_BUG_ON(sizeof(calctag) != sizeof(ak->key));

	if (taglen < 16)
		return -EINVAL;

	lc_ascon_add_padbyte(ak, ak->rate_offset);

	/* Finalization */
	lc_ascon_finalization(ak, calctag, taglen);

	ret = (lc_memcmp_secure(calctag, taglen, tag, taglen) ? -EBADMSG : 0);
	lc_memset_secure(calctag, 0, taglen);

	return ret;
}

/* Complete one-shot decryption */
static int lc_ascon_decrypt(void *state, const uint8_t *ciphertext,
			    uint8_t *plaintext, size_t datalen,
			    const uint8_t *aad, size_t aadlen,
			    const uint8_t *tag, size_t taglen)
{
	struct lc_ascon_cryptor *ascon = state;

	/* Authenticated Data - Insert into rate */
	lc_ascon_aad(ascon, aad, aadlen);

	/* Ciphertext - Insert into rate */
	lc_ascon_dec_update(ascon, ciphertext, plaintext, datalen);

	/* Finalize operation and authenticate operation */
	return lc_ascon_dec_final(ascon, tag, taglen);
}

static void lc_ascon_aad_interface(void *state, const uint8_t *aad,
				   size_t aadlen)
{
	struct lc_ascon_cryptor *ascon = state;

	lc_ascon_aad(ascon, aad, aadlen);
}

static void lc_ascon_enc_update_interface(void *state, const uint8_t *plaintext,
					  uint8_t *ciphertext, size_t datalen)
{
	struct lc_ascon_cryptor *ascon = state;

	lc_ascon_enc_update(ascon, plaintext, ciphertext, datalen);
}

static void lc_ascon_enc_final_interface(void *state, uint8_t *tag,
					 size_t taglen)
{
	struct lc_ascon_cryptor *ascon = state;

	lc_ascon_enc_final(ascon, tag, taglen);
}

static void lc_ascon_dec_update_interface(void *state,
					  const uint8_t *ciphertext,
					  uint8_t *plaintext, size_t datalen)
{
	struct lc_ascon_cryptor *ascon = state;

	lc_ascon_dec_update(ascon, ciphertext, plaintext, datalen);
}

static int lc_ascon_dec_final_interface(void *state, const uint8_t *tag,
					size_t taglen)
{
	struct lc_ascon_cryptor *ascon = state;

	return lc_ascon_dec_final(ascon, tag, taglen);
}

static void lc_ascon_zero_interface(void *state)
{
	struct lc_ascon_cryptor *ascon = state;

	lc_ascon_zero(ascon);
}

struct lc_aead _lc_ascon_aead = { .setkey = lc_ascon_setkey,
				  .encrypt = lc_ascon_encrypt,
				  .enc_init = lc_ascon_aad_interface,
				  .enc_update = lc_ascon_enc_update_interface,
				  .enc_final = lc_ascon_enc_final_interface,
				  .decrypt = lc_ascon_decrypt,
				  .dec_init = lc_ascon_aad_interface,
				  .dec_update = lc_ascon_dec_update_interface,
				  .dec_final = lc_ascon_dec_final_interface,
				  .zero = lc_ascon_zero_interface };
LC_INTERFACE_SYMBOL(const struct lc_aead *, lc_ascon_aead) = &_lc_ascon_aead;
