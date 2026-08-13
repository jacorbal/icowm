/**
 * @file client.h
 *
 * @brief Window definition and declaration
 *
 * Defines the data structure that encapsulates all relevant information
 * about a specific client and its state within the windowing system
 * environment.  In this regards, provides a framework for managing the
 * visual and operational properties of the window, including its
 * identifier, name, state, graphical attributes, geometry, and behavior
 * preferences.
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


/**
 * @brief Possible client states a client can be in
 */
enum client_state_e {
    CLIENT_STATE_NORMAL,            /**< Regular state */
    CLIENT_STATE_ICONIFIED,         /**< Iconified */
//    CLIENT_STATE_SHADED,            /**< Shaded (rolled-up), if decorated */
    CLIENT_STATE_MAXIMIZED,         /**< Maximized */
    CLIENT_STATE_MAXIMIZED_HORZ,    /**< Maximized horiz. */
    CLIENT_STATE_MAXIMIZED_VERT,    /**< Maximized vert. */
    CLIENT_STATE_FULLSCREEN,        /**< Full screen */
};


/**
 * @brief Possible client types that can be
 */
enum client_type_e {
    CLIENT_TYPE_NORMAL,         /* Normal client */
    CLIENT_TYPE_DIALOG,         /* Dialogues or interaction needed */
    CLIENT_TYPE_TOOLBAR,        /* Quick actions or tools */
    CLIENT_TYPE_NOTIFICATION,   /* Temporal messages */
    CLIENT_TYPE_MENU,           /* Menu options */
    CLIENT_TYPE_DESKTOP,        /* The desktop "client" */
    CLIENT_TYPE_SPLASH,         /* The client is a loading message */
    CLIENT_TYPE_UTILITY,        /* Additional functions: control panels... */
    CLIENT_TYPE_DROPDOWN_MENU,  /* Drop-down menu */
    CLIENT_TYPE_POPUP_MENU,     /* Contextual menu */
    CLIENT_TYPE_COMBO,          /* Part of a combined frame */
    CLIENT_TYPE_TOOLTIP,        /* The client is a tooltip */
    CLIENT_TYPE_DOCK,           /* Dock or panel feature */
    CLIENT_TYPE_DND,            /* The client is being dragged */
};


/**
 * @brief Operations on a client
 */
enum window_operation_e {
    CLIENT_OPERATION_IDLE,      /* No operation ongoing */
    CLIENT_OPERATION_MOVING,    /* Window is being moved */
    CLIENT_OPERATION_RESIZING,  /* Window is being resized */
};


/**
 * @brief Window characteristics using flags using bitwise flags
 */
enum window_flags_e {
    CLIENT_FLAG_HIDDEN       = 1 << 0,
    CLIENT_FLAG_FOCUSABLE    = 1 << 1,
    CLIENT_FLAG_PIN       = 1 << 2,
    CLIENT_FLAG_SHADED       = 1 << 3,
    CLIENT_FLAG_DECORATED    = 1 << 4,
    CLIENT_FLAG_URGENT       = 1 << 5,
    CLIENT_FLAG_RESIZABLE    = 1 << 6,
    CLIENT_FLAG_DISABLED     = 1 << 7,
    CLIENT_FLAG_SKIP_TASKBAR = 1 << 8,
    CLIENT_FLAG_SKIP_PAGER   = 1 << 9,
    CLIENT_FLAG_MODAL        = 1 << 10, /**< Window is modal (EWMH) */
    CLIENT_FLAG_UNRESPONSIVE = 1 << 11, /**< No ping reply received */
    CLIENT_FLAG_MAX = 12,
};


/**
 * @brief Mutual exclusive status about the client focus
 */
enum client_focusing_e {
    CLIENT_FOCUSING_UNFOCUSED,  /**< No focus state */
    CLIENT_FOCUSING_FOCUSED,    /**< Window has focus */
};


/**
 * @brief Window mutual exclusive possible layers
 *
 * Identifies the layering options for clients, which affect their
 * visibility order on the screen.
 */
enum client_layer_e {
    CLIENT_LAYER_ABOVE,     /**< Always on top, in front */
    CLIENT_LAYER_NORMAL,    /**< Normal behavior */
    CLIENT_LAYER_BELOW,     /**< Always behind every client */
};


/**
 * @brief Anchor point used to determine how a client's position is
 *        adjusted relative to its size when resized
 *
 * @note ICCCM: "Window Managers MUST honor the @c win_gravity field of
 *              @c WM_NORMAL_HINTS for both @c MapRequest @e and
 *              @c ConfigureRequest events (ICCCM Version 2.0, §4.1.2.3
 *              and §4.1.5)"
 */
