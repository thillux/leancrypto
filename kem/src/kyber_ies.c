/* Kyber Hybrid Integrated Encryption Schema - KyberIES
 *
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

/******************************************************************************
 * Abstract
 *
 * This specification defines a hybrid Integrated Encryption Schema (IES) based
 * on the principles outlined in [SHOUP] chapter 5 named KyberIES. KyberIES
 * uses the the Kyber Key Encapsulation Mechanism (KEM) and combines it with a
 * symmetric encryption algorithm based on Authenticated Encryption with
 * Additional Data (AEAD). The KyberIES algorithm allows the encryption of
 * arbitrary plaintext data using the Kyber public key. The resulting
 * ciphertext can be decrypted using the Kyber secret key.
 *
 * 1. Introduction
 *
 * This specification defines a hybrid encryption algorithm combining the Kyber
 * KEM with an AEAD symmetric algorithm. This algorithm can be used to encrypt
 * and decrypt arbitrary user data using a Kyber asymmetric key pair. Using the
 * Kyber KEM, a shared secret is obtained that is used to generate the symmetric
 * key and IV, and possibly the MAC key for the AEAD algorithm. The AEAD
 * algorithm is instantiated with the key, IV and possibly the MAC key to be
 * used for encrypting plaintext data or decrypting ciphertext data.
 *
 * The use of Kyber KEM offers a post-quantum computing asymmetric algorithm. If
 * the AEAD algorithm is equally unaffected by quantum computes - which is
 * commonly the case - KyberIES is quantum computer safe. In addition, the use
 * of an AEAD algorithm offers data privacy along with intrinsic data integrity
 * verification.
 *
 * The KyberIES serves the same purposes as ECIES specified in [SEC1] chapter 5
 * or similar algorithms and is intended to serve as a suitable replacement
 * for ECIES. The main difference is that instead of generating an ephemeral
 * key pair with which the shared secret is generated and the encryptor must
 * communicate the public ephemeral key to the decryptor, KyberIES returns
 * the Kyber ciphertext along with the data ciphertext to the encryptor to be
 * communicated to the decryptor.
 *
 * The purpose of the KyberIES algorithm is to allow the interaction of 2
 * entities where one entity performs the encryption (called Alice henceforth)
 * of the data and the second entity performs the decryption (called Bob
 * henceforth). Bob is the owner of a Kyber asymmetric key pair and communicates
 * the public key to Alice before the start of the KyberIES operations. Alice
 * performs the encryption using Bob's public key. The resulting data is
 * communicated to Bob allowing Bob to decrypt the data with his secret key.
 * This schema therefore serves the following purposes:
 *
 * 1. Only Bob's public key must be communicated to Alice. This key only
 * requires integrity and authenticity protection during communication, but not
 * privacy protection.
 *
 * 2. The data generated by Alice during encryption can be send to Bob without
 * further protection. By using an AEAD algorithm, data integrity is verified.
 * This data integrity guarantees (a) that the ciphertext was not changed, and
 * (b) the Kyber KEM data sent along with the ciphertext are also modified.
 *
 * Another purpose of the KyberIES algorithm is the encryption of data for local
 * storage. The use of an hybrid asymmetric algorithm allows data storage such
 * that data can be securely stored, but only retrieved when the key owner
 * requests the reading. In this scenario, the data owner's public Kyber key is
 * available to the system all the time. However, the data owner's secret key
 * is not required to be available while the data is stored protected (i.e.
 * encrypted). The data owner only need to provide the secret key when the data
 * shall be retrieved (i.e. decrypted). As mentioned before, the use of an AEAD
 * algorithm allows that the ciphertext as well as the additional Kyber KEM
 * data can be stored without applying additional protection mechanisms.
 *
 * 2. KyberIES Algorithm
 *
 * The KyberIES algorithm specification first outlines the notation followed by
 * the specification of the different components of the algorithm. Those
 * components are finally merged into the KyberIES algorithm specification.
 *
 * 2.1 Notation
 *
 * 2.1.1 KyberEnc
 *
 * KyberEnc(pk, ss_len) denotes the Kyber.CCAKEM.Enc(pk) algorithm specified in
 * [KYBER] section 1.3. It takes the Kyber public key pk as input as well as the
 * length of the shared key to be generated and generates the ciphertext c and
 * the shared key K. The used KDF is SHAKE as defined in section 2.1.4.
 *
 * KyberEnc(pk, ss_len) -> c, shared key
 *
 * 2.1.2 KyberDec
 *
 * KyberDec(c, sk, ss_len) denotes the Kyber.CCAKEM.Dec(c, sk) algorithm
 * specified in [KYBER] section 1.3. It takes the Kyber secret key sk, the
 * ciphertext c and the length of the shared secret to be generated as input
 * and generates the shared key K. The used KDF is SHAKE as defined in section
 * 2.1.4.
 *
 * KyberDec(c, sk, ss_len) -> shared key
 *
 * 2.1.3 RND
 *
 * RND denotes the random bit generator to generate a random bit strings of any
 * size. It takes the numbers of bits to be generated as input to generate the
 * requested amount of random bits.
 *
 * 2.1.4 SHAKE
 *
 * SHAKE denotes the SHAKE256 algorithm as specified in [FIPS202]. It takes the
 * message M as input as well as the bit size d of the message digest to be
 * generated. The inputs to the SHAKE algorithm are specified with references
 * to these parameters.
 *
 * SHAKE256 is used to be consistent with [KYBER] section 1.4.
 *
 * 2.1.5 AEAD Algorithm
 *
 * AEAD denotes an authenticated encryption with additional data algorithm. The
 * particular algorithm is explicitly unspecified allowing different types of
 * AEAD algorithms, including AES-GCM, AES-CCM, Encrypt-Then-MAC algorithms
 * allowed as part of IPSEC and others. All these algorithms share a common
 * definition as follows which is used by KyberIES:
 *
 * AEADEncrypt(key, IV, MAC Key, plaintext, AAD, taglen) -> ciphertext, tag
 *
 * Input:
 *   key: Encryption key - KyberIES requires a key size of 256 bits to be
 *	  supported by the AEAD algorithm
 *
 *   IV: Initialization vector - KyberIES supports an arbitrary IV size, which
 *	 must be defined with the used AEAD algorithm
 *
 *   MAC key: The optional MAC key - KyberIES supports an arbitrary IV size,
 *	      which must be defined with the used AEAD algorithm (if the AEAD
 *	      algorithm does not require a MAC key, the MAC key is an empty
 *	      string with zero bits in size)
 *
 *   plaintext: The caller-provided plaintext data.
 *
 *   AAD: The caller-provided additional authenticated data.
 *
 *   taglen: The length of the message authentication tag to be generated.
 *
 * Output
 *   ciphertext: The ciphertext that can exchanged with the recipient over
 *               insecure channels.
 *
 *   tag: The message authentication tag that can be exchanged with the
 *        recipient over insecure channels.
 *
 * AEADDecrypt(key, IV, MAC key, AAD, ciphertext, tag) -> plaintext, authentication result
 *
 * Input:
 *   key: See AEADEncrypt
 *
 *   IV: See AEADEncrypt
 *
 *   MAC key: See AEADEncrypt
 *
 *   AAD: The caller-provided additional authenticated data.
 *
 *   ciphertext: The ciphertext that was received from the send over potentially
 *               insecure channels.
 *
 *   tag: The message authentication tag that was received from the send over
 *        potentially insecure channels.
 *
 * Output
 *   plaintext: The plaintext of the data.
 *
 *   authentication result: A boolean indicator specifying whether the
 *			    authentication was successful. If it was
 *			    unsuccessful the caller shall reject the ciphertext.
 *
 * 2.2 Cryptographic Aspects
 *
 * 2.2.1 Diversification of Shared Secret
 *
 * The Kyber algorithm generates shared key using random bits. This is
 * important for the security of KyberIES to ensure that every encryption /
 * decryption operation generates a different shared secret. This allows the use
 * of the same public/secret key pair for different plaintexts in conjunction
 * with stream-cipher-based AEAD algorithms. Stream ciphers commonly loose their
 * security strength if the key/IV is reused for protecting different data.
 *
 * The Kyber shared key is used as part of the key derivation mechanism whose
 * input from the Kyber KEM is based on 256 bits generated from a random bit
 * generator to obtain the key / IV used by the AEAD algorithm. This use of the
 * random bit generator guarantees that the resulting key/IV pair is always
 * different irrespective whether the same Kyber KEM keypair is used. This
 * implies that even stream-cipher-based AEAD algorithms can be safely used.
 *
 * 2.2.2 Security Strength
 *
 * The KyberIES algorithm provides a security strength of 256 bits as all its
 * components provide this security strength:
 *
 * * The Kyber KEM algorithm of type Kyber1024 is required to be used as
 *   defined in [KYBER] chapter 5. This algorithm provides a security strength
 *   of 256 bit.
 *
 * * The AEAD algorithm is required to also provide a security strength of
 *   256 bits. As is security strength is based on the used AEAD key size,
 *   the KyberIES algorithm defines the use of a 256 bit key size to mandate a
 *   security strength of 256 bits.
 *
 * * KyberIES defines the use of SHAKE256 which implies a security strength of
 *   256 bits.
 *
 * * RND defines a random bit generator that has a security strength of 256
 *   bits and is seeded with at least 256 bits of entropy.
 *
 * 2.3 Encryption of Data
 *
 * KyberIESEnc(pk, plaintext, AAD, taglen) -> Kyber ciphertext, ciphertext, tag
 *
 * Input:
 *   pk: Kyber public key of the data owner
 *
 *   plaintext: The caller-provided plaintext data.
 *
 *   AAD: The caller-provided additional authenticated data. The AAD can have
 *	  any size including an empty bit-string.
 *
 *   taglen: The length of the message authentication tag to be generated.
 *
 * Output:
 *   Kyber ciphertext: Kyber ciphertext c as defined for KyberEnc
 *
 *   ciphertext: The ciphertext that can exchanged with the recipient over
 *               insecure channels.
 *
 *   tag: The message authentication tag that can be exchanged with the
 *        recipient over insecure channels.
 *
 *
 * The KyberIES encryption operation is performed as follows:
 *
 * Kyber ciphertext, shared key = KyberEnc(pk, 256 + AEAD IV length + AEAD MAC key length)
 * AEADkey = shared key[0:255] - the left-most 256 bits of shared key
 * AEADIV = shared key[256:AEAD IV length] - shared key bits starting with 256th bit of AEAD IV length
 * AEADMACKey = shared key[256 + AEAD IV length: AEAD MAC key length] - shared key bits starting with first bit after AEAD IV bits of AEAD MAC key length
 * ciphertext, tag = AEADEnc(AEADKey, AEADIV, AEADMACKey, AAD, plaintext, taglen)
 *
 * 2.4 Decryption of Data
 *
 * KyberIESDec(sk, Kyber ciphertext, ciphertext, tag) -> plaintext, authentication result
 *
 * * Input:
 *   sk: Kyber secret key of the data owner
 *
 *   Kyber ciphertext: Kyber ciphertext c as defined for KyberEnc
 *
 *   ciphertext: The ciphertext that was received from the send over
 *               insecure channels.
 *
 *   AAD: The caller-provided additional authenticated data. The AAD can have
 *	  any size including an empty bit-string.
 *
 *   tag: The message authentication tag that was received from the send over
 *        insecure channels.
 *
 * Output:
 *   plaintext: The plaintext of the data.
 *
 *   authentication result: A boolean indicator specifying whether the
 *			    authentication was successful. If it was
 *			    unsuccessful the caller shall reject the ciphertext.
 *
 * The KyberIES decryption operation is performed as follows:
 *
 * shared key = KyberDec(sk, Kyber ciphertext, 256 + AEAD IV length + AEAD MAC key length)
 * AEADkey = shared key[0:255] - the left-most 256 bits of shared key
 * AEADIV = shared key[256:AEAD IV length] - shared key bits starting with 256th bit of AEAD IV length
 * AEADMACKey = shared key[256 + AEAD IV length: AEAD MAC key length] - shared key bits starting with first bit after AEAD IV bits of AEAD MAC key length
 * plaintext, authentication result = AEADDec(AEADKey, AEADIV, AEADMACKey, AAD, ciphertext, tag)
 *
 * 3. References
 *
 * [FIPS202] FIPS PUB 202 SHA-3 Standard: Permutation-Based Hash and
 *	      Extendable-Output Functions, August 2015
 *
 * [KYBER] CRYSTALS-Kyber Algorithm Specifications And Supporting Documentation,
 *	   Roberto Avanzi, Joppe Bos, Léo Ducas, Eike Kiltz, Tancrède Lepoint,
 *	   Vadim Lyubashevsky, John M. Schanck, Peter Schwabe, Gregor Seiler,
 *	   Damien Stehlé, Version 3.02, August 4, 2021
 *
 * [SEC1] SEC 1: Elliptic Curve Cryptography, Daniel R. L. Brown, Version 2.0,
 *	  May 21, 2009
 *
 * [SHOUP] A Proposal for an ISO Standard for Public Key Encryption,
 *	   Victor Shoup, Version 2.1, December 20, 2001
 *
 ******************************************************************************/

