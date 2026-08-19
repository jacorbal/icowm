/**
 * @file adt/cdlist.c
 *
 * @brief Doubly linked circular list data structure implementation
 */

/* System includes */
#include <stdlib.h>     /* malloc, free, NULL */

/* Local includes */
#include <adt/cdlist.h>


/* Initialize a doubly linked circular list structure */
cdlist_td *cdlist_init(void (*destroy)(void *data))
{
    cdlist_td *cdlist;

    /* Allocate memory for the structure */
    cdlist = malloc(sizeof(cdlist_td));
    if (cdlist == NULL) {
        return NULL;
    }

    /* Initialize the list */
    cdlist->size = 0;
    cdlist->head = NULL;
    cdlist->tail = NULL;
    cdlist->destroy = destroy;

    return cdlist;
}


/* Destroy a doubly linked circular list */
void cdlist_destroy(cdlist_td *cdlist)
{
    /* Remove the entire list */
    cdlist_clear(cdlist);
    free(cdlist);
}


/* Clear a doubly linked circular list */
void cdlist_clear(cdlist_td *cdlist)
{
    void *data;

    /* Remove each item */
    while (cdlist_size(cdlist) > 0) {
        if ((cdlist_rem_next(cdlist, NULL, (void **) &data) == 0) &&
                cdlist->destroy != NULL) {
            cdlist->destroy(data);
        }
    }
}


/* Insert an item before a given item */
int cdlist_ins_prev(cdlist_td *cdlist, cdlist_item_td *item,
        const void *data)
{
    cdlist_item_td *new_item;

    new_item = malloc(sizeof(cdlist_item_td));
    if (new_item == NULL) {
        return -1;
    }

    /* Insert the item into the list */
    new_item->data = (void *) data;

    if (cdlist_size(cdlist) == 0) {
        /* Handle insertion at the tail of the list */
        new_item->next = new_item;  /* Point to itself */
        new_item->prev = new_item;  /* Point to itself */
        cdlist->head = new_item;
        cdlist->tail = new_item;
    } else if (item == NULL) {
        new_item->next = cdlist->head;
        new_item->prev = cdlist->tail;
        cdlist->tail->next = new_item;
        cdlist->head->prev = new_item;
        cdlist->tail = new_item;
    } else {
        cdlist_item_td *old_prev = item->prev;

        /* Handle insertion before the current item.  'old_prev' is
         * never NULL here: every node in this circular list always
         * has a valid 'prev' (pointing to itself when it is the only
         * one), so the only real decision is whether 'item' was the
         * head, not whether 'old_prev' exists. */
        new_item->next = item;
        new_item->prev = old_prev;
        old_prev->next = new_item;

        if (item == cdlist->head) {
            cdlist->head = new_item;
        }

        /* Update the current item's previous pointer */
        item->prev = new_item;
    }

    /* Adjust the size of the list to account for the inserted item */
    cdlist->size++;

    return 0;
}


/* Insert an item after a given item */
int cdlist_ins_next(cdlist_td *cdlist, cdlist_item_td *item,
        const void *data)
{
    cdlist_item_td *new_item;

    new_item = malloc(sizeof(cdlist_item_td));
    if (new_item == NULL) {
        return -1;
    }

    /* Insert the item into the list */
    new_item->data = (void *) data;

    if (cdlist_size(cdlist) == 0) {
        /* Handle insertion at the head of the list */
        new_item->next = new_item;  /* Point to itself */
        new_item->prev = new_item;  /* Point to itself */
        cdlist->head = new_item;
        cdlist->tail = new_item;
    } else if (item == NULL) {
        new_item->next = cdlist->head;
        new_item->prev = cdlist->tail;
        cdlist->tail->next = new_item;
        cdlist->head->prev = new_item;
        cdlist->head = new_item;
    } else {
        /* Handle insertion somewhere other than at the head */
        new_item->next = item->next;
        new_item->prev = item;

        /* Inserting at tail */
        if (item->next == cdlist->head) {
            cdlist->tail = new_item;
        }
        /* Link current item to new item */
        item->next = new_item;
        new_item->next->prev = new_item;
    }

    /* Adjust the size of the list to account for the inserted item */
    cdlist->size++;

    return 0;
}


