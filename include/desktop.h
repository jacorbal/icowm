/**
 * @file desktop.h
 *
 * @brief Desktop structure declaration
 *
 * Defines the structure that holds information and properties about
 * a particular workspace (desktop) within a client manager environment.
 * It includes a unique identifier, the window associated with the last
 * recorded event, the currently focused window, and additional metadata
 * such as the workspace name, workspace dimensions, and the available
 * area for clients.  It also contains references to hash tables and
 * lists that manage the stacking of clients within that workspace.
 *
 * @defgroup desktop Virtual desktop management
 * @ingroup surface
 */
/*
 * Copyright (c) 2026, J. A. Corbal.
 * All rights reserved.
 *
 * This file is licensed under the 'ISC License'.
 * Read the 'LICENSE' file in the root of this repository for details.
 */

#ifndef DESKTOP_H
#define DESKTOP_H


/* System includes */
#include <stdbool.h>
#include <sys/types.h>  /* pid_t */

/* XCB includes */
#include <xcb/xcb.h>
#include <xcb/xcb_ewmh.h>

/* ADT includes */
#include <adt/cdlist.h> /* Doubly linked circular list */
#include <adt/ohtbl.h>  /* Open-addressed hash table (closed hashing) */

/* Default initial values */
#include <defs/desktop.h>

/* Project includes */
#include <client.h>
#include <config.h>


/** Stable primary seed used by desktop client hash tables */
#define DESKTOP_HASH_SEED_PRIMARY (0x9E3779B9u)

/** Stable secondary seed used by desktop client hash tables */
#define DESKTOP_HASH_SEED_SECONDARY (0x85EBCA6Bu)


/**
 * @brief Structure for a virtual desktop within an XCB screen
 *
 * Each desktop can be customized with unique backgrounds and themes,
 * where the background can either be a solid color or a pixmap image.
 * The structure tracks its own active client, facilitating the
 * management of user interactions within that desktop space.
 *
 * The @p is_outdated flag serves to identify when the desktop's
 * attributes or properties have changed and need to be updated,
 * ensuring that users always have access to the most current
 * information about their environment.
 */
typedef struct desktop_s {
    xcb_connection_t *connection;           /**< XCB connection */
    xcb_ewmh_connection_t *ewmh;            /**< EWMH connection */
    uint32_t screen_id;                     /**< Screen index */

    /**
     * @brief Resolved pointer to this desktop's own XCB screen
     *
     * Resolved once, in @a desktop_init, from the same
     * 'xcb_setup_roots_iterator' walk already needed there to read
     * this screen's own pixel dimensions; kept here afterward so
     * every later caller that needs this desktop's own screen (e.g.,
     * @a desktop_render_background, render/desktop.c) reads this
     * field directly instead of repeating that same O(n) walk again
     * from scratch, an O(1) lookup either way.
     */
    xcb_screen_t *screen;

    xcb_window_t id;                        /**< Desktop index */

    char name[WM_DESKTOP_MAX_LENGTH_NAME];  /**< Desktop name */

    struct background_s {
        bool is_image;                      /**< BG color or image? */
        bool use_root_pixmap;               /**< Reuse externally set
                                                 root pixmap on redraw */
        union {
            uint32_t color;                 /**< Background color */
            char *image_path;               /**< Background image */
        } bg;                               /**< Background information */
    } background;
    /* No 'bg_applied_once'/'bg_color_applied' cache here: the root
     * window a solid-color background actually paints is one single X
     * resource shared by every desktop on the same screen, so that
     * cache lives per screen instead (see 's_root_bg_applied_once' in
     * render/desktop.c), not per desktop. */

    ohtbl_td *clients;                      /**< Clients hash table */
    cdlist_td *stacking;                    /**< Stacking list */
    xcb_window_t client_active_id;          /**< Active window */

    struct config_base_s *config_base;      /**< Base configuration */
    struct config_theme_s *config_theme;    /**< Theme configuration */

    struct geometry_s geometry;
    struct geometry_s workarea;

    bool is_outdated;                       /**< Flag when data needs to
                                                 be updated */
    bool focus_dirty;                       /**< Active client changed
                                                 since last render pass;
                                                 decoration colors must be
                                                 refreshed on all clients */

    /**
     * @brief Whether at least one client on this desktop currently
     *        has its own urgency hint set
     *
     * Kept correct by @c desktop_action_recompute_urgent
     * (desktop/dclient.c), called from every site that could change
     * the answer: a client's own urgency being set or cleared
     * (@c ccmd_client_urge/_unurge, cmds/client/basic.c) while
     * already on this desktop, and a client entering or leaving it
     * altogether (@c desktop_action_client_add/_rem, this same
     * file), which already covers a client created already urgent,
     * one destroyed while still urgent, and one sent to a different
     * desktop while still urgent -- every one of those changes who
     * this desktop's own set of clients is, not a client already on
     * it changing its own urgency, the other case those two
     * functions exist to handle instead.  Not consulted by anything
     * yet: a hook for a future feature (e.g., drawing this desktop's
     * own entry differently while the surface is showing a different
     * one), included now so a client's own urgency is never missed
     * regardless of which desktop it lands on.
     */
    bool is_urgent;
} desktop_td;


