/**
 * @file utils/safemem.c
 *
 * @brief Implementation of safe memory handling functions
 */
/*
 * Copyright (c) 2026, J. A. Corbal.
 * All rights reserved.
 *
 * This file is licensed under the 'ISC License'.
 * Read the 'LICENSE' file in the root of this repository for details.
 */

/* System includes */
#include <stdarg.h>     /* va_args, va_list, va_start, va_end */
#include <stdlib.h>     /* free */

/* Local includes */
#include <utils/safe/safemem.h>


/* Free a allocated memory block if the pointer is non-null */
void safe_free(void **ptr)
{
    if (ptr && *ptr) {
        free(*ptr);
        *ptr = NULL;
    }
}


/* Free multiple dynamically allocated pointers */
int safe_free_var(void **first, ...)
{
    va_list args;

    /* Return a negative error if the first pointer is null */
    if (first == NULL) {
        return -1;
    }

    va_start(args, first);
    for (void **ptr = first;
            ptr != SAFE_FREE_VAR_END;
            ptr = va_arg(args, void **)) {
        safe_free(ptr);
    }
    va_end(args);

    return 0;
}
