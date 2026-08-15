/**
 * @file tests/adt/test_list.c
 *
 * @brief Test battery for the singly linked list
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

/* Local includes */
#include <adt/list.h>
#include <harness/tap.h>


/** Counts how many times a 'destroy' callback actually ran, so a
 *  test can confirm 'list_clear'/'list_destroy' freed every item it
 *  should have, not just that the call didn't crash. */
static int s_destroy_calls = 0;

static void s_count_destroy(void *data)
{
    s_destroy_calls += 1;
    free(data);
}


/* Basic lifecycle: an empty list reports itself correctly */
static void s_test_empty_list(void)
{
    list_td *list = list_init(NULL);

    TAP_NOT_NULL(list, "list_init succeeds");
    TAP_EQ_INT(list_size(list), 0, "starts at size 0");
    TAP_NULL(list_head(list), "head is NULL when empty");
    TAP_NULL(list_tail(list), "tail is NULL when empty");
    TAP_OK(list_is_empty(list), "list_is_empty agrees");

    list_destroy(list);
}


/* list_ins_next(list, NULL, data) always inserts at the head, so
 * repeated calls build the list in reverse insertion order */
static void s_test_ins_next_null_builds_head(void)
{
    list_td *list = list_init(NULL);
    int a = 1, b = 2, c = 3;

    list_ins_next(list, NULL, &a);
    list_ins_next(list, NULL, &b);
    list_ins_next(list, NULL, &c);

    TAP_EQ_INT(list_size(list), 3, "three items inserted");
    TAP_EQ_INT(*(int *) list_data(list_head(list)), 3,
            "most recent ins_next(NULL) is the new head");
    TAP_EQ_INT(*(int *) list_data(list_tail(list)), 1,
            "first-ever insert ended up as the tail");

    list_destroy(list);
}


/* A single-item list has head == tail, and that item's own 'next'
 * is NULL (this list is not circular, unlike 'cdlist') */
static void s_test_single_item(void)
{
    list_td *list = list_init(NULL);
    int a = 42;

    list_ins_next(list, NULL, &a);

    TAP_EQ_INT(list_size(list), 1, "one item");
    TAP_OK(list_head(list) == list_tail(list),
            "head and tail are the same node");
    TAP_NULL(list_next(list_head(list)),
            "the only item's own next is NULL, not itself");
    TAP_OK(list_is_head(list, list_head(list)),
            "list_is_head agrees on the head");
    TAP_OK(list_is_tail(list_head(list)),
            "list_is_tail agrees on the same, only node");

    list_destroy(list);
}


/* Removing via rem_next(list, NULL, ...) always removes the head */
static void s_test_rem_next_null_removes_head(void)
{
    list_td *list = list_init(NULL);
    int a = 1, b = 2;
    void *removed = NULL;

    list_ins_next(list, NULL, &a);
    list_ins_next(list, NULL, &b);

    /* head is currently 'b' */
    list_rem_next(list, NULL, &removed);

    TAP_EQ_INT(list_size(list), 1, "one item removed, one remains");
    TAP_OK(removed == &b, "the old head's own data came back");
    TAP_OK(list_head(list) == list_tail(list),
            "the remaining item is now both head and tail");
    TAP_EQ_INT(*(int *) list_data(list_head(list)), 1,
            "'a' is the new head");

    list_destroy(list);
}


/* Removing the very last remaining item resets both head and tail
 * to NULL, leaving the list empty and reusable, not corrupted */
static void s_test_rem_next_last_item_resets_tail(void)
{
    list_td *list = list_init(NULL);
    int a = 1;
    void *removed = NULL;

    list_ins_next(list, NULL, &a);
    list_rem_next(list, NULL, &removed);

    TAP_EQ_INT(list_size(list), 0, "back to size 0");
    TAP_NULL(list_head(list), "head reset to NULL");
    TAP_NULL(list_tail(list), "tail reset to NULL");
    TAP_OK(removed == &a, "the removed item's own data came back");

    /* Still usable afterward */
    list_ins_next(list, NULL, &a);
    TAP_EQ_INT(list_size(list), 1, "list is still usable after emptying");

    list_destroy(list);
}


/* Removing after a specific item (not the head) exercises the
 * general, non-head path, including re-linking around the removed
 * node and updating the tail when the removed item was the tail */