enum client_gravity_e {         /* Reference point fixed on resize: */
    CLIENT_GRAVITY_NORTH_WEST = 1,  /*  1: top-left corner of frame */
    CLIENT_GRAVITY_NORTH      = 2,  /*  2: center of top edge */
    CLIENT_GRAVITY_NORTH_EAST = 3,  /*  3: top-right corner of frame */
    CLIENT_GRAVITY_EAST       = 4,  /*  4: center of right edge */
    CLIENT_GRAVITY_SOUTH_EAST = 5,  /*  5: bottom-right corner of frame */
    CLIENT_GRAVITY_SOUTH      = 6,  /*  6: center of bottom edge */
    CLIENT_GRAVITY_SOUTH_WEST = 7,  /*  7: bottom-left corner of frame */
    CLIENT_GRAVITY_WEST       = 8,  /*  8: center of left edge */
    CLIENT_GRAVITY_CENTER     = 9,  /*  9: center of frame */
    CLIENT_GRAVITY_STATIC     = 10, /* 10: top-left corner of client area */
};

/**
 * @brief Window properties
 *
 * Encapsulates various properties of a client: state, layering
 * behavior, and any applicable flags.
 */
struct client_properties_s {
    uint16_t state;      /**< State (maximized, iconified,...) */
    uint16_t layer;      /**< Layer (above, normal, below) */
    uint16_t flags;      /**< Flags (hidden, sticky, focusable,...) */
    uint16_t type;       /**< Type (normal, notification...) */
    uint16_t operation;  /**< Operation (moving, resizing...) */
    uint16_t focusing;   /**< Focusing (focused, unfocused) */

    /**
     * @brief The @c state this client was in right before it was last
     *        iconified, so @c ccmd_client_restore can re-enter that
     *        exact state (normal, maximized in any of its three
     *        variants, or fullscreen) instead of always landing back
     *        on plain @c CLIENT_STATE_NORMAL
     *
     * Only meaningful while @c state is @c CLIENT_STATE_ICONIFIED;
     * @c CLIENT_STATE_NORMAL otherwise.  Also drives the icon's own
     * state-hint letter (see @c ri_draw_icon_hints in
     * render/icon.c): none for @c CLIENT_STATE_NORMAL, 'f' for
     * @c CLIENT_STATE_FULLSCREEN, 'm' for @c CLIENT_STATE_MAXIMIZED,
     * 'h' for @c CLIENT_STATE_MAXIMIZED_HORZ, and 'v' for
     * @c CLIENT_STATE_MAXIMIZED_VERT.
     */
    uint16_t pre_iconify_state;
};


/**
 * @brief Window layout, position, dimensions and strut
 */
struct client_layout_s {
    /* @biref This are the position and dimensions of the client
     *
     * @note The "old" one is to save the position when the "cur" one is
     *       needed to be recovered later; as in saving the current
     *       geometry before maximizing, and restoring it with the "old"
     *       position and dimensions.
     */
    struct {
        struct geometry_s cur;
        struct geometry_s old;
    } geometry;

    /**
     * @brief Area where the client exist on the screen, plus the area
     *        are marked off-bounds for client placement
     *
     * @note Traditional strut property will be @p strut_partial.sides
     *       when @p .start and @p .end are zero
     */
    struct strut_partial_s strut_partial;
    uint16_t gravity;               /* Window gravity */
    struct sides_s frame_extents;   /* [left, right, top, bottom] */
};


/**
 * @brief Structure for a client in an XCB environment
 *
 * The @p screen_id and @p desktop_id fields link the client to its
 * respective screen and desktop, and @p parent is a pointer to its
 * parent client if needed.
 *
 * Additionally, the @p properties field contains various settings that
 * define the behavior and appearance of the client, while the @p theme
 * pointer allows for dynamic theming, enabling customization of the
 * client's visual aspects based on user preferences or system
 * themes.
 */