/* Public interface */
/**
 * @brief Initialize a new desktop
 *
 * @param connection   Pointer to the XCB connection
 * @param screen_id    Screen identifier where this desktop belongs
 * @param desktop_id   Desktop identifier
 * @param ewmh         EWMH connection pointer
 * @param config_base  Pointer to base configuration
 * @param config_theme Pointer to theme configuration
 *
 * @return Pointer to new desktop or @c NULL otherwise
 *
 * @note Complexity: @e O(1)
 */
desktop_td *desktop_init(xcb_connection_t *connection,
        xcb_ewmh_connection_t *ewmh,
        uint32_t screen_id, uint32_t desktop_id,
        struct config_base_s *config_base,
        struct config_theme_s *config_theme);

/**
 * @brief Free memory for allocated desktop
 *
 * @param desktop Desktop to deallocate
 *
 * @note Complexity: @e O(n), where @e n is the number of managed
 *       clients, as it iterates through the array of clients to free
 *       each one of them
 */
void desktop_destroy(desktop_td *desktop);

/**
 * @brief Soft desktop update
 *
 * @param desktop Pointer to the desktop to update softly
 *
 * @note Complexity: @e O(1)
 */
void desktop_update(desktop_td *desktop);

/**
 * @brief Full desktop update
 *
 * Updates the desktop by updating all its clients.
 *
 * @param desktop Pointer to the desktop to update fully
 *
 * @note Complexity: @e O(1) because that's the order of the access to
 *       a hash table of clients
 */
void desktop_update_full(desktop_td *desktop);

/**
 * @brief Clear a desktop by removing all its clients
 *
 * Deallocates each and every client of the desktop and resets the
 * client counter to zero.
 *
 * @param desktop Pointer to the desktop to be cleared from clients
 *
 * @note Complexity: @e O(n), where @e n is the number of managed
 *       clients, as it iterates through the array of clients to free
 *       each one of them
 */
void desktop_clear(desktop_td *desktop);

/**
 * @brief Add a previously allocated client to the desktop
 *
 * @param desktop Pointer to the desktop where to add the new client
 * @param client  Pointer to the client to be added to the desktop
 *
 * @return Status of the operation
 * @retval  0 Success on removal
 * @retval  1 Failed to perform the action
 *
 * @note Complexity: @e O(1)
 */
int desktop_action_client_add(desktop_td *desktop, client_td *client);

/**
 * @brief Remove a client from the desktop
 *
 * @param desktop Pointer to the desktop where to remove the client
 * @param client  Pointer to the client to be removed from the desktop
 *
 * @return Status of the operation
 * @retval  0 Success on removal
 * @retval  1 Failed to perform the action
 *
 * @note Complexity: @e O(1)
 */
int desktop_action_client_rem(desktop_td *desktop, client_td *client);

/**
 * @brief Recompute @c desktop->is_urgent from scratch, against every
 *        client currently on it
 *
 * @param desktop Desktop to recompute; a no-op if @c NULL or its own
 *                @c clients table is
 *
 * @note Complexity: @e O(n), where @e n is the number of clients
 *       currently on @p desktop
 */
void desktop_action_recompute_urgent(desktop_td *desktop);

/**
 * @brief Rename the desktop
 *
 * @param desktop Pointer to the desktop to receive the action
 * @param name    New name for the desktop
 *
 * @return Status of the operation
 * @retval  0 Success
 * @retval  1 Failed to perform the action
 *
 * @note Complexity: @e O(1)
 */
int desktop_action_rename(desktop_td *desktop, const char *name);

/**
 * @brief Send a client to another desktop
 *
 * @param desktop    Pointer to the desktop to receive the action
 * @param client     Pointer to the client to be sent
 * @param desktop_id Destination desktop identifier
 *
 * @return Status of the operation
 * @retval  0 Success
 * @retval  1 Failed to perform the action
 *
 * @note Complexity: @e O(1)
 */
