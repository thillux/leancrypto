/*
The eXtended Keccak Code Package (XKCP)
https://github.com/XKCP/XKCP

The Keccak-p permutations, designed by Guido Bertoni, Joan Daemen, Michaël Peeters and Gilles Van Assche.

Implementation by Ronny Van Keer, hereby denoted as "the implementer".

For more information, feedback or questions, please refer to the Keccak Team website:
https://keccak.team/

To the extent possible under law, the implementer has waived all copyright
and related or neighboring rights to the source code in this file.
http://creativecommons.org/publicdomain/zero/1.0/

---

Please refer to SnP-documentation.h for more details.
*/

#ifndef _KeccakP_1600_SnP_h_
#define _KeccakP_1600_SnP_h_

#include "ext_headers.h"

#define KeccakF1600_FastLoop_supported
#define KeccakP1600_AVX2_12rounds_FastLoop_supported

#define KeccakP1600_AVX2_StaticInitialize()
void KeccakP1600_AVX2_Initialize(void *state);
void KeccakP1600_AVX2_AddByte(void *state, unsigned char data,
			      unsigned int offset);
void KeccakP1600_AVX2_AddBytes(void *state, const unsigned char *data,
			       unsigned int offset, unsigned int length);
void KeccakP1600_AVX2_OverwriteBytes(void *state, const unsigned char *data,
				     unsigned int offset, unsigned int length);
void KeccakP1600_AVX2_OverwriteWithZeroes(void *state, unsigned int byteCount);
void KeccakP1600_AVX2_Permute_Nrounds(void *state, unsigned int nrounds);
void KeccakP1600_AVX2_Permute_12rounds(void *state);
void KeccakP1600_AVX2_Permute_24rounds(void *state);
void KeccakP1600_AVX2_ExtractBytes(const void *state, unsigned char *data,
				   size_t offset, size_t length);
void KeccakP1600_AVX2_ExtractAndAddBytes(const void *state,
					 const unsigned char *input,
					 unsigned char *output,
					 unsigned int offset,
					 unsigned int length);
size_t KeccakF1600_AVX2_FastLoop_Absorb(void *state, unsigned int laneCount,
					const unsigned char *data,
					size_t dataByteLen);
size_t KeccakP1600_AVX2_12rounds_FastLoop_Absorb(void *state,
						 unsigned int laneCount,
						 const unsigned char *data,
						 size_t dataByteLen);

#endif /* _KeccakP_1600_SnP_h_ */
