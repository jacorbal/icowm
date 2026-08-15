/**
 * @file tests/adt/test_ohtbl.c
 *
 * @brief Test battery for the open-addressed hash table
 *
 * Uses a deliberately simple pair of hash functions throughout: 'h1'
 * returns the key's own integer value, and 'h2' is a constant 1,
 * which makes probing linear.  This keeps slot placement predictable
 * for assertions without weakening what is actually being tested
 * (insertion, duplicate detection, tombstone reuse, resizing) -- the
 * table's own logic does not care whether probing is linear or a
 * real double hash, only that 'h2' never causes a probe cycle
 * shorter than the table itself, which a constant of 1 guarantees.
 */
/*
 * Copyright (c) 2026, J. A. Corbal.
 * All rights reserved.
 *
 * This file is licensed under the 'ISC License'.
 * Read the 'LICENSE' file in the root of this repository for details.
 */

/* System includes */
#include <stdbool.h>
#include <stdlib.h>

/* Local includes */
#include <adt/ohtbl.h>
#include <harness/tap.h>


static size_t s_hash1(const void *key)
{
    return (size_t) *(const int *) key;
}


static size_t s_hash2(const void *key)
{
    (void) key;
    return 1u;
}


static bool s_int_match(const void *key1, const void *key2)
{
    return *(const int *) key1 == *(const int *) key2;
}


/** Counts how many times a 'destroy' callback actually ran */
static int s_destroy_calls = 0;

static void s_count_destroy(void *data)
{
    s_destroy_calls += 1;
    free(data);
}


/* ohtbl_init resolves 'min_positions' according to its own documented
 * rule: a caller-given value below the initial 'positions' is kept
 * as-is (the whole point of passing one explicitly, so the table
 * never shrinks past it); a value of 0, or one at or above
 * 'positions' itself, falls back to 'positions' */
static void s_test_init_resolves_min_positions_correctly(void)
{
    ohtbl_td *below = ohtbl_init(16, 8, s_hash1, s_hash2,
            s_int_match, NULL);
    ohtbl_td *zero = ohtbl_init(16, 0, s_hash1, s_hash2,
            s_int_match, NULL);
    ohtbl_td *above = ohtbl_init(16, 32, s_hash1, s_hash2,
            s_int_match, NULL);

    TAP_EQ_INT((long) below->min_positions, 8,
            "a min_positions below the initial positions is kept as-is");
    TAP_EQ_INT((long) zero->min_positions, 16,
            "min_positions of 0 falls back to the initial positions");
    TAP_EQ_INT((long) above->min_positions, 16,
            "a min_positions above the initial positions also falls" \
            " back to it");

    ohtbl_destroy(below);
    ohtbl_destroy(zero);
    ohtbl_destroy(above);
}


/* Basic lifecycle: an empty table reports itself correctly */
static void s_test_empty_table(void)
{
    ohtbl_td *htbl = ohtbl_init(8, 8, s_hash1, s_hash2,
            s_int_match, NULL);

    TAP_NOT_NULL(htbl, "ohtbl_init succeeds");
    TAP_EQ_INT(ohtbl_size(htbl), 0, "starts at size 0");
    TAP_EQ_INT((long) htbl->positions, 8, "starts with the requested" \
            " positions");

    ohtbl_destroy(htbl);
}


/* A basic insert can be found again by lookup afterward */
static void s_test_insert_and_lookup(void)
{
    ohtbl_td *htbl = ohtbl_init(8, 8, s_hash1, s_hash2,
            s_int_match, NULL);
    int a = 5;
    int key = 5;
    void *found = &key;
    int rc_insert;
    int rc_lookup;

    rc_insert = ohtbl_insert(htbl, &a);
    TAP_EQ_INT(rc_insert, 0, "insert succeeds");
    TAP_EQ_INT(ohtbl_size(htbl), 1, "size is 1 after one insert");

    rc_lookup = ohtbl_lookup(htbl, &found);
    TAP_EQ_INT(rc_lookup, 0, "lookup finds the inserted item");
    TAP_OK(found == &a, "lookup's own data pointer is the original one");

    ohtbl_destroy(htbl);
}


