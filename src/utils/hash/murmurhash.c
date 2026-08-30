/**
 * @file utils/hash/murmurhash.c
 *
 * @brief Implementation of different variations on MurmurHash algorithm
 */

/* System includes */
#include <stdint.h>     /* uint8_t, uint32_t */
#include <string.h>     /* memcpy, NULL */

/* Local includes */
#include <utils/hash/murmurhash.h>


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
        k_ *= MH1_C;
        k_ ^= k_ >> 24;
        k_ *= MH1_C;
        h ^= k_;
        h *= MH1_C;
    }

    /* Process remaining bytes.  'len & 3' alone is not safe here if
     * 'len' is negative: two's complement means a negative 'len' can
     * still leave a nonzero remainder (e.g., '-1 & 3' is 3), entering
     * a 'case' below that dereferences 'tail', which 'len > 0' above
     * already left null for exactly this ('len' non-positive) case.
     * Forcing the switch itself to 0 whenever 'len' is non-positive
     * keeps it consistent with 'tail' regardless of sign, without
     * changing anything for any 'len' this ever ran correctly on. */
    tail = (len > 0) ? data + nblocks * 4 : NULL;
    k = 0;

    switch ((len > 0) ? (len & 3) : 0) {
        case 3: k ^= (unsigned int) tail[2] << 16;  /* SLL16 */
            /* FALLTHROUGH */
        case 2: k ^= (unsigned int) tail[1] << 8;   /* SLL8 */
            /* FALLTHROUGH */
        case 1: k ^= (unsigned int) tail[0];
                h ^= k;
    }

    /* Finalize hash */
    h ^= h >> 16;
    h *= MH_FIN_C1;
    h ^= h >> 13;
    h *= MH_FIN_C2;
    h ^= h >> 16;

    return h;
}


/* MurmurHash2 32-bit hash function */
uint32_t murmurhash2_32(const void *key, int len, uint32_t seed)
{
    const uint8_t *data = (const uint8_t *) key;
    const int nblocks = len / 4;
    uint32_t h = seed ^ (uint32_t) len;
    const uint8_t *tail;
    uint32_t k;

    /* Process 4-byte blocks */
    for (int i = 0; i < nblocks; ++i) {
        /* uint32_t k_ = *(uint32_t *) (data + i * 4); */
        uint32_t k_;
        memcpy(&k_, data + i * 4, sizeof(uint32_t));
        k_ *= MH2_C1;
        k_ = (k_ << 15) | (k_ >> (32 - 15));        /* ROTL15 */
        k_ *= MH2_C2;
        h ^= k_;
        h = (h << 13) | (h >> (32 - 13));           /* ROTL13 */
        h = h * 5 + MH2_MIX;
    }

    /* Process remaining bytes.  'len & 3' alone is not safe here if
     * 'len' is negative: two's complement means a negative 'len' can
     * still leave a nonzero remainder (e.g., '-1 & 3' is 3), entering
     * a 'case' below that dereferences 'tail', which 'len > 0' above
     * already left null for exactly this ('len' non-positive) case.
     * Forcing the switch itself to 0 whenever 'len' is non-positive
     * keeps it consistent with 'tail' regardless of sign, without
     * changing anything for any 'len' this ever ran correctly on. */
    tail = (len > 0) ? data + nblocks * 4 : NULL;
    k = 0;

    switch ((len > 0) ? (len & 3) : 0) {
        case 3: k ^= (unsigned int) tail[2] << 16;
            /* FALLTHROUGH */
        case 2: k ^= (unsigned int) tail[1] << 8;
            /* FALLTHROUGH */
        case 1: k ^= (unsigned int) tail[0];
                h ^= k;
    }

    /* Finalize hash */
    h ^= h >> 16;
    h *= MH_FIN_C1;
    h ^= h >> 13;
    h *= MH_FIN_C2;
    h ^= h >> 16;

    return h;
}


/* MurmurHash3 32-bit hash function */
uint32_t murmurhash3_32(const void *key, int len, uint32_t seed)
{
    const uint8_t *data = (const uint8_t*) key;
    const int nblocks = len / 4;
    uint32_t h = seed;
    const uint8_t *tail;
    uint32_t k;

    /* Process 4-byte blocks */
    for (int i = 0; i < nblocks; ++i) {
        /* uint32_t k = *(uint32_t *) (data + i * 4); */
        uint32_t k_;
        memcpy(&k_, data + i * 4, sizeof(uint32_t));
        k_ *= MH2_C1;
        k_ = (k_ << 15) | (k_ >> (32 - 15));        /* ROTL15 */
        k_ *= MH2_C2;

        h ^= k_;
        h = (h << 13) | (h >> (32 - 13));           /* ROTL13 */
        h = h * 5 + MH2_MIX;
    }

    /* Process remaining bytes.  'len & 3' alone is not safe here if
     * 'len' is negative: two's complement means a negative 'len' can
     * still leave a nonzero remainder (e.g., '-1 & 3' is 3), entering
     * a 'case' below that dereferences 'tail', which 'len > 0' above
     * already left null for exactly this ('len' non-positive) case.
     * Forcing the switch itself to 0 whenever 'len' is non-positive
     * keeps it consistent with 'tail' regardless of sign, without
     * changing anything for any 'len' this ever ran correctly on. */
    tail = (len > 0)
        ? (const uint8_t *) (data + nblocks * 4) : NULL;
    k = 0;
    switch ((len > 0) ? (len & 3) : 0) {
        case 3:
            k ^= (unsigned int) tail[2] << 16;
            /* FALLTHROUGH */
        case 2:
            k ^= (unsigned int) tail[1] << 8;
            /* FALLTHROUGH */
        case 1: k ^= (unsigned int) tail[0];
                k *= MH_FIN_C1;
                k = (k << 15) | (k >> (32 - 15));   /* ROTL15 */
                k *= MH_FIN_C2;
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
