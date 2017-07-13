#ifndef MURMURHASH3_H
#define MURMURHASH3_H

/*
 * These hash algorithms are copied from the following:
 *     https://github.com/aappleby/smhasher/wiki/MurmurHash3
 * Thanks to the author for the great job.
 */

#include <utility/types.h>

NS_BEGIN(sk)

void murmurhash3_x86_32 (const void *key, size_t len, u32 seed, u32 out[1]);
void murmurhash3_x86_128(const void *key, size_t len, u32 seed, u32 out[4]);
void murmurhash3_x64_128(const void *key, size_t len, u32 seed, u64 out[2]);

NS_END(sk)

#endif // MURMURHASH3_H