/* Inserting the same key twice is rejected as a duplicate, and the
 * table's own size does not grow from the rejected attempt */
static void s_test_duplicate_insert_rejected(void)
{
    ohtbl_td *htbl = ohtbl_init(8, 8, s_hash1, s_hash2,
            s_int_match, NULL);
    int a = 5;
    int b = 5;
    int rc_first;
    int rc_second;

    rc_first = ohtbl_insert(htbl, &a);
    rc_second = ohtbl_insert(htbl, &b);

    TAP_EQ_INT(rc_first, 0, "first insert succeeds");
    TAP_EQ_INT(rc_second, 1, "second insert of an equal key is" \
            " rejected as a duplicate");
    TAP_EQ_INT(ohtbl_size(htbl), 1, "size is unaffected by the" \
            " rejected duplicate");

    ohtbl_destroy(htbl);
}


/* Looking up a key that was never inserted fails cleanly */
static void s_test_lookup_missing_fails(void)
{
    ohtbl_td *htbl = ohtbl_init(8, 8, s_hash1, s_hash2,
            s_int_match, NULL);
    int a = 5;
    int key = 99;
    void *found = &key;
    int rc;

    ohtbl_insert(htbl, &a);
    rc = ohtbl_lookup(htbl, &found);

    TAP_EQ_INT(rc, -1, "looking up a key that was never inserted fails");

    ohtbl_destroy(htbl);
}


/* Removing an item makes it unfindable afterward, and its slot
 * becomes a tombstone ('vacated') that a later insert reuses rather
 * than a fresh, previously-untouched slot */
static void s_test_remove_and_reuse_vacated_slot(void)
{
    ohtbl_td *htbl = ohtbl_init(8, 8, s_hash1, s_hash2,
            s_int_match, NULL);
    int a = 5;
    int b = 5;
    int key = 5;
    void *data = &key;
    int rc_remove;
    int rc_lookup_after_remove;

    ohtbl_insert(htbl, &a);
    rc_remove = ohtbl_remove(htbl, &data);

    TAP_EQ_INT(rc_remove, 0, "remove of an existing item succeeds");
    TAP_OK(data == &a, "remove's own data pointer is the removed item");
    TAP_EQ_INT(ohtbl_size(htbl), 0, "size drops back to 0 after remove");

    data = &key;
    rc_lookup_after_remove = ohtbl_lookup(htbl, &data);
    TAP_EQ_INT(rc_lookup_after_remove, -1,
            "the removed item is no longer found");

    /* Insert a fresh item with the same key; should succeed by
     * reusing the vacated slot rather than being treated as a
     * duplicate of the already-removed one */
    TAP_EQ_INT(ohtbl_insert(htbl, &b), 0,
            "re-inserting the same key after removal succeeds");
    TAP_EQ_INT(ohtbl_size(htbl), 1, "size is 1 again after re-insert");

    ohtbl_destroy(htbl);
}


/* Removing a key that was never inserted fails cleanly */
static void s_test_remove_missing_fails(void)
{
    ohtbl_td *htbl = ohtbl_init(8, 8, s_hash1, s_hash2,
            s_int_match, NULL);
    int key = 99;
    void *data = &key;
    int rc;

    rc = ohtbl_remove(htbl, &data);
    TAP_EQ_INT(rc, -1, "removing a key that was never inserted fails");

    ohtbl_destroy(htbl);
}


/* Inserting past OHTBL_MAX_LOAD_FACTOL doubles the table's own
 * positions automatically, and every previously inserted item is
 * still findable afterward (correctly rehashed) */
