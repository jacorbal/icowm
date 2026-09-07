/**
 * @file client.h
 *
 * @brief Window definition and declaration
 *
 * Defines the structure holding everything the window manager knows
 * about one client window: the identifiers it is addressed by, its name
 * and class, its state and layer, its geometry and decoration, the icon
 * it is shown by while iconified, and the preferences it declares about
 * how it wishes to be treated.
 *
 * The two blocks of hints a client carries, one read from its ICCCM
 * properties and one from its EWMH ones, are declared in
 * @c client/icccm.h and @c client/ewmh.h and held here by value.
 *
 * @defgroup client Managed client windows
 * @ingroup desktop
 */
/*
 * Copyright (c) 2026, J. A. Corbal.
 * All rights reserved.
 *
 * This file is licensed under the 'ISC License'.
 * Read the 'LICENSE' file in the root of this repository for details.
 */

#ifndef _POSIX_C_SOURCE
#define _POSIX_C_SOURCE 200112L /* struct timespec */
#endif

#ifndef CLIENT_H
#define CLIENT_H


/* System includes */
#include <stdbool.h>
#include <stdint.h>
#include <sys/types.h>  /* pid_t */
#include <time.h>       /* struct timespec */

/* XCB includes */
#include <xcb/xcb.h>
#include <xcb/xcb_ewmh.h>

/* ADT includes */
#include <adt/cdlist.h>

/* Default initial values */
#include <defs/client.h>

/* Type includes */
#include <types/pair.h> /* geometry_s, sides_s */

/* Util includes */
#include <utils/safe/safeflg.h>

/* Project includes */
#include <action.h>
#include <config.h>
#include <render/wmicon.h>

/* Local includes */
#include <client/layout.h>
#include <client/predicates.h>
#include <client/ewmh.h>
#include <client/icccm.h>
#include <client/props.h>
#include <client/state.h>


#ifndef CLIENT_TD_DECLARED
#define CLIENT_TD_DECLARED
/** Handle to a @c client_s; the definition follows below */
typedef struct client_s client_td;
#endif


/**
 * @brief Structure for a client in an XCB environment
 *
 * The @p screen_id and @p desktop_id fields link the client to its
 * respective screen and desktop, and @p parent_id is the XCB window ID
 * of its parent client if needed.
 *
 * Additionally, the @p properties field contains various settings that
 * define the behavior and appearance of the client, while @p config
 * (shared with every other client on the same surface) provides the
 * base, theme, and accessibility settings driving its visual aspects.
 */
struct client_s {

    /**
     * @brief Direct pointer to the managed parent this client is
     *        transient for, or @c NULL if it is not transient for
     *        anything (or its declared parent is not, or not yet,
     *        managed)
     *
     * Resolved once, right after this client is added to its desktop,
     * from @a transient_for by looking up the matching managed client;
     * kept in sync from then on by whichever code removes a client from
     * the transient tree (see @a transients and @a transient_node
     * below).  Walking this pointer directly is @e O(1) per step,
     * unlike re-resolving @a transient_for's raw window ID through
     * a lookup every time the parent is needed.
     */
    struct client_s *transient_parent;

    /**
     * @brief This client's direct transient children, or @c NULL if it
     *        has none
     *
     * Lazily allocated the first time a child registers itself here
     * (see @a transient_parent above), never created up front for every
     * client regardless of whether it will ever have any: the
     * overwhelming majority of managed clients are never transient for
     * anything and never have any children either, so paying for an
     * empty list on all of them would be pure waste.  Each item's @c
     * data is the child @c client_td* itself.
     */
    cdlist_td *transients;

    /**
     * @brief This client's position within @a transient_parent's own
     *        @a transients list, or @c NULL if it is not currently
     *        registered in any parent's list
     *
     * The whole reason a real, maintained list of transient children is
     * worth having at all instead of scanning every client on every
     * desktop whenever the transient family needs walking: with this
     * node cached, removing this client from its parent's list when it
     * closes, or is reparented, is a true @e O(1) operation
     * (@a cdlist_rem_next on this node's @c prev), never a search.
     */
    cdlist_item_td *transient_node;

    /**
     * @brief Monotonic time of the client's last shade or unshade
     *
     * Lets @a handler_configure_request recognize a client's
     * @c ConfigureRequest as a stale reaction to that transition,
     * rather than a genuine independent resize, and ignore it.
     */
    struct timespec shade_transition_time;

    /**
     * @brief Monotonic time of the client's last entry into or exit
     *        from fullscreen
     *
     * Used the same way as @c shade_transition_time, and for the same
     * reason.
     */
    struct timespec fullscreen_transition_time;

    /**
     * @brief Shared base/theme/a11y configuration for this client's
     *        surface
     *
     * The same @c config_td every other client on this surface also
     * points to; never reassigned after @a client_init (see
     * @a client_init's callers), though the configuration it points to
     * can still change in place at any time from a live reload or
     * a runtime theme request, which every client picking it up on its
     * next read is exactly the point of sharing one pointer instead of
     * each client copying its snapshot.
     */
    const config_td *config;

    struct {
        pid_t pid;                  /**< PID being executed */

