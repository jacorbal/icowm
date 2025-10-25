/**
 * @file utils/murmurhash.c
 *
 * @brief MurmurHash different variations implementation
 */

/* System includes */
#include <stdint.h>     /* uint8_t, uint32_t */
#include <string.h>     /* memcpy */

/* Local includes */
#include <utils/murmurhash.h>


/* MurmurHash1 32-bit hash function */
uint32_t murmurhash1_32(const void *key, int len, uint32_t seed)
{
    const uint8_t *data = (const uint8_t *) key;
    const int nblocks = len / 4;
    uint32_t h = seed;
    const uint8_t *tail;
    uint32_t k;

    /* Process 4-byte blocks */
    for (int i = 0; i < nblocks; ++i) {
        /* uint32_t k_ = *(uint32_t *) (data + i * 4); */
        uint32_t k_;
        memcpy(&k_, data + i * 4, sizeof(uint32_t));
        k_ *= 0xc6a4a793;
        k_ ^= k_ >> 24;
        k_ *= 0xc6a4a793;
        h ^= k_;
        h *= 0xc6a4a793;
    }

    /* Process remaining bytes */
    tail = data + nblocks * 4;
    k = 0;

    switch (len & 3) {
        case 3: k ^= (unsigned int) tail[2] << 16;      /* SLL16 */
            __attribute__((fallthrough));
        case 2: k ^= (unsigned int) tail[1] << 8;       /* SLL8 */
            __attribute__((fallthrough));
        case 1: k ^= (unsigned int) tail[0];
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
    const uint8_t *tail;
    uint32_t k;

    /* Process 4-byte blocks */
    for (int i = 0; i < nblocks; ++i) {
        /* uint32_t k_ = *(uint32_t *) (data + i * 4); */
        uint32_t k_;
        memcpy(&k_, data + i * 4, sizeof(uint32_t));
        k_ *= c1;
        k_ = (k_ << 15) | (k_ >> (32 - 15));           /* ROTL15 */
        k_ *= c2;
        h ^= k_;
        h = (h << 13) | (h >> (32 - 13));           /* ROTL13 */
        h = h * 5 + 0xe6546b64;
    }

    /* Process remaining bytes */
    tail = data + nblocks * 4;
    k = 0;

    switch (len & 3) {
        case 3: k ^= (unsigned int) tail[2] << 16;
            __attribute__((fallthrough));
        case 2: k ^= (unsigned int) tail[1] << 8;
            __attribute__((fallthrough));
        case 1: k ^= (unsigned int) tail[0];
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
    const uint8_t *tail;
    uint32_t k;

    /* Process 4-byte blocks */
    for (int i = 0; i < nblocks; ++i) {
        /* uint32_t k = *(uint32_t *) (data + i * 4); */
        uint32_t k_;
        memcpy(&k_, data + i * 4, sizeof(uint32_t));
        k_ *= c1;
        k_ = (k_ << 15) | (k_ >> (32 - 15));           /* ROTL15 */
        k_ *= c2;

        h ^= k_;
        h = (h << 13) | (h >> (32 - 13));           /* ROTL13 */
        h = h * 5 + 0xe6546b64;
    }

    /* Process remaining bytes */
    tail = (const uint8_t *) (data + nblocks * 4);
    k = 0;
    switch (len & 3) {
        case 3:
            k ^= (unsigned int) tail[2] << 16;
            __attribute__((fallthrough));
        case 2:
            k ^= (unsigned int) tail[1] << 8;
            __attribute__((fallthrough));
        case 1: k ^= (unsigned int) tail[0];
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
