/**
 * @file tests/adt/test_cdlist.c
 *
 * @brief Test battery for the doubly linked circular list
 *
 * Weighted toward the single-element degenerate cases this module's
 * own comments already flag as previously buggy (see @c cdlist_rem_
 * prev and @c cdlist_rem_next's own doc comments in @c adt/cdlist.c):
 * removing a circular list's only item through @c cdlist_prev(head)
 * or @c cdlist_next(tail) rather than through @c NULL used to leave
 * @c head/@c tail pointing at freed memory.  Run under AddressSanitizer
 * (see the Makefile's own @c test target), so a regression of that
 * exact bug fails loudly, not just wrong values silently returned.
 */
/*
 * Copyright (c) 2026, J. A. Corbal.
 * All rights reserved.
 *
 * This file is licensed under the 'ISC License'.
 * Read the 'LICENSE' file in the root of this repository for details.
 */

/* System includes */
#include <stdlib.h>
#include <string.h>

/* Local includes */
#include <adt/cdlist.h>
#include <harness/tap.h>


/** Counts how many times a 'destroy' callback actually ran, so a
 *  test can confirm 'cdlist_clear'/'cdlist_destroy' freed every item
 *  it should have, not just that the call didn't crash. */
static int s_destroy_calls = 0;

static void s_count_destroy(void *data)
{
    s_destroy_calls += 1;
    free(data);
}


/* Basic lifecycle: an empty list reports itself correctly */
static void s_test_empty_list(void)
{
    cdlist_td *list = cdlist_init(NULL);

    TAP_NOT_NULL(list, "cdlist_init succeeds");
    TAP_EQ_INT(cdlist_size(list), 0, "starts at size 0");
    TAP_OK(cdlist_is_empty(list), "starts empty");
    TAP_NULL(cdlist_head(list), "head is NULL when empty");
    TAP_NULL(cdlist_tail(list), "tail is NULL when empty");

    cdlist_destroy(list);
}


/* ins_next(NULL, data) inserts at the head; repeated calls build the
 * list in reverse insertion order */
static void s_test_ins_next_null_builds_head(void)
{
    cdlist_td *list = cdlist_init(NULL);
    int a = 1, b = 2, c = 3;

    cdlist_ins_next(list, NULL, &a);
    cdlist_ins_next(list, NULL, &b);
    cdlist_ins_next(list, NULL, &c);

    TAP_EQ_INT(cdlist_size(list), 3, "three items inserted");
    TAP_EQ_INT(*(int *) cdlist_data(cdlist_head(list)), 3,
            "most recent ins_next(NULL) is the new head");
    TAP_EQ_INT(*(int *) cdlist_data(cdlist_tail(list)), 1,
            "first-ever insert stays at the tail");

    cdlist_destroy(list);
}


/* ins_prev(NULL, data) inserts at the tail; repeated calls build the
 * list in insertion order */
static void s_test_ins_prev_null_builds_tail(void)
{
    cdlist_td *list = cdlist_init(NULL);
    int a = 1, b = 2, c = 3;

    cdlist_ins_prev(list, NULL, &a);
    cdlist_ins_prev(list, NULL, &b);
    cdlist_ins_prev(list, NULL, &c);

    TAP_EQ_INT(cdlist_size(list), 3, "three items inserted");
    TAP_EQ_INT(*(int *) cdlist_data(cdlist_head(list)), 1,
            "first-ever insert stays at the head");
    TAP_EQ_INT(*(int *) cdlist_data(cdlist_tail(list)), 3,
            "most recent ins_prev(NULL) is the new tail");

    cdlist_destroy(list);
}


/* A single-item list is circular on itself: head and tail are the
 * same node, and next/prev of that node both point back to it */
static void s_test_single_item_is_self_circular(void)
{
    cdlist_td *list = cdlist_init(NULL);
    int a = 42;
    cdlist_item_td *only;

    cdlist_ins_next(list, NULL, &a);
    only = cdlist_head(list);

    TAP_OK(cdlist_head(list) == cdlist_tail(list),
            "single item is both head and tail");
    TAP_OK(cdlist_is_head(list, only), "cdlist_is_head agrees");
    TAP_OK(cdlist_is_tail(list, only), "cdlist_is_tail agrees");
    TAP_OK(cdlist_next(only) == only,
            "next of the only item is itself");
    TAP_OK(cdlist_prev(only) == only,
            "prev of the only item is itself");

    cdlist_destroy(list);
}


/* Removing a single-item list's only item through NULL (the
 * documented, ordinary path) resets head/tail to NULL */
static void s_test_remove_only_item_via_null(void)
{
    cdlist_td *list = cdlist_init(NULL);
    int a = 1;
    void *removed = NULL;

    cdlist_ins_next(list, NULL, &a);
    cdlist_rem_next(list, NULL, &removed);

    TAP_EQ_INT(cdlist_size(list), 0, "size drops to 0");
    TAP_NULL(cdlist_head(list), "head reset to NULL");
    TAP_NULL(cdlist_tail(list), "tail reset to NULL");
    TAP_OK(removed == &a, "removed data handed back correctly");

    cdlist_destroy(list);
}