        /** Whether @c pid names a process on this same host, per
         *  @c WM_CLIENT_MACHINE; @c false both for a client naming
         *  another host and for one setting no such property, since
         *  either way this local @c pid cannot be trusted to be a live
         *  process this host itself is running */
        bool pid_is_local;
        char *command;              /**< Command being executed */
    } process;                      /**< Process information */

    struct {
        char *icon_name;            /**< Icon image path */
        char *visible_icon_name;    /**< Icon on task bar */
        char **icons;
    } icon_info;

    struct {
        char *name;                 /**< Window name */
        char *visible_name;         /**< Visible name on taskbar */
        char *role_name;            /**< Role (for compatibility) */
        char *class_name[2];        /**< Window class (for grouping) */
    } info;

    xcb_window_t window;            /**< The actual XCB client */
    xcb_window_t parent_id;         /**< Pointer to the parent client */
    xcb_window_t id;                /**< Unique client identifier */

    xcb_window_t frame;             /**< Optional decoration frame */
    xcb_window_t titlebar;          /**< Optional titlebar window */
    xcb_window_t icon_window;       /**< Optional iconified placeholder */
    xcb_window_t transient_for;     /**< Parent window for dialogs,
                                         @c XCB_WINDOW_NONE when there
                                         is none */

    uint32_t title_height;          /**< Cached titlebar height */

    /**
     * @brief Border width most recently sent to the X server for this
     *        client's frame or window
     *
     * Named by @p target in @a ri_render_client.  The render pass skips
     * re-sending @a xcb_configure_window when the width would not
     * actually change.  @a client_init sets it to @c UINT32_MAX so that
     * the very first render always applies the real value, whatever
     * that value is.
     */
    uint32_t last_border_width;

    /**
     * @brief Border color most recently sent to the X server for this
     *        client's window
     *
     * The undecorated counterpart of @p last_border_width above:
     * a decorated client shows its focus through the frame the render
     * pass repaints anyway, and one without a frame has only this
     * border to show it with.  Skipped in the same way when the color
     * would not change, and set by @a client_init to @c UINT32_MAX so
     * the first render always applies the real one.
     */
    uint32_t last_border_color;

    /** Desktop index, @c 0xFFFFFFFF meaning every desktop */
    uint32_t desktop_id;
    uint32_t screen_id;         /**< Screen index */

    /**
     * @brief Timestamp of the client's most recent genuine
     *        keyboard/pointer input, kept live throughout its whole
     *        lifetime, not just read once at map time
     *
     * Read once from @c _NET_WM_USER_TIME when the client first maps
     * (@a client_init, in @c client.c), then kept up to date afterward
     * by @a client_update_user_time every time a real (not synthetic)
     * @c KeyPress or @c ButtonPress actually reaches this specific
     * client, the same @c _NET_WM_USER_TIME concept EWMH itself already
     * defines, just refreshed continuously rather than trusted only
     * once.
     *
     * The one place this whole thing exists for: deciding whether an
     * incoming @c _NET_ACTIVE_WINDOW request deserves real focus, by
     * comparing this field against the currently active client's (see
     * @c hi_handle_net_active_window, in @c handler/message.c).
     * A client whose genuine input predates the one already holding
     * focus has a weaker claim on the user's attention right now, so it
     * gets marked urgent instead of stealing focus outright.
     *
     * @see @a client_user_time_is_newer for the wraparound-safe way to
     *      compare two values of this field
     */
    uint32_t user_time;

    /**
     * @brief A client's border color and width, independent of the
     *        theme
     *
     * Overrides @c theme.window.active.border and
     * @c theme.window.inactive.border.  Deliberately generic, not tied
     * to any one feature.  Unset by default, in which case
     * @a client_border_color_apply falls back to the theme default
     * a plain client gets on every focus change.  A caller that sets
     * this needs no further involvement from @a ccmd_client_focus or
     * @a ccmd_client_unfocus beyond this one field.  The scratchpad in
     * @c scratchpad.c is the only caller that sets it today.
     */
    struct {
        bool is_set;
        uint32_t color;
        uint32_t width;
    } border_override;

    /**
     * @brief Cached, already built @c _NET_WM_ICON Picture
     *
     * Built once by @a wmicon_draw the first time this client's icon is
     * drawn, then reused on every later draw instead of re-fetching the
     * property, re-premultiplying every pixel, and re-uploading
     * a pixmap each time, none of which changes between draws unless
     * the client's @c _NET_WM_ICON property itself changes.
     *
     * Freed by @a wmicon_invalidate, called from the @c PropertyNotify
     * handler when that property changes, and from @a client_destroy so
     * the cached server-side resource does not leak.
     *
     * @see @c render/wmicon.h
     */
    wmicon_cache_td icon_pixmap_cache;

    /**
     * @brief EWMH-sourced hints: pre-existing @c _NET_WM_STATE,
     *        @c _NET_WM_PING, @c _NET_WM_SYNC_REQUEST
     *
     * @see @c client/ewmh.h
     */
    struct client_hints_ewmh_s hints_ewmh;