typedef struct client_s {
    xcb_connection_t *connection;   /**< XCB display / connection */
    xcb_ewmh_connection_t *ewmh;    /**< Pointer to EWMH connection */
    xcb_window_t window;            /**< The actual XCB client */
    xcb_window_t parent_id;         /**< Pointer to the parent client */
    xcb_window_t id;                /**< Unique client identifier */

    xcb_window_t frame;             /**< Optional decoration frame */
    xcb_window_t titlebar;          /**< Optional titlebar window */
    xcb_window_t icon_window;       /**< Optional iconified placeholder */

    /**
     * @brief Cached, already built '_NET_WM_ICON' Picture
     *
     * Built once by 'wmicon_draw' (see render/wmicon.h) the first
     * time this client's icon is drawn, then reused on every later
     * draw instead of re-fetching the property, re-premultiplying
     * every pixel, and re-uploading a pixmap each time, none of which
     * changes between draws unless the client's own '_NET_WM_ICON'
     * property itself changes.  Freed by 'wmicon_invalidate', called
     * from the 'PropertyNotify' handler when that property changes,
     * and from 'client_destroy' so the cached server-side resource
     * does not leak.
     */
    wmicon_cache_td icon_pixmap_cache;

    bool is_icon_mapped;            /**< Whether icon window is mapped */
    bool was_decorated_fullscreen;  /**< Save decor. state for full screen */
    uint8_t ignore_unmap;           /**< WM-initiated unmaps to suppress */
    uint8_t ignore_focus_unmap;     /**< Synthetic unmaps; mustn't move focus */
    int16_t icon_x;                 /**< Saved icon X (-1 = unset) */
    int16_t icon_y;                 /**< Saved icon Y (-1 = unset) */
    uint16_t title_height;          /**< Cached titlebar height */
    struct timespec shade_transition_time; /**< Monotonic time of the
                                                 client's last shade or
                                                 unshade; used to
                                                 recognize and ignore a
                                                 client's own
                                                 'ConfigureRequest' as a
                                                 stale reaction to that
                                                 transition rather than
                                                 a genuine independent
                                                 resize, see
                                                 'handler_configure_request' */

    uint32_t desktop_id;            /**< Desktop index (0xFFFFFFFF for all) */
    uint32_t screen_id;             /**< Screen index */

    struct {
        char *name;                 /**< Window name */
        char *visible_name;         /**< Visible name on taskbar */
        char *role_name;            /**< Role (for compatibility) */
        char *class_name[2];        /**< Window class (for grouping) */
    } info;

    struct {
        char *icon_name;            /**< Icon image path */
        char *visible_icon_name;    /**< Icon on task bar */
        char **icons;
    } icon_info;

    struct config_theme_s *theme;               /**< User defined theme */
    const struct config_base_s *config_base;    /**< Base configuration */

    uint32_t user_time;             /**< Time since last used */

    struct {
        pid_t pid;                  /**< PID being executed */
        char *command;              /**< Command being executed */
    } process;                      /**< Process information */

    struct client_layout_s layout;
    struct client_properties_s properties;

    bool has_wm_delete_window;      /**< Supports 'WM_DELETE_WINDOW' */
    xcb_atom_t wm_delete_atom;      /**< Cached 'WM_DELETE_WINDOW' atom */
    xcb_window_t transient_for;     /**< Parent window for dialogs
                                         (0 or 'XCB_WINDOW_NONE' if none) */
    /**
     * @brief ICCCM WM_NORMAL_HINTS size constraints
     */
    struct {
        bool valid;         /**< True when hints were read from server */
        int32_t min_w;      /**< Minimum width  (0 = unset) */
        int32_t min_h;      /**< Minimum height (0 = unset) */
        int32_t max_w;      /**< Maximum width  (0 = unset) */
        int32_t max_h;      /**< Maximum height (0 = unset) */
        int32_t base_w;     /**< Base width for increment arithmetic */
        int32_t base_h;     /**< Base height for increment arithmetic */
        int32_t inc_w;      /**< Width increment  (0 or 1 = no grid) */
        int32_t inc_h;      /**< Height increment (0 or 1 = no grid) */
    } size_hints;

    /**
     * @brief ICCCM 'WM_PROTOCOLS' state
     */
    bool has_wm_take_focus;         /**< Supports 'WM_TAKE_FOCUS' */
    xcb_atom_t wm_take_focus_atom;  /**< Cached 'WM_TAKE_FOCUS' atom */

    /**
     * @brief ICCCM 'WM_HINTS' fields
     */
    bool wm_input_hint;         /**< Client accepts input (default true) */
    bool initial_iconic;        /**< Map iconic for 'WM_HINTS' initial state */
    xcb_window_t group_leader;  /**< Window group leader, or 'XCB_NONE' */
    xcb_window_t client_leader; /**< ICCCM 'WM_CLIENT_LEADER' window, or
                                     'XCB_NONE' if unset.  Used together
                                     with @p group_leader (see
                                     @c client_group_leader) to cluster
                                     windows belonging to the same
                                     application for placement */

    bool rule_position_locked;  /**< Position was set by a rule; ignore
                                     client-initiated @c ConfigureRequests
                                     that try to move the window */

    /**
     * @brief EWMH '_NET_WM_PING' state
     */
    bool has_net_wm_ping;       /**< Supports '_NET_WM_PING' protocol */
    uint32_t last_ping_sent;    /**< X timestamp of last ping sent */
    uint32_t last_ping_reply;   /**< X timestamp of last ping reply */

    /**
     * @brief EWMH @c _NET_WM_SYNC_REQUEST state
     *
     * @c sync_counter and @c sync_alarm hold plain XCB XIDs (an
     * @c xcb_sync_counter_t / @c xcb_sync_alarm_t are both a
     * @c uint32_t under the hood) rather than the XSync-typed values,
     * so this header does not need to pull in @c xcb/sync.h; call sites
     * that actually issue XSync requests cast as needed.
     *
     * @see @c ccmd_client_resize (throttling) and @c handler_sync_event
     * (acknowledgement) for how these fields are driven
     */
    bool has_net_wm_sync_request;   /**< Supports @c _NET_WM_SYNC_REQUEST */
    uint32_t sync_counter;          /**< XSync counter XID the CLIENT
                                         created and advertised via its
                                         own @c _NET_WM_SYNC_REQUEST_COUNTER
                                         property (read, not created, by
                                         'client_init'), or 0 if unset */
    uint32_t sync_alarm;            /**< WM-owned alarm XID watching
                                         @p sync_counter for positive
                                         transitions, or 0 */
    uint32_t sync_value;            /**< Local shadow of the last
                                         counter value sent to the
                                         client (low 32 bits; a single
                                         resize session never comes
                                         close to wrapping) */
    bool sync_waiting;              /**< @c true between sending a sync
                                         request and receiving the
                                         matching @c AlarmNotify (or
                                         giving up after @c sync_wait_ticks) */
    uint8_t sync_wait_ticks;        /**< Consecutive resize attempts
                                         spent waiting for the current
                                         request; past
                                         @c WM_SYNC_MAX_WAIT_TICKS the
                                         pending geometry is
                                         force-applied so an
                                         unresponsive client can never
                                         freeze interactive resize */
    bool sync_has_pending;          /**< @c true when a newer geometry
                                         arrived while @p sync_waiting
                                         and still needs to be applied */
    struct {
        int32_t x;
        int32_t y;
        uint32_t w;
        uint32_t h;
    } sync_pending_geom;            /**< Geometry to apply once the
                                         pending request is acknowledged
                                         or times out */

    bool is_outdated;               /**< Geometry or decoration changed;
                                         full configure+repaint needed
                                         on next render pass (cleared
                                         after render) */
    uint32_t last_border_width; /**< Border width most recently sent to
                                     the X server for this client's
                                     frame/window (see 'target' in
                                     'ri_render_client'), so the render
                                     pass can skip re-sending
                                     'xcb_configure_window' when it
                                     would not actually change anything.
                                     Initialized to 'UINT32_MAX' by
                                     'client_init' so the very first
                                     render always applies the real
                                     value regardless of what it is */
    bool icon_last_cycle_sel;   /**< Whether the icon window was drawn
                                     with cycle-selection styling on
                                     its own most recent render, so
                                     'ri_render_client_icon' can skip
                                     its own work (window attributes,
                                     border, caption, pixmap, hints)
                                     when neither that nor 'is_outdated'
                                     changed since; safe to default to
                                     'false' uninitialized, since a
                                     freshly iconified client is always
                                     'is_outdated' on its first render
                                     regardless of this field's value */
} client_td;


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


