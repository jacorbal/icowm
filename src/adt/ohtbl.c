/**
 * @file adt/ohtbl.c
 *
 * @brief Open-addressed hash table (closed hashing) implementation
 */

#define _POSIX_C_SOURCE 200112L /* CLOCK_MONOTONIC, clock_gettime */


/* System includes */
#include <stdbool.h>
#include <stdint.h>     /* int64_t */
#include <stdlib.h>     /* calloc, malloc, free, NULL */
#include <time.h>       /* clock_gettime, CLOCK_MONOTONIC, timespec */

/* Local includes */
#include <adt/ohtbl.h>


/* Reserve a sentinel memory address for vacated elements */
static char s_vacated;


/**
 * @brief Greatest common divisor of @p a and @p b, by Euclid's
 *        algorithm
 *
 * @param a First value
 * @param b Second value
 *
 * @return @c gcd(a, b)
 *
 * @note Complexity: @e O(log(min(a, b)))
 */
static size_t s_ohtbl_gcd(size_t a, size_t b)
{
    while (b != 0u) {
        size_t remainder = a % b;

        a = b;
        b = remainder;
    }
    return a;
}


/**
 * @brief Turn a raw secondary-hash value into a double-hashing step
 *        guaranteed coprime with @p positions
 *
 * Double hashing only visits every one of a table's @p positions
 * across a full probe (@c i from @c 0 to @c positions @c - @c 1) when
 * the step @c i is multiplied by is itself coprime with @p positions;
 * otherwise the probe cycles through only the limited, fixed subset
 * of positions reachable from that step, which can be far short of
 * the whole table.  @a s_h1 / @a s_h2 (desktop.c, the only @c h1 /
 * @c h2 pair this project defines today) already forces its own raw
 * value away from zero, but a hash function has no way to know
 * @p positions in the first place (its own signature never receives
 * it), so nothing before this point could have enforced coprimality
 * with it specifically.  Left unenforced, a key whose own raw @c h2
 * happens to share a factor with @p positions, an ordinary,
 * unremarkable value for that key, no different from any other, can
 * probe a table that is otherwise mostly empty and still never once
 * land on a free slot, exactly as if it were genuinely full.
 *
 * Every one of @c ohtbl_insert / @c ohtbl_update / @c ohtbl_remove /
 * @c ohtbl_lookup / @a s_ohtbl_resize's own rehashing calls this
 * before ever using an @c h2 result as a step, so the exact same
 * step a key was inserted under is always the one it is later
 * searched or removed under too; only @p positions changing (a
 * resize) ever legitimately changes it, and a resize rehashes every
 * entry into the new table under its own new steps regardless.
 *
 * @param h2_raw    Unadjusted return value of @c htbl->h2
 * @param positions Table size the step must be coprime with
 *
 * @return A step in @c [1, positions - 1] with
 *         @c gcd(step, @c positions) @c == @c 1
 *
 * @note @p positions @c < @c 2 (a table with zero or one position)
 *       is never actually probed with more than one candidate slot
 *       to begin with, so this is never called in that case
 * @note Complexity: @e O(positions) worst case, but see this
 *       function's comment above for why that bound is never
 *       actually approached in practice
 */
static size_t s_ohtbl_step(size_t h2_raw, size_t positions)
{
    size_t step = h2_raw % positions;

    if (step == 0u) {
        step = 1u;
    }

    /* Guaranteed to terminate within at most 'positions' iterations:
     * 'step' cycles through every value in [1, positions - 1] before
     * ever repeating, and gcd(1, positions) is always 1, so a step of
     * 1 itself is always an eventual, guaranteed exit if nothing
     * sooner already was one. */
    while (s_ohtbl_gcd(step, positions) != 1u) {
        step = (step >= positions - 1u) ? 1u : step + 1u;
    }

    return step;
}