    /**
     * @brief ICCCM @c WM_COLORMAP_WINDOWS (§4.1.8): windows whose
     *        colormap this client wants installed when it gets the
     *        colormap focus, in priority order
     *
     * Absent (@p count of @c 0) for the overwhelming majority of
     * clients today, true-color displays having made per-application
     * private colormaps largely obsolete; tracked at all only for the
     * rare client still declaring one, typically on an 8-bit
     * @c PseudoColor-class display.  @c ColormapChangeMask is
     * subscribed on every window listed here, by
     * @a client_props_refresh_colormap_windows in
     * @c client/props.c, so a later change to any of their colormap
     * attributes is caught via @c ColormapNotify
     * (@a handler_colormap_notify, @c handler/colormap.c) even between
     * refreshes of this list.
     *
     * @see @a ccmd_client_focus, which installs these, in order,
     *      alongside @c SetInputFocus itself
     */
    struct {
        xcb_window_t windows[WM_COLORMAP_WINDOWS_MAX]; /**<
                                        Window IDs, in the client's
                                        priority order */
        xcb_colormap_t colormap_ids[WM_COLORMAP_WINDOWS_MAX]; /**<
                                        Each @p windows entry's cached
                                        colormap attribute, fetched once
                                        when the list is read and kept
                                        current by @c ColormapNotify
                                        afterward, so installing them
                                        on focus never needs a fresh
                                        round trip.  See
                                        @a handler_colormap_notify in
                                        @c handler/colormap.c. */
        uint32_t count;       /**< Number of entries in @p windows
                                   actually in use */
    } colormap_windows;

    /**
     * @brief ICCCM-sourced hints: @c WM_NORMAL_HINTS,
     *        @c WM_PROTOCOLS, @c WM_HINTS
     *
     * @see @c client/icccm.h
     */
    struct client_hints_icccm_s hints_icccm;

    struct client_layout_s layout;

    struct {
        int16_t x;                   /**< Saved icon X (-1 = unset) */
        int16_t y;                   /**< Saved icon Y (-1 = unset) */
    } icon_pos;

    struct client_properties_s properties;

    /**
     * @brief Whether @a transient_for names the root window: ICCCM
     *        §4.1.2.6 says this means transient for the client's whole
     *        application group rather than one specific window
     *
     * Set once, alongside @a transient_for itself, when
     * @c WM_TRANSIENT_FOR is read by
     * @a s_client_read_wm_hints_and_leader in @c client.c.
     * @a transient_parent stays @c NULL for a client with this set,
     * the same as for one that is not transient for anything at all.
     * Root is never a managed client itself, so the ordinary lookup
     * @a client_link_transient performs never finds a match.
     * @a client_group_transient_anchor in @c cmds/client/transient.c
     * resolves, fresh each time rather than a stored pointer, whichever
     * currently-mapped sibling sharing this client's group leader
     * should stand in for a specific parent wherever one is needed.
     * Stacking, raising and focus redirect all read it the same way @a
     * transient_parent itself is read elsewhere.
     * @a s_place_window_transient_centered in
     * @c policy/placement/window.c already resolves an equivalent
     * sibling for this client's initial centering.
     */
    bool is_transient_for_group;

    /* Flags with no shared theme */
    bool is_icon_mapped;            /**< Whether the icon is mapped */
    bool was_decorated_fullscreen;  /**< Save decor. state for full
                                         screen */
    bool has_rule_position_locked;  /**< Position was set by a rule;
                                         ignore client-initiated
                                         @c ConfigureRequests that try
                                         to move the window */
    bool has_rule_iconified;   /**< A rule gave 'apply.iconified' at map
                                    time; consulted in place of
                                    @c WM_HINTS initial state instead of
                                    calling @a ccmd_client_iconify
                                    directly, since the window is not
                                    mapped yet when rules run */
    bool is_rule_iconified;    /**< Value that rule asked for */
    bool has_rule_fullscreen;  /**< A rule gave @c apply.fullscreen at
                                    map time; applied after mapping, for
                                    the same reason as
                                    @c has_rule_iconified */
    bool is_rule_fullscreen;   /**< Value that rule asked for */
    bool has_rule_maximized;   /**< A rule gave @c apply.maximized at
                                    map time; applied after mapping, for
                                    the same reason as
                                    @c has_rule_iconified */
    bool is_rule_maximized;    /**< Value that rule asked for */
    bool has_rule_shaded;      /**< A rule gave @c apply.shaded at map
                                    time; applied after mapping, for the
                                    same reason as 'has_rule_iconified' */
    bool is_rule_shaded;       /**< Value that rule asked for */
    bool has_rule_hidden;      /**< A rule gave @c apply.hidden at map
                                    time; applied after mapping, for the
                                    same reason as @c has_rule_iconified */
    bool is_rule_hidden;       /**< Value that rule asked for */

    bool was_icon_cycle_selected;   /**< Whether the icon window was
                                         drawn with cycle-selection
                                         styling on its own most recent
                                         render, so
                                         @a ri_render_client_icon can
                                         skip its own work (window
                                         attributes, border, caption,
                                         pixmap, hints) when neither
                                         that nor @p is_outdated changed
                                         since; safe to default to
                                         @c false uninitialized, since
                                         a freshly iconified client is
                                         always @p is_outdated on its
                                         first render regardless of this
                                         field's value */