/* Set the focus on client, if focusable, but take no action */
static inline void client_focus(client_td *client)
{
    if (client->properties.flags & CLIENT_FLAG_FOCUSABLE) {
        client->properties.focusing = CLIENT_FOCUSING_FOCUSED;
    }
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
    if (client->client_leader != XCB_WINDOW_NONE) {
        return client->client_leader;
    }

    return client->group_leader;
}


/* Public interface */
/**
 * @brief Destroy the specified client and free associated resources
 *
 * Deallocates all memory associated with the client, including the
 * XCB window, all string buffers, and the client structure itself.
 *
 * @param client Pointer to the client structure to be destroyed
 *
 * @note Complexity: @e O(1)
 */
void client_destroy(client_td *client);

/**
 * @brief Synchronize the inner client and titlebar windows with the
 *        frame extents stored in @p client
 *
 * After the outer frame is repositioned or resized, this function
 * repositions and resizes the reparented client content window and the
 * titlebar so that they stay correctly aligned inside the frame.  The
 * function is a no-op when the client is not decorated or has no frame.
 *
 * @param client Pointer to the decorated client to synchronize
 *
 * @note Complexity: @e O(1)
 */
void client_sync_decoration_layout(client_td *client);

/**
 * @brief Update a decorated client's border width and titlebar height
 *        to match the current theme and focus state, resizing the frame
 *        around its content so the content's own size never changes
 *
 * A theme's @c window.active.border.width and
 * @c window.inactive.border.width need not be equal; when they differ,
 * this grows or shrinks the frame's outer edge by the difference on
 * every side. @c window.titlebar.height can also have changed
 * (e.g., a configuration reload picked up an edited theme file), in
 * which case only the top edge grows or shrinks by that additional
 * amount. Either way the client's own content window never moves or
 * resizes (only how much frame surrounds it changes) and the client is
 * marked for a redraw so the next render pass applies it and repaints
 * the border, titlebar, and its buttons at the new size. A fast no-op
 * when neither value actually changed (the common case for a plain
 * focus change with the built-in default theme, whose active and
 * inactive border widths are equal), when @p client has no theme, when
 * @p client is not decorated, or when @p client is currently
 * fullscreen: a fullscreen client's frame extents are deliberately
 * zeroed by @c ccmd_client_fullscreen regardless of what the theme
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
void client_resync_theme_layout(client_td *client, bool is_active);

/**
 * @brief One computed titlebar button position
 */
struct titlebar_button_layout_s {
    enum config_titlebar_button_e button;
    int16_t x;  /**< Frame-relative X of the button's left edge */
};

