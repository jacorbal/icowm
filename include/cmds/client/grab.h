/**
 * @file cmds/client/grab.h
 *
 * @brief Functions on a client's passive button grab
 *
 * @defgroup cmds Client, desktop, and surface commands
 * @ingroup enact
 */
/*
 * Copyright (c) 2026, J. A. Corbal.
 * All rights reserved.
 *
 * This file is licensed under the 'ISC License'.
 * Read the 'LICENSE' file in the root of this repository for details.
 */

#ifndef CMDS_CCMD_GRAB_H
#define CMDS_CCMD_GRAB_H


/* Project includes */
#include <types/handles.h>


/* Public interface */
/**
 * @brief Passively grab all mouse buttons on an undecorated client
 *
 * Installs a synchronous passive grab on the client window so the
 * window manager can focus the client on click before replaying or
 * consuming the button event.
 *
 * @param client Pointer to the client
 *
 * @note Implemented in @c cmds/client/grab.c
 * @note Complexity: @e O(1)
 */
void ccmd_client_grab_buttons(client_td *client);

/**
 * @brief Remove passive button grabs from an undecorated client
 *
 * Releases the passive grab installed by @c ccmd_client_grab_buttons so
 * mouse input flows directly to the client again.
 *
 * @param client Pointer to the client
 *
 * @note Complexity: @e O(1)
 */
void ccmd_client_ungrab_buttons(client_td *client);


#endif  /* ! CMDS_CCMD_GRAB_H */