    bool is_outdated;           /**< Geometry or decoration changed;
                                     full configure+repaint needed on
                                     next render pass (cleared after
                                     render) */

    struct {
        /** Unmaps the window manager started, to be suppressed */
        uint8_t unmap;
        uint8_t focus_unmap;    /**< Synthetic unmaps;
                                     mustn't move focus */
    } ignore;

    /**
     * @brief Per-window opacity override from a matched rule
     *
     * The rules engine's equivalent of @p border_override above.
     * @a is_set_active / @a is_set_inactive independently mark whether
     * a rule overrode that one state's percentage, since a rule may
     * only ever override one of the two (see @c rules_apply_s in
     * @c rules/internal.h).  Whichever half is not overridden keeps
     * falling back to the theme's @p window.active.opacity /
     * @p window.inactive.opacity, the same way @p border_override
     * itself falls back to the theme when unset.
     */
    struct {
        bool is_set_active;
        bool is_set_inactive;
        uint8_t active;
        uint8_t inactive;
    } opacity_override;
};


/* Inline functions */
/* Save the current geometry of the client to the original geometry */
static inline void client_geometry_save(client_td *client)
{
    client->layout.geometry.old = client->layout.geometry.cur;
}


/* Restore the client's geometry from the saved original geometry */
static inline void client_geometry_restore(client_td *client)
{
    client->layout.geometry.cur = client->layout.geometry.old;
}


/* Remove the focus from the client, but take no action */
static inline void client_unfocus(client_td *client)
{
    client->properties.focusing = CLIENT_FOCUSING_UNFOCUSED;
}


/* Return the window that identifies which application 'client' belongs
 * to, for grouping purposes: 'WM_CLIENT_LEADER' when set, otherwise the
 * 'WM_HINTS' window group, otherwise 'XCB_WINDOW_NONE' when the client
 * declares no group at all */
static inline xcb_window_t client_group_leader(const client_td *client)
{
    if (client->hints_icccm.hints.client_leader != XCB_WINDOW_NONE) {
        return client->hints_icccm.hints.client_leader;
    }

    return client->hints_icccm.hints.group_leader;
}


/* Public interface */
/**
 * @brief Destroy the specified client and free associated resources
 *
 * Deallocates all memory associated with the client, including the XCB
 * window, all string buffers, and the client structure itself.
 *
 * @param client Pointer to the client structure to be destroyed
 *
 * @note Complexity: @e O(1)
 */
void client_destroy(client_td *client);

/**
 * @brief Refresh a client's @c user_time from a genuine input event
 *        that just reached it
 *
 * Called once per real (not synthetic) @c KeyPress or @c ButtonPress
 * that the X server actually delivered for this specific client's own
 * window, so its @c user_time stays a true, live record of when it was
 * last genuinely used, rather than the one-time snapshot
 * @c _NET_WM_USER_TIME provided back when it first mapped.
 *
 * @param client Client that just received the genuine input event
 * @param time   X server timestamp of the event, e.g., @c event->time
 *               straight off the @c xcb_key_press_event_t /
 *               @c xcb_button_press_event_t itself
 *
 * @note No-op if @p client is null, or if @p time is not actually newer
 *       than the client's current @c user_time (per
 *       @a client_user_time_is_newer), guarding against events a caller
 *       might ever hand over out of their true chronological order
 * @note Complexity: @e O(1)
 */
void client_update_user_time(client_td *client, uint32_t time);

/**
 * @brief Record the timestamp of a genuine user input event
 *
 * Kept apart from any one client, because what needs it is the
 * focus-granting path: ICCCM §4.1.7 requires the @c WM_TAKE_FOCUS
 * message to carry a valid timestamp and says in as many words that it
 * must not be @c CurrentTime, since the client is to echo that value
 * back in its @c SetInputFocus and is itself forbidden from using
 * @c CurrentTime there.  Focus is granted from places holding no event
 * of their own, a fallback after a window closed or a desktop switch
 * among them, and this is what they use.
 *
 * @param time X server timestamp of the event, straight off the
 *             @c xcb_key_press_event_t or
 *             @c xcb_button_press_event_t
 *
 * @note No-op for a timestamp not actually newer than the one held,
 *       guarding against events arriving out of order
 * @note Complexity: @e O(1)
 */
void client_note_user_time(uint32_t time);

/**
 * @brief Most recent genuine user input timestamp seen
 *
 * @return The timestamp, or @c 0 when no real input has arrived yet, in
 *         which case a caller has nothing better than
 *         @c XCB_CURRENT_TIME to fall back on
 *
 * @note Complexity: @e O(1)
 */
uint32_t client_last_user_time(void);

/**
 * @brief Send this client's border color and opacity for a focus state
 *
 * What an undecorated client shows its focus with, a decorated one
 * showing it through the frame the render pass repaints anyway.  Both
 * therefore follow from the same @p is_focused in the same pass, and
 * a path that changes the focus no longer has to remember to repaint
 * anything: forgetting to left a window still wearing the active border
 * after another had taken the focus from it.
 *
 * The border's width is not touched here.  X draws it outside the
 * window's rectangle, so changing it would shift the window's outer
 * edge; the render pass sends the width itself, where the geometry is
 * being settled anyway.
 *
 * @param client     Client to send the color for; may be @c NULL
 * @param is_focused Whether it currently holds the focus
 *
 * @note A no-op for a decorated client with a frame, for a fullscreen
 *       one, and whenever the color would not change
 * @note Complexity: @e O(1)
 */
