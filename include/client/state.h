/**
 * @file client/state.h
 *
 * @brief Client state, type, layer and gravity enumerations
 *
 * The value sets behind every field of @c client_properties_s, plus the
 * geometry helper that translates a gravity into the frame displacement
 * it implies.
 *
 * @ingroup client
 */
/*
 * Copyright (c) 2026, J. A. Corbal.
 * All rights reserved.
 *
 * This file is licensed under the 'ISC License'.
 * Read the 'LICENSE' file in the root of this repository for details.
 */

#ifndef CLIENT_STATE_H
#define CLIENT_STATE_H


/* System includes */
#include <stdint.h>

/* Utils includes */
#include <utils/safe/safeflg.h>


/**
 * @brief States a client can be in, as independent bits
 *
 * A bitmask rather than an enumeration of alternatives, because EWMH
 * treats @c _NET_WM_STATE_MAXIMIZED_HORZ,
 * @c _NET_WM_STATE_MAXIMIZED_VERT and @c _NET_WM_STATE_FULLSCREEN as
 * independent of one another.
 *
 * A client may hold any combination of them, and asks for each to be
 * added, removed or toggled on its own.  There is no
 * @c _NET_WM_STATE_MAXIMIZED atom at all in the specification; a window
 * maximized in both directions simply holds both bits, which is what
 * @a client_is_maximized tests for.
 *
 * What is drawn follows a precedence rather than the bits being
 * exclusive: full screen covers a maximized window, which covers
 * a normal one.  Holding the bits separately is what lets a client that
 * was maximized before going full screen still be maximized on leaving
 * it, having never asked for that to be forgotten.
 *
 * @note Iconified is a bit here too, but of a different kind: EWMH
 *       spells that @c _NET_WM_STATE_HIDDEN, and it says nothing about
 *       whether the window is also maximized underneath, which it may
 *       well be
 */
enum client_state_e {
    CLIENT_STATE_NORMAL = 0u,           /**< No state bit at all */
    CLIENT_STATE_ICONIFIED = 1u << 0,   /**< Iconified */
    CLIENT_STATE_MAXIMIZED_HORZ = 1u << 1,
                                        /**< Maximized horizontally */
    CLIENT_STATE_MAXIMIZED_VERT = 1u << 2,
                                        /**< Maximized vertically */
    CLIENT_STATE_FULLSCREEN = 1u << 3,  /**< Full screen */
};

