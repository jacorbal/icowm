/**
 * @file tests/adt/test_heap.c
 *
 * @brief Test battery for the binary heap
 *
 * Uses a plain numeric comparator throughout ('s_int_compare'), which
 * per 'heap_insert''s own swap condition ('compare(parent, child) <
 * 0' triggers a swap) makes this a max-heap: the largest value
 * extracts first.
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
#include <adt/heap.h>
#include <harness/tap.h>


static int s_int_compare(const void *key1, const void *key2)
{
    int a = *(const int *) key1;
    int b = *(const int *) key2;

    return (a > b) - (a < b);
}


/** Counts how many times a 'destroy' callback actually ran */
static int s_destroy_calls = 0;

static void s_count_destroy(void *data)
{
    s_destroy_calls += 1;
    free(data);
}


/* Basic lifecycle: an empty heap reports itself correctly */
static void s_test_empty_heap(void)
{
    heap_td *heap = heap_init(s_int_compare, NULL);

    TAP_NOT_NULL(heap, "heap_init succeeds");
    TAP_EQ_INT(heap_size(heap), 0, "starts at size 0");
    TAP_OK(heap_is_empty(heap), "heap_is_empty agrees");

    heap_destroy(heap);
}


/* A single inserted node is both the size and the one node extracted */
static void s_test_single_insert_extract(void)
{
    heap_td *heap = heap_init(s_int_compare, NULL);
    int a = 42;
    void *out = NULL;

    heap_insert(heap, &a);
    TAP_EQ_INT(heap_size(heap), 1, "one item after insert");

    heap_extract(heap, &out);
    TAP_OK(out == &a, "the only node's own data came back");
    TAP_EQ_INT(heap_size(heap), 0, "back to size 0 after extracting it");

    heap_destroy(heap);
}


/* Nodes always extract in descending order (max-heap), regardless of
 * the order they were inserted in */
static void s_test_extraction_order(void)
{
    heap_td *heap = heap_init(s_int_compare, NULL);
    int values[] = {5, 1, 9, 3, 7, 2, 8, 4, 6, 0};
    int expected[] = {9, 8, 7, 6, 5, 4, 3, 2, 1, 0};
    size_t n = sizeof(values) / sizeof(values[0]);
    bool order_correct = true;

    for (size_t i = 0; i < n; ++i) {
        heap_insert(heap, &values[i]);
    }
    TAP_EQ_INT(heap_size(heap), (long) n, "every value inserted");

    for (size_t i = 0; i < n; ++i) {
        void *out = NULL;

        heap_extract(heap, &out);
        if (*(int *) out != expected[i]) {
            order_correct = false;
        }
    }

    TAP_OK(order_correct, "extraction order is fully descending");
    TAP_EQ_INT(heap_size(heap), 0, "empty again after extracting all");

    heap_destroy(heap);
}


/* Inserting past HEAP_MIN_CAPACITY forces the backing array to grow
 * at least once, and the heap keeps working correctly afterward */
static void s_test_grows_past_min_capacity(void)
{
    heap_td *heap = heap_init(s_int_compare, NULL);
    int values[20];
    void *out = NULL;

    for (int i = 0; i < 20; ++i) {
        values[i] = i;
        heap_insert(heap, &values[i]);
    }

    TAP_EQ_INT(heap_size(heap), 20, "all 20 items inserted");
    TAP_OK(heap->capacity >= 20u,
            "backing array grew to fit past HEAP_MIN_CAPACITY");

    heap_extract(heap, &out);
    TAP_EQ_INT(*(int *) out, 19,
            "the largest value still extracts first after growing");

    heap_destroy(heap);
}


/* Extracting enough nodes to fall under HEAP_SHRINK_LOAD_FACTOR
 * shrinks the backing array, but never below HEAP_MIN_CAPACITY, and
 * the heap keeps working correctly either way */
