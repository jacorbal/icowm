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
 * @brief Cancel a shrink wait outstanding, if @p size has climbed
 *        back up to or above @c OHTBL_MIN_LOAD_FACTOR since it began
 *
 * Called right after @p size grows by one, so a burst of insertions
 * arriving before @c OHTBL_SHRINK_COOLDOWN_MS runs out drops the
 * whole pending shrink outright rather than merely pausing it; see
 * @c OHTBL_SHRINK_COOLDOWN_MS's own doc comment (ohtbl.h) for why
 * this is worth doing at all.
 *
 * @param htbl Table just grown by one element
 *
 * @note Complexity: @e O(1)
 */
static void s_ohtbl_cancel_pending_shrink_if_recovered(ohtbl_td *htbl)
{
    if (htbl->shrink_pending &&
            htbl->size >= (size_t)
                ((float) htbl->positions * OHTBL_MIN_LOAD_FACTOR)) {
        htbl->shrink_pending = false;
    }
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
     * own doc comment (ohtbl.h) */
    htbl->shrink_pending = false;

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

    /* Re-dimension the table if size is bigger than
     * (OHTBL_MAX_LOAD_FACTOR * 100)% of its positions */
    if (htbl->size >=
            (size_t) ((float) htbl->positions * OHTBL_MAX_LOAD_FACTOR)) {
        if (ohtbl_resize_double(htbl) != 0) {
            return -2;
        }
    }

    /* Single-pass probe: detect duplicates and locate the first
     * available slot simultaneously using double hashing.
     * The first vacated slot is recorded as a candidate insertion
     * position; a null slot ends the probe chain (no duplicate can lie
     * beyond it), so we commit there immediately. */
    for (size_t i = 0; i < htbl->positions; ++i) {
        size_t position = (htbl->h1(data) +
                (i * htbl->h2(data))) % htbl->positions;

        if (htbl->table[position] == NULL) {
            /* Empty slot ends the probe; prefer any earlier vacated
             * slot so deleted tombstones are reused first. */
            if (!has_insert_pos) {
                insert_pos = position;
            }

            htbl->table[insert_pos] = (void *) data;
            htbl->size++;
            s_ohtbl_cancel_pending_shrink_if_recovered(htbl);
            return 0;
        } else if (htbl->table[position] == htbl->vacated) {
            /* Vacated slot: record as candidate but keep probing for
             * a possible duplicate further in the chain. */
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

    /* Re-dimension the table if size is bigger than
     * (OHTBL_MAX_LOAD_FACTOR * 100)% of its positions, the same
     * guard 'ohtbl_insert' applies before probing at all, so the
     * probe below never needs to resize partway through itself */
    if (htbl->size >=
            (size_t) ((float) htbl->positions * OHTBL_MAX_LOAD_FACTOR)) {
        if (ohtbl_resize_double(htbl) != 0) {
            return -2;
        }
    }

    /* Same single-pass probe 'ohtbl_insert' uses: detect a match to
     * overwrite and locate the first available slot simultaneously
     * using double hashing.  The first vacated slot is recorded as a
     * candidate insertion position; a null slot ends the probe chain
     * (no match can lie beyond it), so we commit there immediately. */
    for (size_t i = 0; i < htbl->positions; ++i) {
        size_t position = (htbl->h1(data) +
                (i * htbl->h2(data))) % htbl->positions;

        if (htbl->table[position] == NULL) {
            /* Empty slot ends the probe; prefer any earlier vacated
             * slot so deleted tombstones are reused first. */
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
             * own now-vacated original probe path. */
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
    for (size_t i = 0; i < htbl->positions; ++i) {
        size_t position = (htbl->h1(*data) +
                (i * htbl->h2(*data))) % htbl->positions;

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
             * on it the moment it is first true. */
            if (htbl->size < (size_t)
                    ((float) htbl->positions * OHTBL_MIN_LOAD_FACTOR)) {
                struct timespec now;
                bool cooldown_elapsed = true;

                if (clock_gettime(CLOCK_MONOTONIC, &now) == 0) {
                    if (!htbl->shrink_pending) {
                        htbl->shrink_pending = true;
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
                    htbl->shrink_pending = false;

                    /* A return of '1' from 'ohtbl_resize_halve()'
                     * means the table is already at its minimum
                     * size and was intentionally left untouched,
                     * which is not an error; only a negative return
                     * (allocation failure) must turn this already-
                     * successful removal into an error, hence the
                     * '<0' and not '!=0'. */
                    if (ohtbl_resize_halve(htbl) < 0) {
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
    for (size_t i = 0; i < htbl->positions; ++i) {
        size_t position = (htbl->h1(*data) +
                (i * htbl->h2(*data))) % htbl->positions;

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


/* Resize the table to a new size */
int ohtbl_resize(ohtbl_td *htbl, size_t new_positions)
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
            for (size_t j = 0; j < new_positions; ++j) {
                /* Search new position */
                size_t position = (htbl->h1(element) +
                        (j * htbl->h2(element))) % new_positions;
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
     * 'OHTBL_SHRINK_COOLDOWN_MS''s own doc comment (ohtbl.h) */
    htbl->shrink_pending = false;

    return 0;
}


/* Doubles the current size of the open-addressed hash table */
int ohtbl_resize_double(ohtbl_td *htbl)
{
    size_t new_positions;

    /* Set initial positions by doubling the previous value */
    new_positions = (size_t) (htbl->positions * 2);
    return ohtbl_resize(htbl, new_positions);
}


/* Halves the current size of the open-addressed hash table */
int ohtbl_resize_halve(ohtbl_td *htbl)
{
    size_t new_positions;

    /* Set initial positions by halving the previous value */
    new_positions = (size_t) (htbl->positions / 2);

    /* Prevent the table to go below a threshold */
    if (new_positions < htbl->min_positions) {
        return 1;
    }

    return ohtbl_resize(htbl, new_positions);
}
