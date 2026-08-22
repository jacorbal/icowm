/**
 * @file surface.h
 *
 * @brief Surface structure declaration
 *
 * Defines the structure that represents a screen within the window
 * manager environment that includes a list of workspaces associated
 * with that surface and an index indicating which workspace is
 * currently active.
 *
 * Named "surface" rather than "screen" specifically to avoid
 * a conceptual collision with @c xcb_screen_t, XCB's own raw protocol
 * struct for a screen, the type @c surface_td is a wrapper around one
 * (see its own @p screen field) that adds everything the window manager
 * itself tracks about it (desktops, monitors, RandR state, and
 * a reference to the active configuration) so the two names stay
 * distinct even though, conceptually, one @c surface_td corresponds to
 * exactly one X screen.  @c config.json's own @c screens section refers
 * to this same thing under its more X11-familiar name instead,
 * deliberately, as that name is for whoever writes @c config.json, not
 * for this header's own internal implementation detail.
 *
 * @see @p config_base_s, in @c config.h
 *
 * @defgroup surface Physical surface (screen) management
 * @ingroup wm
 */
/*
 * Copyright (c) 2026, J. A. Corbal.
 * All rights reserved.
 *
 * This file is licensed under the 'ISC License'.
 * Read the 'LICENSE' file in the root of this repository for details.
 */

#ifndef SURFACE_H
#define SURFACE_H


/* System includes */
#include <stdbool.h>
#include <stdint.h>
#include <stddef.h>     /* size_t */

/* XCB includes */
#include <xcb/xcb.h>
#include <xcb/xcb_ewmh.h>

/* ADT includes */
#include <adt/cdlist.h> /* Doubly linked circular list */

/* Type includes */
#include <types/direction.h>
#include <types/pair.h> /* dimensions_s, size_s */

/* Project includes */
#include <config.h>
#include <desktop.h>
#include <monitor.h>

/* Default initial values */
#include <defs/surface.h>


/**
 * @brief Structure to hold properties of the visual
 */
struct visual_properties_s {
    xcb_colormap_t colormap;    /**< Color maps for the visual */
    int depth;                  /**< Color depth (bits per pixel) */
};


/**
 * @brief Structure to hold properties of the surface
 */
struct surface_properties_s {
    struct dimensions_s dim;    /**< Screen dimensions (px) */
    struct dimensions_s dim_mm; /**< Screen dimensions (mm) */
    struct dpi_s dpi;           /**< Dots per inch */

    struct {
        xcb_visualid_t visual_id;               /**< Associated visual */
        struct visual_properties_s properties;  /**< Visual properties */
    } visual_info;
};


#ifndef SURFACE_TD_DECLARED
#define SURFACE_TD_DECLARED
/**
 * @brief Opaque-from-outside surface handle
 *
 * Declared here as a plain forward alias, guarded so a header that
 * only ever needs @c surface_td through a pointer (@c desktop.h,
 * already included above so a surface can own its own desktops) can
 * declare the exact same alias on its own too, without this file's
 * own full definition further down colliding with it as a duplicate
 * @c typedef of the same name, illegal under strict C99 (unlike
 * C11) even when, as here, both name the exact same underlying type.
 */
typedef struct surface_s surface_td;
#endif

/**
 * @brief Structure for a surface in an XCB environment
 *
 * Maintains a circular list of desktops to support multiple virtual
 * desktops on the surface, allowing for more organized and flexible
 * user interfaces.  The structure also includes parameters for color
 * depth, DPI settings, and a reference to configuration data for
 * dynamic management and customization.
 *
 * The @p is_outdated flag indicates whether the surface data needs to
 * be refreshed, ensuring the surface information remains synchronized
 * with underlying changes in the XCB environment or user preferences.
 */
struct surface_s {
    uint32_t id;                    /**< Screen unique identifier or index */

    xcb_connection_t *connection;   /**< Pointer to XCB connection */
    xcb_screen_t *screen;           /**< Pointer to XCB screen */
    xcb_ewmh_connection_t *ewmh;    /**< Pointer to EWMH connection */