/**
 * @brief Compute where every configured titlebar button goes, and the
 *        horizontal span left over for the title text
 *
 * The single source of truth for titlebar layout: both
 * @c desktop_draw_titlebar_buttons (what gets painted) and the titlebar
 * click handler (what a click at a given X actually hits) call this, so
 * the two can never desynchronize the way two independently
 * hand-written copies of the same arithmetic could.
 *
 * Left buttons are placed left-to-right starting at
 * @c titlebar.padding.horizontal from the frame's left edge; right
 * buttons are placed right-to-left starting the same distance from the
 * right edge, with @c WM_DECOR_BTN_GAP between adjacent buttons on the
 * same side. Every button is also inset from top and bottom by
 * @c titlebar.padding.vertical and vertically centered within whatever
 * room that leaves in @p title_h (falling back to plain centering with
 * no inset if the padding alone would not leave room for a full
 * button). The title span starts immediately after the left buttons
 * (plus one more padding gap and @c WM_DECOR_BTN_GAP for extra
 * breathing room, or just the edge padding if there are none) and ends
 * immediately before the right buttons (symmetrically), clamped to
 * never go negative.
 *
 * @param theme       Theme providing the button lists and padding;
 *                    a @c NULL theme produces an empty layout
 * @param frame_w     Total frame width in pixels
 * @param title_h     Titlebar height in pixels, used to vertically
 *                    center the buttons
 * @param hide_pin    When @c true, the pin button (if configured) is
 *                    skipped entirely rather than placed and drawn:
 *                    the row closes the gap and shifts the following
 *                    buttons over, exactly as if the theme had never
 *                    listed it, rather than leaving it in place inert
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
        uint16_t frame_w, uint16_t title_h, bool hide_pin,
        struct titlebar_button_layout_s *out_left,
        uint8_t *out_left_n,
        struct titlebar_button_layout_s *out_right,
        uint8_t *out_right_n,
        int16_t *out_title_x, uint16_t *out_title_w,
        int16_t *out_btn_y);

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
void client_constrain_size(const client_td *client,
        uint32_t *width, uint32_t *height);

/**
 * @brief Send a synthetic @c ConfigureNotify to an ICCCM-compliant
 *        client
 *
 * Sends a @c ConfigureNotify event directly to @p client->window with
 * screen-relative coordinates, as required by ICCCM §4.2.3 for
 * reparented clients.  Without this event an application that caches
 * its screen position from the @c ConfigureNotify generated by the
 * X server (which carries frame-relative coordinates) would have
 * a incorrect understanding of where it sits on screen.
 *
 * This must be called after any WM-initiated change to the client's
 * screen-relative position or content size:
 *   - after the initial frame placement (@c place_apply)
 *   - after a keyboard or programmatic resize (@c ccmd_client_resize)
 *   - after a gravity-triggered repositioning
 *
 * @param connection XCB connection handle
 * @param client     Target client; must have a valid @c window field
 *
 * @note No-op when @p connection or @p client is null, or when
 *       @p client->window is 0
 * @note Complexity: @e O(1)
 */
void client_send_synthetic_configure_notify(xcb_connection_t *connection,
        const client_td *client);

/**
 * @brief Initialize a new client, adopting an existing X window
 *
 * Wraps an existing X window in a client structure without creating a
 * new window.  Reads the @c WM_NAME and @c WM_CLASS hints, queries the
 * current window geometry, and subscribes to property and structure
 * events on the window.
 *
 * @param connection  Pointer to the XCB connection
 * @param ewmh        Pointer to EWMH connection
 * @param window      ID of the existing X window to adopt
 * @param theme       Pointer to the theme configuration
 * @param config_base Pointer to the base configuration
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
        struct config_theme_s *theme,
        const struct config_base_s *config_base);

/**
 * @brief Update the content of the specified client
 *
 * Performs a soft update on the client by refreshing its internal state
 * as needed.  This may include checking for property changes and
 * synchronizing the visual state with the internal representation.
 *
 * @param client Pointer to the client to be updated
 *
 * @note Complexity: @e O(1)
 */
void client_update(client_td *client);

/**
 * @brief Refresh the managed client's name from X11 properties
 *
 * Queries @c _NET_WM_NAME (UTF-8) first, then falls back to
 * @c WM_NAME, and writes the result into @p client->info.
 *
 * @param client Client to update
 *
 * @note Complexity: @e O(n), where @e n is the length of the name
 */
void client_props_refresh_name(client_td *client);

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
 * @brief Refresh @c WM_HINTS fields from X11 properties
 *
 * Re-reads @c WM_HINTS from the X server and updates @p client with the
 * current input model, urgency flag, and window group.
 *
 * @param client Client to update
 *
 * @note Complexity: @e O(1)
 */
void client_props_refresh_wm_hints(client_td *client);

/**
 * @brief Refresh @c WM_NORMAL_HINTS size-constraints from X11
 *        properties
 *
 * Re-reads @c WM_NORMAL_HINTS from the X server and updates the stored
 * size-hint fields (@c size_hints) in @p client.  Should be called both
 * at manage time and whenever a @c PROPERTY_NOTIFY event for
 * @c WM_NORMAL_HINTS is received, because applications such as gVim
 * update their increment grid and base size after initial startup.
 *
 * @param client Client to update
 *
 * @note Complexity: @e O(1)
 */
void client_props_refresh_normal_hints(client_td *client);

/**
 * @brief Macro that evaluates to the client iconify state
 *
 * @note Complexity: @e O(1)
 */
#define client_is_iconified(w) \
    ((w)->properties.state == (uint16_t) CLIENT_STATE_ICONIFIED)

/**
 * @brief Macro that evaluates to the client maximization state
 *
 * @note Complexity: @e O(1)
 */
