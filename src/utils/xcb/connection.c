/**
 * @file utils/xcb/connection.c
 *
 * @brief The session's own connections to the X server, implementation
 */
/*
 * Copyright (c) 2026, J. A. Corbal.
 * All rights reserved.
 *
 * This file is licensed under the 'ISC License'.
 * Read the 'LICENSE' file in the root of this repository for details.
 */

/* System includes */
#include <stdlib.h>     /* NULL */

/* XCB includes */
#include <xcb/xcb.h>
#include <xcb/xcb_ewmh.h>

/* Local includes */
#include <utils/xcb/connection.h>


/**
 * @brief The connection this session talks to the server over
 *
 * One per session, opened by @a wm_start and closed by @a wm_stop.
 * Owned by neither this module nor anything that reads it here: this
 * only remembers where it is.
 */
static xcb_connection_t *s_connection = NULL;


/**
 * @brief The EWMH connection built over the one above
 *
 * Set up once the atoms EWMH needs have been interned, and owned by
 * @a wm_start, which allocates and frees it.
 */
static xcb_ewmh_connection_t *s_ewmh = NULL;


/* Record the connection this session talks to the server over */
void xcb_connection_set(xcb_connection_t *connection)
{
    s_connection = connection;
}


/* The connection this session talks to the server over */
xcb_connection_t *xcb_connection_get(void)
{
    return s_connection;
}


/* Record the EWMH connection built over the session's own */
void xcb_ewmh_connection_set(xcb_ewmh_connection_t *ewmh)
{
    s_ewmh = ewmh;
}


/* The EWMH connection built over the session's own */
xcb_ewmh_connection_t *xcb_ewmh_connection_get(void)
{
    return s_ewmh;
}