    /* Properties */
    struct surface_properties_s properties;
    struct {
        bool is_known;              /**< Whether output metadata is known */
        uint32_t output_id;         /**< Active output identifier */
        uint32_t crtc_id;           /**< Active CRTC identifier */
        uint32_t mode_id;           /**< Active mode identifier */
        uint16_t rotation;          /**< Effective rotation mask */
    } randr;

    uint32_t desktop_count;         /**< No. of desktops for this surface */
    uint32_t desktop_cur;           /**< Index of current desktop */
    cdlist_td *desktops;            /**< Circular list of desktops */

    /**
     * @brief Physical monitors within this surface's combined area
     *
     * Populated from the RandR 1.5 monitor list
     * (@a surface_refresh_monitors), or a single entry spanning the
     * whole surface when RandR is unavailable or reports none.
     *
     * Lets placement and maximize resolve which physical monitor
     * a point or window falls on, instead of always treating the
     * surface's whole combined area (every monitor sharing this one
     * X screen, the common case in a modern multi-monitor setup) as one
     * block.
     */
    monitor_td monitors[WM_SURFACE_MAX_MONITORS];
    uint32_t monitor_count;         /**< No. of entries in @p monitors */
    uint32_t primary_monitor_index; /**< Index into 'monitors' RandR
                                         reports as primary, or @c 0 (the
                                         first monitor) if none was
                                         flagged */

    config_td *config;              /**< Configuration */

    /**
     * @brief Whether panel/tray struts are set aside when computing
     *        every desktop's own work area on this surface
     *
     * @see @a surface_action_toggle_strutless_maximize (surface.h)
     * @see @a desktop_update_workarea's own @p ignore_struts
     *      (desktop.h), which this flag feeds directly
     */
    bool strutless_maximize;
    bool showing_desktop;           /**< EWMH @c _NET_SHOWING_DESKTOP state */
    bool is_outdated;               /**< Flag if data needs to be updated */
};


/* Public interface */
/**
 * @brief Initialize a new surface
 *
 * @param connection    Pointer to XCB connection
 * @param ewmh          EWMH connection pointer
 * @param surface_id    Surface identifier
 * @param desktop_count Number of desktops on this surface
 * @param config        Configuration this surface reads its own theme
 *                      and window settings from
 *
 * @return Pointer to new surface, or @c NULL otherwise
 *
 * @note Complexity: @e O(1)
 */
surface_td *surface_init(xcb_connection_t *connection,
        xcb_ewmh_connection_t *ewmh,
        uint32_t surface_id, uint32_t desktop_count,
        config_td *config);

/**
 * @brief Free allocated memory for a surface
 *
 * @param surface Pointer to the surface to deallocate
 *
 * @note Complexity: @e O(n), where @e n is the number of desktop, as it
 *       iterates through the array of windows to free each one of them
 */
void surface_destroy(surface_td *surface);

/**
 * @brief Soft surface update
 *
 * @param surface Pointer to the surface to soft update
 *
 * @note Complexity: @e O(1)
 */
void surface_update(surface_td *surface);

/**
 * @brief Full surface update
 *
 * Updates the surface by updating every window of every desktop.
 *
 * @param surface Pointer to the surface to full update
 *
 * @note Complexity: @e O(n), where @e n is the number of desktops
 */
void surface_update_full(surface_td *surface);

/**
 * @brief Resize the specified surface to the new dimensions
 *
 * @param surface Pointer to the surface to be resized
 * @param width   New width for the surface in pixels
 * @param height  New height for the surface in pixels
 *
 * @note Complexity: @e O(1)
 */
void surface_resize(surface_td *surface,
        uint32_t width, uint32_t height);

/**
 * @brief Refresh the surface's own list of physical monitors
 *
 * Queries the RandR 1.5 monitor list (@a xcb_randr_get_monitors) for
 * @p surface's root window and rebuilds @p surface->monitors from the
 * reply, clamped to @c WM_SURFACE_MAX_MONITORS entries.  Falls back to
 * a single entry spanning @p surface->properties.dim (the whole
 * combined surface) when RandR is unavailable, the query fails, or the
 * reply lists no monitors, so @p surface->monitor_count is never left
 * at zero.
 *
 * @param surface Pointer to the surface whose monitor list to refresh
 *
 * @note Complexity: @e O(n), where @e n is the number of monitors
 *       RandR reports
 */
