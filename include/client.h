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


/**
 * @brief Possible client states a client can be in
 */
enum client_state_e {
    CLIENT_STATE_NORMAL,            /**< Regular state */
    CLIENT_STATE_ICONIFIED,         /**< Iconified */
    CLIENT_STATE_MAXIMIZED,         /**< Maximized */
    CLIENT_STATE_MAXIMIZED_HORZ,    /**< Maximized horizontally */
    CLIENT_STATE_MAXIMIZED_VERT,    /**< Maximized vertically */
    CLIENT_STATE_FULLSCREEN,        /**< Full screen */
};


/**
 * @brief Possible client types that can be
 */
enum client_type_e {
    CLIENT_TYPE_NORMAL,         /**< Normal client */
    CLIENT_TYPE_DIALOG,         /**< Dialogues or interaction needed */
    CLIENT_TYPE_TOOLBAR,        /**< Quick actions or tools */
    CLIENT_TYPE_NOTIFICATION,   /**< Temporal messages */
    CLIENT_TYPE_MENU,           /**< Menu options */
    CLIENT_TYPE_DESKTOP,        /**< The desktop "client" */
    CLIENT_TYPE_SPLASH,         /**< The client is a loading message */
    CLIENT_TYPE_UTILITY,        /**< Additional functions: control panels... */
    CLIENT_TYPE_DROPDOWN_MENU,  /**< Drop-down menu */
    CLIENT_TYPE_POPUP_MENU,     /**< Contextual menu */
    CLIENT_TYPE_COMBO,          /**< Part of a combined frame */
    CLIENT_TYPE_TOOLTIP,        /**< The client is a tooltip */
    CLIENT_TYPE_DOCK,           /**< Dock or panel feature */
    CLIENT_TYPE_DND,            /**< The client is being dragged */
};


/**
 * @brief Operations on a client
 */
enum window_operation_e {
    CLIENT_OPERATION_IDLE,      /**< No operation ongoing */
    CLIENT_OPERATION_MOVING,    /**< Window is being moved */
    CLIENT_OPERATION_RESIZING,  /**< Window is being resized */
};


/**
 * @brief Window characteristics using flags using bitwise flags
 */
enum window_flags_e {
    CLIENT_FLAG_HIDDEN       = 1 << 0,
    CLIENT_FLAG_FOCUSABLE    = 1 << 1,
    CLIENT_FLAG_PIN          = 1 << 2,
    CLIENT_FLAG_SHADED       = 1 << 3,
    CLIENT_FLAG_DECORATED    = 1 << 4,
    CLIENT_FLAG_URGENT       = 1 << 5,
    CLIENT_FLAG_RESIZABLE    = 1 << 6,
    CLIENT_FLAG_DISABLED     = 1 << 7,
    CLIENT_FLAG_SKIP_TASKBAR = 1 << 8,
    CLIENT_FLAG_SKIP_PAGER   = 1 << 9,
    CLIENT_FLAG_MODAL        = 1 << 10, /**< Window is modal (EWMH) */
    CLIENT_FLAG_UNRESPONSIVE = 1 << 11, /**< No ping reply received */

    /**
     * @brief Every aspect of this client's own presentation and extent
     *        is entirely policy-controlled, never subject to any
     *        mutation initiated by a user or script
     *
     * Deliberately generic, not tied to any one feature: set once by
     * whichever policy owns a client with this flag (the scratchpad,
     * @c scratchpad.c, is the only one that does so today), then
     * checked everywhere an action initiated by user or script might
     * otherwise change decoration, pin state, layer, iconified state,
     * position, or size; none of which need to know what feature
     * actually set this, or why, only that it is set.
     *
     * A caller with feature-specific behavior beyond "refuse this
     * mutation entirely" (the scratchpad hiding itself on losing focus,
     * say, rather than merely refusing to be unfocused) still calls
     * into the owning feature's own module directly for that, the same
     * as before; this flag only ever centralizes the "refuse" half
     * shared by every such feature, not anything specific to one of
     * them.
     *
     * @see @a client_is_locked's own callers
     */
    CLIENT_FLAG_LOCKED = 1 << 12,

    /**
     * @brief Never offered as the fallback focus target when some other
     *        client on the same desktop loses focus
     *
     * Deliberately generic, not tied to any one feature, the same
     * spirit as @c CLIENT_FLAG_LOCKED above, i.e., set once by
     * whichever policy owns a client with this flag (the scratchpad,
     * @c scratchpad.c, is the only one that does so now), then checked
     * by every "who should get focus next?" search on the current
     * desktop, neither of which needs to know what feature actually set
     * this, or why, only that this client's own visibility is managed
     * by something else entirely (its own toggle, in the scratchpad's
     * case) and should never be picked as an incidental side effect of
     * another client merely losing focus.
     *
     * @see @a ccmd_client_focus_fallback in @c cmds/client/focus.c, and
     *      @a s_restore_focus_after_client_loss in @c handler/map.c
     */
    CLIENT_FLAG_NO_FOCUS_FALLBACK = 1 << 13,

