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

#ifndef LEANCRYPTO_KERNEL_H
#define LEANCRYPTO_KERNEL_H

#include <linux/module.h>
#include <linux/types.h>

#ifdef __cplusplus
extern "C" {
#endif

#define LC_KERNEL_DEFAULT_PRIO 5000

#ifdef CONFIG_LEANCRYPTO_SHA3
int __init lc_kernel_sha3_init(void);
void lc_kernel_sha3_exit(void);
#else
static inline int __init lc_kernel_sha3_init(void)
{
	return 0;
}

static inline void lc_kernel_sha3_exit(void)
{
}
#endif

#ifdef CONFIG_LEANCRYPTO_KMAC
int __init lc_kernel_kmac256_init(void);
void lc_kernel_kmac256_exit(void);
#else
static inline int __init lc_kernel_kmac256_init(void)
{
	return 0;
}

static inline void lc_kernel_kmac256_exit(void)
{
}
#endif

#ifdef CONFIG_LEANCRYPTO_XDRBG256_DRNG
int __init lc_kernel_rng_init(void);
void lc_kernel_rng_exit(void);
#else
static inline int __init lc_kernel_rng_init(void)
{
	return 0;
}

static inline void lc_kernel_rng_exit(void)
{
}
#endif

#ifdef CONFIG_LEANCRYPTO_DILITHIUM_87
int __init lc_kernel_dilithium_init(void);
void lc_kernel_dilithium_exit(void);
#else
static inline int __init lc_kernel_dilithium_init(void)
{
	return 0;
}

static inline void lc_kernel_dilithium_exit(void)
{
}
#endif

#ifdef CONFIG_LEANCRYPTO_DILITHIUM_65
int __init lc_kernel_dilithium_65_init(void);
void lc_kernel_dilithium_65_exit(void);
#else
static inline int __init lc_kernel_dilithium_65_init(void)
{
	return 0;
}

static inline void lc_kernel_dilithium_65_exit(void)
{
}
#endif

#ifdef CONFIG_LEANCRYPTO_DILITHIUM_44
int __init lc_kernel_dilithium_44_init(void);
void lc_kernel_dilithium_44_exit(void);
#else
static inline int __init lc_kernel_dilithium_44_init(void)
{
	return 0;
}

static inline void lc_kernel_dilithium_44_exit(void)
{
}
#endif

#ifdef CONFIG_LEANCRYPTO_KEM_KYBER_1024
int __init lc_kernel_kyber_init(void);
void lc_kernel_kyber_exit(void);
#else
static inline int __init lc_kernel_kyber_init(void)
{
	return 0;
}

static inline void lc_kernel_kyber_exit(void)
{
}
#endif

#ifdef CONFIG_LEANCRYPTO_KEM_KYBER_768
int __init lc_kernel_kyber_768_init(void);
void lc_kernel_kyber_768_exit(void);
#else
static inline int __init lc_kernel_kyber_768_init(void)
{
	return 0;
}

static inline void lc_kernel_kyber_768_exit(void)
{
}
#endif

#ifdef CONFIG_LEANCRYPTO_KEM_KYBER_512
int __init lc_kernel_kyber_512_init(void);
void lc_kernel_kyber_512_exit(void);
#else
static inline int __init lc_kernel_kyber_512_init(void)
{
	return 0;
}

static inline void lc_kernel_kyber_512_exit(void)
{
}
#endif

#if (defined(CONFIG_LEANCRYPTO_KEM_X25519) &&                                  \
     defined(CONFIG_LEANCRYPTO_KEM_KYBER_1024))
int __init lc_kernel_kyber_x25519_init(void);
void lc_kernel_kyber_x25519_exit(void);
#else
static inline int __init lc_kernel_kyber_x25519_init(void)
{
	return 0;
}

static inline void lc_kernel_kyber_x25519_exit(void)
{
}
#endif

#if (defined(CONFIG_LEANCRYPTO_KEM_X25519) &&                                  \
     defined(CONFIG_LEANCRYPTO_KEM_KYBER_768))
int __init lc_kernel_kyber_x25519_768_init(void);
void lc_kernel_kyber_x25519_768_exit(void);
#else
static inline int __init lc_kernel_kyber_x25519_768_init(void)
{
	return 0;
}

static inline void lc_kernel_kyber_x25519_768_exit(void)
{
}
#endif

#if (defined(CONFIG_LEANCRYPTO_KEM_X25519) &&                                  \
     defined(CONFIG_LEANCRYPTO_KEM_KYBER_512))
int __init lc_kernel_kyber_x25519_512_init(void);
void lc_kernel_kyber_x25519_512_exit(void);
#else
static inline int __init lc_kernel_kyber_x25519_512_init(void)
{
	return 0;
}

static inline void lc_kernel_kyber_x25519_512_exit(void)
{
}
#endif

#ifdef CONFIG_LEANCRYPTO_ASCON_HASH
int __init lc_kernel_ascon_init(void);
void lc_kernel_ascon_exit(void);
#else
static inline int __init lc_kernel_ascon_init(void)
{
	return 0;
}

static inline void lc_kernel_ascon_exit(void)
{
}
#endif

#ifdef __cplusplus
}
#endif

#endif /* LEANCRYPTO_KERNEL_H */