void surface_refresh_monitors(surface_td *surface);

/**
 * @brief Find which of the surface's monitors contains a point
 *
 * @param surface Pointer to the surface to search
 * @param pos     Coordinate, in the surface's own space
 *
 * @return The containing monitor, or, if the point falls outside every
 *         known monitor (e.g., a stale coordinate after a monitor was
 *         unplugged), the closest one by center-point distance.  Spans
 *         the whole surface if @p surface has no monitors of its own or
 *         @p surface is @c NULL.
 *
 * @note Complexity: @e O(n), where @e n is @p surface->monitor_count
 */
monitor_td surface_monitor_for_point(const surface_td *surface,
        struct position_s pos);

/**
 * @brief Get the surface's primary monitor, if RandR flagged one
 *
 * @param surface Pointer to the surface to query
 *
 * @return The monitor RandR reports as primary.  Falls back to
 *         @p (surface->monitors[0]) if none was flagged as primary, and
 *         to a monitor spanning the whole surface if @p surface has no
 *         monitors of its own or @p surface is @c NULL.
 *
 * @note Complexity: @e O(1)
 */
monitor_td surface_primary_monitor(const surface_td *surface);

/**
 * @brief Get the surface's own monitor in a given compass direction
 *        from another one
 *
 * Unlike a desktop, a monitor already has a real, physical position
 * (@p current's own @c x/@c y/@c w/@c h, as RandR reported it), so
 * no configured layout is needed to answer "which one is to the
 * north" at all: among every one of @p surface's own monitors whose
 * center genuinely lies in @p direction from @p current's own
 * center, whichever one is nearest by that same measure is the
 * answer, matching what a person looking at the arrangement would
 * call it even when the monitors involved differ in size or are not
 * perfectly aligned to one another.  Deliberately never wraps around
 * to the farthest monitor the opposite way when none lies in
 * @p direction at all, unlike a desktop's own equivalent, which
 * optionally does when @c desktops.wrap-at-bounds is enabled:
 * wrapping a definite, ordered list (a desktop's own circular one)
 * has one obviously correct meaning, but wrapping a genuinely 2-D
 * physical arrangement does not (does "east, wrapped" mean the
 * westmost monitor overall, or only the westmost one still on the
 * same row?), so no attempt is made to invent one here; a monitor
 * has no equivalent of @c wrap-at-bounds to make that choice
 * configurable in the first place, for the same reason.
 *
 * @param surface   Pointer to the surface structure
 * @param current   The monitor to search from; need not itself be
 *                  one of @p surface's own current monitors (an
 *                  already-stale caller-held copy is fine, since
 *                  only its own @c x/@c y/@c w/@c h are read)
 * @param direction Compass direction to search in
 *
 * @return The neighboring monitor, or @p current itself, unchanged,
 *         if @p surface is @c NULL or no monitor lies in
 *         @p direction at all
 *
 * @note Complexity: @e O(n), where @e n is @p surface->monitor_count
 */
monitor_td surface_monitor_direction(const surface_td *surface,
        monitor_td current, enum compass_direction_e direction);

/**
 * @brief Add a new desktop to the list
 *
 * @param surface Pointer to the surface structure
 * @param desktop Pointer to the desktop to be added
 *
 * @return Status of the adding operation
 * @retval  0 Success
 * @retval  1 Failed to insert the desktop to the list
 * @retval -1 Invalid surface
 */
int surface_desktop_add(surface_td *surface, const desktop_td *desktop);

/**
 * @brief Remove a desktop from the list by its ID
 *
 * @param surface    Pointer to the surface structure.
 * @param desktop_id ID of the desktop to be removed
 *
 * @return Status of the removal operation
 * @retval  0 Success
 * @retval  1 Failed to remove the desktop from the list
 * @retval  2 Could not find the desktop matching that ID
 * @retval -1 Invalid surface or no desktops
 */
int surface_desktop_rem(surface_td *surface, uint32_t desktop_id);

