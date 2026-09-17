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
 * a conceptual collision with @c xcb_screen_t, XCB's raw protocol
 * struct for a screen, the type @c surface_td is a wrapper around one
 * (see its @p screen field) that adds everything the window manager
 * itself tracks about it (desktops, monitors, RandR state, and
 * a reference to the active configuration) so the two names stay
 * distinct even though, conceptually, one @c surface_td corresponds to
 * exactly one X screen.  @c config.json's @c screens section refers to
 * this same thing under its more X11-familiar name instead,
 * deliberately, as that name is for whoever writes @c config.json, not
 * for this header's internal implementation detail.
 *
 * Holds only the struct itself and the handful of functions that act
 * on a surface as a whole (create, destroy, resize).  Everything else
 * that acts on a surface, desktop management, viewport queries,
 * monitor queries, work area recomputation, high-level actions, and
 * bulk client operations, is split into its own @c surface/ header
 * instead, so a file that only needs one of those does not also pull
 * in, and rebuild against, every other one declared alongside it.
 *
 * @see @p config_base_s, in @c config.h
 * @see @c surface/desktop.h, @c surface/viewport.h,
 *      @c surface/monitor.h, @c surface/workarea.h,
 *      @c surface/action.h, @c surface/client.h
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

/* XCB includes */
#include <xcb/xcb.h>
#include <xcb/xcb_ewmh.h>

/* Type includes */
#include <types/handles.h>
#include <types/pair.h> /* dimensions_s, size_s */

/* Project includes */
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
        xcb_visualid_t visual_id;       /**< Associated visual */
        /** Visual properties */
        struct visual_properties_s properties;
    } visual_info;
};


#ifndef SURFACE_TD_DECLARED
#define SURFACE_TD_DECLARED
/**
 * @brief Opaque-from-outside surface handle
 *
 * Declared here as a plain forward alias, guarded so a header that only
 * ever needs @c surface_td through a pointer (@c desktop.h, already
 * included above so a surface can own its desktops) can declare the
 * exact same alias on its own too, without this file's full definition
 * further down colliding with it as a duplicate @c typedef of the same
 * name, illegal under strict C99 even when, as here, both name the
 * exact same underlying type.
 */
typedef struct surface_s surface_td;
#endif


/**
 * @brief Called once per desktop by @a surface_desktop_walk_all
 *        (@c surface/desktop.h)
 *
 * @param desktop Desktop reached by the walk; never @c NULL
 * @param data    Whatever the caller handed the walk
 */
typedef void (*surface_desktop_visitor_fn)(desktop_td *desktop,
        void *data);


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
    xcb_screen_t *screen;           /**< Pointer to XCB screen */
    cdlist_td *desktops;            /**< Circular list of desktops */
    config_td *config;              /**< Configuration */
    uint32_t id;                    /**< Screen identifier or index */
    uint32_t desktop_count;         /**< Num. of desktops on this surface */
    uint32_t desktop_cur;           /**< Index of current desktop */
    uint32_t monitor_count;         /**< Entries in @p monitors */

    /**
     * @brief Index into @p monitors of the one RandR reports as primary
     *
     * Left at @c 0, the first monitor, when none was flagged.
     */
    uint32_t primary_monitor_index;

    struct {
        uint32_t output_id;         /**< Active output identifier */
        uint32_t crtc_id;           /**< Active CRTC identifier */
        uint32_t mode_id;           /**< Active mode identifier */
        uint16_t rotation;          /**< Effective rotation mask */
        bool is_known;              /**< Whether metadata is known */
    } randr;

    /* Properties */
    struct surface_properties_s properties;

    /**
     * @brief Physical monitors within this surface's combined area
     *
     * Populated from the RandR 1.5 monitor list
     * (@a surface_monitor_refresh_all, @c surface/monitor.h), or
     * a single entry spanning the whole surface when RandR is
     * unavailable or reports none.
     *
     * Lets placement and maximize resolve which physical monitor
     * a point or window falls on, instead of always treating the
     * surface's whole combined area (every monitor sharing this one
     * X screen, the common case in a modern multi-monitor setup) as one
     * block.
     */
    monitor_td monitors[WM_SURFACE_MAX_MONITORS];

    /**
     * @brief Whether panel/tray struts are set aside when computing
     *        every desktop's work area on this surface
     *
     * @see @a surface_action_maximize_toggle_strutless
     *      (@c surface/action.h)
     * @see @a desktop_update_workarea's @p ignore_struts
     *      (@c desktop.h), which this flag feeds directly
     */
    bool strutless_maximize;

    /** EWMH @c _NET_SHOWING_DESKTOP state */
    bool is_showing_desktop;
    bool is_outdated;               /**< Data needs updating */
};


/**
 * @brief Initialize a new surface
 *
 * @param connection    Pointer to XCB connection
 * @param surface_id    Surface identifier
 * @param desktop_count Number of desktops on this surface
 * @param config        Configuration this surface reads its theme
 *                      and window settings from
 *
 * @return Pointer to new surface, or @c NULL otherwise
 *
 * @note The caller takes ownership of the returned surface and
 *       releases it with @a surface_destroy
 * @note Complexity: @e O(d + n), where @e d is @p desktop_count and
 *       @e n is the number of monitors @a surface_monitor_refresh_all
 *       discovers
 */
surface_td *surface_init(xcb_connection_t *connection,
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


#endif /* SURFACE_H */