#define client_is_maximized(w) \
    ((w)->properties.state == (uint16_t) CLIENT_STATE_MAXIMIZED)

/**
 * @brief Macro that evaluates to the client horizontal maximization
 *        state
 *
 * @note Complexity: @e O(1)
 */
#define client_is_maximized_horz(w) \
    ((w)->properties.state == (uint16_t) CLIENT_STATE_MAXIMIZED_HORZ)

/**
 * @brief Macro that evaluates to the client vertical maximization
 *        state
 *
 * @note Complexity: @e O(1)
 */
#define client_is_maximized_vert(w) \
    ((w)->properties.state == (uint16_t) CLIENT_STATE_MAXIMIZED_VERT)

/**
 * @brief Macro that evaluates to whether the client is maximized in any
 *        way (fully, horizontally-only, or vertically-only)
 *
 * Used wherever an operation needs to know only that the client's
 * @p layout.geometry.old already holds a valid pre-maximize geometry
 * (regardless of which maximize variant is currently active), most
 * notably to decide whether it is safe to call @c client_geometry_save
 * again without stranding that original geometry.
 *
 * @note Complexity: @e O(1)
 *
 * @see @c ccmd_client_maximize, @c ccmd_client_maximize_horz,
 *      @c ccmd_client_maximize_vert, and @c ccmd_client_iconify.
 */
#define client_is_maximized_any(w) \
    (client_is_maximized(w) || client_is_maximized_horz(w) || \
     client_is_maximized_vert(w))

/**
 * @brief Macro that evaluates to the client full screen state
 *
 * @note Complexity: @e O(1)
 */
#define client_is_fullscreen(w) \
    ((w)->properties.state == (uint16_t) CLIENT_STATE_FULLSCREEN)

/**
 * @brief Macro that evaluates to the client hidden flag
 *
 * @note Complexity: @e O(1)
 */
#define client_is_hidden(w) \
    ((w)->properties.flags & CLIENT_FLAG_HIDDEN)

/**
 * @brief Macro that evaluates to the client focused flag
 *
 * @note Complexity: @e O(1)
 */
#define client_is_focusable(w) \
    ((w)->properties.flags & CLIENT_FLAG_FOCUSABLE)

/**
 * @brief Macro that evaluates to the client shade flag
 *
 * @note Complexity: @e O(1)
 */
#define client_is_shaded(w) \
    ((w)->properties.flags & CLIENT_FLAG_SHADED)

/**
 * @brief Macro that evaluates to the client pinned flag
 *
 * @note Complexity: @e O(1)
 */
#define client_is_pinned(w) \
    ((w)->properties.flags & CLIENT_FLAG_PIN)

/**
 * @brief Macro that evaluates to the negation of the client pinned
 *        flag
 *
 * @note Complexity: @e O(1)
 */
#define client_is_unpinned(w) \
    (!client_is_pinned(w))

/**
 * @brief Macro that evaluates to the client decoration flag
 *
 * @note Complexity: @e O(1)
 */
#define client_is_decorated(w) \
    ((w)->properties.flags & CLIENT_FLAG_DECORATED)

/**
 * @brief Macro that evaluates to the client urgency flag
 *
 * @note Complexity: @e O(1)
 */
#define client_is_urgent(w) \
    ((w)->properties.flags & CLIENT_FLAG_URGENT)

/**
 * @brief Macro that evaluates to the client disabled flag
 *
 * @note Complexity: @e O(1)
 */
#define client_is_disabled(w) \
    ((w)->properties.flags & CLIENT_FLAG_DISABLED)

/**
 * @brief Macro that evaluates to the client resizable flag
 *
 * @note Complexity: @e O(1)
 */
#define client_is_resizable(w) \
    ((w)->properties.flags & CLIENT_FLAG_RESIZABLE)

/**
 * @brief Macro that evaluates to the client modal flag
 *
 * @note Complexity: @e O(1)
 */
#define client_is_modal(w) \
    ((w)->properties.flags & CLIENT_FLAG_MODAL)

/**
 * @brief Macro that evaluates to the client unresponsive flag
 *
 * @note Complexity: @e O(1)
 */
#define client_is_unresponsive(w) \
    ((w)->properties.flags & CLIENT_FLAG_UNRESPONSIVE)

/**
 * @brief Macro that sets the modal flag of a client
 *
 * @note Complexity: @e O(1)
 */
#define client_mark_modal(w) \
    safeflg_set(&((w)->properties.flags), \
            CLIENT_FLAG_MODAL, (1 << CLIENT_FLAG_MAX))

/**
 * @brief Macro that clears the modal flag of a client
 *
 * @note Complexity: @e O(1)
 */
#define client_unmark_modal(w) \
    safeflg_unset(&((w)->properties.flags), \
            CLIENT_FLAG_MODAL, (1 << CLIENT_FLAG_MAX))

/**
 * @brief Macro that sets the unresponsive flag of a client
 *
 * @note Complexity: @e O(1)
 */