/**
 * @brief Get a desktop from the list by its ID
 *
 * Retrieves a pointer to a desktop with the specified ID from the
 * surface.
 *
 * @param surface    Pointer to the surface structure
 * @param desktop_id ID of the desktop to retrieve
 *
 * @return Pointer to the desktop if found, or @c NULL otherwise
 */
desktop_td *surface_desktop_get(surface_td *surface,
        uint32_t desktop_id);

/**
 * @brief Get a desktop's own row/column position in its surface's
 *        configured layout
 *
 * Always the same reading order the flat desktop list itself already
 * had before layout existed at all when no @c topology.screens.
 * desktops layout is configured (the common case, still the
 * default): @c row @c 0, @c col @c desktop_id.  With one configured,
 * the position @p desktop_id's own @c orientation/@c corner
 * combination actually places it at, which is not simply @c row
 * @c 0, @c col @c desktop_id once @c corner is anything other than
 * top-left, nor once @c orientation is vertical (see @a s_layout_
 * row_col's own doc comment, surface/desktops.c, for the fuller
 * reasoning).
 *
 * @param surface    Pointer to the surface structure
 * @param desktop_id ID of the desktop to locate
 * @param row_out    Resulting row, updated in place only on a
 *                   @c true return
 * @param col_out    Resulting column, updated in place only on a
 *                   @c true return
 *
 * @return @c false if @p surface, its own configuration, or either
 *         output pointer is unavailable
 *
 * @note Complexity: @e O(1)
 */
bool surface_desktop_row_col(const surface_td *surface,
        uint32_t desktop_id, uint32_t *row_out, uint32_t *col_out);

/**
 * @brief Get the desktop toward the north in the configured layout,
 *        optionally cycling
 *
 * A no-op search (always @c NULL, cycling or not) on a surface with
 * no @c topology.screens.desktops layout configured, the same as on
 * any single-row layout: there is no "north" to find at all when
 * every desktop already sits in the one and only row.  Skips past
 * any desktop-less gap cell a configured layout's own @c rows @c *
 * @c columns may legitimately exceed the real desktop count with
 * (see @c ci_config_load_screens's own doc comment, config/base/
 * desktops.c), rather than landing on one.
 *
 * @param surface    Pointer to the surface structure
 * @param desktop_id ID of the current desktop
 * @param cycle      If @c true, wraps to the bottom row of the same
 *                   column when already at the top
 *
 * @return Pointer to the desktop toward the north, or @c NULL if
 *         none exists in that direction
 */
desktop_td *surface_desktop_north(surface_td *surface,
        uint32_t desktop_id, bool cycle);

/**
 * @brief Get the desktop toward the south in the configured layout,
 *        optionally cycling
 *
 * A no-op search (always @c NULL, cycling or not) on a surface with
 * no @c topology.screens.desktops layout configured, the same as on
 * any single-row layout: there is no "south" to find at all when
 * every desktop already sits in the one and only row.  Skips past
 * any desktop-less gap cell the same way @a surface_desktop_north
 * does.
 *
 * @param surface    Pointer to the surface structure
 * @param desktop_id ID of the current desktop
 * @param cycle      If @c true, wraps to the top row of the same
 *                   column when already at the bottom
 *
 * @return Pointer to the desktop toward the south, or @c NULL if
 *         none exists in that direction
 */
desktop_td *surface_desktop_south(surface_td *surface,
        uint32_t desktop_id, bool cycle);

/**
 * @brief Get the desktop toward the west in the configured layout,
 *        optionally cycling
 *
 * Searches for the desktop with the given ID and returns the one
 * toward the west of it: list-previous on a surface with no @c
 * topology.screens.desktops layout configured (the common case,
 * still the default), the desktop one cell west along the
 * configured grid otherwise, whatever @c desktop_id that cell's own
 * @c orientation/@c corner combination happens to hold (never simply
 * "@c desktop_id @c - @c 1": once @c corner is anything other than
 * top-left, a lower ID can sit visually east of a higher one, not
 * west; see @a s_layout_row_col's own doc comment, surface/
 * desktops.c, for the full reasoning), skipping past any desktop-
 * less gap cell along the way.  If the current desktop is the
 * westmost in its own row, and cycling is enabled, wraps to the
 * eastmost desktop in that same row.
 *
 * @param surface    Pointer to the surface structure
 * @param desktop_id ID of the current desktop
 * @param cycle      If @c true, wraps to the eastmost desktop in the
 *                   same row when already at the westmost
 *
 * @return Pointer to the desktop toward the west, or @c NULL if not
 *         found or not valid
 */
