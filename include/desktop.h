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
#include <defs/surface.h>

/* Project includes */
#include <types/handles.h>
#include <config.h>


/**
 * @brief Stable primary seed used by desktop client hash tables
 *
 * The 32-bit golden ratio constant, @c floor(2^32 / phi), a classic
 * multiplicative hashing seed whose own bit pattern is already close to
 * maximally irregular, making it a reliable fixed seed without needing
 * any further tuning of its own.
 *
 * @see Knuth, *The Art of Computer * Programming*, volume 3
 */
#define DESKTOP_HASH_SEED_PRIMARY (0x9E3779B9u)

/**
 * @brief Stable secondary seed used by desktop client hash tables
 *
 * Not an arbitrary "different" value, since it is one of the two
 * mixing constants (@a murmurhash3_32's own @e fmix32 step,
 * @c utils/hash/murmurhash.c) MurmurHash3 itself already uses to
 * scramble its own output into a well-avalanched final hash.
 *
 * Reusing it here as a seed, rather than picking a second unrelated
 * number, implies that both seeds are already independently well-vetted
 * for bit dispersion, which is exactly the property double hashing
 * needs from @p h1 and @p h2 to stay uncorrelated for the same key.
 */
#define DESKTOP_HASH_SEED_SECONDARY (0x85EBCA6Bu)


/**
 * @brief Forward declaration only: a surface owns its own desktops
 *        (@c surface.h includes this header, never the other way
 *        around), so this header can only ever reference
 *        @c surface_td through a pointer, never the full definition.
 *        Guarded (see @c surface.h's own matching guard) since
 *        whichever of the two headers a translation unit includes
 *        first sets it, so a later include of the other one skips
 *        redeclaring the exact same alias, illegal under strict C99
 *        (unlike C11) even for two textually identical typedefs.
 */
#ifndef SURFACE_TD_DECLARED
#define SURFACE_TD_DECLARED
typedef struct surface_s surface_td;
#endif


#ifndef DESKTOP_TD_DECLARED
#define DESKTOP_TD_DECLARED
/** Handle to a @c desktop_s; the definition follows below */
typedef struct desktop_s desktop_td;
#endif

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
struct desktop_s {
    xcb_connection_t *connection;   /**< XCB connection */
    xcb_ewmh_connection_t *ewmh;    /**< EWMH connection */

    /**
     * @brief Resolved pointer to this desktop's own XCB screen
     *
     * Resolved once, in @a desktop_init, from the same
     * @a xcb_setup_roots_iterator walk already needed there to read
     * this screen's own pixel dimensions; kept here afterward so every
     * later caller that needs this desktop's own screen (e.g.,
     * @a desktop_render_background, @c render/desktop.c) reads this
     * field directly instead of repeating that same @e O(n) walk again
     * from scratch, an @e O(1) lookup either way.
     */
    xcb_screen_t *screen;

    ohtbl_td *clients;                      /**< Clients hash table */

    cdlist_td *stacking;                    /**< Stacking list */

    /**
     * @brief This desktop's own surface's shared configuration
     *
     * Always @c &surface->config (see @a desktop_init's own two
     * callers, surface.c and surface/switch.c), never a config from
     * any other source: every desktop on the same surface points at
     * the exact same @c config_td, so @p base and @p theme (what
     * this field used to be two separate pointers for) never
     * actually diverge from one another in practice.  One pointer
     * also reaches @p bindings/@p randr/@p desktops/@p a11y, none of
     * which had a field of their own here before, should a future
     * caller ever need one of those from a desktop directly.
     */
    config_td *config;

    struct background_s {
        bool is_image;              /**< Color or image */
        /** Reuse an externally set root pixmap on redraw */
        bool use_root_pixmap;
        union {
            uint32_t color;         /**< Background color */
            char *image_path;       /**< Background image */
        } bg;                       /**< Background itself */
    } background;
    /* No 'bg_applied_once'/'bg_color_applied' cache here: the root
     * window a solid-color background actually paints is one single
     * X resource shared by every desktop on the same screen, so that
     * cache lives per screen instead (cfr. 's_root_bg_applied_once' in
     * 'render/desktop.c'), not per desktop. */