/* The degenerate case 'cdlist_rem_next's own doc comment calls out
 * by name: removing a single-item list's only item by passing
 * 'cdlist_prev(head)' (which, on a one-node circular list, is that
 * same node) instead of NULL.  'item' and the node being freed are
 * the same node here; head/tail must still end up NULL, not
 * pointing at the just-freed node. */
static void s_test_remove_only_item_via_prev_head(void)
{
    cdlist_td *list = cdlist_init(NULL);
    int a = 1;
    cdlist_item_td *only;
    void *removed = NULL;

    cdlist_ins_next(list, NULL, &a);
    only = cdlist_head(list);

    cdlist_rem_next(list, cdlist_prev(only), &removed);

    TAP_EQ_INT(cdlist_size(list), 0,
            "size drops to 0 (rem_next via prev(head))");
    TAP_NULL(cdlist_head(list),
            "head reset to NULL, not left dangling");
    TAP_NULL(cdlist_tail(list),
            "tail reset to NULL, not left dangling");
    TAP_OK(removed == &a, "removed data handed back correctly");

    cdlist_destroy(list);
}


/* The mirror-image degenerate case for 'cdlist_rem_prev': removing a
 * single-item list's only item by passing 'cdlist_next(tail)'
 * (again, that same node on a one-node list) instead of NULL. */
static void s_test_remove_only_item_via_next_tail(void)
{
    cdlist_td *list = cdlist_init(NULL);
    int a = 1;
    cdlist_item_td *only;
    void *removed = NULL;

    cdlist_ins_prev(list, NULL, &a);
    only = cdlist_tail(list);

    cdlist_rem_prev(list, cdlist_next(only), &removed);

    TAP_EQ_INT(cdlist_size(list), 0,
            "size drops to 0 (rem_prev via next(tail))");
    TAP_NULL(cdlist_head(list),
            "head reset to NULL, not left dangling");
    TAP_NULL(cdlist_tail(list),
            "tail reset to NULL, not left dangling");
    TAP_OK(removed == &a, "removed data handed back correctly");

    cdlist_destroy(list);
}


/* Same degenerate single-item removal, but immediately followed by
 * inserting a fresh item: if head/tail were left dangling (the
 * original bug) rather than reset to NULL, this insert would follow
 * the "list already has items" branch instead of the "list is
 * empty" one, corrupting the new node's own links against freed
 * memory instead of starting a clean one-item list. */
static void s_test_insert_after_degenerate_removal(void)
{
    cdlist_td *list = cdlist_init(NULL);
    int a = 1, b = 2;
    cdlist_item_td *only;

    cdlist_ins_next(list, NULL, &a);
    only = cdlist_head(list);
    cdlist_rem_next(list, cdlist_prev(only), NULL);

    cdlist_ins_next(list, NULL, &b);

    TAP_EQ_INT(cdlist_size(list), 1, "back to a clean one-item list");
    TAP_OK(cdlist_head(list) == cdlist_tail(list),
            "new item is again both head and tail");
    TAP_EQ_INT(*(int *) cdlist_data(cdlist_head(list)), 2,
            "the new item's own data, not leftover state");
    TAP_OK(cdlist_next(cdlist_head(list)) == cdlist_head(list),
            "self-circular again, not linked to a freed node");

    cdlist_destroy(list);
}


/* Removing the middle of a three-item list relinks its neighbors
 * correctly, in both directions */
static void s_test_remove_middle_of_three(void)
{
    cdlist_td *list = cdlist_init(NULL);
    int a = 1, b = 2, c = 3;
    cdlist_item_td *first;
    void *removed = NULL;

    cdlist_ins_prev(list, NULL, &a);
    cdlist_ins_prev(list, NULL, &b);
    cdlist_ins_prev(list, NULL, &c);
    first = cdlist_head(list);

    /* Remove 'b' (the middle item) via rem_next from 'first' ('a') */
    cdlist_rem_next(list, first, &removed);

    TAP_EQ_INT(cdlist_size(list), 2, "one item removed, two remain");
    TAP_OK(removed == &b, "the middle item's own data came back");
    TAP_EQ_INT(*(int *) cdlist_data(cdlist_next(first)), 3,
            "the head's own next now skips straight to 'c'");
    TAP_EQ_INT(*(int *) cdlist_data(cdlist_prev(cdlist_head(list))),
            3, "circular link the other way also skips 'b'");

    cdlist_destroy(list);
}


/* Regression for a bug 'cdlist_ins_prev' used to have: inserting
 * before the current head (with more than one item already in the
 * list) linked every pointer correctly but never updated 'cdlist->
 * head' itself, silently leaving the new first item unreachable from
 * 'cdlist_head'.  The sibling 'cdlist_ins_next' never had this bug,
 * since it always compares against 'cdlist->head'/'cdlist->tail'
 * rather than against 'NULL', which 'cdlist_ins_prev' now does too. */