desktop_td *surface_desktop_west(surface_td *surface,
        uint32_t desktop_id, bool cycle);

/**
 * @brief Get the desktop toward the east in the configured layout,
 *        optionally cycling
 *
 * Searches for the desktop with the given ID and returns the one
 * toward the east of it: list-next on a surface with no @c
 * topology.screens.desktops layout configured (the common case,
 * still the default), the desktop one cell east along the
 * configured grid otherwise (see @a surface_desktop_west's own doc
 * comment for the fuller reasoning on why this is not simply
 * "@c desktop_id @c + @c 1"), skipping past any desktop-less gap
 * cell along the way.  If the current desktop is the eastmost in its
 * own row, and cycling is enabled, wraps to the westmost desktop in
 * that same row.
 *
 * @param surface    Pointer to the surface structure
 * @param desktop_id ID of the current desktop
 * @param cycle      If @c true, wraps to the westmost desktop in the
 *                   same row when already at the eastmost
 *
 * @return Pointer to the desktop toward the east, or @c NULL if not
 *         found or not valid
 */
desktop_td *surface_desktop_east(surface_td *surface,
        uint32_t desktop_id, bool cycle);

/**
 * @brief Select the desktop toward the north, optionally cycling
 *
 * Attempts to select the desktop toward the north of the current one
 * (see @a surface_desktop_north's own doc comment).  Updates
 * @p desktop_cur only on success.
 *
 * @param surface Pointer to the surface structure
 * @param cycle   If @c true, wraps to the bottom row of the same
 *                column when already at the top
 *
 * @return Status of the selection
 * @retval  0 Success
 * @retval  1 No desktop to the north found
 * @retval -1 Invalid surface or no desktops
 */
int surface_desktop_select_north(surface_td *surface, bool cycle);

/**
 * @brief Select the desktop toward the south, optionally cycling
 *
 * Attempts to select the desktop toward the south of the current one
 * (see @a surface_desktop_south's own doc comment).  Updates
 * @p desktop_cur only on success.
 *
 * @param surface Pointer to the surface structure
 * @param cycle   If @c true, wraps to the top row of the same
 *                column when already at the bottom
 *
 * @return Status of the selection
 * @retval  0 Success
 * @retval  1 No desktop to the south found
 * @retval -1 Invalid surface or no desktops
 */
int surface_desktop_select_south(surface_td *surface, bool cycle);

/**
 * @brief Select the desktop toward the west (list-previous),
 *        optionally cycling
 *
 * Attempts to select the desktop toward the west of the current one
 * (see @a surface_desktop_west's own doc comment for what "west"
 * means with and without a configured layout).  If the current
 * desktop is the first and cycle mode is enabled, it will select the
 * last desktop, updating @p desktop_cur.
 *
 * @param surface Pointer to the surface structure
 * @param cycle   If @c true, will cycle to the last desktop if the
 *                current is the first
 *
 * @return Status of the selection
 * @retval  0 Success
 * @retval  1 No desktop to the west found
 * @retval -1 Invalid surface or no desktops
 */
int surface_desktop_select_west(surface_td *surface, bool cycle);

/**
 * @brief Select the desktop toward the east (list-next), optionally
 *        cycling
 *
 * Attempts to select the desktop toward the east of the current one
 * (see @a surface_desktop_east's own doc comment for what "east"
 * means with and without a configured layout).  If the current
 * desktop is the last and cycle mode is enabled, it will select the
 * first desktop, updating @p desktop_cur.
 *
 * @param surface Pointer to the surface structure
 * @param cycle   If @c true, will cycle to the first desktop if the
 *                current is the last
 *
 * @return Status of the selection
 * @retval  0 Success
 * @retval  1 No desktop to the east found
 * @retval -1 Invalid surface or no desktops
 */