    uint32_t screen_id;             /**< Screen index */
    xcb_window_t id;                        /**< Desktop index */
    xcb_window_t client_active_id;          /**< Active window */

    /** @brief Number of valid entries in @p monitor_workareas */
    uint32_t monitor_workarea_count;

    struct geometry_s geometry;
    struct geometry_s workarea;

    /**
     * @brief Per-monitor work area, the same reservations @p workarea
     *        itself folds in but scoped to each individual monitor
     *        instead of the whole surface at once
     *
     * A strut whose own along-edge span (@c _NET_WM_STRUT_PARTIAL's
     * @c start/@c end) only covers part of the combined surface, a
     * panel docked to just one monitor in a multi-monitor setup being
     * the common case, still reduces @p workarea across the @e whole
     * surface: EWMH's own strut model has no native notion of "which
     * monitor" at all, a reservation from one of the four @e screen
     * edges either way.  This array is this project's own answer,
     * folding the exact same struts against each monitor's own
     * along-edge span in turn instead of the whole surface's, so a
     * monitor with no panel of its own keeps its full physical area
     * here even while @p workarea, surface-wide, already reflects a
     * neighboring monitor's own panel.  A monitor with no panel
     * reservation touching it at all simply equals its own physical
     * @c surface->monitors entry.
     *
     * Indices line up with @c surface->monitors, as
     * @a desktop_update_workarea's comment describes; only the first
     * @c monitor_workarea_count entries are valid.
     */
    struct geometry_s monitor_workareas[WM_SURFACE_MAX_MONITORS];

    bool is_outdated;                       /**< Flag when data needs to
                                                 be updated */

    /**
     * @brief Whether the active client changed since the last render
     *        pass
     *
     * Decoration colors must be refreshed on every client when it
     * did.
     */
    bool is_focus_dirty;

    /**
     * @brief Whether at least one client on this desktop currently has
     *        its own urgency hint set
     *
     * Kept correct by @a desktop_action_recompute_urgent
     * (@c desktop/dclient.c), called from every site that could change
     * the answer.  A client's own urgency being set or cleared
     * (@a ccmd_client_urge and @a ccmd_client_unurge, in
     * @c cmds/client/flags.c) while already on this desktop, and
     * a client entering or leaving it altogether
     * (@a desktop_action_client_add and @a desktop_action_client_rem,
     * this same file), which already covers a client created already
     * urgent, one destroyed while still urgent, and one sent to
     * a different desktop while still urgent.  Every one of those
     * changes who this desktop's own set of clients is, not a client
     * already on it changing its own urgency, the other case those two
     * functions exist to handle instead.
     *
     * @note Not consulted by anything yet.  A hook for a future feature
     *       (e.g., drawing this desktop's own entry differently while
     *       the surface is showing a different one), included now so
     *       a client's own urgency is never missed regardless of which
     *       desktop it lands on.
     */
    bool is_urgent;

    char name[WM_DESKTOP_MAX_LENGTH_NAME];  /**< Desktop name */
};


/* Public interface */
/**
 * @brief Initialize a new desktop
 *
 * @param connection Pointer to the XCB connection
 * @param ewmh       EWMH connection pointer
 * @param screen_id  Screen identifier where this desktop belongs
 * @param desktop_id Desktop identifier
 * @param config     This desktop's own surface's shared configuration
 *
 * @return Pointer to new desktop or @c NULL otherwise
 *
 * @note The caller takes ownership of the returned desktop and
 *       releases it with @a desktop_destroy
 * @note Complexity: @e O(1)
 */