int desktop_action_send_client(desktop_td *desktop, client_td *client,
        uint32_t desktop_id);

/**
 * @brief Update the desktop background color
 *
 * @param desktop Pointer to the desktop to receive the action
 * @param color   New color
 *
 * @return Status of the operation
 * @retval  0 Success
 * @retval  1 Failed to perform the action
 *
 * @note Complexity: @e O(1)
 */
int desktop_action_background_update(desktop_td *desktop,
        uint32_t color);

/**
 * @brief Set a client to the front
 *
 * Brings the specified client to the top of the stacking order.
 *
 * @param desktop Pointer to the desktop to receive the action
 * @param client  Pointer to the client to be sent to the front
 *
 * @return Status of the operation
 * @retval  0 Success
 * @retval  1 Failed to perform the action
 *
 * @note Complexity: @e O(1)
 */
int desktop_action_client_send_front(desktop_td *desktop,
        client_td *client);

/**
 * @brief Set a client to the back
 *
 * Sends the specified client to the bottom of the stacking order.
 *
 * @param desktop Pointer to the desktop to receive the action
 * @param client  Pointer to the client to be sent to the back
 *
 * @return Status of the operation
 * @retval  0 Success
 * @retval  1 Failed to perform the action
 *
 * @note Complexity: @e O(1)
 */
int desktop_action_client_send_back(desktop_td *desktop,
        client_td *client);

/**
 * @brief Rearrange clients on the current desktop
 *
 * Alters the positions of clients on the current desktop.
 *
 * @param desktop Pointer to the desktop to receive the action
 *
 * @return Status of the operation
 * @retval  0 Success
 * @retval  1 Failed to perform the action
 *
 * @note Complexity: @e O(n), where @e n is the number of clients to
 *       rearrange
 */
int desktop_action_clients_rearrange(desktop_td *desktop);

/**
 * @brief Iconify (minimize) all clients on the current desktop
 *
 * Sets all visible clients on the current desktop to an iconified state
 * (also, technically, minimized).
 *
 * @param desktop Pointer to the desktop to receive the action
 *
 * @return Status of the operation
 * @retval  0 Success
 * @retval  1 Failed to perform the action
 *
 * @note Complexity: @e O(n), where @e n is the number of clients on the
 *       desktop
 */
int desktop_action_clients_iconify_all(desktop_td *desktop);

/**
 * @brief Restore every iconified client on the current desktop
 *
 * Sets every client on the current desktop that is currently iconified
 * (minimized) back to its normal state.  Clients that are not
 * iconified are left untouched.
 *
 * @param desktop Pointer to the desktop to receive the action
 *
 * @return Status of the operation
 * @retval  0 Success
 * @retval  1 Failed to perform the action
 *
 * @note Complexity: @e O(n), where @e n is the number of clients on the
 *       desktop
 */
int desktop_action_clients_deiconify_all(desktop_td *desktop);

/**
 * @brief Cycle through active clients on the current desktop
 *
 * @param desktop Pointer to the desktop to receive the action
 *
 * @return Status of the operation
 * @retval  0 Success
 * @retval  1 Failed to perform the action
 *
 * @note Complexity: @e O(n), where @e n is the number of active clients
 */
int desktop_action_cycle_clients_active(desktop_td *desktop);

/**
 * @brief Cycle through active clients in reverse order on the desktop
 *
 * Focuses the nearest non-iconified client that comes before the
 * currently active client in the stacking order.
 *
 * @param desktop Pointer to the desktop to receive the action
 *
 * @return Status of the operation
 * @retval  0 Success
 * @retval -1 Invalid desktop pointer
 *
 * @note Complexity: @e O(n), where @e n is the number of active clients
 */
int desktop_action_cycle_clients_prev(desktop_td *desktop);

/**
 * @brief Cycle through iconified clients on the current desktop
 *
 * @param desktop Pointer to the desktop to receive the action
 *
 * @return Status of the operation
 * @retval  0 Success
 * @retval  1 Failed to perform the action
 *
 * @note Complexity: @e O(n), where @e n is the number of iconified
 *       clients
 */
int desktop_action_cycle_clients_icons(desktop_td *desktop);

/**
 * @brief Lock the current desktop session, preventing unauthorized
 *        access
 *
 * @param desktop Pointer to the desktop to receive the action
 *
 * @return Status of the operation
 * @retval  0 Success
 * @retval  1 Failed to perform the action
 *
 * @note Complexity: @e O(1)
 */