void client_border_color_apply(client_td *client, bool is_focused);

/**
 * @brief The border width @p client currently themes its window or
 *        frame with
 *
 * @p border_override's width when @p is_set (the scratchpad,
 * @c scratchpad.c, is the only client that sets one as for today, and
 * never varies it with focus), or
 * @p theme->window.active/inactive.border.width otherwise (@p is_active
 * selects which).
 *
 * The same width the render pass applies for the exact same client and
 * focus state.  Meant for any caller that has to reserve room for
 * a border ahead of actually drawing on, e.g., sizing a client to fill
 * an area without its border ever spilling past that area's edge.
 *
 * @param client       Client to query
 * @param is_active    Ignored when @c border_override.is_set;
 *                     otherwise @c true selects
 *                     @c theme.window.active.border.width and
 *                     @c false selects the inactive one
 * @param ignore_frame Skip the @c 0 short-circuit this function,
 *                     otherwise always takes for an already-framed
 *                     client (see this function's body); needed by
 *                     a caller computing the width to (re)establish
 *                     @p client->layout.frame_extents with in the first
 *                     place, e.g., @a ccmd_client_unfullscreen, for
 *                     which @p client->frame already being non-zero
 *                     does not yet mean the frame already accounts for
 *                     it the way it does for every other caller
 *
 * @return @p client's current border width; @c 0 if @p client is
 *         @c NULL or has no theme
 *
 * @note Complexity: @e O(1)
 *
 * @see @a ccmd_client_maximize in @c cmds/client/geom.c
 */
static inline uint32_t client_border_width(const client_td *client,
        bool is_active, bool ignore_frame)
{
    uint32_t base_width;

    /* A decorated client's frame is always created with a native X11
     * 'border_width' of 0 ('ci_create_decorations' in 'client/geom.c'):
     * the themed margin around a decorated client's own content is
     * drawn as background color inset within the frame's own declared
     * width/height ('layout.frame_extents'), already fully accounted
     * for there, not as an X11 border layered on top of it the way
     * 'client_border_color_apply' (above) draws one on an undecorated
     * client's window.
     *
     * A caller reserving room for a client's border has nothing to
     * reserve here, so this returns 0 for a decorated client
     * ('frame != 0') even though 'window.active/inactive.border.width'
     * below is not itself 0, unless 'ignore_frame' says this specific
     * caller's frame does not actually reflect that yet. */
    if (client == NULL || client->config == NULL ||
            (!ignore_frame && client->frame != 0)) {
        return 0u;
    }

    /* 'border_override' (the scratchpad's case now) never varies with
     * focus (its width is set once and never revisited as seen in
     * 'scratchpad_notice_client_created', in 'scratchpad.c') so
     * 'is_active' only ever matters for the theme's fallback below,
     * where active and inactive can configure two genuinely different
     * widths, not just two colors. */
    if (client->border_override.is_set) {
        base_width = client->border_override.width;
    } else {
        base_width = (is_active)
            ? client->config->theme.window.active.border.width
            : client->config->theme.window.inactive.border.width;
    }

    /* Accessibility: never let a caller reserve less room than
     * 'a11y.focus-indicator.min-border-width' actually needs,
     * regardless of what the theme or 'border_override' specify; the
     * same floor the render pass already applies when it actually draws
     * the border this reserves room for */
    return (client->config->a11y.focus_indicator.min_border_width >
                base_width)
        ? client->config->a11y.focus_indicator.min_border_width
        : base_width;
}

/**
 * @brief Synchronize the inner client and titlebar windows with the
 *        frame extents stored in @p client
 *
 * After the outer frame is repositioned or resized, this function
 * repositions and resizes the reparented client content window and the
 * titlebar so that they stay correctly aligned inside the frame.
 *
 * @param client Pointer to the decorated client to synchronize
 *
 * @note A no-op when the client is not decorated or has no frame
 * @note Complexity: @e O(1)
 */
void client_decoration_layout_sync(client_td *client);

/**
 * @brief Update a decorated client's border width and titlebar height
 *        to match the current theme and focus state, resizing the frame
 *        around its content so the content's size never changes
 *
 * A theme's @p window.active.border.width and
 * @p window.inactive.border.width need not be equal; when they differ,
 * this grows or shrinks the frame's outer edge by the difference on
 * every side.
 *
 * @p window.titlebar.height can also have changed (e.g.,
 * a configuration reload picked up an edited theme file), in which case
 * only the top edge grows or shrinks by that additional amount.  Either
 * way the client's content window never moves or resizes (only how much
 * frame surrounds it changes) and the client is marked for a redraw so
 * the next render pass applies it and repaints the border, titlebar,
 * and its buttons at the new size.
 *
 * A fast no-op when neither value actually changed (the common case for
 * a plain focus change with the built-in default theme, whose active
 * and inactive border widths are equal), when @p client has no theme,
 * when @p client is not decorated, or when @p client is currently
 * fullscreen.  A fullscreen client's frame extents are deliberately
 * zeroed by @a ccmd_client_fullscreen regardless of what the theme
 * says, and this function would otherwise read that as "the theme
 * changed" and restore the border/titlebar space, reintroducing a gap
 * where the titlebar used to be even though it stays unmapped.
 *
 * @param client    Client whose layout is to be resynchronized
 * @param is_active Whether @p client currently holds focus (selects
 *                  which of the two theme border widths applies)
 *
 * @note Complexity: @e O(1)
 */