    /**
     * @brief This client currently holds real X11 input focus
     *
     * Set by @a ccmd_client_focus itself (@c cmds/client/focus.c),
     * cleared by @a ccmd_client_unfocus, right alongside the real
     * focus grant/revocation each one performs, rather than derived
     * on demand from @c desktop->client_active_id: unlike every other
     * flag in this @c enum, "is this the desktop's own active client"
     * needs an external lookup (which desktop, and whether that
     * desktop's own bookkeeping has actually been updated yet by
     * whichever caller is in the middle of granting focus right now)
     * that the other, genuinely self-contained flags never do, and
     * that external dependency is exactly the kind of fragility
     * @a ccmd_client_sync_states (@c cmds/client/ewmh.c) is built to
     * avoid: every @c _NET_WM_STATE atom it publishes reads directly
     * off @p client's own fields, this one included, with nothing
     * else to go stale or disagree with it.
     *
     * @see @a client_is_focused below
     */
    CLIENT_FLAG_FOCUSED = 1 << 14,

    CLIENT_FLAG_MAX = 15,
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
 * @note ICCCM: "Window Managers MUST honor the @p win_gravity field of
 *       @c WM_NORMAL_HINTS for both @c MapRequest @e and
 *       @c ConfigureRequest events (ICCCM Version 2.0, §4.1.2.3 and
 *       §4.1.5)"
 */
enum client_gravity_e {         /* Reference point fixed on resize: */
    CLIENT_GRAVITY_NORTH_WEST = 1,  /**<  1: top-left corner of frame */
    CLIENT_GRAVITY_NORTH      = 2,  /**<  2: center of top edge */
    CLIENT_GRAVITY_NORTH_EAST = 3,  /**<  3: top-right corner of frame */
    CLIENT_GRAVITY_EAST       = 4,  /**<  4: center of right edge */
    CLIENT_GRAVITY_SOUTH_EAST = 5,  /**<  5: bottom-right corner of frame */
    CLIENT_GRAVITY_SOUTH      = 6,  /**<  6: center of bottom edge */
    CLIENT_GRAVITY_SOUTH_WEST = 7,  /**<  7: bottom-left corner of frame */
    CLIENT_GRAVITY_WEST       = 8,  /**<  8: center of left edge */
    CLIENT_GRAVITY_CENTER     = 9,  /**<  9: center of frame */
    CLIENT_GRAVITY_STATIC     = 10, /**< 10: top-left corner of client area */
};

/**
 * @brief Adjust a frame position to keep a gravity anchor fixed
 *        across a size change
 *
 * Computes the displacement that preserves the anchor point defined by
 * @p gravity after the frame changes from (@p old_w x @p old_h) to
 * (@p new_w x @p new_h) and adds it to @p *out_x and @p *out_y.
 * No-op for @c CLIENT_GRAVITY_NORTH_WEST and @c CLIENT_GRAVITY_STATIC.
 * See ICCCM §§4.1.2.3 and 4.1.5.
 *
 * @param out_x   Frame x to adjust in place
 * @param out_y   Frame y to adjust in place
 * @param old_w   Frame width before the size change
 * @param old_h   Frame height before the size change
 * @param new_w   Frame width after the size change
 * @param new_h   Frame height after the size change
 * @param gravity Client @a win_gravity value
 *
 * @note Complexity: @e O(1)
 */
void client_gravity_adjust_pos(int32_t *restrict out_x,
        int32_t *restrict out_y,
        uint32_t old_w, uint32_t old_h,
        uint32_t new_w, uint32_t new_h,
        uint16_t gravity);

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
     *        iconified, so @a ccmd_client_restore can re-enter that
     *        exact state (normal, maximized in any of its three
     *        variants, or fullscreen) instead of always landing back on
     *        plain @c CLIENT_STATE_NORMAL
     *
     * Only meaningful while @p state is @c CLIENT_STATE_ICONIFIED;
     * @c CLIENT_STATE_NORMAL otherwise.  Also drives the icon's own
     * state-hint letter:
     *
     * - nil for @c CLIENT_STATE_NORMAL;
     * - 'f' for @c CLIENT_STATE_FULLSCREEN;
     * - 'm' for @c CLIENT_STATE_MAXIMIZED;
     * - 'h' for @c CLIENT_STATE_MAXIMIZED_HORZ; and
     * - 'v' for @c CLIENT_STATE_MAXIMIZED_VERT.
     *
     * @see @a ri_icon_hints_draw in @c render/icon.c:
     */
    uint16_t pre_iconify_state;
};


/**
 * @brief Window layout, position, dimensions and strut
 */
struct client_layout_s {
    /**
     * @brief Position and dimensions of the client
     *
     * @note The @p old one is to save the position when the @p cur one
     *       is needed to be recovered later; as in saving the current
     *       geometry before maximizing, and restoring it with the
     *       @p old position and dimensions.
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
     *        when @p .start and @p .end are zero
     */
    struct strut_partial_s strut_partial;

