/**
 * @file stage.h
 *
 * @brief Stage structure declaration
 *
 * Defines the structure that represents a screen within the window
 * manager environment that includes a list of workspaces associated
 * with that stage and an index indicating which workspace is currently
 * active.
 *
 * Named "stage" rather than "screen" specifically to avoid a conceptual
 * collision with @c xcb_screen_t, XCB's raw protocol struct for
 * a screen, the type @c stage_td is a wrapper around one (see its @p
 * screen field) that adds everything the window manager itself tracks
 * about it (desktops, monitors, RandR state, and a reference to the
 * active configuration) so the two names stay distinct even though,
 * conceptually, one @c stage_td corresponds to exactly one X screen.
 * @c config.json's @c screens section refers to this same thing under
 * its more X11-familiar name instead, deliberately, as that name is for
 * whoever writes @c config.json, not for this header's internal
 * implementation detail.
 *
 * Holds only the struct itself and the handful of functions that act on
 * a stage as a whole (create, destroy, resize).  Everything else that
 * acts on a stage, desktop management, viewport queries, monitor
 * queries, work area recomputation, high-level actions, and bulk client
 * operations, is split into its own @c stage/ header instead, so a file
 * that only needs one of those does not also pull in, and rebuild
 * against, every other one declared alongside it.
 *
 * @see @p config_base_s, in @c config.h
 * @see @c stage/desktop.h, @c stage/viewport.h,
 *      @c stage/monitor.h, @c stage/workarea.h,
 *      @c stage/action.h, @c stage/client.h
 *
 * @defgroup stage Physical stage (screen) management
 * @ingroup wm
 */
/*
 * Copyright (c) 2026, J. A. Corbal.
 * All rights reserved.
 *
 * This file is licensed under the 'ISC License'.
 * Read the 'LICENSE' file in the root of this repository for details.
 */

#ifndef STAGE_H
#define STAGE_H


/* System includes */
#include <stdbool.h>
#include <stdint.h>

/* XCB includes */
#include <xcb/xcb.h>
#include <xcb/xcb_ewmh.h>

/* Type includes */
#include <types/handles.h>
#include <types/pair.h> /* dimensions_s, size_s */

/* Default initial values */
#include <defs/stage.h>

/* Project includes */
#include <monitor.h>


/**
 * @brief Structure to hold properties of the visual
 */
struct visual_properties_s {
    xcb_colormap_t colormap;    /**< Color maps for the visual */
    int depth;                  /**< Color depth (bits per pixel) */
};


/**
 * @brief Structure to hold properties of the stage
 */
struct stage_properties_s {
    struct dimensions_s dim;    /**< Screen dimensions (px) */
    struct dimensions_s dim_mm; /**< Screen dimensions (mm) */
    struct dpi_s dpi;           /**< Dots per inch */

    struct {
        /** Associated visual */
        xcb_visualid_t visual_id;

        /** Visual properties */
        struct visual_properties_s properties;
    } visual_info;
};


#ifndef STAGE_TD_DECLARED
#define STAGE_TD_DECLARED
/**
 * @brief Opaque-from-outside stage handle
 *
 * Declared here as a plain forward alias, guarded so a header that only
 * ever needs @c stage_td through a pointer (@c desktop.h, already
 * included above so a stage can own its desktops) can declare the exact
 * same alias on its own too, without this file's full definition
 * further down colliding with it as a duplicate @c typedef of the same
 * name, illegal under strict C99 even when, as here, both name the
 * exact same underlying type.
 */
typedef struct stage_s stage_td;
#endif


/**
 * @brief Called once per desktop by @a stage_desktop_walk_all
 *        (@c stage/desktop.h)
 *
 * @param desktop Desktop reached by the walk; never null
 * @param data    Whatever the caller handed the walk
 */
typedef void (*stage_desktop_visitor_fn)(desktop_td *desktop,
        void *data);


/**
 * @brief Structure for a stage in an XCB environment
 *
 * Maintains a circular list of desktops to support multiple virtual
 * desktops on the stage, allowing for more organized and flexible
 * user interfaces.  The structure also includes parameters for color
 * depth, DPI settings, and a reference to configuration data for
 * dynamic management and customization.
 *
 * The @p is_outdated flag indicates whether the stage data needs to
 * be refreshed, ensuring the stage information remains synchronized
 * with underlying changes in the XCB environment or user preferences.
 */
struct stage_s {
    xcb_screen_t *screen;           /**< Pointer to XCB screen */
    cdlist_td *desktops;            /**< Circular list of desktops */
    config_td *config;              /**< Configuration */
    uint32_t id;                    /**< Screen identifier or index */
    uint32_t desktop_count;         /**< Num. of desktops on this stage */
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
    struct stage_properties_s properties;

    /**
     * @brief Physical monitors within this stage's combined area
     *
     * Populated from the RandR 1.5 monitor list
     * (@a stage_monitor_refresh_all, @c stage/monitor.h), or a single
     * entry spanning the whole stage when RandR is unavailable or
     * reports none.
     *
     * Lets placement and maximize resolve which physical monitor
     * a point or window falls on, instead of always treating the
     * stage's whole combined area (every monitor sharing this one
     * X screen, the common case in a modern multi-monitor setup) as one
     * block.
     */
    monitor_td monitors[WM_STAGE_MAX_MONITORS];

    /**
     * @brief Whether panel/tray struts are set aside when computing
     *        every desktop's work area on this stage
     *
     * @see @a stage_action_maximize_toggle_strutless
     *      (@c stage/action.h)
     * @see @a desktop_update_workarea's @p ignore_struts
     *      (@c desktop.h), which this flag feeds directly
     */
    bool strutless_maximize;

    /** EWMH @c _NET_SHOWING_DESKTOP state */
    bool is_showing_desktop;
    bool is_outdated;               /**< Data needs updating */
};


/**
 * @brief Initialize a new stage
 *
 * @param connection    Pointer to XCB connection
 * @param stage_id      Stage identifier
 * @param desktop_count Number of desktops on this stage
 * @param config        Configuration this stage reads its theme
 *                      and window settings from
 *
 * @return Pointer to new stage, or @c NULL otherwise
 *
 * @note The caller takes ownership of the returned stage and releases
 *       it with @a stage_destroy
 * @note Complexity: @e O(d + n), where @e d is @p desktop_count and
 *       @e n is the number of monitors @a stage_monitor_refresh_all
 *       discovers
 */
stage_td *stage_init(xcb_connection_t *connection,
        uint32_t stage_id, uint32_t desktop_count,
        config_td *config);

/**
 * @brief Free allocated memory for a stage
 *
 * @param stage Pointer to the stage to deallocate
 *
 * @note Complexity: @e O(n), where @e n is the number of desktop, as it
 *       iterates through the array of windows to free each one of them
 */
void stage_destroy(stage_td *stage);

/**
 * @brief Resize the specified stage to the new dimensions
 *
 * @param stage  Pointer to the stage to be resized
 * @param width  New width for the stage in pixels
 * @param height New height for the stage in pixels
 *
 * @note Complexity: @e O(1)
 */
void stage_resize(stage_td *stage,
        uint32_t width, uint32_t height);

/**
 * @brief Macro that evaluates to the stage width
 *
 * @note Complexity: @e O(1)
 */
#define stage_width(s) ((s) ? (s)->properties.dim.w : 0)

/**
 * @brief Macro that evaluates to the stage height
 *
 * @note Complexity: @e O(1)
 */
#define stage_height(s) ((s) ? (s)->properties.dim.h : 0)


#endif /* STAGE_H */
