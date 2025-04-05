/**
 * @file safemem.c
 *
 * @brief Implementation of safe memory handling functions
 */

/* System includes */
#include <stdarg.h>     /* va_list, va_args, va_start, va_end */
#include <stdlib.h>     /* free */

/* Local includes */
#include <utils/safemem.h>


/* Free a allocated memory block if the pointer is not 'NULL' */
void safe_free(void **ptr)
{
    if (ptr && *ptr) {
        free(*ptr);
        *ptr = NULL;
    }
}


/* Free var dynamically allocated pointers */
int safe_free_var(void **first, ...)
{
    va_list args;
    void** ptr = first; /* The first argument is the first pointer */
    int index = 0;      /* Index to track the position */

    /* Return a negative error if the first pointer is 'NULL' */
    if (ptr == NULL) {
        return -1;
    }

    /* Start the argument list */
    va_start(args, first);

    /* Iterate until a 'NULL' pointer is found */
    while (ptr != NULL) {
        if (*ptr) {
            free(*ptr);
            *ptr = NULL;
        } else {
            /* Return the index of the first pointer that could not be
             * freed ('NULL' pointer) */
            va_end(args);
            return index;
        }

        /* Get the next pointer */
        ptr = va_arg(args, void**);
        index++;
    }

    /* End the argument list */
    va_end(args);

    /* All pointers were freed successfully */
    return 0;
}