desktop_td *desktop_init(xcb_connection_t *connection,
        xcb_ewmh_connection_t *ewmh,
        uint32_t screen_id, uint32_t desktop_id,
        config_td *config);

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
 * @brief Mark a desktop and every one of its own clients as outdated
 *
 * @c desktop_render_one_client and @c ri_render_client_icon (both
 * render/desktop.c and render/icon.c) each gate their own repaint on
 * the specific client's own @c is_outdated, not just its desktop's:
 * marking only @p desktop itself, the mistake this function exists
 * to stop repeating, correctly triggers a render pass for @p desktop
 * but that pass then skips every one of its own clients, since none
 * of them individually asks for a repaint.  A client stays visually
 * stale (a stale pin button, a stale icon still shown at a now-
 * occupied grid slot, and so on) until some unrelated event happens
 * to mark that one client outdated on its own, e.g., a focus change.
 *
 * @param desktop Desktop, and every one of its own clients, to mark
 *                outdated
 *
 * @note No-op if @p desktop or its own stacking list is @c NULL
 * @note Complexity: @e O(n), where @e n is the number of clients on
 *       @p desktop
 */
void desktop_mark_outdated(desktop_td *desktop);

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
 * @brief Move a client from one desktop to another
 *
 * Removes @p client from @p from, adds it to @p to, and records the
 * new desktop on the client itself, all as one step: a removal that
 * is followed by a failed insertion would otherwise leave the client
 * in no desktop's own table at all, still mapped on screen, reachable
 * through nothing.  On such a failure the client is put back where it
 * came from and @p client keeps naming its original desktop.
 *
 * @param from   Desktop the client currently belongs to; may be
 *               @c NULL when it belongs to none
 * @param to     Desktop to move it to
 * @param client Client to move
 *
 * @return Status of the operation
 * @retval  0 The client now belongs to @p to
 * @retval  1 The insertion failed; the client was put back in @p from
 * @retval -1 @p to or @p client was @c NULL
 *
 * @note Complexity: @e O(1) amortized, two hash-table operations
 */
int desktop_action_client_move(desktop_td *from, desktop_td *to,
        client_td *client);

/**
 * @brief Find the client on a desktop matching a given client ID
 *
 * @param desktop Desktop whose own clients are searched
 * @param id Client ID to search for
 *
 * @return Pointer to the matching client, or @c NULL if @p desktop is
 *         null, or no client on it has that ID
 *
 * @note Complexity: @e O(n), where @e n is the number of clients on
 *       @p desktop
 */
client_td *desktop_find_client_by_id(const desktop_td *desktop,
        uint32_t id);

/**
 * @brief Recompute @p desktop->is_urgent from scratch, against every
 *        client currently on it
 *
 * @param desktop Desktop to recompute; a no-op if @c NULL or its own
 *                @p clients table is
 *
 * @note Complexity: @e O(n), where @e n is the number of clients
 *       currently on @p desktop
 */
void desktop_action_recompute_urgent(desktop_td *desktop);

/**
 * @brief Set a client to the front
 *
 * Brings the specified client to the top of the stacking order,
 * along with every transient descendant it has (e.g., an open
 * dialog belonging to it), so none of them get left behind below it.
 *
 * @param desktop Pointer to the desktop to receive the action
 * @param client  Pointer to the client to be sent to the front
 *
 * @return Status of the operation
 * @retval  0 Success
 * @retval  1 Failed to perform the action
 *
 * @note Complexity: @e O(n ^ 2), where @e n is the number of clients
 *       on @p desktop
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


/**
 * @brief Launch a new process, overriding its @c WM_CLASS
 *
 * The override only takes effect if the launched application's own
 * toolkit honors the standard @c RESOURCE_NAME/RESOURCE_CLASS
 * environment variables this sets in the child before @c execvp; not
 * every toolkit does, so a caller matching the eventually-created
 * client against this launch should prefer @p out_pid over the class
 * it ends up with.
 *
 * @param desktop         Pointer to the desktop to receive the action
 * @param executable_path Path to the binary file
 * @param class_name      @c WM_CLASS to request via the child's own
 *                        environment; @c NULL or empty leaves it alone
 * @param out_pid         If non-@c NULL, receives the launched
 *                        process's PID on success; left untouched on
 *                        any failure, so only ever read this after
 *                        checking the return value is @c 0
 *
 * @return Status of the operation
 * @retval  0 Success
 * @retval  1 Failed to fork the process
 * @retval -2 The executable itself could not be run
 *
 * @note Complexity: @e O(1)
 */