#include "lc_aead.h"
#include "lc_cshake_crypt.h"
#include "lc_kyber.h"
#include "lc_rng.h"
#include "memset_secure.h"
#include "ret_checkers.h"
#include "visibility.h"

#define LC_KYBER_IES_SYM_KEYSIZE	32
#define LC_KYBER_IES_SYM_IVSIZE		16

DSO_PUBLIC
int lc_kyber_ies_enc(const struct lc_kyber_pk *pk,
		     struct lc_kyber_ct *ct,
		     const uint8_t *plaintext, uint8_t *ciphertext,
		     size_t datalen,
		     const uint8_t *aad, size_t aadlen,
		     uint8_t *tag, size_t taglen,
		     struct lc_rng_ctx *rng_ctx)
{
	LC_CC_CTX_ON_STACK(cc, lc_cshake256);
	uint8_t ss[LC_KYBER_IES_SYM_KEYSIZE + LC_KYBER_IES_SYM_IVSIZE];
	uint8_t *ies_key = ss;
	uint8_t *ies_iv = ss + LC_KYBER_IES_SYM_KEYSIZE;
	int ret;

	CKINT(lc_kyber_enc(ct, ss, sizeof(ss), pk, rng_ctx));
	CKINT(lc_aead_setkey(cc,
			     ies_key, LC_KYBER_IES_SYM_KEYSIZE,
			     ies_iv, LC_KYBER_IES_SYM_IVSIZE));
	lc_aead_encrypt(cc, plaintext, ciphertext, datalen, aad, aadlen,
			tag, taglen);

out:
	lc_aead_zero(cc);
	memset_secure(ss, 0, sizeof(ss));
	return ret;
}

DSO_PUBLIC
int lc_kyber_ies_dec(const struct lc_kyber_sk *sk,
		     const struct lc_kyber_ct *ct,
		     const uint8_t *ciphertext, uint8_t *plaintext,
		     size_t datalen,
		     const uint8_t *aad, size_t aadlen,
		     const uint8_t *tag, size_t taglen)
{
	LC_CC_CTX_ON_STACK(cc, lc_cshake256);
	uint8_t ss[LC_KYBER_IES_SYM_KEYSIZE + LC_KYBER_IES_SYM_IVSIZE];
	uint8_t *ies_key = ss;
	uint8_t *ies_iv = ss + LC_KYBER_IES_SYM_KEYSIZE;
	int ret;

	CKINT(lc_kyber_dec(ss, sizeof(ss), ct, sk));
	CKINT(lc_aead_setkey(cc,
			     ies_key, LC_KYBER_IES_SYM_KEYSIZE,
			     ies_iv, LC_KYBER_IES_SYM_IVSIZE));
	CKINT(lc_aead_decrypt(cc, ciphertext, plaintext, datalen,
			      aad, aadlen, tag, taglen));

out:
	lc_aead_zero(cc);
	memset_secure(ss, 0, sizeof(ss));
	return ret;
}