#define client_mark_unresponsive(w) \
    safeflg_set(&((w)->properties.flags), \
            CLIENT_FLAG_UNRESPONSIVE, (1 << CLIENT_FLAG_MAX))

/**
 * @brief Macro that clears the unresponsive flag of a client
 *
 * @note Complexity: @e O(1)
 */
#define client_mark_responsive(w) \
    safeflg_unset(&((w)->properties.flags), \
            CLIENT_FLAG_UNRESPONSIVE, (1 << CLIENT_FLAG_MAX))

/**
 * @brief Macro that sets the hidden flag of a client
 *
 * @param w Pointer to the client structure whose visibility is to be
 *          cleared
 *
 * @note Complexity: @e O(1)
 */
#define client_hide(w) \
    safeflg_set(&((w)->properties.flags), \
            CLIENT_FLAG_HIDDEN, (1 << CLIENT_FLAG_MAX))

/**
 * @brief Macro that clears the hidden flag of a client
 *
 * @param w Pointer to the client structure whose visibility is to be
 *          set
 *
 * @note Complexity: @e O(1)
 */
#define client_unhide(w) \
    safeflg_unset(&((w)->properties.flags), \
            CLIENT_FLAG_HIDDEN, (1 << CLIENT_FLAG_MAX))

/**
 * @brief Macro that toggles the hidden flag of a client
 *
 * @param w Pointer to the client structure whose visibility is to be
 *          toggled
 *
 * @note Complexity: @e O(1)
 */
#define client_toggle_hide(w) \
    safeflg_toggle(&((w)->properties.flags), \
            CLIENT_FLAG_HIDDEN, (1 << CLIENT_FLAG_MAX))

/**
 * @brief Macro that sets the focus flag of a client
 *
 * @param w Pointer to the client structure whose focus is to be set
 *
 * @note Complexity: @e O(1)
 */
#define client_allow_focus(w) \
    safeflg_set(&((w)->properties.flags), \
            CLIENT_FLAG_FOCUSABLE, (1 << CLIENT_FLAG_MAX))

/**
 * @brief Macro that clears the focus flag of a client
 *
 * @param w Pointer to the client structure whose focus is to be
 *          cleared
 *
 * @note Complexity: @e O(1)
 */
#define client_forbid_focus(w) \
    safeflg_unset(&((w)->properties.flags), \
            CLIENT_FLAG_FOCUSABLE, (1 << CLIENT_FLAG_MAX))

/**
 * @brief Macro that sets the shade flag of a client
 *
 * @param w Pointer to the client structure whose shade is to be set
 *
 * @note Complexity: @e O(1)
 */
#define client_shade(w) \
    safeflg_set(&((w)->properties.flags), \
            CLIENT_FLAG_SHADED, (1 << CLIENT_FLAG_MAX))

/**
 * @brief Macro that clears the shade flag of a client
 *
 * @param w Pointer to the client structure whose shade is to be
 *          cleared
 *
 * @note Complexity: @e O(1)
 */
#define client_unshade(w) \
    safeflg_unset(&((w)->properties.flags), \
            CLIENT_FLAG_SHADED, (1 << CLIENT_FLAG_MAX))

/**
 * @brief Macro that toggles the shade flag of a client
 *
 * @param w Pointer to the client structure whose shade is to be
 *          toggled
 *
 * @note Complexity: @e O(1)
 */
#define client_toggle_shade(w) \
    safeflg_toggle(&((w)->properties.flags), \
            CLIENT_FLAG_SHADED, (1 << CLIENT_FLAG_MAX))

/**
 * @brief Macro that sets the pin flag of a client
 *
 * @param w Pointer to the client structure whose pin is to be set
 *
 * @note Complexity: @e O(1)
 */
#define client_pin(w) \
    safeflg_set(&((w)->properties.flags), \
            CLIENT_FLAG_PIN, (1 << CLIENT_FLAG_MAX))

/**
 * @brief Macro that clears the pin flag of a client
 *
 * @param w Pointer to the client structure whose pin is to be
 *          cleared
 *
 * @note Complexity: @e O(1)
 */
#define client_unpin(w) \
    safeflg_unset(&((w)->properties.flags), \
            CLIENT_FLAG_PIN, (1 << CLIENT_FLAG_MAX))

/**
 * @brief Macro that toggles the pin flag of a client
 *
 * @param w Pointer to the client structure whose pin is to be
 *          toggled
 *
 * @note Complexity: @e O(1)
 */
#define client_toggle_pin(w) \
    safeflg_toggle(&((w)->properties.flags), \
            CLIENT_FLAG_PIN, (1 << CLIENT_FLAG_MAX))

/**
 * @brief Macro that sets the decoration flag of a client
 *
 * @param w Pointer to the client structure whose decoration is to be set
 *
 * @note Complexity: @e O(1)
 */
#define client_decorate(w) \
    safeflg_set(&((w)->properties.flags), \
            CLIENT_FLAG_DECORATED, (1 << CLIENT_FLAG_MAX))

/**
 * @brief Macro that clears the decoration flag of a client
 *
 * @param w Pointer to the client structure whose decoration is to be
 *          cleared
 *
 * @note Complexity: @e O(1)
 */