int surface_desktop_select_east(surface_td *surface, bool cycle);

/**
 * @brief Select a specific desktop by its ID
 *
 * Selects a desktop by its ID.  If the ID is valid, it changes the
 * current desktop to the specified ID updating @p desktop_cur.  Returns
 * an error and updates nothing if the ID is not found in the list of
 * desktops.
 *
 * @param surface    Pointer to the surface structure
 * @param desktop_id ID of the desktop to select
 *
 * @return Status of the selection
 * @retval  0 Success
 * @retval  1 No next desktop found
 * @retval -1 Invalid surface or no desktops
 */
int surface_desktop_select(surface_td *surface, uint32_t desktop_id);

/**
 * @brief Add a new desktop associated with the surface
 *
 * Grows @p surface's own configured @c topology.screens.desktops
 * layout by one row or column first, whichever @c orientation
 * treats as the non-primary axis, when there is not already a
 * desktop-less gap cell in it for the new desktop to land on (see
 * @a s_surface_layout_grow_for's own doc comment, surface/switch.c,
 * for the fuller reasoning on why that one axis specifically).
 * Purely a "create it" action either way: @p surface's own current
 * view never switches to the new desktop, whether growing a new
 * row or column happened or not, and regardless of which desktop,
 * if any, currently has the view.
 *
 * Refused outright once @p surface's own desktop count already
 * reaches @c CONFIG_MAX_DESKTOPS: @c config_base's own
 * @c screens[screen_id].desktops array (@c config.h) is a
 * fixed-size array of exactly that many slots, indexed by the new
 * desktop's own ID, so adding one more past that point would index
 * past the end of it.  Also refused outright, regardless of the
 * current count, while restricted-memory mode is active
 * (@a memguard_max_clients), which is deliberately locked to a
 * single desktop always (see
 * @a config_set_default_values_memguard).
 *
 * @param surface Pointer to the surface to receive the action
 *
 * @return Status of the operation
 * @retval  0 Success
 * @retval  1 Failed to perform the action, including already being
 *            at @c CONFIG_MAX_DESKTOPS, or restricted-memory mode
 *            being active
 *
 * @note Complexity: @e O(1)
 */
int surface_action_desktop_add(surface_td *surface);

/**
 * @brief Remove the last desktop from the surface, moving any client
 *        still on it to the desktop immediately before it first
 *
 * Refuses outright when only one desktop remains (@c surface's own
 * desktop count must stay at least @c 1).  Restricted-memory mode
 * always has exactly one desktop and no way to reach a second one
 * (see @a surface_action_desktop_add's own doc comment), so that
 * same guard alone already refuses this call every time it runs
 * under that mode too, with no separate check of its own needed
 * here.  Every client still on the
 * desktop being removed, pinned or not, is moved onto what becomes
 * the new last desktop before the old one is destroyed: destroying a
 * desktop that still holds clients would otherwise destroy those
 * clients' own @c client_td structures right along with it (see
 * @a desktop_destroy, desktop.c), losing real, live application
 * windows rather than just the virtual desktop container.  A moved
 * client's own EWMH @c _NET_WM_DESKTOP is brought in line with its
 * new desktop, except for a pinned one, whose property already holds
 * the EWMH "all desktops" sentinel and is left alone.  If the
 * desktop being removed is the current one, the view switches to the
 * new last desktop first.
 *
 * @param surface Pointer to the surface to receive the action
 *
 * @return Status of the operation
 * @retval  0 Success
 * @retval  1 Refused (only one desktop left, no desktop to remove
 *            from, no fallback desktop available, or the underlying
 *            removal itself failed)
 * @retval -1 @p surface is @c NULL
 *
 * @note Complexity: @e O(n), where @e n is the number of clients on
 *       the desktop being removed
 */
int surface_action_desktop_remove(surface_td *surface);