    /**
     * @brief Window gravity
     *
     * Set once, at @a client_init (@c client.c), from this client's own
     * @c WM_NORMAL_HINTS if it already declares @c win_gravity there,
     * or from @c windows.gravity in @c config.json otherwise that
     * config field is only ever a @e fallback for a client that never
     * states its own gravity, at any point in its life, not a way to
     * override one that does.
     *
     * A later @c WM_NORMAL_HINTS update keeps this field in sync with
     * whatever 'win_gravity' that update itself carries, per ICCCM's
     * own "MUST honor" mandate; several common toolkits (XTerm's Xt
     * shell, LibreOffice's VCL) only send their real hints a moment
     * after their first map, once fonts and chrome are ready, which is
     * when most real clients' own gravity actually takes hold over the
     * config default.
     *
     * @see @a client_props_refresh_normal_hints in @c client/props.c
     */
    uint16_t gravity;
    struct sides_s frame_extents;   /**< [left, top, right, bottom] */
};


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
typedef struct client_s {
    xcb_connection_t *connection;   /**< XCB display / connection */
    xcb_ewmh_connection_t *ewmh;    /**< Pointer to EWMH connection */
    xcb_window_t window;            /**< The actual XCB client */
    xcb_window_t parent_id;         /**< Pointer to the parent client */
    xcb_window_t id;                /**< Unique client identifier */

    xcb_window_t frame;             /**< Optional decoration frame */
    xcb_window_t titlebar;          /**< Optional titlebar window */
    xcb_window_t icon_window;       /**< Optional iconified placeholder */
    xcb_window_t transient_for;     /**< Parent window for dialogs (0
                                         or @c XCB_WINDOW_NONE if none) */

    /**
     * @brief Direct pointer to the managed parent this client is
     *        transient for, or @c NULL if it is not transient for
     *        anything (or its declared parent is not, or not yet,
     *        managed)
     *
     * Resolved once, right after this client is added to its own
     * desktop, from @a transient_for by looking up the matching
     * managed client; kept in sync from then on by whichever code
     * removes a client from the transient tree (see @a transients
     * and @a transient_node below).  Walking this pointer directly
     * is @e O(1) per step, unlike re-resolving @a transient_for's
     * raw window ID through a lookup every time the parent is
     * needed.
     */
    struct client_s *transient_parent;

    /**
     * @brief This client's own direct transient children, or
     *        @c NULL if it has none
     *
     * Lazily allocated the first time a child registers itself here
     * (see @a transient_parent above), never created up front for
     * every client regardless of whether it will ever have any:
     * the overwhelming majority of managed clients are never
     * transient for anything and never have any children either, so
     * paying for an empty list on all of them would be pure waste.
     * Each item's own @c data is the child @c client_td* itself.
     */
    cdlist_td *transients;

    /**
     * @brief This client's own position within @a transient_parent
     *        's own @a transients list, or @c NULL if it is not
     *        currently registered in any parent's list
     *
     * The whole reason a real, maintained list of transient children
     * is worth having at all instead of scanning every client on
     * every desktop whenever the transient family needs walking:
     * with this node cached, removing this client from its parent's
     * list when it closes, or is reparented, is a true @e O(1)
     * operation (@c cdlist_rem_next on this node's own @c prev),
     * never a search.
     */
    cdlist_item_td *transient_node;

    /**
     * @brief Cached, already built @c _NET_WM_ICON Picture
     *
     * Built once by @a wmicon_draw the first time this client's icon is
     * drawn, then reused on every later draw instead of re-fetching the
     * property, re-premultiplying every pixel, and re-uploading
     * a pixmap each time, none of which changes between draws unless
     * the client's own @c _NET_WM_ICON property itself changes.
     *
     * Freed by @a wmicon_invalidate, called from the @c PropertyNotify
     * handler when that property changes, and from @a client_destroy so
     * the cached server-side resource does not leak.
     *
     * @see @c render/wmicon.h
     */
    wmicon_cache_td icon_pixmap_cache;

    /* Flags with no shared theme; kept together for layout, not
     * meaning -- see each one's own doc comment below */
    bool is_icon_mapped;            /**< Whether icon window is mapped */
    bool was_decorated_fullscreen;  /**< Save decor. state for full
                                         screen */
    bool has_rule_position_locked;  /**< Position was set by a rule;
                                         ignore client-initiated
                                         @c ConfigureRequests that try
                                         to move the window */
    bool was_icon_cycle_selected;   /**< Whether the icon window was
                                         drawn with cycle-selection
                                         styling on its own most recent
                                         render, so
                                         @a ri_render_client_icon can
                                         skip its own work (window
                                         attributes, border, caption,
                                         pixmap, hints) when neither
                                         that nor @p is_outdated
                                         changed since; safe to
                                         default to @c false
                                         uninitialized, since a
                                         freshly iconified client is
                                         always @p is_outdated on its
                                         first render regardless of
                                         this field's value */

    struct {
        uint8_t unmap;               /**< WM-initiated unmaps to suppress */
        uint8_t focus_unmap;         /**< Synthetic unmaps; mustn't move
                                          focus */
    } ignore;

    struct {
        int16_t x;                   /**< Saved icon X (-1 = unset) */
        int16_t y;                   /**< Saved icon Y (-1 = unset) */
    } icon_pos;

    uint32_t title_height;          /**< Cached titlebar height */
    uint32_t last_border_width;     /**< Border width most recently sent
                                         to the X server for this
                                         client's frame/window (see
                                         @p target in @a ri_render_client),
                                         so the render pass can skip
                                         re-sending @a xcb_configure_window
                                         when it would not actually
                                         change anything.  Initialized
                                         to @c UINT32_MAX by
                                         @a client_init so the very
                                         first render always applies the
                                         real value regardless of what
                                         it is */

    /**
     * @brief Monotonic time of the client's last shade or unshade
     *
     * Used to recognize and ignore a client's own @c ConfigureRequest
     * as a stale reaction to that transition rather than a genuine
     * independent resize.
     *
     * @see @a handler_configure_request
     */
    struct timespec shade_transition_time;

    /**
     * @brief Monotonic time of the client's last entry into or exit
     *        from fullscreen
     *
     * Used the same way as @c shade_transition_time for the same reason
     *
     * @see @a handler_configure_request
     */
    struct timespec fullscreen_transition_time;

    uint32_t desktop_id;        /**< Desktop index (@c 0xFFFFFFFF for all) */
    uint32_t screen_id;         /**< Screen index */

    struct {
        char *name;             /**< Window name */
        char *visible_name;     /**< Visible name on taskbar */
        char *role_name;        /**< Role (for compatibility) */
        char *class_name[2];    /**< Window class (for grouping) */
    } info;

    struct {
        char *icon_name;            /**< Icon image path */
        char *visible_icon_name;    /**< Icon on task bar */
        char **icons;
    } icon_info;

    /**
     * @brief Shared base/theme/a11y configuration for this client's own
     *        surface
     *
     * The same @c config_td every other client on this surface also
     * points to; never reassigned after @a client_init (see @a
     * client_init's own callers), though the configuration it points
     * to can still change in place at any time from a live reload or
     * a runtime theme request, which every client picking it up on its
     * next read is exactly the point of sharing one pointer instead of
     * each client copying its own snapshot.
     */
    const config_td *config;

    struct {
        bool is_set;
        uint32_t color;
        uint32_t width;
    } border_override;  /**< A client's own border color and width,
                             independent of
                             @p config->theme.window.active /
                             @p config->theem.inactive.border.
                             Deliberately generic, not tied to any one
                             feature.  Unset by default, in which case
                             @a client_border_apply (@c client.h) falls
                             back to the usual theme default a plain
                             client already gets on every focus change;
                             a caller that sets this (the scratchpad,
                             @c scratchpad.c, is the only one that does
                             so today) needs no further involvement from
                             @a ccmd_client_focus or @a ccmd_client_unfocus
                             (both in @c cmds/client/focus.c) beyond
                             that single field */

    /**
     * @brief Per-window opacity override from a matched rule
     *
     * The rules engine's own equivalent of @p border_override above.
     * @a is_set_active / @a is_set_inactive independently mark whether
     * a rule overrode that one state's own percentage, since a rule may
     * only ever override one of the two (see @c rules_apply_s in
     * @c rules/internal.h).  Whichever half is not overridden keeps
     * falling back to the theme's own @p window.active.opacity /
     * @p window.inactive.opacity, the same way @p border_override
     * itself falls back to the theme when unset.
     */
    struct {
        bool is_set_active;
        bool is_set_inactive;
        uint8_t active;
        uint8_t inactive;
    } opacity_override;

    /**
     * @brief Timestamp of the client's own most recent genuine
     *        keyboard/pointer input, kept live throughout its whole
     *        lifetime, not just read once at map time
     *
     * Read once from @c _NET_WM_USER_TIME when the client first maps
     * (@a client_init, client.c), then kept up to date afterward by
     * @a client_update_user_time every time a real (not synthetic)
     * @c KeyPress or @c ButtonPress actually reaches this specific
     * client, the same @c _NET_WM_USER_TIME concept EWMH itself
     * already defines, just refreshed continuously rather than
     * trusted only once.
     *
     * The one place this whole thing exists for: deciding whether an
     * incoming @c _NET_ACTIVE_WINDOW request deserves real focus, by
     * comparing this field against the currently active client's own
     * (see @c hi_handle_net_active_window, handler/message.c).  A
     * client whose own genuine input predates the one already
     * holding focus has a weaker claim on the user's attention right
     * now, so it gets marked urgent instead of stealing focus
     * outright.
     *
     * @see @a client_user_time_is_newer for the wraparound-safe way
     *      to compare two values of this field
     */
    uint32_t user_time;

    struct {
        pid_t pid;                  /**< PID being executed */
        char *command;              /**< Command being executed */
    } process;                      /**< Process information */

    struct client_layout_s layout;
    struct client_properties_s properties;

    /**
     * @brief ICCCM-sourced hints: @c WM_NORMAL_HINTS, @c WM_PROTOCOLS,
     *        @c WM_HINTS
     */
    struct {
        /**
         * @brief ICCCM @c WM_NORMAL_HINTS size constraints
         */
        struct {
            bool is_valid;          /**< True when hints were read from
                                         server */
            bool has_position;      /**< True when the client itself
                                         requested a position
                                         (@c USPosition or @c PPosition)
                                         rather than leaving it to this
                                         window manager's own policy */
            struct position_s req_pos; /**< Client-requested position,
                                            valid only when
                                            @p has_position is true */
            struct dimensions_s min;   /**< Minimum size (0,0 = unset) */
            struct dimensions_s max;   /**< Maximum size (0,0 = unset) */
            struct dimensions_s base;  /**< Base size for increment
                                            arithmetic */
            struct dimensions_s inc;   /**< Size increment (0 or 1
                                             = no grid) */
            struct aspect_range_s aspect; /**< Minimum/maximum w/h
                                               ratio (0,0 = unset) */
        } size;

        /**
         * @brief ICCCM @c WM_PROTOCOLS state
         */
        struct {
            bool has_delete;            /**< Supports @c WM_DELETE_WINDOW */
            xcb_atom_t delete_atom;     /**< Cached @c WM_DELETE_WINDOW
                                             atom */
            bool has_take_focus;        /**< Supports @c WM_TAKE_FOCUS */
            xcb_atom_t take_focus_atom; /**< Cached @c WM_TAKE_FOCUS atom */
        } protocols;

        /**
         * @brief ICCCM @c WM_HINTS fields
         */
        struct {
            bool has_input_hint;        /**< Client accepts input
                                             (default true) */
            bool is_initial_iconic;     /**< Map iconic for @c WM_HINTS
                                             initial state */
            xcb_window_t group_leader;  /**< Window group leader, or
                                             @c XCB_NONE */
            xcb_window_t client_leader; /**< ICCCM @c WM_CLIENT_LEADER
                                             window, or @c XCB_NONE if
                                             unset.  Used together with
                                             @p group_leader (see
                                             @a client_group_leader) to
                                             cluster windows belonging
                                             to the same application
                                             for placement */
        } hints;
    } hints_icccm;

    /**
     * @brief EWMH-sourced hints: pre-existing @c _NET_WM_STATE,
     *        @c _NET_WM_PING, @c _NET_WM_SYNC_REQUEST
     */
    struct {
        /**
         * @brief Whether the client's own pre-existing @c _NET_WM_STATE
         *        (read before this window was ever mapped) already
         *        included the matching state bit
         *
         * EWMH's own correct way for a client to request one of these
         * states from the outset, distinct from
         * @p hints_icccm.hints.is_initial_iconic (ICCCM @c WM_HINTS,
         * not EWMH) though serving the exact same role:
         * @a handler_map_request consults this once the newly mapped
         * client's own frame/decoration already exist, the same way it
         * already consults @p hints_icccm.hints.is_initial_iconic for
         * @c IconicState.  A client requesting both maximized axes at
         * once is maximized on both, rather than one call each.
         *
         * @see @a s_client_read_pre_existing_state (client.c)
         */
        struct {
            bool is_fullscreen;
            bool is_maximized_horz;
            bool is_maximized_vert;
        } initial_state;

        /**
         * @brief EWMH @c _NET_WM_PING state
         */
        struct {
            bool is_supported;        /**< Supports @c _NET_WM_PING
                                           protocol */
            uint32_t last_sent;       /**< X timestamp of last ping
                                           sent */
            uint32_t last_reply;      /**< X timestamp of last ping
                                           reply */
        } ping;

        /**
         * @brief EWMH @c _NET_WM_SYNC_REQUEST state
         *
         * @p counter and @p alarm hold plain XCB XIDs (an
         * @c xcb_sync_counter_t / @c xcb_sync_alarm_t are both a
         * @c uint32_t under the hood) rather than the XSync-typed values,
         * so this header does not need to pull in @c xcb/sync.h; call
         * sites that actually issue XSync requests cast as needed.
         *
         * @see @c ccmd_client_resize (throttling) and
         *      @c handler_sync_event (acknowledgement) for how these
         *      fields are driven
         */
        struct {
            bool is_supported;    /**< Supports @c _NET_WM_SYNC_REQUEST */
            uint32_t counter;     /**< XSync counter XID the CLIENT
                                       created and advertised via its
                                       own @c _NET_WM_SYNC_REQUEST_COUNTER
                                       property (read, not created, by
                                       @a client_init), or 0 if unset */
            uint32_t alarm;       /**< WM-owned alarm XID watching
                                       @p counter for positive
                                       transitions, or 0 */
            uint32_t value;       /**< Local shadow of the last
                                       counter value sent to the
                                       client (low 32 bits; a single
                                       resize session never comes
                                       close to wrapping) */
            bool is_waiting;      /**< @c true between sending a sync
                                       request and receiving the
                                       matching @c AlarmNotify (or
                                       giving up after @p wait_ticks) */
            uint8_t wait_ticks;   /**< Consecutive resize attempts
                                       spent waiting for the current
                                       request; past
                                       @c WM_SYNC_MAX_WAIT_TICKS the
                                       pending geometry is
                                       force-applied so an
                                       unresponsive client can never
                                       freeze interactive resize */
            bool has_pending;     /**< @c true when a newer geometry
                                       arrived while @p is_waiting
                                       and still needs to be applied */
            struct geometry_s pending_geom; /**< Geometry to apply once
                                                 the pending request is
                                                 acknowledged or times
                                                 out */
        } sync;
    } hints_ewmh;

    bool is_outdated;   /**< Geometry or decoration changed; full
                             configure & repaint needed on next render
                             pass (cleared after render) */
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
 * @brief Refresh a client's own @c user_time from a genuine input
 *        event that just reached it
 *
 * Called once per real (not synthetic) @c KeyPress or @c ButtonPress
 * that the X server actually delivered for this specific client's
 * own window, so its own @c user_time stays a true, live record of
 * when it was last genuinely used, rather than the one-time snapshot
 * @c _NET_WM_USER_TIME provided back when it first mapped.
 *
 * @param client Client that just received the genuine input event
 * @param time X server timestamp of the event, e.g., @c event->time
 *             straight off the @c xcb_key_press_event_t /
 *             @c xcb_button_press_event_t itself
 *
 * @note No-op if @p client is null, or if @p time is not actually
 *       newer than the client's own current @c user_time (per
 *       @a client_user_time_is_newer), guarding against events a
 *       caller might ever hand over out of their true chronological
 *       order
 * @note Complexity: @e O(1)
 */