void client_theme_layout_resync(client_td *client, bool is_active);

/**
 * @brief One computed titlebar button position
 */
struct titlebar_button_layout_s {
    enum config_titlebar_button_e button;
    int16_t x;  /**< Frame-relative X of the button's left edge */
};

/**
 * @brief Side of one titlebar button
 *
 * Whatever @c window.titlebar.buttons.size asks for, held to what the
 * bar can hold: never below @c WM_DECOR_BTN_SIZE_MIN, never above the
 * titlebar height less two, and rounded down to an even number.
 *
 * Deliberately not derived from the titlebar height: making the bar
 * taller is a decision about the bar, and a theme that wanted larger
 * buttons with it would have had them appear without asking.
 *
 * @param theme   Theme asking for a size, or @c NULL for the default
 * @param title_h Titlebar height in pixels, which caps the answer
 *
 * @return Side of one button, in pixels
 *
 * @note Complexity: @e O(1)
 */
uint16_t client_titlebar_button_size(const struct config_theme_s *theme,
        uint16_t title_h);

/**
 * @brief Inset and stroke width for a button of the given side
 *
 * One value for both: the shapes are drawn inset by it and stroked
 * with it, which is what keeps their proportions as the button grows.
 *
 * @param btn_size Side of one button, from
 *                 @a client_titlebar_button_size
 *
 * @return Pixels to inset by and to stroke with
 *
 * @note Never below @c WM_DECOR_BTN_SHAPE_MIN, a single pixel line
 *       being all but invisible against a patterned titlebar
 * @note Complexity: @e O(1)
 */
uint16_t client_titlebar_button_shape_unit(uint16_t btn_size);

/**
 * @brief Compute where every configured titlebar button goes, and the
 *        horizontal span left over for the title text
 *
 * The single source of truth for titlebar layout.  Both
 * @a desktop_titlebar_buttons_draw (what gets painted) and the titlebar
 * click handler (what a click at a given X actually hits) call this, so
 * the two can never desynchronize the way two independently
 * hand-written copies of the same arithmetic could.
 *
 * Left buttons are placed left-to-right starting at
 * @p titlebar.padding.horizontal from the frame's left edge; right
 * buttons are placed right-to-left starting the same distance from the
 * right edge, with @c WM_DECOR_BTN_GAP between adjacent buttons on the
 * same side.  Every button is also inset from top and bottom by
 * @p titlebar.padding.vertical and vertically centered within whatever
 * room that leaves in @p title_h (falling back to plain centering with
 * no inset if the padding alone would not leave room for a full
 * button).
 *
 * The title span starts immediately after the left buttons (plus one
 * more padding gap and @c WM_DECOR_BTN_GAP for extra breathing room, or
 * just the edge padding if there are none) and ends immediately before
 * the right buttons (symmetrically), clamped to never go negative.
 *
 * Buttons are given up, least valuable first, until the row fits the
 * width available: the three that only report state go first, then
 * @c shade, which a double click on the titlebar already does, then
 * @c fullscreen, then @c hide ahead of @c iconize, since an iconified
 * window is recovered by clicking its icon and a hidden one is not,
 * and @c close last of all.  The order is by button, not by side, so
 * a @c close on the right outlives a @c layer on the left.  Nothing
 * is remembered between calls: widening the titlebar brings the
 * buttons back in the reverse order they went.
 *
 * @param theme       Theme providing the button lists and padding;
 *                    a @c NULL theme produces an empty layout
 * @param titlebar_w  Width of the titlebar in pixels, the frame width
 *                    less its left and right borders
 * @param title_h     Titlebar height in pixels, used to vertically
 *                    center the buttons
 * @param hide_pin    When @c true, the pin button (if configured) is
 *                    skipped entirely rather than placed and drawn:
 *                    the row closes the gap and shifts the following
 *                    buttons over, exactly as if the theme had never
 *                    listed it, rather than leaving it in place inert
 * @param hide_sticky Same as @p hide_pin, for the sticky button
 *                    instead
 * @param out_left    Receives up to @c CONFIG_MAX_TITLEBAR_BUTTONS
 *                    entries for the left side, in the theme's order
 * @param out_left_n  Receives the number of entries written to
 *                    @p out_left
 * @param out_right   Same as @p out_left, for the right side
 * @param out_right_n Same as @p out_left_n, for the right side
 * @param out_title_x Receives the left edge of the space available
 *                    for the title text, frame-relative
 * @param out_title_w Receives the width of that space; 0 if the buttons
 *                    leave no room at all
 * @param out_btn_y   Receives the Y position every button shares
 *
 * @note Complexity: @e O(1)
 */