#define client_undecorate(w) \
    safeflg_unset(&((w)->properties.flags), \
            CLIENT_FLAG_DECORATED, (1 << CLIENT_FLAG_MAX))

/**
 * @brief Macro that toggles the decoration flag of a client
 *
 * @param w Pointer to the client structure whose decoration is to be
 *          toggled
 *
 * @note Complexity: @e O(1)
 */
#define client_toggle_decorate(w) \
    safeflg_toggle(&((w)->properties.flags), \
            CLIENT_FLAG_DECORATED, (1 << CLIENT_FLAG_MAX))

/**
 * @brief Macro that sets the urgent flag of a client
 *
 * @param w Pointer to the client structure whose urgent is to be set
 *
 * @note Complexity: @e O(1)
 */
#define client_urge(w) \
    safeflg_set(&((w)->properties.flags), \
            CLIENT_FLAG_URGENT, (1 << CLIENT_FLAG_MAX))

/**
 * @brief Macro that clears the urgent flag of a client
 *
 * @param w Pointer to the client structure whose urgent is to be
 *          cleared
 *
 * @note Complexity: @e O(1)
 */
#define client_unurge(w) \
    safeflg_unset(&((w)->properties.flags), \
            CLIENT_FLAG_URGENT, (1 << CLIENT_FLAG_MAX))

/**
 * @brief Macro that toggles the urgent flag of a client
 *
 * @param w Pointer to the client structure whose urgent is to be
 *          toggled
 *
 * @note Complexity: @e O(1)
 */
#define client_toggle_urge(w) \
    safeflg_toggle(&((w)->properties.flags), \
            CLIENT_FLAG_URGENT, (1 << CLIENT_FLAG_MAX))


/**
 * @brief Macro that sets the resizable flag of a client
 *
 * @param w Pointer to the client structure whose resizable is to be set
 *
 * @note Complexity: @e O(1)
 */
#define client_allow_resize(w) \
    safeflg_set(&((w)->properties.flags), \
            CLIENT_FLAG_RESIZABLE, (1 << CLIENT_FLAG_MAX))

/**
 * @brief Macro that clears the resizable flag of a client
 *
 * @param w Pointer to the client structure whose resizable is to be
 *          cleared
 *
 * @note Complexity: @e O(1)
 */
#define client_forbid_resize(w) \
    safeflg_unset(&((w)->properties.flags), \
            CLIENT_FLAG_RESIZABLE, (1 << CLIENT_FLAG_MAX))

/**
 * @brief Macro that sets the disable flag of a client
 *
 * @param w Pointer to the client structure whose disable is to be set
 *
 * @note Complexity: @e O(1)
 */
#define client_disable(w) \
    safeflg_set(&((w)->properties.flags), \
            CLIENT_FLAG_DISABLED, (1 << CLIENT_FLAG_MAX))

/**
 * @brief Macro that clears the disable flag of a client
 *
 * @param w Pointer to the client structure whose disable is to be
 *          cleared
 *
 * @note Complexity: @e O(1)
 */
#define client_enable(w) \
    safeflg_unset(&((w)->properties.flags), \
            CLIENT_FLAG_DISABLED, (1 << CLIENT_FLAG_MAX))

/**
 * @brief Macro that toggles the disable flag of a client
 *
 * @param w Pointer to the client structure whose disable is to be
 *          toggled
 *
 * @note Complexity: @e O(1)
 */
#define client_toggle_disable(w) \
    safeflg_toggle(&((w)->properties.flags), \
            CLIENT_FLAG_DISABLED, (1 << CLIENT_FLAG_MAX))

/**
 * @brief Macro that sets the skip-taskbar flag of a client
 *
 * @param w Pointer to the client structure
 *
 * @note Complexity: @e O(1)
 */
#define client_skip_taskbar(w) \
    safeflg_set(&((w)->properties.flags), \
            CLIENT_FLAG_SKIP_TASKBAR, (1 << CLIENT_FLAG_MAX))

/**
 * @brief Macro that clears the skip-taskbar flag of a client
 *
 * @param w Pointer to the client structure
 *
 * @note Complexity: @e O(1)
 */
#define client_unskip_taskbar(w) \
    safeflg_unset(&((w)->properties.flags), \
            CLIENT_FLAG_SKIP_TASKBAR, (1 << CLIENT_FLAG_MAX))

/**
 * @brief Macro that sets the skip-pager flag of a client
 *
 * @param w Pointer to the client structure
 *
 * @note Complexity: @e O(1)
 */
#define client_skip_pager(w) \
    safeflg_set(&((w)->properties.flags), \
            CLIENT_FLAG_SKIP_PAGER, (1 << CLIENT_FLAG_MAX))

/**
 * @brief Macro that clears the skip-pager flag of a client
 *
 * @param w Pointer to the client structure
 *
 * @note Complexity: @e O(1)
 */
#define client_unskip_pager(w) \
    safeflg_unset(&((w)->properties.flags), \
            CLIENT_FLAG_SKIP_PAGER, (1 << CLIENT_FLAG_MAX))


#endif  /* ! CLIENT_H */
