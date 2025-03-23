/**
 * @file ohtbl.c
 *
 * @brief Open-addressed hash table (closed hashing) implementation
 */

/* System includes */
#include <stdbool.h>    /* bool */
#include <stdlib.h>     /* calloc, malloc, free, NULL */

/* Local includes */
#include <adt/ohtbl.h>


/* Reserve a sentinel memory address for vacated elements */
static char _vacated;


/* Initialize a new open-addressed hash table */
ohtbl_td *ohtbl_init(size_t positions,
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
        return NULL;
    }

    /* Initialize each positions */
    htbl->positions = positions;

    for (size_t i = 0; i < htbl->positions; ++i) {
        htbl->table[i] = NULL;
    }

    /* Set the vacated member to the sentinel memory address reserved
     * for this */
    htbl->vacated = &_vacated;

    /* Encapsulate the functions */
    htbl->h1 = h1;
    htbl->h2 = h2;
    htbl->match = match;
    htbl->destroy = destroy;

    /* Initialize the number of elements in the table */
    htbl->size = 0;

    return htbl;
}


/* Destroy the open-addressed hash table */
void ohtbl_destroy(ohtbl_td *htbl)
{
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
    void *temp;
    size_t position;

    /* Re-dimension the table if size is bigger than
     * (OHTBL_MAX_LOAD_FACTOR * 100)% of its positions */
    if (htbl->size >=
            (size_t) ((float) htbl->positions * OHTBL_MAX_LOAD_FACTOR)) {
        if (ohtbl_resize(htbl) != 0) {
            return -2;
        }
    }

    /* Do not exceed the number of positions in the table */
    if (htbl->size == htbl->positions) {
        return -1;
    }

    /* Do nothing if the data is already in the table */
    temp = (void *) data;
    if (ohtbl_lookup(htbl, &temp) == 0) {
        return 1;
    }

    /* Use double hashing to hash the key */
    for (size_t i = 0; i < htbl->positions; ++i) {
        position = (htbl->h1(data) +
                (i * htbl->h2(data))) % htbl->positions;

        if (htbl->table[position] == NULL ||
                htbl->table[position] == htbl->vacated) {
            /* Insert the data into the table */
            htbl->table[position] = (void *) data;
            htbl->size++;
            return 0;
        }
    } /* ! for */

    /* Return that the hash functions were selected incorrectly */
    return -1;
}


/* Update an existing element, or insert it as new if didn't exist */
int ohtbl_update(ohtbl_td *htbl, const void *data)
{
    size_t position;

    /* Use double hashing to hash the key */
    for (size_t i = 0; i < htbl->positions; ++i) {
        position = (htbl->h1(data) +
                (i * htbl->h2(data))) % htbl->positions;

        if (htbl->table[position] == NULL ||
                htbl->table[position] == htbl->vacated) {
            /* Do not exceed the number of positions in the table */
            if (htbl->size == htbl->positions) {
                return -1;
            }
            /* Insert the element as new increasing the table size */
            htbl->table[position] = (void *) data;
            htbl->size++;
            return 0;
        } else if (htbl->match(htbl->table[position], data)) {
            /* Overwrite the old value with the new one*/
            htbl->table[position] = (void *) data;
            return 0;
        }
    } /* ! for */

    /* Return that nothing was done */
    return -1;
}


/* Remove an item from the hash table */
int ohtbl_remove(ohtbl_td *htbl, void **data)
{
    size_t position;

    for (size_t i = 0; i < htbl->positions; ++i) {
        position = (htbl->h1(*data) +
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
            return 0;
        }
    } /* ! for */

    /* Return that the data was not found */
    return -1;
}


/* Look up in the hash table */
int ohtbl_lookup(const ohtbl_td *htbl, void **data)
{
    size_t position;

    /* Use double hashing to hash the key */
    for (size_t i = 0; i < htbl->positions; ++i) {
        position = (htbl->h1(*data) +
                (i * htbl->h2(*data))) % htbl->positions;

        if (htbl->table[position] == NULL) {
            /*  Return that the data was not found */
            return -1;
        } else if (htbl->match(htbl->table[position], *data)) {
            /* Pass back the data from the table */
            *data = htbl->table[position];
            return 0;
        }
    } /* ! for */

    /* Return that the data was not found */
    return -1;
}


/* Resize the table if exceeds the maximum positions */
int ohtbl_resize(ohtbl_td *htbl)
{
    size_t new_positions;
    void **new_table;

    /* Set initial positions by doubling the previous value*/
    new_positions = htbl->positions * 2;

    /* Initialize a new table */
    new_table = malloc(new_positions * sizeof(void *));
    if (new_table == NULL) {
        return -1;
    }

    /* Re-hash previous elements in new table */
    for (size_t i = 0; i < htbl->positions; ++i) {
        void *element = htbl->table[i];
        if (element != NULL && element != htbl->vacated) {
            /* Only re-insert valid elements */
            size_t position;
            for (size_t j = 0; j < new_positions; ++j) {
                /* Search new position */
                position = (htbl->h1(element) +
                        (j * htbl->h2(element))) % new_positions;
                if (new_table[position] == NULL) {
                    /* Found empty position */
                    new_table[position] = element;
                    break;
                }
            } /* ! for (j) */
        }
    } /* ! for (i) */

    /* Deallocate previous table*/
    free(htbl->table);

    /* Update pointers and new positions */
    htbl->table = new_table;
    htbl->positions = new_positions;

    return 0;
}