/**
 * @brief Toggle whether panel/tray struts are set aside when
 *        computing this surface's own desktops' work areas
 *
 * Strutless maximization: while on, @a desktop_update_workarea
 * (desktop.h) folds in only @c desktops.margins from configuration,
 * never a panel's own @c _NET_WM_STRUT_PARTIAL nor the systray's own
 * reservation, so a maximized or smart-placed window can use the
 * full screen underneath wherever a panel would otherwise have
 * reserved space.  Every desktop's own work area is recomputed
 * immediately (@a surface_refresh_workareas), not left for whatever
 * unrelated event happens to trigger that next.
 *
 * @param surface Pointer to the surface to receive the action
 *
 * @return Status of the operation
 * @retval  0 Success
 * @retval  1 Failed to perform the action
 *
 * @note Complexity: @e O(n), where @e n is the number of desktops on
 *       @p surface
 */
int surface_action_toggle_strutless_maximize(surface_td *surface);

/**
 * @brief Apply every configured RandR output profile that matches
 *        a currently-connected output on this surface
 *
 * For each configured profile whose name matches a currently-connected
 * output.  An enabled profile's desired resolution (falling back to
 * whatever mode is already active, or the output's own preferred mode,
 * when none is configured or none of the screen's modes matches it),
 * position, and rotation are compared against that output's own CRTC's
 * actual current state first, and @a xcb_randr_set_crtc_config is
 * issued only when at least one of them actually differs (claiming
 * a free compatible CRTC first if the output had none, which always
 * counts as a difference, since it was driving nothing at all before);
 * @p is_primary is compared and, if needed, applied the same way as
 * a separate, independent step afterward.
 *
 * A disabled profile instead turns the output's own CRTC off, but only
 * if it is not already off.  A configured profile whose name matches no
 * currently-connected output is skipped, logged at debug level only,
 * not as a warning.  This comparison is what keeps an unchanged
 * @c randr.json from writing anything to the X server at all on
 * a reload that touched some other file instead, or on a hotplug event
 * for an unrelated output.
 *
 * Meant to be called once at startup (after RandR is confirmed
 * available) and again whenever @c XCB_RANDR_NOTIFY_OUTPUT_CHANGE
 * fires, so a profile for an output that was not yet connected at
 * startup still gets applied once it is.
 *
 * @param surface       Surface whose outputs to apply configured
 *                      profiles to
 * @param take_snapshot Whether to save each changed CRTC's prior
 *                      state first, so a subsequent
 *                      @a surface_action_revert_randr_profiles call can
 *                      put it back; pass @c false for a startup or
 *                      hotplug call, where there is nothing to revert
 *                      to (the newly-applied state @e is the intended
 *                      one), and @c true only when the caller means to
 *                      offer a person a chance to undo this specific
 *                      call
 *
 * @return Status of the operation
 * @retval  true if at least one output's actual state was changed
 * @retval false if every configured profile already matched (or
 *               @p surface, RandR management, or every matching output
 *               could not be resolved at all)
 *
 * @note A no-op when @p surface, its connection, or its screen is
 *       @c NULL, or when RandR profile management is off altogether
 *       (@p config->randr.is_enabled is @c false)
 * @note @p config->randr.outputs is not scoped per screen
 * @note Complexity: @e O(p * (n + c)), where @e p is the number of
 *       configured profiles, @e n the number of outputs the screen
 *       currently reports, and @e c the number of CRTCs compatible
 *       with whichever output a profile matches (only when it has
 *       none active yet)
 *
 * @see @a wm_action_config_reload, and @p config_randr_s, loaded from
 *      @c randr.json
 */
bool surface_action_apply_randr_profiles(surface_td *surface,
        bool take_snapshot);

/**
 * @brief Undo the most recent snapshotting
 *        @a surface_action_apply_randr_profiles call
 *
 * Restores every CRTC that call actually changed to exactly the state
 * it captured first (mode, position, rotation, or off if it was off),
 * and RandR's primary output to whichever one held it before, then
 * clears the snapshot.  A safe, cheap no-op if nothing is currently
 * snapshotted, including when the surface it belonged to no longer has
 * a usable connection.
 *
 * @note Complexity: @e O(s), where @e s is the number of CRTCs the
 *       snapshotted call actually changed
 */
void surface_action_revert_randr_profiles(void);

