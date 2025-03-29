/**
 * @file cdlist.c
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

    /* Handle insertion at the tail of the list */
    if (item == NULL) {
        if (cdlist_size(cdlist) == 0) {
            new_item->next = new_item;  /* Point to itself */
            new_item->prev = new_item;  /* Point to itself */
            cdlist->head = new_item;
            cdlist->tail = new_item;
        } else {
            /* New item points to current head */
            new_item->next = cdlist->head;
            /* New item points back to current tail */
            new_item->prev = cdlist->tail;
            /* Current tail points to new item */
            cdlist->tail->next = new_item;
            /* Current head's previous pointer points to new item */
            cdlist->head->prev = new_item;
            /* Update tail to new item */
            cdlist->tail = new_item;
        }
    /* Handle insertion before the current item */
    } else {
        /* New item points to current item */
        new_item->next = item;
        /* New item points to previous item */
        new_item->prev = item->prev;

        /* Update the previous item's next pointer */
        if (item->prev != NULL) {
            /* Link the previous item's next to new item */
            item->prev->next = new_item;
        /* If there's no previous item, we're inserting at the head */
        } else {
            /* Update head if we're at the start */
            cdlist->head = new_item;
        }

        /* Update the current item's previous pointer */
        item->prev = new_item;

        /* If we're inserting at the head, update tail's next pointer */
        if (item == cdlist->head) {
            /* New head should go to the old head */
            cdlist->tail->next = new_item;
        }
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

    /* Handle insertion at the head of the list */
    if (item == NULL) {
        if (cdlist_size(cdlist) == 0) {
            new_item->next = new_item;  /* Point to itself */
            new_item->prev = new_item;  /* Point to itself */
            cdlist->head = new_item;
            cdlist->tail = new_item;
        } else {
            /* New item points to current head */
            new_item->next = cdlist->head;
            /* New item points to current tail */
            new_item->prev = cdlist->tail;
            /* Current tail points to new item */
            cdlist->tail->next = new_item;
            /* New item points back to current head */
            cdlist->head->prev = new_item;
            /* Update tail to new item */
            cdlist->tail = new_item;
        }
    } else {
        /* Handle insertion somewhere other than at the head */
        new_item->next = item->next;
        new_item->prev = item;

        if (item->next == NULL) {
            /* Inserting at tail */
            cdlist->tail = new_item;
        } else {
            /* Link new item to the next item's previous pointer */
            item->next->prev = new_item;
        }
        /* Link current item to new item */
        item->next = new_item;
        /* Link new item back to the next item */
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

    /* Do not allow removal from an empty list */
    if (cdlist_size(cdlist) == 0) {
        return -1;
    }

    /* Remove the item from the list */
    if (item == NULL) {
        /* Handle removal from the tail of the list */
        *data = cdlist->tail->data;
        old_item = cdlist->tail;

        if (cdlist_size(cdlist) == 1) {
            cdlist->head = NULL;  /* No items left */
            cdlist->tail = NULL;  /* No items left */
        } else {
            /* Move tail to the previous item */
            cdlist->tail = cdlist->tail->prev;
            /* New tail points to the head */
            cdlist->tail->next = cdlist->head;
            /* Head points back to new tail */
            cdlist->head->prev = cdlist->tail;
        }
    } else {
        /* Handle removal from somewhere other than the tail */
        if (item->prev == NULL) {
            return -1;  /* No item to remove */
        }

        *data = item->prev->data;
        old_item = item->prev;
        item->prev = old_item->prev;

        if (old_item->prev != NULL) {
            /* Link previous item to current item's previous */
            old_item->prev->next = item;
        } else {
            /* If we're removing the head, update head */
            cdlist->head = item;
        }

        if (old_item == cdlist->tail) {
            /* If we are removing the tail */
            cdlist->tail = item->prev;
        } else {
            /* Link current item's previous to the next item */
            item->prev->next = item;
        }
    }

    /* Free the storage allocated by the abstract data type */
    free(old_item);

    /* Adjust the size of the list */
    cdlist->size--;

    return 0;
}


/* Remove an item after a given item */
int cdlist_rem_next(cdlist_td *cdlist, cdlist_item_td *item,
        void **data)
{
    cdlist_item_td *old_item;

    /* Do not allow removal from an empty list */
    if (cdlist_size(cdlist) == 0) {
        return -1;
    }

    /* Remove the item from the list */
    if (item == NULL) {
        /* Handle removal from the head of the list */
        *data = cdlist->head->data;
        old_item = cdlist->head;

        if (cdlist_size(cdlist) == 1) {
            cdlist->head = NULL;  /* No items left */
            cdlist->tail = NULL;  /* No items left */
        } else {
            /* Move head to the next item */
            cdlist->head = cdlist->head->next;
            /* New head points back to the tail */
            cdlist->head->prev = cdlist->tail;
            /* Tail points to new head */
            cdlist->tail->next = cdlist->head;
        }
    } else {
        /* Handle removal from somewhere other than the head */
        if (item->next == NULL) {
            return -1;  /* No item to remove */
        }

        *data = item->next->data;
        old_item = item->next;
        item->next = old_item->next;

        if (old_item->next != NULL) {
            /* Link next item back to current item */
            old_item->next->prev = item;
        } else {
            /* If removing last item, update tail */
            cdlist->tail = item;
        }
    }

    /* Free the storage allocated by the abstract data type */
    free(old_item);

    /* Adjust the size of the list */
    cdlist->size--;

    return 0;
}