int desktop_action_process_launch_with_class(desktop_td *desktop,
        const char *restrict executable_path,
        const char *restrict class_name,
        pid_t *restrict out_pid);

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
 * window manager's own built-in systray reservation, aggregated exactly
 * like a client's strut since the systray's own dock window is never
 * itself a managed client).
 *
 * The same way, adds @p config_desktop's own @p margins on top of that
 * and subtracts the combined maximum reservation on each edge from the
 * full screen dimensions.  For partial struts, the corresponding
 * start/end range is honored so reservations that do not overlap the
 * screen edge span are ignored; configured margins always apply along
 * the whole edge, having no start/end range of their own to honor.
 * The result is stored in @p desktop->workarea and broadcast to the
 * X server as @c _NET_WORKAREA.
 *
 * Repeats the same reservation math once more per individual monitor
 * on @p surface, storing the result in
 * @c desktop->monitor_workareas, whose own comment in this same
 * file says why: each
 * monitor's own along-edge span, rather than the whole surface's, is
 * what a strut's own start/end range is checked against there, so a
 * monitor with no panel of its own keeps its full area even while a
 * neighboring monitor's own panel already reduces @p workarea,
 * surface-wide.  A configured margin, having no start/end of its own
 * to scope it the way a strut's own does, only ever reduces a given
 * monitor's own entry on whichever of its four sides actually
 * coincides with that same side of the whole surface (its own left
 * edge sits at surface @c x=0, say); elsewhere, an internal boundary
 * between two monitors is not "the screen edge" a margin is meant to
 * carve out in the first place.
 *
 * Call this after a panel (strut client) is mapped or unmapped, after
 * the systray's own reservation changes (reposition, resize, or being
 * shown/hidden), after @p surface's own monitor list itself changes
 * (a RandR hotplug), and after a configuration reload that may have
 * changed @p margins, so that maximize and smart-placement work on
 * the correct available area.
 *
 * @param desktop        Desktop whose work area should be refreshed
 * @param surface        Surface @p desktop lives on; supplies both
 *                       the full screen dimensions and the monitor
 *                       list @p desktop->monitor_workareas is scoped
 *                       to
 * @param config_desktop Active desktop-behavior configuration, for
 *                       its @p margins; a @c NULL treats every margin
 *                       as @c 0, same as if none were configured
 * @param systray_strut  The systray's own current reservation on
 *                       @p desktop's surface; a @c NULL value folds in
 *                       nothing, same as if the systray reserved no
 *                       space
 * @param ignore_struts  When @c true, neither @p systray_strut nor any
 *                       client's own @c _NET_WM_STRUT_PARTIAL is
 *                       folded in, only @p config_desktop's own
 *                       @p margins (the surface's own "full surface"
 *                       distraction-free toggle; see
 *                       @a surface_action_toggle_strutless_maximize,
 *                       surface.h): a deliberate, static reservation
 *                       stays honored even then, only the dynamic
 *                       presence of a panel or the tray is set aside
 *
 * @note A @c NULL @p surface is a silent no-op
 * @note Complexity: @e O(n*m), where @e n is the number of clients on
 *       the desktop and @e m is @p surface->monitor_count
 *
 * @see @a systray_get_reserved_strut
 * @see @p config_desktop_s in @c config.h, for a program that reserves
 *      screen space without publishing either property itself
 */
void desktop_update_workarea(desktop_td *desktop,
        const surface_td *surface,
        const struct config_desktop_s *config_desktop,
        const struct strut_partial_s *systray_strut,
        bool ignore_struts);

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
#define desktop_client_count(d) ((d) ? (d)->clients->size : 0)


#endif  /* ! DESKTOP_H */