/**
 * @brief Unmap all non-sticky client windows belonging to a desktop
 *
 * Iterates the stacking list of the specified desktop and calls
 * @a xcb_unmap_window for each client that does not have the
 * @c CLIENT_FLAG_PIN flag set.  Used when switching away from a desktop
 * to hide its windows.
 *
 * @param surface    Pointer to the surface that owns the desktop
 * @param desktop_id ID of the desktop whose clients should be hidden
 *
 * @note Complexity: @e O(n), where @e n is the number of stacked
 *       clients on the desktop
 */
void surface_clients_hide(surface_td *surface, uint32_t desktop_id);

/**
 * @brief Map all visible client windows belonging to a desktop
 *
 * Iterates the stacking list of the specified desktop and calls
 * @a xcb_map_window for each client that is neither hidden
 * (@c CLIENT_FLAG_HIDDEN) nor iconified
 * (@c CLIENT_STATE_ICONIFIED).  Used when switching to a desktop to
 * reveal its windows.
 *
 * @param surface    Pointer to the surface that owns the desktop
 * @param desktop_id ID of the desktop whose clients should be shown
 *
 * @note Complexity: @e O(n), where @e n is the number of stacked
 *       clients on the desktop
 */
void surface_clients_show(surface_td *surface, uint32_t desktop_id);

/**
 * @brief Move all sticky clients from every other desktop to @p to_id
 *
 * Iterates all desktops on the surface and relocates any client that
 * carries the @c CLIENT_FLAG_PIN flag to the desktop identified by
 * @p to_id.  Called during desktop switches so that pinned windows are
 * present in the new desktop's stacking list and therefore respond to
 * keyboard shortcuts and focus management on the destination desktop.
 *
 * @param surface Pointer to the surface that owns all desktops
 * @param to_id   ID of the desktop to which sticky clients are moved
 *
 * @note Complexity: @e O(d * n), where @e d is the number of desktops
 *       and @e n is the average number of clients per desktop
 */
void surface_clients_sticky_transfer_all(surface_td *surface,
        uint32_t to_id);

/**
 * @brief Recompute the work area for every desktop on a surface
 *
 * Iterates over all desktops belonging to the given surface and updates
 * each desktop work area using the surface's current width and height.
 * This keeps per-desktop usable geometry in sync after changes such as
 * screen resizing or strut updates.
 *
 * @param surface Pointer to the surface whose desktops will be
 *                refreshed
 *
 * @note Complexity: @e O(n), where @e n is the number of desktops
 */
void surface_refresh_workareas(surface_td *surface);

/**
 * @brief Reposition clients left with no overlap against any known
 *        monitor
 *
 * Iterates all desktops and their stacking lists.  A client whose frame
 * (or client window when undecorated) still overlaps at least one of
 * @p surface's own monitors (see @p surface->monitors) is left
 * untouched, even if it is not fully contained within a single one.
 * A window legitimately spanning two adjacent monitors must not be
 * "corrected" just for straddling their seam.  Only a client with no
 * overlap against any current monitor at all (typically because the one
 * it used to be on was disconnected, or a RandR layout change left it
 * in a gap) is moved, clamped into whichever monitor
 * @a surface_monitor_for_point resolves for its own center point so
 * that at least a minimum strip of the window remains visible there.
 * Both the X server geometry and the cached
 * @p client->layout.geometry.cur are updated.
 *
 * @param surface Pointer to the surface whose clients will be reflowed
 *
 * @note Complexity: @e O(d * n * m), where @e d is the number of
 *       desktops, @e n is the average number of clients per desktop,
 *       and @e m is @p surface->monitor_count
 */
void surface_clients_reflow(surface_td *surface);

/**
 * @brief Macro that evaluates to the surface width
 *
 * @note Complexity: @e O(1)
 */
#define surface_width(s) ((s) ? (s)->properties.dim.w : 0)

/**
 * @brief Macro that evaluates to the surface height
 *
 * @note Complexity: @e O(1)
 */
#define surface_height(s) ((s) ? (s)->properties.dim.h : 0)

/**
 * @brief Macro that evaluates to the desktop count of the surface
 *
 * @note Complexity: @e O(1)
 */
#define surface_desktop_count(s) ((s) ? (s)->desktops->size : 0)


#endif  /* ! SURFACE_H */