void client_update_user_time(client_td *client, uint32_t time);

/**
 * @brief Apply a client's own themed border color and width to its own
 *        window, honoring @p border_override when set
 *
 * A no-op for a decorated client @p (client->frame != 0) or
 * a fullscreen one, regardless of decoration: a decorated client's own
 * border lives on its frame instead, repainted by
 * @a desktop_repaint_frame_decoration (@c render/desktop.c), not on
 * @p client->window itself; a fullscreen client, decorated or not, is
 * never meant to show any border at all.  For every other (undecorated,
 * non-fullscreen) client, applies @p client->border_override's own
 * color and width when @p is_set, or
 * @p theme->window.active/inactive.border otherwise
 * (@p use_active_style selects which), the same border a plain client
 * already gets restored to on every focus change.
 *
 * @param client            Client to apply the border to
 * @param use_active_style  Ignored when @p border_override.is_set;
 *                          otherwise @c true for
 *                          @p theme->window.active.border, @c false
 *                          for @p .inactive
 *
 * @note Complexity: @e O(1)
 */
void client_border_apply(client_td *client, bool use_active_style);

/**
 * @brief The border width @p client currently themes its own window
 *        or frame with
 *
 * @p border_override's own width when @p is_set (the scratchpad,
 * @c scratchpad.c, is the only client that sets one as for today, and
 * never varies it with focus), or
 * @p theme->window.active/inactive.border.width otherwise (@p is_active
 * selects which); the same width @a client_border_apply applies for the
 * exact same client and focus state.  Meant for any caller that has to
 * reserve room for a border ahead of actually drawing on, e.g., sizing
 * a client to fill an area without its own border ever spilling past
 * that area's own edge
 *
 * @param client       Client to query
 * @param is_active    Ignored when @p border_override.is_set; otherwise
 *                      @c true for @p theme->window.active.border.width,
 *                      @c false for @p .inactive
 * @param ignore_frame Skip the @c 0 short-circuit this function
 *                      otherwise always takes for an already-framed
 *                      client (see this function's own body); needed
 *                      by a caller computing the width to (re)establish
 *                      @p client->layout.frame_extents with in the
 *                      first place, e.g. @a ccmd_client_unfullscreen,
 *                      for which @p client->frame already being
 *                      non-zero does not yet mean the frame already
 *                      accounts for it the way it does for every other
 *                      caller
 *
 * @return @p client's own current border width; @c 0 if @p client is
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

    /* A decorated client's own frame is always created with a native
     * X11 'border_width' of 0 ('ci_create_decorations' in
     * 'client/geom.c'): the themed margin around a decorated client's
     * own content is drawn as background color inset within the frame's
     * own declared width/height ('layout.frame_extents'), already fully
     * accounted for there, not as an X11 border layered on top of it
     * the way 'client_border_apply' (above) draws one directly on an
     * undecorated client's own window.  A caller reserving room for
     * a client's own border has nothing to reserve here, so this
     * returns 0 for a decorated client ('frame != 0') even though
     * 'window.active/inactive.border.width' below is not itself 0,
     * unless @p ignore_frame says this specific caller's own frame
     * does not actually reflect that yet. */
    if (client == NULL || client->config == NULL ||
            (!ignore_frame && client->frame != 0)) {
        return 0u;
    }

    /* 'border_override' (the scratchpad's own case now) never varies
     * with focus (its own width is set once and never revisited as seen
     * in 'scratchpad_notice_client_created', in 'scratchpad.c') so
     * 'is_active' only ever matters for the theme's own fallback below,
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
     * same floor 'client_border_apply' (above) already applies when it
     * actually draws the border this reserves room for */
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
 * titlebar so that they stay correctly aligned inside the frame.  The
 * function is a no-op when the client is not decorated or has no frame.
 *
 * @param client Pointer to the decorated client to synchronize
 *
 * @note Complexity: @e O(1)
 */