/**
 * @brief Cancel a shrink wait outstanding, if @p size has climbed
 *        back up to or above @c OHTBL_MIN_LOAD_FACTOR since it began
 *
 * Called right after @p size grows by one, so a burst of insertions
 * arriving before @c OHTBL_SHRINK_COOLDOWN_MS runs out drops the
 * whole pending shrink outright rather than merely pausing it; see
 * @c OHTBL_SHRINK_COOLDOWN_MS's comment (ohtbl.h) for why
 * this is worth doing at all.
 *
 * @param htbl Table just grown by one element
 *
 * @note Complexity: @e O(1)
 */
static void s_ohtbl_cancel_pending_shrink_if_recovered(ohtbl_td *htbl)
{
    if (htbl->is_shrink_pending &&
            htbl->size >= (size_t)
                ((float) htbl->positions * OHTBL_MIN_LOAD_FACTOR)) {
        htbl->is_shrink_pending = false;
    }
}


/**
 * @brief Resize the open-addressed hash table to a new capacity
 *
 * Resizes the current positions of the hash table to the specific new
 * capacity, and rehashes all existing items into the new table. If the
 * memory allocation for the new table fails, the operation is aborted
 * and an error code is returned.
 *
 * @param htbl          Pointer to the hash table to be resized
 * @param new_positions New positions to resize to
 *
 * @return Status of the resize operation
 * @retval  0 Resize operation was successful
 * @retval -1 Failed to allocate memory for the new table
 *
 * @note It is assumed that the current table is using open addressing
 *       and that structures for each item are still valid after
 *       rehashing
 * @note Complexity: @e O(n), where @e n is the number of elements in
 *       the hash table
 */
static int s_ohtbl_resize(ohtbl_td *htbl, size_t new_positions)
{
    void **new_table;

    /* Initialize a new table */
    new_table = calloc(new_positions, sizeof(void *));
    if (new_table == NULL) {
        return -1;
    }

    /* Re-hash previous elements in new table */
    for (size_t i = 0; i < htbl->positions; ++i) {
        void *element = htbl->table[i];
        if (element != NULL && element != htbl->vacated) {
            /* Only re-insert valid elements */
            size_t step = s_ohtbl_step(htbl->h2(element), new_positions);

            for (size_t j = 0; j < new_positions; ++j) {
                /* Search new position */
                size_t position =
                    (htbl->h1(element) + (j * step)) % new_positions;
                if (new_table[position] == NULL) {
                    /* Found empty position */
                    new_table[position] = element;
                    break;
                }
            } /* ! for (j) */
        }
    } /* ! for (i) */

    /* Deallocate previous table */
    free(htbl->table);

    /* Update pointers and new positions */
    htbl->table = new_table;
    htbl->positions = new_positions;

    /* Any resize, in either direction, starts a fresh assessment of
     * whether the new capacity is itself underused; see
     * 'OHTBL_SHRINK_COOLDOWN_MS''s comment (ohtbl.h) */
    htbl->is_shrink_pending = false;

    return 0;
}


/**
 * @brief Double the current size of the open-addressed hash table
 *
 * Invokes the resize operation to increase the capacity of the hash
 * table by doubling the current number of positions.
 *
 * This doubling is triggered on insertions when the load factor exceeds
 * the defined maximum load factor (@c OHTBL_MAX_LOAD_FACTOR).
 *
 * @param htbl Pointer to the hash table to be doubled in size
 *
 * @return Status of the resize operation
 * @retval  0 Resize operation was successful
 * @retval -1 Failed to allocate memory for the new table
 *
 * @note It is assumed that the current table is using open addressing
 *       and that structures for each item are still valid after
 *       rehashing
 * @note Complexity: @e O(n), where @e n is the number of elements in
 *       the hash table
 *
 * @see @a s_ohtbl_resize
 */
static int s_ohtbl_resize_double(ohtbl_td *htbl)
{
    size_t new_positions;

    /* Set initial positions by doubling the previous value */
    new_positions = (size_t) (htbl->positions * 2);
    return s_ohtbl_resize(htbl, new_positions);
}