static void s_test_grows_past_max_load_factor(void)
{
    ohtbl_td *htbl = ohtbl_init(8, 4, s_hash1, s_hash2,
            s_int_match, NULL);
    int values[7];
    bool all_found = true;

    /* OHTBL_MAX_LOAD_FACTOR is 0.75; 6 of 8 positions already meets
     * or exceeds it, so this must trigger at least one resize */
    for (int i = 0; i < 7; ++i) {
        values[i] = i;
        ohtbl_insert(htbl, &values[i]);
    }

    TAP_EQ_INT(ohtbl_size(htbl), 7, "all 7 items inserted");
    TAP_OK(htbl->positions > 8u,
            "positions grew automatically past the load factor");

    for (int i = 0; i < 7; ++i) {
        int key = i;
        void *found = &key;

        if (ohtbl_lookup(htbl, &found) != 0 || found != &values[i]) {
            all_found = false;
        }
    }
    TAP_OK(all_found, "every item is still correctly found after" \
            " growing and rehashing");

    ohtbl_destroy(htbl);
}


/* Removing enough items to fall under OHTBL_MIN_LOAD_FACTOR shrinks
 * the table, but never below its own min_positions */
static void s_test_shrinks_but_not_below_min_positions(void)
{
    ohtbl_td *htbl = ohtbl_init(16, 8, s_hash1, s_hash2,
            s_int_match, NULL);
    int values[16];

    for (int i = 0; i < 16; ++i) {
        values[i] = i;
        ohtbl_insert(htbl, &values[i]);
    }

    /* Remove all but one, well under the 0.25 min load factor */
    for (int i = 0; i < 15; ++i) {
        int key = i;
        void *data = &key;

        ohtbl_remove(htbl, &data);
    }

    TAP_EQ_INT(ohtbl_size(htbl), 1, "one item left");
    TAP_OK(htbl->positions >= htbl->min_positions,
            "positions never fell below min_positions");
    TAP_EQ_INT((long) htbl->min_positions, 8,
            "min_positions itself is unchanged");

    ohtbl_destroy(htbl);
}


/* ohtbl_update inserts a genuinely new key exactly like ohtbl_insert
 * would, but overwrites the stored pointer in place for a key that
 * already exists, rather than rejecting it as a duplicate */
static void s_test_update_inserts_new_and_overwrites_existing(void)
{
    ohtbl_td *htbl = ohtbl_init(8, 8, s_hash1, s_hash2,
            s_int_match, NULL);
    int a = 5;
    int a_new = 5;
    int key = 5;
    void *found = &key;
    int rc_new;
    int rc_overwrite;

    rc_new = ohtbl_update(htbl, &a);
    TAP_EQ_INT(rc_new, 0, "update on a new key inserts it");
    TAP_EQ_INT(ohtbl_size(htbl), 1, "size is 1 after inserting via" \
            " update");

    rc_overwrite = ohtbl_update(htbl, &a_new);
    TAP_EQ_INT(rc_overwrite, 0, "update on an existing key overwrites" \
            " it");
    TAP_EQ_INT(ohtbl_size(htbl), 1, "size stays 1 after an overwrite," \
            " not a fresh insert");

    ohtbl_lookup(htbl, &found);
    TAP_OK(found == &a_new,
            "lookup now returns the overwritten pointer, not the" \
            " original one");

    ohtbl_destroy(htbl);
}


/* ohtbl_reset clears every stored item (size back to 0) without
 * changing the table's own positions, and the table stays usable
 * for fresh inserts afterward */
static void s_test_reset_clears_without_freeing_positions(void)
{
    ohtbl_td *htbl = ohtbl_init(8, 8, s_hash1, s_hash2,
            s_int_match, NULL);
    int a = 1, b = 2;
    size_t positions_before;

    ohtbl_insert(htbl, &a);
    ohtbl_insert(htbl, &b);
    positions_before = htbl->positions;

    ohtbl_reset(htbl);

    TAP_EQ_INT(ohtbl_size(htbl), 0, "size is 0 after reset");
    TAP_EQ_INT((long) htbl->positions, (long) positions_before,
            "positions themselves are unchanged by reset");

    TAP_EQ_INT(ohtbl_insert(htbl, &a), 0,
            "still usable for a fresh insert after reset");

    ohtbl_destroy(htbl);
}


/* ohtbl_foreach visits every currently stored item exactly once,
 * skipping both empty and vacated (tombstoned) slots */