void client_decoration_layout_sync(client_td *client);

/**
 * @brief Update a decorated client's border width and titlebar height
 *        to match the current theme and focus state, resizing the frame
 *        around its content so the content's own size never changes
 *
 * A theme's @p window.active.border.width and
 * @p window.inactive.border.width need not be equal; when they differ,
 * this grows or shrinks the frame's outer edge by the difference on
 * every side.
 *
 * @p window.titlebar.height can also have changed (e.g.,
 * a configuration reload picked up an edited theme file), in which case
 * only the top edge grows or shrinks by that additional amount.  Either
 * way the client's own content window never moves or resizes (only how
 * much frame surrounds it changes) and the client is marked for
 * a redraw so the next render pass applies it and repaints the border,
 * titlebar, and its buttons at the new size.
 *
 * A fast no-op when neither value actually changed (the common case for
 * a plain focus change with the built-in default theme, whose active
 * and inactive border widths are equal), when @p client has no theme,
 * when @p client is not decorated, or when @p client is currently
 * fullscreen: a fullscreen client's frame extents are deliberately
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
 * button).  The title span starts immediately after the left buttons
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
 * @brief Clamp a width/height pair into a client's own aspect-ratio
 *        bounds, adjusting height only
 *
 * Applies the @c PAspect portion of @c WM_NORMAL_HINTS on its own
 * (ICCCM §4.1.2.3), separately from @a client_size_constrain's own
 * minimum/maximum/increment handling, so a caller that already
 * produced a fully snapped size for one axis (see
 * @c input/kbd/interact.c's own @c ik_handle_resize) can still apply
 * just this one constraint without @a client_size_constrain's other
 * rules snapping the values a second time.
 *
 * @param client Pointer to the client owning the size hints
 * @param width  Width the ratio is measured against; never adjusted
 * @param height In/out height, adjusted to fit @p width's own ratio
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
 * X server (which carries frame-relative coordinates) would have
 * a incorrect understanding of where it sits on screen.
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
void client_send_synthetic_configure_notify(xcb_connection_t *connection,
        const client_td *client);

/**
 * @brief Initialize a new client, adopting an existing X window
 *
 * Wraps an existing X window in a client structure without creating
 * a new window.  Reads the @c WM_NAME and @c WM_CLASS hints, queries
 * the current window geometry, and subscribes to property and structure
 * events on the window.
 *
 * @param connection  Pointer to the XCB connection
 * @param ewmh        Pointer to EWMH connection
 * @param window      ID of the existing X window to adopt
 * @param config      Shared base/theme/a11y configuration for the
 *                    surface this client is being adopted onto
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
 *        @c _NET_WM_VISIBLE_ICON_NAME) in sync with whether @p
 *        rendered actually differs from @p full_name right now
 *
 * Called every time something re-renders a name that may have needed
 * truncating to fit (a titlebar too narrow for the full title, an
 * icon caption under the same constraint): a no-op, without any XCB
 * round trip, whenever @p rendered already matches what @p cached
 * currently holds, which is the ordinary case on every repaint after
 * the first where truncation itself has not changed.  When @p
 * rendered equals @p full_name (no truncation needed), the property
 * is deleted rather than set to a redundant copy of the underlying
 * name, per this project's own reading of EWMH §5.4/5.5: nothing
 * behaves incorrectly if a client only checks for the property's own
 * presence rather than comparing its value.
 *
 * @param client    Client the property belongs to
 * @param cached    @c client_td's own cached buffer for this name
 *                   (@c info.visible_name or
 *                   @c icon_info.visible_icon_name), at least
 *                   @c CONFIG_MAX_LENGTH_NAME bytes
 * @param full_name The client's own full, untruncated name
 * @param rendered  What was actually just rendered, truncated or not
 * @param set_fn    @c xcb_ewmh_set_wm_visible_name_checked or
 *                   @c xcb_ewmh_set_wm_visible_icon_name_checked,
 *                   whichever matches @p atom
 * @param atom      @c client->ewmh->_NET_WM_VISIBLE_NAME or
 *                   @c client->ewmh->_NET_WM_VISIBLE_ICON_NAME,
 *                   whichever matches @p set_fn
 *
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
 * Re-reads @c WM_NORMAL_HINTS from the X server and updates the
 * stored size-hint fields (@c hints_icccm.size) in @p client.  Should
 * be called both
 * at manage time and whenever a @c PROPERTY_NOTIFY event for
 * @c WM_NORMAL_HINTS is received (because applications such as gVim
 * update their increment grid and base size after initial startup).
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
 * @brief Macro that evaluates to the client focusable flag
 *
 * @c CLIENT_FLAG_FOCUSABLE is cleared for a window whose own
 * @c _NET_WM_WINDOW_TYPE marks it as a kind that should never take
 * real keyboard focus (a dock or a notification; see @c client.c).
 * This is a distinct concept from @a client_accepts_input_focus:
 * this one is about the window's own @e type, that one is about its
 * ICCCM input model.  A plain @c CLIENT_TYPE_NORMAL window is always
 * focusable by this macro's own measure, regardless of what its
 * @c WM_HINTS may say about whether it actually accepts input.
 *
 * @note Complexity: @e O(1)
 */
