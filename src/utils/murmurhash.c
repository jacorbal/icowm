/** @file murmurhash.c
 *
 * @brief MurmurHash different variations implementation
 */

/* System includes */
#include <stdint.h>     /* uint8_t, uint32_t */

/* Local includes */
#include <utils/murmurhash.h>


/* MurmurHash1 32-bit hash function */
uint32_t murmurhash1_32(const void *key, int len, uint32_t seed)
{
    const uint8_t *data = (const uint8_t *) key;
    const int nblocks = len / 4;
    uint32_t h = seed;

    /* Process 4-byte blocks */
    for (int i = 0; i < nblocks; ++i) {
        uint32_t k = *(uint32_t *) (data + i * 4);
        k *= 0xc6a4a793;
        k ^= k >> 24;
        k *= 0xc6a4a793;
        h ^= k;
        h *= 0xc6a4a793;
    }

    /* Process remaining bytes */
    const uint8_t *tail = data + nblocks * 4;
    uint32_t k = 0;

    switch (len & 3) {
        case 3: k ^= tail[2] << 16;             /* SLL16 */
            /* fall through */
        case 2: k ^= tail[1] << 8;              /* SLL8 */
            /* fall through */
        case 1: k ^= tail[0];
                h ^= k;
    }

    /* Finalize hash */
    h ^= h >> 16;
    h *= 0x85ebca6b;
    h ^= h >> 13;
    h *= 0xc2b2ae35;
    h ^= h >> 16;

    return h;
}

 
/* MurmurHash2 32-bit hash function */
uint32_t murmurhash2_32(const void *key, int len, uint32_t seed)
{
    const uint8_t *data = (const uint8_t *) key;
    const int nblocks = len / 4;
    uint32_t h = seed ^ (uint32_t) len;

    const uint32_t c1 = 0xcc9e2d51;
    const uint32_t c2 = 0x1b873593;

    /* Process 4-byte blocks */
    for (int i = 0; i < nblocks; ++i) {
        uint32_t k = *(uint32_t *) (data + i * 4);
        k *= c1;
        k = (k << 15) | (k >> (32 - 15));           /* ROTL15 */
        k *= c2;
        h ^= k;
        h = (h << 13) | (h >> (32 - 13));           /* ROTL13 */
        h = h * 5 + 0xe6546b64;
    }

    /* Process remaining bytes */
    const uint8_t *tail = data + nblocks * 4;
    uint32_t k = 0;

    switch (len & 3) {
        case 3: k ^= tail[2] << 16;
            /* fall through */
        case 2: k ^= tail[1] << 8;
            /* fall through */
        case 1: k ^= tail[0];
                h ^= k;
    }

    /* Finalize hash */
    h ^= h >> 16;
    h *= 0x85ebca6b;
    h ^= h >> 13;
    h *= 0xc2b2ae35;
    h ^= h >> 16;

    return h; // Retornar el hash resultante
}


/* MurmurHash3 32-bit hash function */
uint32_t murmurhash3_32(const void *key, int len, uint32_t seed)
{
    const uint8_t *data = (const uint8_t*) key;
    const int nblocks = len / 4;

    uint32_t h = seed;
    uint32_t c1 = 0xcc9e2d51;
    uint32_t c2 = 0x1b873593;

    /* Process 4-byte blocks */
    for (int i = 0; i < nblocks; ++i) {
        uint32_t k = *(uint32_t *) (data + i * 4);
        k *= c1;
        k = (k << 15) | (k >> (32 - 15));           /* ROTL15 */
        k *= c2;

        h ^= k;
        h = (h << 13) | (h >> (32 - 13));           /* ROTL13 */
        h = h * 5 + 0xe6546b64;
    }

    /* Process remaining bytes */
    const uint8_t *tail = (const uint8_t *) (data + nblocks * 4);
    uint32_t k = 0;
    switch (len & 3) {
        case 3:
            k ^= tail[2] << 16;
            /* fall through */
        case 2:
            k ^= tail[1] << 8;
            /* fall through */
        case 1: k ^= tail[0];
                k *= c1;
                k = (k << 15) | (k >> (32 - 15));   /* ROTL15 */
                k *= c2;
                h ^= k;
    }

    /* Finalize hash */
    h ^= (uint32_t) len;
    h ^= h >> 16;
    h *= 0x85ebca6b;
    h ^= h >> 13;
    h *= 0xc2b2ae35;
    h ^= h >> 16;

    return h;
}
