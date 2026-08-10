/**
 * @file utils/hash/murmurhash.h
 *
 * @brief Declaration for different variations on MurmurHash algorithm
 *
 * Hash function purpose:
 *
 * All MurmurHash algorithms are designed for producing a fixed-size
 * hash value (usually 32 or 128 bits) from arbitrary input data, which
 * is widely used in data structure implementations, such as hash
 * tables, for fast data retrieval.
 *
 * Non-cryptographic:
 *
 * They are non-cryptographic hash functions, meaning they are not
 * intended for cryptographic security purposes, such as hashing
 * passwords.  Instead, their focus is on speed and efficiency while
 * minimizing hash collisions.
 *
 * Efficiency:
 *
 * Each version emphasizes computational efficiency, performing well on
 * typical workloads with a low overhead when hashing inputs.
 *
 * Handling of input:
 *
 * All MurmurHash algorithms effectively handle inputs of varying
 * lengths, processing data in blocks to maximize throughput.  They also
 * utilize a variety of techniques (such as mixing and rotating bits) to
 * ensure a good distribution of hash values.
 *
 * Seed value:
 *
 * Each algorithm allows the inclusion of a seed value, which adds
 * variability to the hash results, making it possible to obtain
 * different hash outputs for the same input data if a different seed is
 * used.
 *
 * Presence of block processing:
 *
 * They process input data in chunks (e.g., 4 bytes or 8 bytes), which
 * helps to maintain performance on larger datasets.
 *
 * Final mixing and avalanche effect:
 *
 * Each algorithm implements a final mixing stage that ensures that
 * small changes in the input lead to significant changes in the output
 * hash (known as the avalanche effect), optimizing the distribution of
 * the hash values.
 *
 * @defgroup utils_hash Hashing utilities
 * @ingroup utils
 */

#ifndef MURMURHASH_H
#define MURMURHASH_H


/* System includes */
#include <stdint.h>     /* uint8_t, uint32_t */


/**
 * @name MurmurHash1 constants
 * @{
 */
/** Mixing and block constant for MurmurHash1 */
#define MH1_C (0xc6a4a793u)
/** @} */

/**
 * @name MurmurHash2 and MurmurHash3 mixing constants
 *
 * @note MurmurHash3 reuses the same @c c1 / @c c2 pair as MurmurHash2
 * @{
 */
#define MH2_C1 (0xcc9e2d51u)    /**< First mixing constant */
#define MH2_C2 (0x1b873593u)    /**< Second mixing constant */
#define MH2_MIX (0xe6546b64u)   /**< Avalanche-mix addend */
/** @} */

/**
 * @name Shared finalization constants
 *
 * These two constants appear in the finalization (avalanche) step of
 * all three MurmurHash variants.
 * @{
 */
#define MH_FIN_C1 (0x85ebca6bu) /**< First finalization constant */
#define MH_FIN_C2 (0xc2b2ae35u) /**< Second finalization constant */
/** @} */


/**
 * @brief MurmurHash1 32-bit hash function
 *
 * Generates a 32-bit hash value from the given input data using the
 * MurmurHash1 algorithm, which, while less popular than its successors,
 * is still an efficient and straightforward hashing technique.
 *
 * @param key  Pointer to the input data.
 * @param len  Length of the input data in bytes
 * @param seed The seed value mixed into the hash, allowing different
 *             hash results for the same data
 *
 * @return Computed 32-bit hash value of the input data
 *
 * @note This version is similar to MurmurHash2, but optimized for
 *       different use cases
 * @note Parameter @p len should be the size of the data pointed by the
 *       @p key parameter
 * @note Parameter @p seed can be used to produce different hash results
 *       for the same input key
 * @note Complexity: @e O(n)
 */
uint32_t murmurhash1_32(const void *key, int len, uint32_t seed);

/**
 * @brief MurmurHash2 32-bit hash function
 *
 * Generates a 32-bit hash value from the provided input data using the
 * MurmurHash2 algorithm, which is designed to be fast, with good
 * statistical properties, making it suitable for hash tables and
 * similar structures.
 *
 * @param key  Pointer to the input data.
 * @param len  Length of the input data in bytes
 * @param seed The seed value that is mixed into the hash, allowing for
 *             different hash outputs for the same input data
 *
 * @return Computed 32-bit hash value of the input data
 *
 * @note Parameter @p len should be the size of the data pointed by the
 *       @p key parameter
 * @note Parameter @p seed can be used to produce different hash results
 *       for the same input key
 * @note Complexity: @e O(n)
 */
uint32_t murmurhash2_32(const void *key, int len, uint32_t seed);

/**
 * @brief MurmurHash3 32-bit hash function
 *
 * Generates a 32-bit hash value based on the input key and an optional
 * seed value using the MurmurHash3 algorithm implementation, which is
 * a fast, non-cryptographic hash function suitable for general
 * hash-based lookup tasks.
 *
 * @param key  Pointer to the data to be hashed
 * @param len  Length of the input data in bytes
 * @param seed Seed value that is mixed into the hash, allowing for
 *             different hash outputs for the same input data
 *
 * @return Computed 32-bit hash value as an unsigned integer
 *
 * @note Parameter @p len should be the size of the data pointed by the
 *       @p key parameter
 * @note Parameter @p seed can be used to produce different hash results
 *       for the same input key
 * @note Complexity: @e O(n)
 */
uint32_t murmurhash3_32(const void *key, int len, uint32_t seed);


#endif  /* ! MURMURHASH_H */