#define client_is_focusable(w) \
    ((w)->properties.flags & CLIENT_FLAG_FOCUSABLE)

/**
 * @brief Macro that evaluates to whether a client can receive real
 *        keyboard focus under its own declared ICCCM input model
 *
 * ICCCM §4.1.7 defines three ways a client may end up receiving
 * keyboard focus: a @e Passive client (@c WM_HINTS input field
 * @c true, no @c WM_TAKE_FOCUS) takes it via @c SetInputFocus alone;
 * a @e Locally @e Active or @e Globally @e Active client (registered
 * @c WM_TAKE_FOCUS) additionally or exclusively takes it via that
 * protocol message; a @e No @e Input client (input @c false, no
 * @c WM_TAKE_FOCUS) never takes real keyboard focus at all, by its
 * own explicit declaration.  This macro evaluates true for the
 * first two and false for the third, mirroring Openbox's own
 * @c can_focus @c || @c focus_notify check in @c focus_valid_target
 * (@c focus.c).
 *
 * This is a distinct concept from @a client_is_focusable: that one
 * is about the window's own @e type (a dock or notification never
 * wants focus, whatever its input model says); this one is about
 * the ICCCM input model any window, dock or not, may declare.  A
 * caller that skips this check before routing a client into
 * @a focus_apply (@c policy/focus.c) risks unfocusing whatever
 * already holds real keyboard focus in favor of a client that can
 * never actually receive it, leaving keyboard input directed
 * nowhere until the person clicks something else by hand.
 *
 * @note Complexity: @e O(1)
 */