/** Both maximize bits at once, which is what "maximized" means */
#define CLIENT_STATE_MAXIMIZED \
    ((uint16_t) (CLIENT_STATE_MAXIMIZED_HORZ | \
                 CLIENT_STATE_MAXIMIZED_VERT))


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
    /** Additional functions, such as a control panel */
    CLIENT_TYPE_UTILITY,
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

    /**
     * @brief Client is visible on every desktop, instead of only the
     *        one it actually belongs to
     *
     * Not to be confused with @c CLIENT_FLAG_STICKY below, a wholly
     * unrelated, orthogonal concept despite the similar-sounding name:
     * this one is about @e which @e desktop shows the client (all of
     * them at once), while @c CLIENT_FLAG_STICKY is about
     * @e where-on-screen the client sits once one is showing it.
     *
     * A client may hold either bit, both, or neither; none of the four
     * combinations implies or excludes any of the others.  The
     * historical EWMH atom for this very flag is even named
     * @c _NET_WM_STATE_STICKY (see @c handler/ewmh.c and
     * @c cmds/client/ewmh.c), a naming collision from a spec written
     * before this window manager had any notion of viewport panning to
     * need a second, genuinely @e sticky flag of its own; that atom
     * name is fixed by the protocol and cannot be changed, which is
     * exactly why this comment exists.
     *
     * @see @a client_is_pinned below
     */
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
     * @brief Every aspect of this client's presentation and extent is
     *        entirely policy-controlled, never subject to any mutation
     *        initiated by a user or script
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
     * into the owning feature's module directly for that, the same as
     * before; this flag only ever centralizes the "refuse" half shared
     * by every such feature, not anything specific to one of them.
     *
     * @see @a client_is_locked's callers
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
     * this, or why, only that this client's visibility is managed
     * by something else entirely (its toggle, in the scratchpad's
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
     * cleared by @a ccmd_client_unfocus, right alongside the real focus
     * grant/revocation each one performs, rather than derived on demand
     * from @c desktop->client_active_id: unlike every other flag in
     * this @c enum, "is this the desktop's active client" needs an
     * external lookup (which desktop, and whether that desktop's
     * bookkeeping has actually been updated yet by whichever caller is
     * in the middle of granting focus right now) that the other,
     * genuinely self-contained flags never do, and that external
     * dependency is exactly the kind of fragility
     * @a ccmd_client_sync_states (@c cmds/client/ewmh.c) is built to
     * avoid.
     *
     * Every @c _NET_WM_STATE atom it publishes reads directly off
     * @p client's fields, this one included, with nothing else to
     * go stale or disagree with it.
     *
     * @see @a client_is_focused below
     */
    CLIENT_FLAG_FOCUSED = 1 << 14,

    /**
     * @brief Client stays fixed at its current position on screen
     *        whenever the desktop's viewport pans, regardless of
     *        which desktop it belongs to
     *
     * Not to be confused with @c CLIENT_FLAG_PIN above, a wholly
     * unrelated, orthogonal concept despite the similar-sounding name:
     * pinning is about @e which @e desktop shows the client (all of
     * them at once, rather than only its own), while this flag is about
     * @e where @e on @e screen the client sits once a desktop showing
     * it pans its viewport.  A client may hold either bit, both, or
     * neither; none of the four combinations implies or excludes any of
     * the others.  Unlike @c CLIENT_FLAG_PIN, this one has no EWMH
     * counterpart at all: no @c _NET_WM_STATE atom describes staying
     * fixed across a viewport pan, because the specification has no
     * notion of a viewport to pan in the first place, so this flag is
     * never published to, or read from, any client property.
     *
     * Toggled by @c ACTION_CLIENT_TOGGLE_STICKY (see
     * @a ccmd_client_toggle_stick in @c cmds/client/flags.c).  Read by
     * @a s_viewport_translate_visit (@c cmds/surface.c), which skips
     * translating any client holding this flag whenever the desktop's
     * viewport pans, whether that pan was triggered by a keyboard
     * shortcut, the pointer resting against a screen edge, or a window
     * or icon being dragged against one
     *
     * @see @a client_is_sticky below
     */
    CLIENT_FLAG_STICKY = 1 << 15,

    CLIENT_FLAG_MAX = 16,
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
 *       @c ConfigureRequest events
 *       (ICCCM Version 2.0, §4.1.2.3 and §4.1.5)"
 */
enum client_gravity_e {         /* Reference point fixed on resize: */
    CLIENT_GRAVITY_NORTH_WEST = 1,  /**<  1: top-left corner of frame */
    CLIENT_GRAVITY_NORTH      = 2,  /**<  2: center of top edge */
    CLIENT_GRAVITY_NORTH_EAST = 3,  /**<  3: frame top-right */
    CLIENT_GRAVITY_EAST       = 4,  /**<  4: center of right edge */
    CLIENT_GRAVITY_SOUTH_EAST = 5,  /**<  5: frame bottom-right */
    CLIENT_GRAVITY_SOUTH      = 6,  /**<  6: center of bottom edge */
    CLIENT_GRAVITY_SOUTH_WEST = 7,  /**<  7: frame bottom-left */
    CLIENT_GRAVITY_WEST       = 8,  /**<  8: center of left edge */
    CLIENT_GRAVITY_CENTER     = 9,  /**<  9: center of frame */
    /** 10: top-left corner of the client area */
    CLIENT_GRAVITY_STATIC     = 10,
};


/**
 * @brief Adjust a frame position to keep a gravity anchor fixed across
 *        a size change
 *
 * Computes the displacement that preserves the anchor point defined by
 * @p gravity after the frame changes from (@p old_w x @p old_h) to
 * (@p new_w x @p new_h) and adds it to @p *out_x and @p *out_y.
 *
 * @param out_x   Frame x to adjust in place
 * @param out_y   Frame y to adjust in place
 * @param old_w   Frame width before the size change
 * @param old_h   Frame height before the size change
 * @param new_w   Frame width after the size change
 * @param new_h   Frame height after the size change
 * @param gravity Client @a win_gravity value
 *
 * @note A no-op for @c CLIENT_GRAVITY_NORTH_WEST and
 *       @c CLIENT_GRAVITY_STATIC
 * @note Complexity: @e O(1)
 */
void client_gravity_adjust_pos(int32_t *restrict out_x,
        int32_t *restrict out_y,
        uint32_t old_w, uint32_t old_h,
        uint32_t new_w, uint32_t new_h,
        uint16_t gravity);


#endif  /* ! CLIENT_STATE_H */