void client_titlebar_layout(const struct config_theme_s *theme,
        uint16_t titlebar_w, uint16_t title_h, bool hide_pin,
        bool hide_sticky,
        struct titlebar_button_layout_s *restrict out_left,
        uint8_t *restrict out_left_n,
        struct titlebar_button_layout_s *restrict out_right,
        uint8_t *restrict out_right_n,
        int16_t *restrict out_title_x, uint16_t *restrict out_title_w,
        int16_t *restrict out_btn_y);

/**
 * @brief Apply ICCCM size hints to a requested client size
 *
 * Clamps and rounds @p width and @p height according to the client's
 * cached @c WM_NORMAL_HINTS constraints (minimum, maximum, and resize
 * increments).  If no valid hints are available, the requested size is
 * left unchanged.
 *
 * @param client Pointer to the client owning the size hints
 * @param width  In/out requested width
 * @param height In/out requested height
 *
 * @note No-op if any pointer argument is null
 * @note Complexity: @e O(1)
 */
void client_size_constrain(const client_td *client,
        uint32_t *restrict width, uint32_t *restrict height);

/**
 * @brief Clamp a width/height pair into a client's aspect-ratio bounds,
 *        adjusting height only
 *
 * Applies the @c PAspect portion of @c WM_NORMAL_HINTS on its own
 * (ICCCM §4.1.2.3), separately from @a client_size_constrain's
 * minimum/maximum/increment handling, so a caller that already produced
 * a fully snapped size for one axis (see @c input/kbd/interact.c's
 * @a ik_handle_resize) can still apply just this one constraint without
 * @a client_size_constrain's other rules snapping the values a second
 * time.
 *
 * @param client Pointer to the client owning the size hints
 * @param width  Width the ratio is measured against; never adjusted
 * @param height In/out height, adjusted to fit @p width's ratio
 *
 * @note No-op if any pointer argument is null, or if the client sets
 *       neither aspect-ratio bound
 * @note Complexity: @e O(1)
 */
void client_aspect_ratio_clamp(const client_td *client,
        uint32_t width, uint32_t *height);

/**
 * @brief Send a synthetic @c ConfigureNotify to an ICCCM-compliant
 *        client
 *
 * Sends a @c ConfigureNotify event directly to @p client->window with
 * screen-relative coordinates, as required by ICCCM §4.2.3 for
 * reparented clients.  Without this event an application that caches
 * its screen position from the @c ConfigureNotify generated by the
 * X server (which carries frame-relative coordinates) would be mistaken
 * about where it sits on screen.
 *
 * This must be called after any WM-initiated change to the client's
 * screen-relative position or content size:
 *
 * - after the initial frame placement (@a place_window_apply);
 * - after a keyboard or programmatic resize (@a ccmd_client_resize);
 * - after a gravity-triggered repositioning.
 *
 * @param connection XCB connection handle
 * @param client     Target client; must have a valid @c window field
 *
 * @note No-op when @p connection or @p client is null, or when
 *       @p client->window is 0
 * @note Complexity: @e O(1)
 */
void client_send_synthetic_configure_notify(
        xcb_connection_t *connection, const client_td *client);

/**
 * @brief Initialize a new client, adopting an existing X window
 *
 * Wraps an existing X window in a client structure without creating
 * a new window.  Reads the @c WM_NAME and @c WM_CLASS hints, queries
 * the current window geometry, and subscribes to property and structure
 * events on the window.
 *
 * @param connection Pointer to the XCB connection
 * @param ewmh       Pointer to EWMH connection
 * @param window     ID of the existing X window to adopt
 * @param config     Shared base/theme/a11y configuration for the
 *                   surface this client is being adopted onto
 *
 * @return A pointer to the client structure wrapping the window, or
 *         @c NULL if the window should not be managed (e.g.,
 *         override-redirect) or on allocation failure
 *
 * @note The returned client is @e not mapped by this function; the
 *       caller is responsible for calling @c xcb_map_window when ready
 * @note Complexity: @e O(1)
 */
client_td *client_init(xcb_connection_t *connection,
        xcb_ewmh_connection_t *ewmh, xcb_window_t window,
        const config_td *config);

/**
 * @brief Refresh the managed client's name from X11 properties
 *
 * Queries @c _NET_WM_NAME (UTF-8) first, then falls back to @c WM_NAME,
 * and writes the result into @p client->info.
 *
 * @param client Client to update
 *
 * @note Complexity: @e O(n), where @e n is the length of the name
 */
void client_props_refresh_name(client_td *client);