static void s_test_rem_next_middle_and_tail(void)
{
    list_td *list = list_init(NULL);
    int a = 1, b = 2, c = 3;
    list_item_td *first;
    void *removed = NULL;

    list_ins_next(list, NULL, &c);
    list_ins_next(list, NULL, &b);
    list_ins_next(list, NULL, &a);
    /* list is now: a -> b -> c */
    first = list_head(list);

    /* Remove 'b' (the middle item), via rem_next from 'a' */
    list_rem_next(list, first, &removed);

    TAP_EQ_INT(list_size(list), 2, "one item removed, two remain");
    TAP_OK(removed == &b, "the middle item's own data came back");
    TAP_EQ_INT(*(int *) list_data(list_next(first)), 3,
            "the head's own next now skips straight to 'c'");
    TAP_OK(list_is_tail(list_next(first)),
            "'c' is still correctly the tail");

    /* Remove 'c' (now the tail), via rem_next from 'a' again */
    list_rem_next(list, first, &removed);

    TAP_EQ_INT(list_size(list), 1, "down to a single item");
    TAP_OK(removed == &c, "the tail's own data came back");
    TAP_OK(list_head(list) == list_tail(list),
            "the remaining single item is head and tail alike");
    TAP_NULL(list_next(list_head(list)),
            "the new tail's own next was correctly cleared");

    list_destroy(list);
}


/* rem_next fails cleanly (rather than corrupting anything) both on
 * an empty list and when asked to remove after the current tail */
static void s_test_rem_next_failure_cases(void)
{
    list_td *list = list_init(NULL);
    int a = 1;
    void *removed = NULL;
    int rc_empty;
    int rc_after_tail;

    rc_empty = list_rem_next(list, NULL, &removed);
    TAP_EQ_INT(rc_empty, -1, "rem_next on an empty list fails");

    list_ins_next(list, NULL, &a);
    rc_after_tail = list_rem_next(list, list_tail(list), &removed);
    TAP_EQ_INT(rc_after_tail, -1,
            "rem_next after the current tail fails (nothing follows it)");
    TAP_EQ_INT(list_size(list), 1,
            "the failed removal left the list untouched");

    list_destroy(list);
}


/* list_clear runs the destroy callback exactly once per item, and
 * leaves the list itself empty and reusable afterward, not freed */
static void s_test_clear_calls_destroy_once_each(void)
{
    list_td *list = list_init(s_count_destroy);

    s_destroy_calls = 0;
    list_ins_next(list, NULL, malloc(sizeof(int)));
    list_ins_next(list, NULL, malloc(sizeof(int)));
    list_ins_next(list, NULL, malloc(sizeof(int)));

    list_clear(list);

    TAP_EQ_INT(s_destroy_calls, 3, "destroy ran once per item");
    TAP_EQ_INT(list_size(list), 0, "list is empty after clear");
    TAP_OK(list_is_empty(list), "list_is_empty agrees");

    /* Still usable after clear */
    list_ins_next(list, NULL, malloc(sizeof(int)));
    TAP_EQ_INT(list_size(list), 1,
            "still usable for a fresh insert after clear");

    list_destroy(list);
}


/* list_clear/list_destroy with no destroy callback registered is
 * safe: nothing is called, nothing crashes */
static void s_test_null_destroy_callback_is_safe(void)
{
    list_td *list = list_init(NULL);
    int a = 1, b = 2;

    list_ins_next(list, NULL, &a);
    list_ins_next(list, NULL, &b);
    list_clear(list);

    TAP_EQ_INT(list_size(list), 0,
            "cleared without a destroy callback, no crash");

    list_destroy(list);
    TAP_OK(true, "destroy without a destroy callback, no crash");
}


int main(void)
{
    TAP_PLAN(39);

    s_test_empty_list();
    s_test_ins_next_null_builds_head();
    s_test_single_item();
    s_test_rem_next_null_removes_head();
    s_test_rem_next_last_item_resets_tail();
    s_test_rem_next_middle_and_tail();
    s_test_rem_next_failure_cases();
    s_test_clear_calls_destroy_once_each();
    s_test_null_destroy_callback_is_safe();

    return TAP_DONE();
}