/**
 * @brief Halve the current size of the open-addressed hash table
 *
 * Invokes the resize operation to decrease the capacity of the hash
 * table by halving the current number of positions.
 *
 * This halving is triggered on removals when the load factor falls
 * below the minimum load factor (@c OHTBL_MIN_LOAD_FACTOR), as long as
 * the table size does not go below the specified minimum positions.
 *
 * @param htbl Pointer to the hash table to be halved in size
 *
 * @return Status of the resize operation
 * @retval  0 Resize operation was successful
 * @retval  1 Below threshold of minimum size
 * @retval -1 Memory allocation for the new table failed
 *
 * @note It is assumed that the current table is using open addressing
 *       and that structures for each item are still valid after
 *       rehashing
 * @note Complexity: @e O(n), where @e n is the number of elements in
 *       the hash table
 *
 * @see @a s_ohtbl_resize
 */
static int s_ohtbl_resize_halve(ohtbl_td *htbl)
{
    size_t new_positions;

    /* Set initial positions by halving the previous value */
    new_positions = (size_t) (htbl->positions / 2);

    /* Prevent the table to go below a threshold */
    if (new_positions < htbl->min_positions) {
        return 1;
    }

    return s_ohtbl_resize(htbl, new_positions);
}


/* Initialize a new open-addressed hash table */
ohtbl_td *ohtbl_init(size_t positions, const size_t min_positions,
        size_t (*h1)(const void *key), size_t (*h2)(const void *key),
        bool (*match)(const void *key1, const void *key2),
        void (*destroy)(void *data))
{
    ohtbl_td *htbl;

    /* Allocate memory for the structure */
    htbl = malloc(sizeof(ohtbl_td));
    if (htbl == NULL) {
        return NULL;
    }

    /* Allocate space for the hash table */
    htbl->table = malloc((size_t) positions * sizeof(void *));
    if (htbl->table == NULL) {
        free(htbl);
        return NULL;
    }

    /* Initialize each positions */
    htbl->min_positions =
        (min_positions > positions || min_positions == 0) ? positions
                                                          : min_positions;
    htbl->positions = positions;
    for (size_t i = 0; i < htbl->positions; ++i) {
        htbl->table[i] = NULL;
    }

    /* Set the vacated member to the sentinel memory address reserved
     * for this */
    htbl->vacated = &s_vacated;

    /* Encapsulate the functions */
    htbl->h1 = h1;
    htbl->h2 = h2;
    htbl->match = match;
    htbl->destroy = destroy;

    /* Initialize the number of elements in the table */
    htbl->size = 0;

    /* No shrink wait outstanding yet; see 'OHTBL_SHRINK_COOLDOWN_MS'
     * comment (ohtbl.h) */
    htbl->is_shrink_pending = false;

    return htbl;
}


/* Destroy the open-addressed hash table */
void ohtbl_destroy(ohtbl_td *htbl)
{
    if (htbl == NULL) {
        return;
    }

    if (htbl->destroy != NULL) {
        /* Free dynamically allocated data */
        for (size_t i = 0; i < htbl->positions; ++i) {
            /* Double check if the pointer address is valid */
            if (htbl->table[i] != NULL &&
                    htbl->table[i] != htbl->vacated) {
                htbl->destroy(htbl->table[i]);
                htbl->table[i] = htbl->vacated;
            }
        } /* ! for */
    }
    free(htbl->table);

    /* Free the allocated memory of the hash table */
    free(htbl);
}


/* Resets the open-addressed hash table */
void ohtbl_reset(ohtbl_td *htbl)
{
    if (htbl == NULL) {
        return;
    }

    /* Free dynamically allocated data */
    for (size_t i = 0; i < htbl->positions; ++i) {
        if (htbl->table[i] != NULL &&
                htbl->table[i] != htbl->vacated) {
            htbl->table[i] = htbl->vacated;
            htbl->size--;
        }
    } /* ! for */
}


