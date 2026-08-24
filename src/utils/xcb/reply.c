/**
 * @file utils/xcb/reply.c
 *
 * @brief Reporting the reason an XCB request failed
 */
/*
 * Copyright (c) 2026, J. A. Corbal.
 * All rights reserved.
 *
 * This file is licensed under the 'ISC License'.
 * Read the 'LICENSE' file in the root of this repository for details.
 */

/* System includes */
#include <stddef.h>     /* NULL */
#include <stdlib.h>     /* free */

/* XCB includes */
#include <xcb/xcb.h>

/* Project includes */
#include <logger.h>

/* Local includes */
#include <utils/xcb/reply.h>


/* Log why a request failed, and release the error */
void xcb_reply_log_error(xcb_generic_error_t *error, const char *what)
{
    if (error == NULL) {
        return;
    }

    LOGGER_WARNING("The X server refused a request for %s" \
            " (code=%u, major=%u, minor=%u, resource=0x%x)",
            (what != NULL) ? what : "something",
            error->error_code, error->major_code, error->minor_code,
            error->resource_id);

    free(error);
}