#define client_accepts_input_focus(w) \
    ((w)->hints_icccm.hints.has_input_hint || \
     (w)->hints_icccm.protocols.has_take_focus)

/**
 * @brief Macro that evaluates to the client shade flag
 *
 * @note Complexity: @e O(1)
 */
#define client_is_shaded(w) \
    ((w)->properties.flags & CLIENT_FLAG_SHADED)

/**
 * @brief Macro that evaluates to whether this client currently holds
 *        real X11 input focus
 *
 * @see @c CLIENT_FLAG_FOCUSED's own doc comment above for why this is
 *      its own tracked flag rather than derived from @c desktop->
 *      client_active_id on demand
 *
 * @note Complexity: @e O(1)
 */
#define client_is_focused(w) \
    ((w)->properties.flags & CLIENT_FLAG_FOCUSED)

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
 * Normalized to @c 0 or @c 1, unlike leaving the raw flag bit's own
 * numeric value (@c CLIENT_FLAG_DECORATED, not necessarily @c 1)
 * exposed: a caller comparing this against a proper @c bool with @c
 * !=/@c == (as @c ccmd_client_toggle_decorate's own callers in @c
 * rules/apply.c and @c handler/focus.c both do) would otherwise
 * mismatch and toggle decoration off by mistake, every single time,
 * whenever the client already happened to be decorated (the common
 * case for an ordinary client) and the caller wanted it to stay that
 * way.
 *
 * @note Complexity: @e O(1)
 */