/* Insert a new item in the hash table */
int ohtbl_insert(ohtbl_td *htbl, const void *data)
{
    size_t insert_pos = 0;
    bool has_insert_pos = false;
    size_t step;

    /* Re-dimension the table if size is bigger than
     * ('OHTBL_MAX_LOAD_FACTOR' * 100)% of its positions */
    if (htbl->size >=
            (size_t) ((float) htbl->positions * OHTBL_MAX_LOAD_FACTOR)) {
        if (s_ohtbl_resize_double(htbl) != 0) {
            return -2;
        }
    }

    /* Single-pass probe: detect duplicates and locate the first
     * available slot simultaneously using double hashing.
     * The first vacated slot is recorded as a candidate insertion
     * position; a null slot ends the probe chain (no duplicate can lie
     * beyond it), so we commit there immediately. */
    step = s_ohtbl_step(htbl->h2(data), htbl->positions);
    for (size_t i = 0; i < htbl->positions; ++i) {
        size_t position = (htbl->h1(data) +
                (i * step)) % htbl->positions;

        if (htbl->table[position] == NULL) {
            /* Empty slot ends the probe; prefer any earlier vacated
             * slot so deleted tombstones are reused first */
            if (!has_insert_pos) {
                insert_pos = position;
            }

            htbl->table[insert_pos] = (void *) data;
            htbl->size++;
            s_ohtbl_cancel_pending_shrink_if_recovered(htbl);
            return 0;
        } else if (htbl->table[position] == htbl->vacated) {
            /* Vacated slot: record as candidate but keep probing for
             * a possible duplicate further in the chain */
            if (!has_insert_pos) {
                insert_pos = position;
                has_insert_pos = true;
            }
        } else if (htbl->match(htbl->table[position], data)) {
            /* Duplicate found: do nothing */
            return 1;
        }
    } /* ! for */

    /* All positions probed; insert at the first vacated slot if one was
     * found (table is full of vacated/occupied but non-null) */
    if (has_insert_pos) {
        htbl->table[insert_pos] = (void *) data;
        htbl->size++;
        s_ohtbl_cancel_pending_shrink_if_recovered(htbl);
        return 0;
    }

    /* Return that the hash functions were selected incorrectly */
    return -1;
}


/* Update an existing element, or insert it as new if didn't exist */
int ohtbl_update(ohtbl_td *htbl, const void *data)
{
    size_t insert_pos = 0;
    bool has_insert_pos = false;
    size_t step;

    /* Re-dimension the table if size is bigger than
     * ('OHTBL_MAX_LOAD_FACTOR' * 100)% of its positions */
    if (htbl->size >=
            (size_t) ((float) htbl->positions * OHTBL_MAX_LOAD_FACTOR)) {
        if (s_ohtbl_resize_double(htbl) != 0) {
            return -2;
        }
    }

    /* Same single-pass probe */
    step = s_ohtbl_step(htbl->h2(data), htbl->positions);
    for (size_t i = 0; i < htbl->positions; ++i) {
        size_t position = (htbl->h1(data) +
                (i * step)) % htbl->positions;

        if (htbl->table[position] == NULL) {
            /* Empty slot ends the probe; prefer any earlier vacated
             * slot so deleted tombstones are reused first */
            if (!has_insert_pos) {
                insert_pos = position;
            }

            htbl->table[insert_pos] = (void *) data;
            htbl->size++;
            s_ohtbl_cancel_pending_shrink_if_recovered(htbl);
            return 0;
        } else if (htbl->table[position] == htbl->vacated) {
            /* Vacated slot: record as candidate but keep probing for
             * a possible match further in the chain, rather than
             * inserting a second copy of an existing key past its
             * own now-vacated original probe path */
            if (!has_insert_pos) {
                insert_pos = position;
                has_insert_pos = true;
            }
        } else if (htbl->match(htbl->table[position], data)) {
            /* Overwrite the old value with the new one */
            htbl->table[position] = (void *) data;
            return 0;
        }
    } /* ! for */

    /* All positions probed; insert at the first vacated slot if one was
     * found (table is full of vacated/occupied but non-null) */
    if (has_insert_pos) {
        htbl->table[insert_pos] = (void *) data;
        htbl->size++;
        s_ohtbl_cancel_pending_shrink_if_recovered(htbl);
        return 0;
    }

    /* Return that the hash functions were selected incorrectly */
    return -1;
}