static void s_test_foreach_visits_every_valid_item_once(void)
{
    ohtbl_td *htbl = ohtbl_init(8, 8, s_hash1, s_hash2,
            s_int_match, NULL);
    int values[3] = {1, 2, 3};
    int visit_count[3] = {0, 0, 0};
    int total_visits = 0;
    void *item;
    int key_to_remove = 2;
    void *removed = &key_to_remove;
    bool every_value_visited_once = true;

    for (int i = 0; i < 3; ++i) {
        ohtbl_insert(htbl, &values[i]);
    }
    /* Leave a tombstone behind, which foreach must skip */
    ohtbl_remove(htbl, &removed);

    ohtbl_foreach(htbl, item) {
        int v = *(int *) item;

        total_visits++;
        if (v >= 1 && v <= 3) {
            visit_count[v - 1]++;
        }
    }

    TAP_EQ_INT(total_visits, 2, "foreach visits exactly the" \
            " remaining, non-removed items");
    for (int i = 0; i < 3; ++i) {
        if (values[i] == 2) {
            continue;
        }
        if (visit_count[i] != 1) {
            every_value_visited_once = false;
        }
    }
    TAP_OK(every_value_visited_once,
            "each surviving item was visited exactly once");

    ohtbl_destroy(htbl);
}


/* ohtbl_init_quick is equivalent to calling ohtbl_init with the same
 * value for both 'positions' and 'min_positions' */
static void s_test_init_quick_matches_positions(void)
{
    ohtbl_td *htbl = ohtbl_init_quick(8, s_hash1, s_hash2,
            s_int_match, NULL);

    TAP_NOT_NULL(htbl, "ohtbl_init_quick succeeds");
    TAP_EQ_INT((long) htbl->positions, 8, "positions match the" \
            " requested value");
    TAP_EQ_INT((long) htbl->min_positions, 8, "min_positions defaults" \
            " to the same value");

    ohtbl_destroy(htbl);
}


/* ohtbl_destroy runs the destroy callback exactly once per remaining
 * item, but never for an already-removed (vacated) slot */
static void s_test_destroy_calls_destroy_once_each(void)
{
    ohtbl_td *htbl = ohtbl_init(8, 8, s_hash1, s_hash2,
            s_int_match, s_count_destroy);
    int *a = malloc(sizeof(int));
    int *b = malloc(sizeof(int));
    int *c = malloc(sizeof(int));
    int key_to_remove;
    void *removed;

    *a = 1;
    *b = 2;
    *c = 3;
    ohtbl_insert(htbl, a);
    ohtbl_insert(htbl, b);
    ohtbl_insert(htbl, c);

    /* Manually remove and free one, so destroy must not double-free
     * its slot on the way out */
    key_to_remove = 2;
    removed = &key_to_remove;
    ohtbl_remove(htbl, &removed);
    free(removed);

    s_destroy_calls = 0;
    ohtbl_destroy(htbl);

    TAP_EQ_INT(s_destroy_calls, 2,
            "destroy ran once per remaining (non-vacated) item");
}


/* ohtbl_destroy and ohtbl_reset with a NULL table are safe: nothing
 * crashes */
static void s_test_null_table_is_safe(void)
{
    ohtbl_destroy(NULL);
    TAP_OK(true, "destroying a NULL table, no crash");

    ohtbl_reset(NULL);
    TAP_OK(true, "resetting a NULL table, no crash");
}


int main(void)
{
    TAP_PLAN(43);

    s_test_init_resolves_min_positions_correctly();
    s_test_empty_table();
    s_test_insert_and_lookup();
    s_test_duplicate_insert_rejected();
    s_test_lookup_missing_fails();
    s_test_remove_and_reuse_vacated_slot();
    s_test_remove_missing_fails();
    s_test_grows_past_max_load_factor();
    s_test_shrinks_but_not_below_min_positions();
    s_test_update_inserts_new_and_overwrites_existing();
    s_test_reset_clears_without_freeing_positions();
    s_test_foreach_visits_every_valid_item_once();
    s_test_init_quick_matches_positions();
    s_test_destroy_calls_destroy_once_each();
    s_test_null_table_is_safe();

    return TAP_DONE();
}