/* Remove an item before a given item */
int cdlist_rem_prev(cdlist_td *cdlist, cdlist_item_td *item,
        void **data)
{
    cdlist_item_td *old_item;
    void *old_data = NULL;

    /* Do not allow removal from an empty list */
    if (cdlist_size(cdlist) == 0) {
        return -1;
    }

    if (item == NULL) {
        /* Handle removal from the tail of the list */
        old_item = cdlist->tail;
        old_data = old_item->data;

        if (cdlist_size(cdlist) == 1) {
            cdlist->head = NULL;  /* No items left */
            cdlist->tail = NULL;  /* No items left */
        } else {
            cdlist->tail = old_item->prev;
            cdlist->tail->next = cdlist->head;
            cdlist->head->prev = cdlist->tail;
        }
    } else {
        /* Handle removal from somewhere other than the tail */
        if (item->prev == NULL) {
            return -1;  /* No item to remove */
        }

        old_item = item->prev;
        old_data = old_item->data;
        item->prev = old_item->prev;

        if (old_item->prev != cdlist->tail) {
            /* Link prev item back to current item */
            old_item->prev->next = item;
        } else {
            /* Removing the head: update head and fix the circular
             * back-link so 'tail->next' points to the new head.
             * Without this, 'cdlist_next(cdlist_tail())' would
             * return a dangling pointer to the freed node */
            cdlist->head = item;
            cdlist->tail->next = item;
        }

        /* If the removed item was the tail (reached via
         * 'cdlist_next(tail) = head'), update the tail pointer and
         * fix the head's backward link.  The size guard excludes the
         * degenerate single-element circular case where
         * 'item == old_item' and the subsequent
         * 'cdlist_ins_prev(size==0)' resets head/tail. */
        if (old_item == cdlist->tail && cdlist->size > 1) {
            cdlist->tail = old_item->prev;
            cdlist->head->prev = cdlist->tail;
        }
    }

    /* Free the storage allocated by the abstract data type */
    free(old_item);

    if (data != NULL) {
        *data = old_data;
    }

    /* Adjust the size of the list */
    cdlist->size--;

    /* Same gap as 'cdlist_rem_next''s own fix, and the same reason:
     * the 'item != NULL' branch has no equivalent of the 'item ==
     * NULL' branch's explicit head/tail reset above, so removing a
     * degenerate single-element list's only item through it (item
     * and old_item the same node, reached whenever a caller passes
     * 'cdlist_next(tail)' as 'item') leaves 'cdlist->head'/'cdlist->
     * tail' pointing at now-freed memory. */
    if (cdlist->size == 0) {
        cdlist->head = NULL;
        cdlist->tail = NULL;
    }

    return 0;
}


/* Remove an item after a given item */
int cdlist_rem_next(cdlist_td *cdlist, cdlist_item_td *item,
        void **data)
{
    cdlist_item_td *old_item;
    void *old_data = NULL;

    /* Do not allow removal from an empty list */
    if (cdlist_size(cdlist) == 0) {
        return -1;
    }

    /* Remove the item from the list */
    if (item == NULL) {
        /* Handle removal from the head of the list */
        if (cdlist->head == NULL) {
            return -1;
        }

        old_data = cdlist->head->data;
        old_item = cdlist->head;

        if (cdlist_size(cdlist) == 1) { /* Only one item on the list */
            cdlist->head = NULL;  /* No items left */
            cdlist->tail = NULL;  /* No items left */
        } else {
            cdlist->head = cdlist->head->next;
            cdlist->head->prev = cdlist->tail;
            cdlist->tail->next = cdlist->head;
        }
    } else {
        /* Handle removal from somewhere other than the head */
        if (item->next == NULL) {
            return -1;  /* No item to remove */
        }

        old_item = item->next;
        old_data = old_item->data;
        item->next = old_item->next;

        if (old_item->next != cdlist->head) {
            /* Link next item back to current item */
            old_item->next->prev = item;
        } else {
            /* Removing the tail: update tail and fix the circular
             * back-link so 'head->prev' points to the new tail.
             * Without this, 'cdlist_prev(cdlist_head())' would return
             * a dangling pointer to the freed node */
            cdlist->tail = item;
            cdlist->head->prev = item;
        }

        /* If the removed item was the head (reached via
         * 'cdlist_prev(head) = tail'), update the head pointer and fix
         * the tail's forward link.  The size guard excludes the
         * degenerate single-element circular case where
         * 'item == old_item' and the subsequent
         * 'cdlist_ins_next(size==0)' resets head/tail. */
        if (old_item == cdlist->head && cdlist->size > 1) {
            cdlist->head = old_item->next;
            cdlist->tail->next = cdlist->head;
        }
    }

    /* Free the storage allocated by the abstract data type */
    free(old_item);

    if (data != NULL) {
        *data = old_data;
    }

    /* Adjust the size of the list */
    cdlist->size--;

    /* The 'item == NULL' branch above already resets head/tail to
     * NULL for the one-element case it handles directly (lines
     * 241-243), but the 'item != NULL' branch has no equivalent step
     * of its own: when 'item' and 'old_item' are the same node (the
     * degenerate single-element circular list, reached here whenever
     * a caller passes 'cdlist_prev(head)' as 'item' to remove the
     * list's only element, since 'cdlist_prev(head) == head' in that
     * case), every assignment above targets fields on that same node
     * about to be freed, so 'cdlist->head'/'cdlist->tail' are left
     * pointing at now-freed memory with nothing here to catch it.
     * Handling both branches in one place, after the free, closes
     * that gap without duplicating this check into each one. */
    if (cdlist->size == 0) {
        cdlist->head = NULL;
        cdlist->tail = NULL;
    }

    return 0;
}
