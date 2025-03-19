/**
 * @file list.c
 *
 * @brief Linked list data structure implementation
 */

/* System includes */
#include <stdlib.h>     /* malloc, free, NULL */

/* Local includes */
#include <adt/list.h>


/* Initialize a linked list structure */
list_td *list_init(void (*destroy)(void *data))
{
    list_td *list;

    /* Allocate memory for the structure */
    list = malloc(sizeof(list_td));
    if (list == NULL) {
        return NULL;
    }

    /* Initialize the list */
    list->size = 0;
    list->head = NULL;
    list->tail = NULL;
    list->match = NULL;
    list->destroy = destroy;

    return list;
}


/* Destroy a linked list */
void list_destroy(list_td *list)
{
    /* Remove the entire list */
    list_clear(list);
    free(list);
}


/* Clear a linked list */
void list_clear(list_td *list)
{
    void *data;

    /* Remove each item */
    while (list_size(list) > 0) {
        if ((list_rem_next(list, NULL, (void **) &data) == 0) &&
                list->destroy != NULL) {
            list->destroy(data);
        }
    }
}


/* Insert an item after a given item */
int list_ins_next(list_td *list, list_item_td *item,
        const void *data) {

    list_item_td *new_item;

    new_item = malloc(sizeof(list_item_td));
    if (new_item == NULL) {
        return -1;
    }

    /* Insert the item into the list */
    new_item->data = (void *) data;

    if (item == NULL) {
        /* Handle insertion at the head of the list */
        if (list_size(list) == 0) {
            list->tail = new_item;
        }

        new_item->next = list->head;
        list->head = new_item;
    } else {
        /* Handle insertion somewhere other than at the head */
        if (item->next == NULL) {
            list->tail = new_item;
        }

        new_item->next = item->next;
        item->next = new_item;
    }

    /* Adjust the size of the list to account for the inserted item */
    list->size++;

    return 0;
}


/* Remove an item after a given item */
int list_rem_next(list_td *list, list_item_td *item, void **data)
{
    list_item_td *old_item;

    /* Do not allow removal from an empty list */
    if (list_size(list) == 0) {
        return -1;
    }

    /* Remove the item from the list */
    if (item == NULL) {
        /* Handle removal from the head of the list */
        *data = list->head->data;
        old_item = list->head;
        list->head = list->head->next;

        if (list_size(list) == 1) {
            list->tail = NULL;
        }
    } else {
        /* Handle removal from somewhere other than the head */
        if (item->next == NULL) {
            return -1;
        }

        *data = item->next->data;
        old_item = item->next;
        item->next = item->next->next;

        if (item->next == NULL) {
            list->tail = item;
        }
    }

    /* Free the storage allocated by the abstract data type */
    free(old_item);

    /* Adjust the size of the list */
    list->size--;

    return 0;
}
