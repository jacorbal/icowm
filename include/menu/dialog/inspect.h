/**
 * @file menu/dialog/inspect.h
 *
 * @brief Window property inspector dialog
 *
 * Shows everything the window manager holds about one client, laid out
 * as labelled rows in a read-only dialog with a single "OK" button.
 * Nothing is asked of the X server: every field comes from the client
 * record the manager already keeps up to date, so opening this costs
 * a walk over one structure.
 *
 * The flags are spelled out rather than printed as a number, which is
 * what separates this from the transient popup reached by the sibling
 * @c info binding: that one is a glance, this one is a full account.
 *
 * Example usage:
 * @code{.c}
 * dialog_inspect_show(connection, stage, config, client);
 * @endcode
 *
 * @ingroup menu_dialog
 */

#ifndef MENU_DIALOG_INSPECT_H
#define MENU_DIALOG_INSPECT_H


/* System includes */
#include <xcb/xcb.h>

/* Project includes */
#include <client.h>
#include <config.h>
#include <stage.h>


/**
 * @brief Show the inspector for one client
 *
 * @param connection XCB connection
 * @param stage      Stage to show the dialog on
 * @param config     Active configuration
 * @param client     Client to describe; nothing is shown for @c NULL
 *
 * @note Silently does nothing without a client, so a binding pressed
 *       over the root window opens no empty dialog
 * @note Complexity: @e O(t), where @e t is the number of @p client's
 *       transient children, every other field being a direct read
 */
void dialog_inspect_show(xcb_connection_t *connection,
        stage_td *stage, const config_td *config,
        const client_td *client);


#endif  /* ! MENU_DIALOG_INSPECT_H */