static void s_test_ins_prev_at_head_updates_head(void)
{
    cdlist_td *list = cdlist_init(NULL);
    int a = 1, b = 2, c = 3, x = 4;
    cdlist_item_td *old_head;

    cdlist_ins_prev(list, NULL, &a);
    cdlist_ins_prev(list, NULL, &b);
    cdlist_ins_prev(list, NULL, &c);
    old_head = cdlist_head(list);

    cdlist_ins_prev(list, old_head, &x);

    TAP_EQ_INT(cdlist_size(list), 4, "four items after the insert");
    TAP_EQ_INT(*(int *) cdlist_data(cdlist_head(list)), 4,
            "the new item is the new head");
    TAP_EQ_INT(*(int *) cdlist_data(cdlist_tail(list)), 3,
            "the tail is unaffected by a head-side insert");
    TAP_OK(cdlist_next(cdlist_head(list)) == old_head,
            "the old head immediately follows the new one");
    TAP_OK(cdlist_prev(cdlist_head(list)) == cdlist_tail(list),
            "the circular back-link from the new head reaches the tail");

    cdlist_destroy(list);
}


/* Regression for the same bug's own mirror image in 'cdlist_rem_
 * prev': removing the current head (by removing the item before its
 * own next neighbor, with more than one item left afterward) never
 * updated 'cdlist->head' either */
static void s_test_rem_prev_at_head_updates_head(void)
{
    cdlist_td *list = cdlist_init(NULL);
    int a = 1, b = 2, c = 3;
    void *removed = NULL;

    cdlist_ins_prev(list, NULL, &a);
    cdlist_ins_prev(list, NULL, &b);
    cdlist_ins_prev(list, NULL, &c);

    /* 'a' is the head; removing it via rem_prev from its own next
     * neighbor ('b') exercises the head-removal path */
    cdlist_rem_prev(list, cdlist_next(cdlist_head(list)), &removed);

    TAP_EQ_INT(cdlist_size(list), 2, "one item removed, two remain");
    TAP_OK(removed == &a, "the old head's own data came back");
    TAP_EQ_INT(*(int *) cdlist_data(cdlist_head(list)), 2,
            "'b' is the new head");
    TAP_OK(cdlist_prev(cdlist_head(list)) == cdlist_tail(list),
            "the circular back-link from the new head reaches the tail");

    cdlist_destroy(list);
}


/* cdlist_clear runs the destroy callback exactly once per item, and
 * leaves the list itself empty and reusable afterward, not freed */
static void s_test_clear_calls_destroy_once_each(void)
{
    cdlist_td *list = cdlist_init(s_count_destroy);
    int *a = malloc(sizeof(int));
    int *b = malloc(sizeof(int));
    int *c = malloc(sizeof(int));

    s_destroy_calls = 0;
    cdlist_ins_next(list, NULL, a);
    cdlist_ins_next(list, NULL, b);
    cdlist_ins_next(list, NULL, c);

    cdlist_clear(list);

    TAP_EQ_INT(s_destroy_calls, 3, "destroy ran once per item");
    TAP_EQ_INT(cdlist_size(list), 0, "list is empty after clear");
    TAP_OK(cdlist_is_empty(list), "cdlist_is_empty agrees");

    /* The list itself (not its items) is still alive: confirm it is
     * still usable, not just superficially zeroed.  Heap-allocated
     * like a/b/c above, not a stack variable: 'list' still has the
     * same 'destroy' callback bound, so the closing 'cdlist_destroy'
     * below will run it on this item too. */
    {
        int *d = malloc(sizeof(int));

        *d = 4;
        cdlist_ins_next(list, NULL, d);
        TAP_EQ_INT(cdlist_size(list), 1,
                "still usable for a fresh insert after clear");
    }

    cdlist_destroy(list);
}


/* cdlist_init(NULL) (no destroy callback) never calls anything on
 * clear/destroy, and does not crash doing nothing */
static void s_test_null_destroy_callback_is_safe(void)
{
    cdlist_td *list = cdlist_init(NULL);
    int a = 1;

    cdlist_ins_next(list, NULL, &a);
    cdlist_clear(list);

    TAP_EQ_INT(cdlist_size(list), 0,
            "cleared without a destroy callback, no crash");

    cdlist_destroy(list);
    TAP_OK(1, "destroy without a destroy callback, no crash");
}


int main(void)
{
    TAP_PLAN(51);

    s_test_empty_list();
    s_test_ins_next_null_builds_head();
    s_test_ins_prev_null_builds_tail();
    s_test_single_item_is_self_circular();
    s_test_remove_only_item_via_null();
    s_test_remove_only_item_via_prev_head();
    s_test_remove_only_item_via_next_tail();
    s_test_insert_after_degenerate_removal();
    s_test_remove_middle_of_three();
    s_test_ins_prev_at_head_updates_head();
    s_test_rem_prev_at_head_updates_head();
    s_test_clear_calls_destroy_once_each();
    s_test_null_destroy_callback_is_safe();

    return TAP_DONE();
}
