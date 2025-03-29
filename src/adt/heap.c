/**
 * @file heap.c
 *
 * @brief Heap data structure implementation
 */

/* Data type includes */
#include <stdbool.h>

/* System includes */
#include <stdlib.h>     /* malloc, free, NULL */

/* Local includes */
#include <adt/heap.h>


static size_t s_heap_parent(size_t npos)
{
    return (size_t) ((npos - 1) / 2);
}

static size_t s_heap_left(size_t npos)
{
    return (npos * 2) + 1;
}

static size_t s_heap_right(size_t npos)
{
    return (npos * 2) + 2;
}


/* Initialize a new heap structure */
heap_td *heap_init(int (*compare)(const void *key1, const void *key2),
        void (*destroy)(void *data))
{
    heap_td *heap;

    /* Allocate memory for the structure */
    heap = malloc(sizeof(heap_td));
    if (heap == NULL) {
        return NULL;
    }

    /* Initialize the heap */
    heap->size = 0;
    heap->compare = compare;
    heap->destroy = destroy;
    heap->tree = NULL;

    return heap;
}


/* Destroy a heap freeing its allocated memory */
void heap_destroy(heap_td *heap)
{
    /* Remove all the nodes from the heap */
    if (heap->destroy != NULL) {
        for (size_t i = 0; i < heap_size(heap); i++) {
            heap->destroy(heap->tree[i]);
        }
    }

    /* Free the storage allocated for the heap */
    free(heap->tree);
}


/* Insert a node into the heap */
int heap_insert(heap_td *heap, const void *data)
{
    void *temp;
    size_t ipos, ppos;

    /* Allocate storage for the node */
    if ((temp = (void **) realloc(heap->tree,
                    (heap_size(heap) + 1) * sizeof(void *))) == NULL) {
        return -1;
    } else {
        heap->tree = temp;
    }

    /* Insert the node after the last node */
    heap->tree[heap_size(heap)] = (void *) data;

    /* Heapify the tree by pushing the contents of the new node upward */
    ipos = heap_size(heap);
    ppos = s_heap_parent(ipos);

    while (ipos > 0 &&
            heap->compare(heap->tree[ppos], heap->tree[ipos]) < 0) {
        /* Swap the contents of the current node and its parent */
        temp = heap->tree[ppos];
        heap->tree[ppos] = heap->tree[ipos];
        heap->tree[ipos] = temp;

        /* Move up one level in the tree to continue heapifying */
        ipos = ppos;
        ppos = s_heap_parent(ipos);
    }

    /* Adjust the size of the heap to account for the inserted node */
    heap->size++;

    return 0;
}


/* Extract a node at the top of the heap */
int heap_extract(heap_td *heap, void **data)
{
    void *save, *temp;
    size_t ipos, lpos, rpos, mpos;

    /* Do not allow extraction from an empty heap */
    if (heap_size(heap) == 0) {
        return -1;
    }

    /* Extract the node at the top of the heap */
    *data = heap->tree[0];

    /* Adjust the storage used by the heap */
    save = heap->tree[heap_size(heap) - 1];

    if (heap_size(heap) - 1 > 0) {
        if ((temp = (void **) realloc(heap->tree,
                        (heap_size(heap) - 1) * sizeof(void *))) == NULL) {
            return -1;
        } else {
            heap->tree = temp;
        }
        /* Adjust the size of the heap to account for the extracted node */
        heap->size--;
    } else {
        /* Manage the heap when extracting the last node */
        free(heap->tree);
        heap->tree = NULL;
        heap->size = 0;

        return 0;
    }

    /* Copy the last node to the top */
    heap->tree[0] = save;

    /* Heapify the tree by pushing the contents of the new top downward */
    ipos = 0;
    lpos = s_heap_left(ipos);
    rpos = s_heap_right(ipos);

    while (true) {
        /* Select the child to swap with the current node */
        lpos = s_heap_left(ipos);
        rpos = s_heap_right(ipos);

        if (lpos < heap_size(heap) &&
                heap->compare(heap->tree[lpos], heap->tree[ipos]) > 0) {
            mpos = lpos;
        } else {
            mpos = ipos;
        }

        if (rpos < heap_size(heap) &&
                heap->compare(heap->tree[rpos], heap->tree[mpos]) > 0) {
            mpos = rpos;
        }

        /* When 'mpos' is 'ipos', the heap property has been restored */
        if (mpos == ipos) {
            break;
        } else {
            /* Swap the contents of the current node and the selected
             * child */
            temp = heap->tree[mpos];
            heap->tree[mpos] = heap->tree[ipos];
            heap->tree[ipos] = temp;

            /* Move down one level in the tree to continue heapifying */
            ipos = mpos;
        }
    }

    return 0;
}