/* Remove an item from the hash table */
int ohtbl_remove(ohtbl_td *htbl, void **data)
{
    size_t step = s_ohtbl_step(htbl->h2(*data), htbl->positions);

    for (size_t i = 0; i < htbl->positions; ++i) {
        size_t position = (htbl->h1(*data) +
                (i * step)) % htbl->positions;

        if (htbl->table[position] == NULL) {
            /*  Return that the data was not found */
            return -1;
        } else if (htbl->table[position] == htbl->vacated) {
            /* Search beyond vacated positions */
            continue;
        } else if (htbl->match(htbl->table[position], *data)) {
            /*  Pass back the data from the table */
            *data = htbl->table[position];
            htbl->table[position] = htbl->vacated;
            htbl->size--;

            /* Re-dimension the table if size has dropped below
             * ('OHTBL_MIN_LOAD_FACTOR' * 100)% of its positions,
             * once that has held continuously for
             * 'OHTBL_SHRINK_COOLDOWN_MS'; see that constant's own
             * doc comment (ohtbl.h) for why shrinking, unlike
             * growing, is worth delaying at all rather than acting
             * on it the moment it is first true */
            if (htbl->size < (size_t)
                    ((float) htbl->positions * OHTBL_MIN_LOAD_FACTOR)) {
                struct timespec now;
                bool cooldown_elapsed = true;

                if (clock_gettime(CLOCK_MONOTONIC, &now) == 0) {
                    if (!htbl->is_shrink_pending) {
                        htbl->is_shrink_pending = true;
                        htbl->shrink_eligible_since = now;
                        cooldown_elapsed = false;
                    } else {
                        int64_t elapsed_ms =
                            ((int64_t) now.tv_sec -
                                (int64_t)
                                    htbl->shrink_eligible_since.tv_sec)
                                * 1000 +
                            ((int64_t) now.tv_nsec -
                                (int64_t)
                                    htbl->shrink_eligible_since.tv_nsec)
                                / 1000000;

                        cooldown_elapsed = (elapsed_ms >=
                                (int64_t) OHTBL_SHRINK_COOLDOWN_MS);
                    }
                }
                /* else: the clock itself is unavailable; fall back
                 * to shrinking right away ('cooldown_elapsed'
                 * already defaults to 'true' above) rather than
                 * risk never shrinking at all */

                if (cooldown_elapsed) {
                    htbl->is_shrink_pending = false;

                    /* A return of '1' from 'ohtbl_resize_halve()'
                     * means the table is already at its minimum
                     * size and was intentionally left untouched,
                     * which is not an error; only a negative return
                     * (allocation failure) must turn this already-
                     * successful removal into an error, hence the
                     * '<0' and not '!=0' */
                    if (s_ohtbl_resize_halve(htbl) < 0) {
                        return -2;
                    }
                }
            }

            return 0;
        }
    } /* ! for */

    /* Return that the data was not found */
    return -1;
}


/* Look up in the hash table */
int ohtbl_lookup(const ohtbl_td *htbl, void **data)
{
    /* Use double hashing to hash the key */
    size_t step = s_ohtbl_step(htbl->h2(*data), htbl->positions);

    for (size_t i = 0; i < htbl->positions; ++i) {
        size_t position = (htbl->h1(*data) +
                (i * step)) % htbl->positions;

        if (htbl->table[position] == NULL) {
            /*  Return that the data was not found */
            return -1;
        } else if (htbl->table[position] == htbl->vacated) {
            /* Search beyond vacated positions */
            continue;
        } else if (htbl->match(htbl->table[position], *data)) {
            /* Pass back the data from the table */
            *data = htbl->table[position];
            return 0;
        }
    } /* ! for */

    /* Return that the data was not found */
    return -1;
}