#define client_is_decorated(w) \
    (((w)->properties.flags & CLIENT_FLAG_DECORATED) != 0u)

/**
 * @brief Macro that evaluates to the client urgency flag
 *
 * @note Complexity: @e O(1)
 */
#define client_is_urgent(w) \
    ((w)->properties.flags & CLIENT_FLAG_URGENT)

/**
 * @brief Macro that compares two @c user_time values safely across
 *        the 32-bit wraparound X11 timestamps undergo roughly every
 *        49.7 days of continuous X server uptime
 *
 * Nothing breaks server-side at that wraparound; the millisecond
 * counter, defined by the X11 protocol itself as a plain @c CARD32,
 * just wraps back to 0 and keeps counting, ordinary unsigned
 * overflow.  But a naive @c a @c > @c b comparison breaks exactly
 * once per wraparound: right after it, every fresh timestamp is
 * numerically small again, so it would wrongly look older than any
 * timestamp from just before the wraparound.  Subtracting first and
 * reinterpreting the result as signed sidesteps this entirely, the
 * same idiom X11 itself already relies on for its own timestamps,
 * as long as the two values being compared are never more than
 * roughly half the 32-bit range (about 24.8 days) apart, which two
 * genuine user-interaction timestamps meaningfully compared against
 * each other never are in practice.
 *
 * @param a First timestamp
 * @param b Second timestamp
 *
 * @return Whether @p a happened after @p b
 * @retval  true @p a is the more recent timestamp
 * @retval false @p a is not more recent than @p b
 *
 * @note Complexity: @e O(1)
 */
#define client_user_time_is_newer(a, b) \
    (((int32_t) ((a) - (b))) > 0)

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
 * @brief Macro that sets the locked flag of a client
 *
 * @note Complexity: @e O(1)
 */
#define client_lock(w) \
    safeflg_set(&((w)->properties.flags), \
            CLIENT_FLAG_LOCKED, (1 << CLIENT_FLAG_MAX))

/**
 * @brief Macro that clears the locked flag of a client
 *
 * @note Complexity: @e O(1)
 */
#define client_unlock(w) \
    safeflg_unset(&((w)->properties.flags), \
            CLIENT_FLAG_LOCKED, (1 << CLIENT_FLAG_MAX))

/**
 * @brief Macro that marks a client as currently holding real X11
 *        input focus
 *
 * @note Complexity: @e O(1)
 */
#define client_focus_mark(w) \
    safeflg_set(&((w)->properties.flags), \
            CLIENT_FLAG_FOCUSED, (1 << CLIENT_FLAG_MAX))

/**
 * @brief Macro that clears a client's own currently-focused flag
 *
 * @note Complexity: @e O(1)
 */
#define client_unfocus_mark(w) \
    safeflg_unset(&((w)->properties.flags), \
            CLIENT_FLAG_FOCUSED, (1 << CLIENT_FLAG_MAX))

/**
 * @brief Macro that evaluates to the locked flag of a client
 *
 * @note Complexity: @e O(1)
 */
#define client_is_locked(w) \
    ((w)->properties.flags & CLIENT_FLAG_LOCKED)

/**
 * @brief Macro that sets a client's own no-focus-fallback flag
 *
 * @note Complexity: @e O(1)
 */
#define client_set_no_focus_fallback(w) \
    safeflg_set(&((w)->properties.flags), \
            CLIENT_FLAG_NO_FOCUS_FALLBACK, (1 << CLIENT_FLAG_MAX))

/**
 * @brief Macro that evaluates to a client's own no-focus-fallback flag
 *
 * @note Complexity: @e O(1)
 */
#define client_has_no_focus_fallback(w) \
    ((w)->properties.flags & CLIENT_FLAG_NO_FOCUS_FALLBACK)

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