/**
 * @brief Keep a cached, possibly-truncated display name and its
 *        matching EWMH property (@c _NET_WM_VISIBLE_NAME or
 *        @c _NET_WM_VISIBLE_ICON_NAME) in sync with whether @p rendered
 *        actually differs from @p full_name right now
 *
 * Called every time something re-renders a name that may have needed
 * truncating to fit (a titlebar too narrow for the full title, an icon
 * caption under the same constraint).
 *
 * When @p rendered equals @p full_name (no truncation needed), the
 * property is deleted rather than set to a redundant copy of the
 * underlying name, per this project's reading of EWMH §5.4/5.5: nothing
 * behaves incorrectly if a client only checks for the property's
 * presence rather than comparing its value.
 *
 * @param client    Client the property belongs to
 * @param cached    @c client_td's cached buffer for this name
 *                  (@c info.visible_name or
 *                  @c icon_info.visible_icon_name), at least
 *                  @c CONFIG_MAX_LENGTH_NAME bytes
 * @param full_name The client's full, untruncated name
 * @param rendered  What was actually just rendered, truncated or not
 * @param set_fn    @c xcb_ewmh_set_wm_visible_name_checked or
 *                  @c xcb_ewmh_set_wm_visible_icon_name_checked,
 *                  whichever matches @p atom
 * @param atom      @c client->ewmh->_NET_WM_VISIBLE_NAME or
 *                  @c client->ewmh->_NET_WM_VISIBLE_ICON_NAME,
 *                  whichever matches @p set_fn
 *
 * @note A no-op, without any XCB round trip, whenever @p rendered
 *       already matches what @p cached currently holds, which is the
 *       ordinary case on every repaint after the first where truncation
 *       itself has not changed
 * @note Complexity: @e O(n), where @e n is the length of @p rendered
 */
void client_sync_visible_name(client_td *client, char *cached,
        const char *full_name, const char *rendered,
        xcb_void_cookie_t (*set_fn)(xcb_ewmh_connection_t *,
            xcb_window_t, uint32_t, const char *),
        xcb_atom_t atom);

/**
 * @brief Refresh the managed client's role from @c WM_WINDOW_ROLE
 *
 * Queries @c WM_WINDOW_ROLE and writes the result into
 * @p client->info.role_name.
 *
 * @param client Client to update
 *
 * @note Complexity: @e O(n), where @e n is the role string length
 */
void client_props_refresh_role(client_td *client);

/**
 * @brief Refresh the managed client's icon name from X11 properties
 *
 * Queries @c _NET_WM_ICON_NAME (UTF-8) first, then falls back to
 * @c WM_ICON_NAME, and writes the result into
 * @p client->icon_info.visible_icon_name.
 *
 * @param client Client to update
 *
 * @note Complexity: @e O(n), where @e n is the length of the name
 */
void client_props_refresh_icon_name(client_td *client);

/**
 * @brief Refresh @c WM_NORMAL_HINTS size-constraints from X11
 *        properties
 *
 * Re-reads @c WM_NORMAL_HINTS from the X server and updates the stored
 * size-hint fields (@c hints_icccm.size) in @p client.  Should be
 * called both at manage time and whenever a @c PROPERTY_NOTIFY event
 * for @c WM_NORMAL_HINTS is received (because applications such as gVim
 * update their increment grid and base size after initial startup).
 *
 * @param client Client to update
 *
 * @note Complexity: @e O(1)
 */
void client_props_refresh_normal_hints(client_td *client);

/**
 * @brief Refresh a client's @c WM_COLORMAP_WINDOWS list from X11
 *        properties
 *
 * Re-reads @c WM_COLORMAP_WINDOWS from the X server and populates
 * @p colormap_windows in @p client, capped at
 * @c WM_COLORMAP_WINDOWS_MAX entries, also fetching (a read, not a side
 * effect) each listed window's current colormap attribute
 * into @p colormap_windows.colormap_ids, so installing them on focus
 * never needs a fresh round trip of its own.  Should be called both at
 * manage time and whenever a @c PROPERTY_NOTIFY event for
 * @c WM_COLORMAP_WINDOWS is received, since ICCCM §4.1.8 requires
 * tracking changes to this property for as long as the client is
 * managed.  Never changes X server state itself; subscribing
 * @c ColormapChangeMask on the windows this populates, and actually
 * installing their cached colormap on focus, are both the caller's
 * responsibility (see @a client_init, in @c client.c, and
 * @a ccmd_client_focus, in @c cmds/client/focus.c).
 *
 * @param client Client to update
 *
 * @note Complexity: @e O(1), bounded by @c WM_COLORMAP_WINDOWS_MAX
 */
void client_props_refresh_colormap_windows(client_td *client);

/**
 * @brief Subscribe @c ColormapChangeMask on every window in a client's
 *        @c WM_COLORMAP_WINDOWS list
 *
 * The client's top-level window already gets this same mask bit from @a
 * client_init's event-mask setup; this covers the separate subwindows
 * ICCCM §4.1.8 lets a client list there instead, which
 * @a client_props_refresh_colormap_windows (@c client/props.c) must
 * already have populated @p client's @c colormap_windows with by the
 * time this runs.
 *
 * A later change to the list itself, or to any one of these subwindows'
 * colormap attribute, is caught separately (@c PROPERTY_NOTIFY and
 * @c ColormapNotify respectively; see @a handler_property_notify,
 * @c handler/focus.c, and @a handler_colormap_notify, in
 * @c handler/colormap.c).
 *
 * @param connection XCB connection
 * @param client     Client whose @c colormap_windows list is
 *                   subscribed to
 *
 * @note Complexity: @e O(n), where @e n is
 *       @p client->colormap_windows.count
 */
void client_subscribe_colormap_windows(
        xcb_connection_t *connection, const client_td *client);


#endif  /* ! CLIENT_H */