static void s_test_shrinks_but_not_below_min_capacity(void)
{
    heap_td *heap = heap_init(s_int_compare, NULL);
    int values[20];
    size_t capacity_after_growth;

    for (int i = 0; i < 20; ++i) {
        values[i] = i;
        heap_insert(heap, &values[i]);
    }
    capacity_after_growth = heap->capacity;

    /* Extract all but a couple of items, well under the 0.25 load
     * factor threshold */
    for (int i = 0; i < 18; ++i) {
        void *out = NULL;

        heap_extract(heap, &out);
    }

    TAP_EQ_INT(heap_size(heap), 2, "two items left");
    TAP_OK(heap->capacity < capacity_after_growth,
            "backing array shrank once occupancy fell low enough");
    TAP_OK(heap->capacity >= HEAP_MIN_CAPACITY,
            "backing array never shrinks below HEAP_MIN_CAPACITY");

    heap_destroy(heap);
}


/* Extracting the very last remaining node frees the backing array
 * entirely (size and capacity both reset to 0), and the heap is
 * still safely reusable afterward */
static void s_test_extract_last_item_resets_capacity(void)
{
    heap_td *heap = heap_init(s_int_compare, NULL);
    int a = 1;
    void *out = NULL;

    heap_insert(heap, &a);
    heap_extract(heap, &out);

    TAP_EQ_INT(heap_size(heap), 0, "size reset to 0");
    TAP_EQ_INT((long) heap->capacity, 0, "capacity reset to 0 as well");
    TAP_NULL(heap->tree, "backing array pointer reset to NULL");

    /* Still usable afterward */
    heap_insert(heap, &a);
    TAP_EQ_INT(heap_size(heap), 1, "still usable for a fresh insert");

    heap_destroy(heap);
}


/* Extracting from an empty heap fails cleanly instead of crashing */
static void s_test_extract_from_empty_fails(void)
{
    heap_td *heap = heap_init(s_int_compare, NULL);
    void *out = NULL;
    int rc;

    rc = heap_extract(heap, &out);
    TAP_EQ_INT(rc, -1, "extracting from an empty heap fails");

    heap_destroy(heap);
}


/* Equal-valued nodes (comparator returns 0) never break the heap
 * property or get lost */
static void s_test_duplicate_values(void)
{
    heap_td *heap = heap_init(s_int_compare, NULL);
    int values[] = {5, 5, 5, 5};
    size_t n = sizeof(values) / sizeof(values[0]);
    bool all_fives = true;

    for (size_t i = 0; i < n; ++i) {
        heap_insert(heap, &values[i]);
    }

    for (size_t i = 0; i < n; ++i) {
        void *out = NULL;

        heap_extract(heap, &out);
        if (*(int *) out != 5) {
            all_fives = false;
        }
    }

    TAP_OK(all_fives, "every duplicate value extracted intact");

    heap_destroy(heap);
}


/* heap_destroy runs the destroy callback exactly once per remaining
 * node */
static void s_test_destroy_calls_destroy_once_each(void)
{
    heap_td *heap = heap_init(s_int_compare, s_count_destroy);

    s_destroy_calls = 0;
    heap_insert(heap, malloc(sizeof(int)));
    heap_insert(heap, malloc(sizeof(int)));
    heap_insert(heap, malloc(sizeof(int)));

    heap_destroy(heap);

    TAP_EQ_INT(s_destroy_calls, 3, "destroy ran once per remaining item");
}


/* heap_destroy with a NULL heap is safe: nothing crashes */
static void s_test_destroy_null_is_safe(void)
{
    heap_destroy(NULL);
    TAP_OK(true, "destroying a NULL heap, no crash");
}


int main(void)
{
    TAP_PLAN(23);

    s_test_empty_heap();
    s_test_single_insert_extract();
    s_test_extraction_order();
    s_test_grows_past_min_capacity();
    s_test_shrinks_but_not_below_min_capacity();
    s_test_extract_last_item_resets_capacity();
    s_test_extract_from_empty_fails();
    s_test_duplicate_values();
    s_test_destroy_calls_destroy_once_each();
    s_test_destroy_null_is_safe();

    return TAP_DONE();
}