int desktop_action_lock(desktop_td *desktop);

/**
 * @brief Unlock the current desktop session, allowing user access
 *
 * @param desktop Pointer to the desktop to receive the action
 *
 * @return Status of the operation
 * @retval  0 Success
 * @retval  1 Failed to perform the action
 *
 * @note Complexity: @e O(1)
 */
int desktop_action_unlock(desktop_td *desktop);

/**
 * @brief Change the layout of the current desktop
 *
 * @param desktop Pointer to the desktop to receive the action
 * @param layout  New layout configuration
 *
 * @return Status of the operation
 * @retval  0 Success
 * @retval  1 Failed to perform the action
 *
 * @note Complexity: @e O(1)
 */
int desktop_action_set_layout(desktop_td *desktop, const char *layout);

/**
 * @brief Launch a new process
 *
 * @param desktop         Pointer to the desktop to receive the action
 * @param executable_path Path to the binary file
 *
 * @return Status of the operation
 * @retval  0 Success
 * @retval  1 Failed to perform the action
 *
 * @note Complexity: @e O(1)
 */
int desktop_action_process_launch(desktop_td *desktop,
        const char *executable_path);


int desktop_action_process_launch_with_class(desktop_td *desktop,
        const char *executable_path, const char *class_name);

/**
 * @brief Terminate a process
 *
 * Stops the specified process that is running in the desktop session by
 * killing it.
 *
 * @param desktop    Pointer to the desktop to receive the action
 * @param process_id Identifier of the process to be terminated
 *
 * @return Status of the operation
 * @retval  0 Success
 * @retval  1 Failed to perform the action
 *
 * @note Complexity: @e O(1)
 */
int desktop_action_process_kill(desktop_td *desktop, pid_t process_id);

/**
 * @brief Recompute the desktop work area from active client struts,
 *        the systray's own reservation, and configured margins
 *
 * Scans all clients in the stacking list for non-zero @c _NET_WM_STRUT
 * / @c _NET_WM_STRUT_PARTIAL values, folds in @p systray_strut (the
 * window manager's own built-in systray reservation, aggregated
 * exactly like a client's strut since the systray's own dock window
 * is never itself a managed client -- see @c systray_get_reserved_
 * strut) the same way, adds @p config_desktop's own @c margins on top
 * of that (see @c config_desktop_s in config.h, for a program that
 * reserves screen space without publishing either property itself),
 * and subtracts the combined maximum reservation on each edge from
 * the full screen dimensions.  For partial struts, the corresponding
 * start/end range is honored so reservations that do not overlap the
 * screen edge span are ignored; configured margins always apply along
 * the whole edge, having no start/end range of their own to honor.
 * The result is stored in @p desktop->workarea and broadcast to the
 * X server as @c _NET_WORKAREA.
 *
 * Call this after a panel (strut client) is mapped or unmapped, after
 * the systray's own reservation changes (reposition, resize, or being
 * shown/hidden), and after a configuration reload that may have
 * changed @c margins, so that maximize and smart-placement work on
 * the correct available area.
 *
 * @param desktop        Desktop whose work area should be refreshed
 * @param screen_w       Full screen width in pixels
 * @param screen_h       Full screen height in pixels
 * @param config_desktop Active desktop-behavior configuration, for
 *                       its @c margins; a @c NULL treats every margin
 *                       as @c 0, same as if none were configured
 * @param systray_strut  The systray's own current reservation on
 *                       @p desktop's surface (see @c systray_get_
 *                       reserved_strut); a @c NULL folds in nothing,
 *                       same as if the systray reserved no space
 *
 * @note Complexity: @e O(n), where @e n is the number of clients on
 *       the desktop
 */
void desktop_update_workarea(desktop_td *desktop,
        uint32_t screen_w, uint32_t screen_h,
        const struct config_desktop_s *config_desktop,
        const struct strut_partial_s *systray_strut);

/**
 * @brief Macro that evaluates to the active client of the desktop
 *
 * @note Complexity: @e O(1)
 */
#define desktop_client_active(d) ((d)->client_active)

/**
 * @brief Macro that sets the active client of the desktop
 *
 * @note If no client show be focused/active, @c NULL is the right value
 * @note Complexity: @e O(1)
 */
#define desktop_set_client_active(d, w) (((d)->client_active) = (w))

/**
 * @brief Macro that evaluates to the client count of the desktop
 *
 * @note Complexity: @e O(1)
 */
#define desktop_client_count(d) ((d) ? d->clients->size : 0)


#endif  /* ! DESKTOP_H */
