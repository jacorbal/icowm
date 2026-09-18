/**
 * @file config/base.h
 *
 * @brief Screen and desktop topology, and the policies that apply to
 *        them
 *
 * Everything @c config.json's top-level objects describe: how many
 * screens and desktops exist and what each is called, plus the focus,
 * placement, snapping, scratchpad and systray policies that apply
 * across them.
 *
 * @ingroup config
 */
/*
 * Copyright (c) 2026, J. A. Corbal.
 * All rights reserved.
 *
 * This file is licensed under the 'ISC License'.
 * Read the 'LICENSE' file in the root of this repository for details.
 */

#ifndef CONFIG_BASE_H
#define CONFIG_BASE_H


/* System includes */
#include <stdbool.h>
#include <stdint.h>

/* Default initial values */
#include <defs/config.h>


/* Theme-related configuration structure */
/**
 * @brief Where a menu appears when it is opened by a means that has no
 *        inherent screen position of its own (e.g., a keyboard
 *        shortcut); shared by every menu type below
 *
 * The four corner values pin the menu to that corner of the current
 * desktop's work area instead, unaffected by wherever the pointer
 * happens to be; @a s_menu_position_resolve (@c input/kbd/execute.c)
 * resolves each to the exact point that leaves the menu flush against
 * it once @a ctxmenu_show's edge clamping runs.
 */
enum config_menu_position_e {
    CONFIG_MENU_POSITION_CENTER = 0,   /**< Always screen-centered */
    CONFIG_MENU_POSITION_UNDER_MOUSE,  /**< Under the current mouse
                                            pointer position */
    CONFIG_MENU_POSITION_TOP_LEFT,     /**< Pinned to the work area's
                                            top-left corner */
    CONFIG_MENU_POSITION_TOP_RIGHT,    /**< Pinned to the work area's
                                            top-right corner */
    CONFIG_MENU_POSITION_BOTTOM_LEFT,  /**< Pinned to the work area's
                                            bottom-left corner */
    CONFIG_MENU_POSITION_BOTTOM_RIGHT  /**< Pinned to the work area's
                                            bottom-right corner */
};


/**
 * @brief Fill order for a configured @c topology.screens.desktops
 *        layout: whether desktop indices advance across one whole
 *        row before moving to the next (@c horizontal), or down one
 *        whole column before moving to the next (@c vertical)
 */
enum config_desktop_orientation_e {
    CONFIG_DESKTOP_ORIENTATION_HORIZONTAL = 0,
    CONFIG_DESKTOP_ORIENTATION_VERTICAL
};

/**
 * @brief Which corner of a configured @c topology.screens.desktops
 *        layout desktop index @c 0 starts at, and so which direction
 *        indices advance in from there
 */
enum config_desktop_corner_e {
    CONFIG_DESKTOP_CORNER_TOP_LEFT = 0,
    CONFIG_DESKTOP_CORNER_TOP_RIGHT,
    CONFIG_DESKTOP_CORNER_BOTTOM_LEFT,
    CONFIG_DESKTOP_CORNER_BOTTOM_RIGHT
};

/**
 * @brief One screen's desktop-grid layout
 *
 * Purely an interpretation over the same flat, zero-based desktop
 * list @c desktops[] itself already is: north/south/east/west
 * navigation (@a stage_desktop_north and its three siblings,
 * stage/desktops.c) reads this to translate a desktop's flat
 * index to and from a row/column position, but nothing about
 * @c desktops[] itself, or a desktop's settings within it, changes
 * depending on whether one is configured at all.
 *
 * Always populated with a valid value, whether
 * @c topology.screens.desktops[].layout was present in @c config.json
 * or not: @c rows @c 1, @c columns the screen's @c desktop_count, @c
 * orientation horizontal, @c corner top-left describes the exact same
 * reading order the desktop list itself already had before this
 * existed, so a config that never mentions layout at all behaves
 * identically to before.  The same fallback also applies whenever a
 * given @c layout fails validation (see @c ci_config_screens_load's
 * comment, @c config/base/desktops.c, for what "fails validation" means
 * here).
 */
struct config_desktop_layout_s {
    enum config_desktop_orientation_e orientation;
    enum config_desktop_corner_e corner;
    uint32_t rows;
    uint32_t columns;
};


/**
 * @brief One screen's pannable virtual-desktop size, in whole screens
 *
 * Not to be confused with @c config_desktop_layout_s above: that one
 * arranges separate desktop entities in a grid for north/south/east/
 * west switching between them, while this one instead sizes a single
 * desktop's pannable area, wider and/or taller than the physical screen
 * by this many whole screens, that @c CLIENT_FLAG_STICKY
 * (@c client/state.h) is defined against.
 *
 * Always populated with a valid value, whether
 * @c topology.screens.desktops[].viewport was present in
 * @c config.json or not: @c columns @c 1, @c rows @c 1 describes a
 * pannable area exactly the size of the physical screen, i.e., panning
 * disabled, the exact same behavior as before this existed.  The same
 * fallback also applies whenever a given @c viewport fails validation.
 */
struct config_viewport_s {
    uint32_t columns;
    uint32_t rows;
};


/**
 * @brief Base settings configuration structure
 */
struct config_base_s {
    /* Desktops, how many and which one is the default */
    uint32_t screen_count;              /**< Number of screens */

    /* Icon placement policy settings */
    struct {
        /** Show geometry overlay on move/resize */
        bool show_geom;
        enum config_icon_placement_e {
            /** Bottom row, the default */
            CONFIG_ICON_PLACEMENT_BOTTOM = 0,
            /** Top row */
            CONFIG_ICON_PLACEMENT_TOP,
            /** Left column */
            CONFIG_ICON_PLACEMENT_LEFT,
            /** Right column */
            CONFIG_ICON_PLACEMENT_RIGHT,
            /** First free slot, falling back to the bottom row when
             *  none is available */
            CONFIG_ICON_PLACEMENT_SMART,
            /** Over the window's top-left corner, or the nearest free
             *  spot to it when something already sits there */
            CONFIG_ICON_PLACEMENT_IN_PLACE
        } placement_policy;
    } icons;

    /**
     * @brief Shutdown behavior: the normal, coordinated quit action,
     *        and the hardcoded emergency exit shortcut
     *
     * @c enable_emergency_shortcut and @c timeout_seconds both live
     * here together, rather than the emergency shortcut sitting apart
     * at the top level, since both are about how the window manager
     * itself shuts down, just by two entirely different paths that
     * never interact with each other.  Each field's comment below
     * explains exactly how they differ.
     */
    struct {
        /**
         * @brief Allow the hardcoded @c Ctrl+Mod1+Backspace emergency
         *        exit shortcut
         *
         * Off (@c false) by default.  When enabled, this shortcut
         * terminates the window manager immediately: no confirmation
         * dialog, no menu, none of the coordinated wait
         * @p timeout_seconds below governs for the normal quit action,
         * and not even the exit session hooks that a normal
         * quit or @c SIGTERM otherwise runs.  This is deliberate, not
         * an oversight: this shortcut exists specifically as a last
         * resort for situations where the window manager itself may be
         * unresponsive or in some broken state, so it is kept to the
         * smallest, most direct possible action, a signal to its
         * process, with nothing else in between that could itself get
         * stuck, hang, or otherwise fail to complete, e.g., a dialog
         * that depends on the very rendering or event loop that might
         * be the reason this shortcut is being reached for in the first
         * place.
         *
         * Not configurable via @c bindings.json like a normal
         * keybinding, for the same reason: a fixed, hardcoded
         * combination that never changes and never depends on
         * @c bindings.json having parsed correctly is itself part of
         * what makes it dependable as a last resort.  While enabled,
         * that exact key combination cannot be reused by any binding in
         * @c bindings.json, whether that would happen intentionally or
         * by accident.
         *
         * @see @c input/kbd/event.c, where this shortcut is detected
         *      ahead of every other keyboard handling, including any
         *      open dialog or menu, so it keeps working even while one
         *      of those has the keyboard grabbed
         */
        bool enable_emergency_shortcut;

        /**
         * @brief Coordinated shutdown behavior for the normal quit
         *        action
         *
         * When quit is confirmed, every managed client is first asked
         * to close (ICCCM @c WM_DELETE_WINDOW where supported, so an
         * application with unsaved changes gets the same chance to warn
         * the user it already gets when its window is closed
         * individually), rather than the window manager simply exiting
         * out from under them.  @p timeout_seconds bounds how long this
         * wait lasts before whichever clients are still open get forced
         * closed regardless and the window manager exits anyway.
         * Seconds to wait; @c 0 skips the wait entirely and
         * force-closes every remaining client right away.
         *
         * @note Never consulted by @p enable_emergency_shortcut above,
         *       which bypasses this, the coordinated wait it governs,
         *       and even the exit session hooks, entirely
         *
         * @see @a wm_request_graceful_stop (@c wm.h), @c wm/shutdown.c
         * @see @a ccmd_client_kill in @c cmds/client/focus.h
         */
        uint32_t timeout_seconds;
    } shutdown;

    /**
     * @brief Whether launching a program begins a startup-notification
     *        sequence at all, and that sequence's timeout
     *
     * @see @a cctl_sn_begin, whose only call site checks
     *      @p is_enabled first, and @a cctl_sn_set_timeout_seconds
     *      and @c SN_TIMEOUT_SECONDS in @c sn.h for what
     *      @p timeout_seconds controls and what it defaults to
     */
    struct {
        bool is_enabled;
        uint32_t timeout_seconds;
    } startup_notification;

    /* Context-menu placement, per menu type */
    struct {
        struct {
            /** Desktop context menu, from @c menu.json */
            enum config_menu_position_e position;
        } root;

        struct {
            /** Window list menu, holding every window on every
             *  desktop */
            enum config_menu_position_e position;
        } windows;
    } menus;

    /* General behavior of environment towards windows */
    struct {
        uint32_t move_step;     /**< Keyboard move step in pixels */
        uint32_t resize_step;   /**< Keyboard resize step in pixels */

        enum config_gravity_e {
            CONFIG_GRAVITY_NORTH_WEST = 1,
            CONFIG_GRAVITY_NORTH = 2,
            CONFIG_GRAVITY_NORTH_EAST = 3,
            CONFIG_GRAVITY_EAST = 4,
            CONFIG_GRAVITY_SOUTH_EAST = 5,
            CONFIG_GRAVITY_SOUTH = 6,
            CONFIG_GRAVITY_SOUTH_WEST = 7,
            CONFIG_GRAVITY_WEST = 8,
            CONFIG_GRAVITY_CENTER = 9,
            CONFIG_GRAVITY_STATIC = 10
        } gravity;

        enum config_focus_policy_e {
            CONFIG_FOCUS_POLICY_CLICK = 0,
            CONFIG_FOCUS_POLICY_SLOPPY
        } focus_policy;

        enum config_placement_policy_e {
            CONFIG_PLACEMENT_POLICY_SMART = 0,
            CONFIG_PLACEMENT_POLICY_CASCADE,
            CONFIG_PLACEMENT_POLICY_CENTERED,
            CONFIG_PLACEMENT_POLICY_UNDER_MOUSE,
            CONFIG_PLACEMENT_POLICY_MANUAL
        } placement_policy;

        /**
         * @brief Which physical monitor a placement decision is
         *        resolved against, on a stage made of more than one
         *        sharing the same combined X screen
         *
         * @c pointer (default) picks whichever monitor the pointer is
         * currently on (not necessarily where on that monitor the
         * pointer actually is; the window can still land far from the
         * cursor within it, depending on the placement policy),
         * @c active picks whichever monitor holds the desktop's
         * currently active client, falling back to @c pointer when
         * there is none, @c primary always picks the one RandR reports
         * as primary, and @c index picks @p monitor_index explicitly (a
         * zero-based index into the stage's monitor list, falling
         * back to monitor 0 if it does not exist, logging a warning,
         * the same as @c systray.monitor.index and @c rules.json's
         * @c apply.monitor).
         *
         * @see @a place_window_apply
         */
        enum config_placement_monitor_e {
            CONFIG_PLACEMENT_MONITOR_POINTER = 0,
            CONFIG_PLACEMENT_MONITOR_ACTIVE,
            CONFIG_PLACEMENT_MONITOR_PRIMARY,
            CONFIG_PLACEMENT_MONITOR_INDEX
        } monitor_policy;

        /**
         * @brief Explicit monitor index @p monitor_policy resolves
         *        against, only meaningful when it is
         *        @c CONFIG_PLACEMENT_MONITOR_INDEX
         */
        uint32_t monitor_index;

        /**
         * @brief Behavior of a window's edges against nearby
         *        screen edges and other windows while being
         *        interactively moved or resized
         */
        struct {
            /**
             * @brief Attraction distance in pixels toward a nearby
             *        edge while dragging; either @c 0 disables that
             *        one specifically
             */
            struct config_edges_snap_s {
                uint32_t window; /**< Toward another window's
                                      edge */
                uint32_t screen; /**< Toward the screen's edge */
            } snap;

            /**
             * @brief How many pixels of deliberate extra drag it
             *        takes for a horizontally or vertically
             *        maximized client's locked axis to actually
             *        start changing while being interactively
             *        resized, matching Openbox's reuse of its
             *        @c config_resist_edge (@c moveresize.c) for the
             *        identical purpose
             *
             * Dragging back under this same threshold before
             * releasing restores the maximized axis, reversibly,
             * for the whole drag; @c 0 removes the axis lock
             * entirely, letting the maximized axis change
             * immediately on the very first pixel of drag.
             */
            uint32_t resistance;
        } edges;

        /** Show the geometry overlay while moving or resizing */
        bool show_geom;
        /** Move and resize the real window live, rather than an
         *  outline stand-in applied only once the drag ends */
        bool solid_drag;

        /**
         * @brief Cluster a newly placed window next to others sharing
         *        its @c WM_CLIENT_LEADER / @c WM_HINTS group (e.g.,
         *        several windows of the same application) instead of
         *        running the placement policy above for it
         *
         * @see @a place_window_apply
         */
        bool group_related;

        /* Focus behavior */
        struct {
            bool focus_new;
            bool raise;

            /**
             * @brief Whether focus left behind by a window that
             *        closes or stops qualifying goes first to another
             *        window of its same application
             *
             * Same application meaning a shared @c WM_CLIENT_LEADER
             * (ICCCM §4.1.2.5).  When @c false (the default), the
             * most recently focused remaining window takes it, whatever
             * application it belongs to.
             *
             * @see @a client_focus_fallback
             */
            bool use_group_fallback;

            /**
             * @brief Milliseconds the pointer must sit still over a
             *        client before it is focused, meaningful only
             *        under @c focus_policy's @c CONFIG_FOCUS_POLICY_
             *        SLOPPY; ignored under @c CLICK, which never
             *        focuses on an @c EnterNotify at all
             *
             * @c 250 (the default) focuses the instant the pointer
             * enters, exactly as before this field existed.  Leaving
             * the client before the delay elapses cancels it, so a
             * pointer only passing through on its way elsewhere never
             * steals focus.
             *
             * @see @a mouse_handle_enter, @a mouse_enter_focus_tick
             */
            uint32_t delay_ms;
        } focus;
    } windows;

    /**
     * @brief What the manager does when a window asks for attention
     *
     * Not to be confused with @c a11y.json's @c urgency section, which
     * holds @c sound-bell and @c blink-interval-ms: those say how the
     * attention request is made perceptible once the accessibility mode
     * is switched on, while this says whether the manager announces one
     * the user cannot see at all.  Ordinary behavior, on by default,
     * rather than something behind @c a11y.json's opt-in.
     *
     * @see @a desktop_clients_recompute_urgent in @c desktop/dclient.h
     */
    struct {
        bool notify_activity;
    } urgency;

    /**
     * @brief The brief popup that names where the view has just
     *        moved to
     *
     * One widget with two triggers, so one section rather than a flag
     * hidden in each of the things that raise it: the theme already
     * calls it @c overlay and sizes and colors it there.  Switching
     * desktops and moving the viewport a whole page are announced
     * separately because panning is much the more frequent of the
     * two, and wanting one without the other is reasonable.
     *
     * @see @a notify_desktop_show in @c menu/notify/desktop.h
     */
    struct {
        bool on_desktop_switch;
        bool on_viewport_move;
    } overlay;

    /* General behavior of environment towards the viewport, a
     * sibling of 'windows' above, 'desktops' (config/desktops.h),
     * and 'topology' (this same struct's 'screens' above) */
    struct {
        uint32_t pan_step;  /**< Keyboard pan step in pixels; mouse
                                 dragging on the desktop background
                                 moves by the exact drag delta
                                 instead, never by this */

        /**
         * @brief Whether panning also moves the desktop icons, rather
         *        than leaving them fixed on the physical screen
         *
         * Off by default: icons sit on the desktop the way a note sits
         * on a monitor's bezel, and most setups want them to stay put
         * while the canvas scrolls under them.
         *
         * @note Meaningless on a screen whose @c viewport is @c 1x1
         *       (no panning configured)
         */
        bool pan_icons;

        /**
         * @brief Whether dragging a window past a screen edge pans the
         *        current desktop's viewport toward that edge, carrying
         *        the dragged window along
         *
         * Held there past @c WM_VIEWPORT_PAN_DELAY_MS
         * (@c defs/desktop.h), pans one screen toward the held edge,
         * then repeats every @c WM_VIEWPORT_PAN_REPEAT_MS for as long
         * as the drag stays held there, exactly like
         * @c pan_on_edge_hover below except triggered by a drag rather
         * than a plain hover.  Takes priority over
         * @c desktops.warp_on_edge_drag for as long as the viewport
         * still has room to pan that way; once it does not (or the
         * screen's @c viewport is @c 1x1), a held edge falls through
         * to that one instead.
         *
         * @note Meaningless on a screen whose @c viewport is @c 1x1 (no
         *       panning configured)
         */
        bool pan_on_edge_drag;

        /**
         * @brief Whether resting the pointer against a screen edge,
         *        with no drag in progress, pans the current desktop's
         *        viewport toward that edge
         *
         * Held there past @c WM_VIEWPORT_PAN_DELAY_MS
         * (@c defs/desktop.h), pans one screen toward the held edge,
         * then repeats every @c WM_VIEWPORT_PAN_REPEAT_MS for as long
         * as the pointer stays held there.
         *
         * @note Meaningless on a screen whose @c viewport is @c 1x1 (no
         *       panning configured), or while a window or icon is being
         *       dragged: an edge held during a drag is
         *       @c pan_on_edge_drag and @c desktops.warp_on_edge_drag's
         *       to answer instead, never this one's
         */
        bool pan_on_edge_hover;

        /**
         * @brief Dot mesh painted on the root window so that panning
         *        the viewport reads as a movement
         *
         * A viewport pan translates every non-sticky client at once,
         * which on an empty desktop can look like nothing happened.
         * The mesh gives the eye a fixed set of marks that travel with
         * the clients, so the movement is visible even with no window
         * near the pointer.  It is an aid to perception only, and
         * deliberately carries no information about how far the
         * viewport moved or where in it the current page sits.
         *
         * @p is_enabled alone does not decide whether anything is
         * painted: a mesh is only ever drawn on a stage whose
         * viewport can actually pan, and never while an external tool
         * owns the root window's pixels (@a viewport_mesh_is_visible,
         * @c render/viewport/mesh.c).
         *
         * @p spacing is the distance between neighboring dots along
         * each axis, in pixels, and doubles as the tile size.
         * @p thickness is the side of one square dot, also in pixels.
         * @p tone_shift is how far the dot color is pushed away from
         * the desktop background color, as a percentage, away from
         * black on a light background and away from white on a dark
         * one.
         *
         * @see @a viewport_mesh_color_from_background
         */
        struct config_viewport_mesh_s {
            uint32_t spacing_horizontal;
            uint32_t spacing_vertical;
            uint32_t thickness;
            uint32_t tone_shift;
            bool is_enabled;
        } mesh;
    } viewport;

    /**
     * @brief Configuration for the scratchpad: a single dedicated
     *        client, launched on demand from @p command, toggled
     *        visible/hidden by its keybind or IPC command instead of
     *        iconified/restored
     *
     * @p is_enabled just gates whether the toggle action does anything
     * at all; @p width and @p height are always applied regardless of
     * whatever geometry the client itself requests, against whichever
      edge @p edge names, centered along that edge's other axis.
     *
     * @see @c scratchpad.c
     */
    struct {
        enum config_scratchpad_edge_e {
            CONFIG_SCRATCHPAD_EDGE_TOP = 0,
            CONFIG_SCRATCHPAD_EDGE_BOTTOM,
            CONFIG_SCRATCHPAD_EDGE_LEFT,
            CONFIG_SCRATCHPAD_EDGE_RIGHT
        } edge;              /**< Screen edge it slides out from */

        /**
         * @brief Either dimension, given as a fixed pixel count or as
         *        the string "max", meaning "however much of that axis
         *        is actually available", so a user is never forced to
         *        hard-code a resolution that may change later
         *
         * @p pixels is only meaningful when @p mode is
         * @c CONFIG_SCRATCHPAD_SIZE_FIXED; under
         * @c CONFIG_SCRATCHPAD_SIZE_MAX the scratchpad's placement code
         * computes it fresh every time instead, against
         * @p desktop->workarea (or the full monitor extent, when
         * @p ignore_margins is @c true), the same as a numeric value
         * would be measured against.
         */
        struct config_scratchpad_size_s {
            enum config_scratchpad_size_e {
                CONFIG_SCRATCHPAD_SIZE_FIXED = 0,
                CONFIG_SCRATCHPAD_SIZE_MAX
            } mode;
            uint32_t pixels;
        } width;                                /**< Always-applied width */
        struct config_scratchpad_size_s height; /**< Always-applied height */

        bool is_enabled;    /**< Enable the scratchpad toggle action */

        /**
         * @brief Whether the scratchpad's placement skips
         *       @p desktops.margins and the systray's reserved
         *       space
         *
         * @c false (the default) places it the same way an ordinary
         * client already respects that reserved space; @c true lets it
         * use the full edge regardless, e.g., a top-edge scratchpad
         * sliding out from underneath an external panel that already
         * reserves that same space rather than starting just below it.
         */
        bool ignore_margins;

        char command[CONFIG_MAX_LENGTH_COMMAND]; /**< Launched the
                                                      first time the
                                                      toggle runs with
                                                      no scratchpad
                                                      client yet */
    } scratchpad;

    /**
     * @brief Configuration for the built-in systray dock
     *
     * Implements the @c _NET_SYSTEM_TRAY_Sn manager selection and the
     * XEMBED protocol needed to actually host tray icons.
     * The boolean @p is_enabled just gates whether that runs.
     *
     * @see @c systray.c
     */
    struct {
        enum config_systray_position_e {
            CONFIG_SYSTRAY_POSITION_TOP_LEFT = 0,
            CONFIG_SYSTRAY_POSITION_TOP_RIGHT,
            CONFIG_SYSTRAY_POSITION_BOTTOM_LEFT,
            CONFIG_SYSTRAY_POSITION_BOTTOM_RIGHT
        } position;         /**< Corner of the screen to dock it in */

        /**
         * @brief Which physical monitor the tray dock is anchored to,
         *        on a stage made of more than one sharing the same
         *        combined X screen
         *
         * @p anchor picks the strategy: @p stage (default) anchors
         * @p position's corner to the whole combined stage, exactly
         * as if there were only one monitor; @p primary anchors it to
         * the monitor RandR reports as primary; @p index anchors it to
         * @p monitor.index specifically, a zero-based index into that
         * stage's monitor list (falls back to monitor 0 if it does
         * not exist, logging a warning, the same as @c rules.json's
         * @c apply.monitor).
         *
         * Only one tray dock ever exists at a time regardless of this
         * setting: the @c _NET_SYSTEM_TRAY_Sn specification permits
         * only one tray manager per screen.  Consequently, multiple
         * independent trays cannot coexist on the same screen.
         *
         * @see @c systray.c
         */
        struct {
            enum config_systray_monitor_anchor_e {
                CONFIG_SYSTRAY_MONITOR_STAGE = 0,
                CONFIG_SYSTRAY_MONITOR_PRIMARY,
                CONFIG_SYSTRAY_MONITOR_INDEX
            } anchor;
            uint32_t index; /**< Only meaningful when @c anchor is
                                 @c CONFIG_SYSTRAY_MONITOR_INDEX */
        } monitor;

        /**
         * @brief Where newly docked icons are placed relative to the
         *        ones already docked
         */
        enum config_systray_order_e {
            /** New icons are appended after the last one */
            CONFIG_SYSTRAY_ORDER_LEFT_TO_RIGHT = 0,
            /** New icons are inserted before the first one */
            CONFIG_SYSTRAY_ORDER_RIGHT_TO_LEFT,
            /** Kept sorted by icon class name, 'A' to 'Z' */
            CONFIG_SYSTRAY_ORDER_ASCENDING,
            /** Kept sorted by icon class name, 'Z' to 'A' */
            CONFIG_SYSTRAY_ORDER_DESCENDING
        } order;
        bool is_enabled;    /**< Enable the built-in systray dock */

        /**
         * @brief Whether the tray ever acquires the
         *        @c _NET_SYSTEM_TRAY_S0 selection at all, letting
         *        third-party applications dock an icon in it
         *
         * Never loaded from any configuration file: an ordinary
         * session's @a config_set_default_values always sets this
         * @c true, and restricted-memory mode's
         * @a config_set_default_values_memguard (@c config/memguard.h)
         * always sets it @c false, as a fixed part of that mode's
         * profile rather than something @c memguard.json itself is
         * allowed to configure.  With this @c false, the tray still
         * shows its clock and battery text when @p is_enabled is also
         * @c true; only docking a third party's icon is ever affected.
         *
         * @see @a systray_init and @a systray_reload
         */
        bool is_embedding_enabled;

        /**
         * @brief Whether the tray publishes an
         *        @c _NET_WM_STRUT_PARTIAL / @c _NET_WM_STRUT, reserving
         *        an on-screen area that maximized windows and placement
         *        leave alone
         *
         * @c false by default, so nothing is reserved and an explicit
         * {0, 0, 0, 0} strut is published, the same as if the tray were
         * not there at all for placement purposes.  Setting this
         * @c true instead makes the tray reserve an on-screen area, per
         * the specification's recommendation for a docking area, a
         * taskbar, or a panel.
         *
         * @see @a systray_get_reserved_strut and
         *      @a desktop_update_workarea
         */
        bool reserve_space;

        /**
         * @brief Whether smart placement (@c windows.placement:
         *        @c "smart") avoids landing a newly mapped window on
         *        top of the tray
         *
         * @c true by default.  Has no effect when @p reserve_space is
         * @c true: the tray's on-screen area is already excluded from
         * the workarea @a place_window_smart searches in that case, so
         * no candidate position could ever land on it regardless of
         * this setting.  Only meaningful, then, for a tray configured
         * strutless (@p reserve_space @c false): every candidate
         * position @a place_window_smart scores is checked against the
         * tray's current on-screen rectangle the same way it already
         * checks every other visible client, so a new window still
         * tends to avoid sitting on top of the tray even though the
         * tray itself reserves no space for that to be guaranteed.
         * This affects placement scoring only; the tray is not a real
         * client, so it still cannot be moved, iconified, or otherwise
         * acted on the way an actual window can.
         *
         * @see @a place_window_smart
         */
        bool avoid_overlap;

        /**
         * @brief Extra space added on top of whatever @c reserve_space
         *        already reserves for the tray, on each of the four
         *        screen edges
         *
         * Mirrors @p config_desktop_s's @p margins exactly: added to
         * the tray's computed strut rather than replacing it, so a
         * taller reservation than the tray's exact visual footprint is
         * possible without having to fake it by inflating @p height
         * instead.  All zero by default, same as no extra margin at
         * all.  Has no effect when @p reserve_space is @c false: an
         * all-zero strut plus a margin is still all zero from
         * @p desktop_update_workarea's point of view, so there is
         * nothing meaningful to add to.
         *
         * @see @a s_systray_strut_update in @c systray/layout.c
         */
        struct {
            uint32_t left;
            uint32_t top;
            uint32_t bottom;
            uint32_t right;
        } margins;

        /**
         * @brief Where the tray dock window sits in the stacking order
         *        relative to normal client windows and fullscreen ones
         */
        enum config_systray_layer_e {
            /** Always behind every normal client window, the default */
            CONFIG_SYSTRAY_LAYER_BELOW = 0,
            /** Above normal windows, though a fullscreen window still
             *  covers it */
            CONFIG_SYSTRAY_LAYER_ABOVE,
            /** Above everything, fullscreen windows included */
            CONFIG_SYSTRAY_LAYER_OVERLAY
        } layer;

        /**
         * @brief Optional clock drawn inside the systray dock
         *
         * Where it is positioned/aligned is shared with @c battery
         * below.
         *
         * @see @c text
         */
        struct {
            bool is_enabled;        /**< Draw the clock or not */
            char format[CONFIG_MAX_LENGTH_NAME]; /**< 'strftime(3)'
                                                      format string */
        } clock;

        /**
         * @brief Optional battery status drawn inside the systray dock,
         *        compatible with either the Linux ACPI or the older APM
         *        battery interface
         *
         * The status text itself follows @p threshold: @p charged is
         * the percentage (typically 100) at or above which it reads
         * "Full" instead of a percentage; @p low and @p critical each
         * append one or two '!' to the percentage while running on
         * battery power (never while on AC), e.g., "20%!" or "5%!!".
         *
         * "AC" is appended whenever AC power is connected, whether or
         * not the battery itself is present or charged.  "N/A" is shown
         * when no battery matching @p backend can be read at all.
         * Where it is positioned/aligned is shared with @p clock above.
         * 
         * @see @p text
         */
        struct {
            /** Draw the battery status at all */
            bool is_enabled;

            struct {
                /** Percentage at or above which the status reads
                 *  "Full" */
                uint32_t charged;
                /** Percentage at or below which a single '!' is
                 *  appended */
                uint32_t low;
                /** Percentage at or below which two '!' are appended
                 *  instead of one */
                uint32_t critical;
            } threshold;

            /**
             * @brief Which kernel battery interface to read, and which
             *        battery to read from it
             */
            struct {
                enum config_battery_backend_type_e {
                    /** Linux sysfs @c /sys/class/power_supply, the
                     *  modern, near-universal interface */
                    CONFIG_BATTERY_BACKEND_ACPI = 0,

                    /** Legacy @c /proc/apm, for older hardware or
                     *  kernels without ACPI */
                    CONFIG_BATTERY_BACKEND_APM
                } type;

                /**
                 * @brief Which battery to read when a system has more
                 *        than one, zero-indexed
                 *
                 * For instance, @c 1 selects @c BAT1 under ACPI.
                 * Meaningless for APM, which only ever exposes one
                 * aggregate battery.
                 */
                uint32_t number;

            } backend;

            /**
             * @brief Seconds between re-reading the battery status
             *
             * @see @c WM_SYSTRAY_BATTERY_POLL_SECONDS in
             *      @c defs/loop.h for the built-in default this
             *      overrides
             */
            uint32_t poll_seconds;
        } battery;

        /**
         * @brief Shared placement and alignment for the clock and
         *        battery status text, when either or both are enabled
         */
        struct {
            /**
             * @brief Which of the two to show, and in what
             *        left-to-right order
             *
             * An item absent from this list does not show even if its
             * @p is_enabled is @c true, and one with @p is_enabled
             * @c false is skipped even if listed here.
             */
            enum config_systray_text_item_e {
                CONFIG_SYSTRAY_TEXT_CLOCK = 0,
                CONFIG_SYSTRAY_TEXT_BATTERY
            } order[2];
            uint8_t order_count;

            enum config_systray_text_position_e {
                CONFIG_SYSTRAY_TEXT_LEFT = 0,  /**< Before the icons,
                                                    in dock order */
                CONFIG_SYSTRAY_TEXT_RIGHT      /**< After the icons,
                                                    in dock order */
            } position;
        } text;
    } systray;

    struct {
        uint32_t desktop_count;         /**< Number of desktops */
        uint32_t desktop_inaugural;     /**< Initial desktop */

        /** Grid interpretation of the desktop list below */
        struct config_desktop_layout_s desktop_layout;

        /** Pannable virtual-desktop size, in whole screens */
        struct config_viewport_s viewport;

        struct {
            char name[CONFIG_MAX_LENGTH_NAME];  /**< Desktop name */
            struct desktop_settings_s {
                union {
                    /* Pixmap image; */         /**< Background image */
                    uint32_t color;             /**< Background color */
                } background;
            } settings;                         /**< Desktop settings */
        } desktops[CONFIG_MAX_DESKTOPS];        /**< Desktops per screen */
    } screens[CONFIG_MAX_SCREENS];              /**< All screens */

    /**
     * @brief Whether 'KEYBIND_LAUNCH_LAUNCHER' opens the built-in
     *        run-box instead of spawning @p programs.launcher
     *
     * Its top-level section, rather than nested under @p programs
     * itself, specifically to avoid the confusion a second, differently
     * typed "launcher" key nested right next to @p programs.launcher (a
     * plain command string) would invite; see @c menu/dialog/run.h for
     * the run-box itself.
     *
     * @p is_enabled defaults to @c false in normal mode; defaults to
     * @c true in restricted-memory mode, where avoiding the extra
     * process @p programs.launcher itself would otherwise spawn (even a
     * minimal one, e.g., 'gmrun', this mode's default for it) fits that
     * mode's whole reason for existing.
     *
     * @see @a ik_handle_launch (@c input/kbd/interact.c) for where this
     *      is consulted
     */
    struct {
        bool is_enabled;
    } prompt;

    /**
     * @brief The @c fortune easter egg, whether its keyboard shortcut
     *        is active at all, and which command it runs
     *
     * @p command is run through a shell (@a popen), so it may be any
     * shell command line, not just a bare executable name (e.g.,
     * @c (fortune -s) for short-only fortunes, @c (fortune -o) for
     * offensive ones, or a specific fortune database/language).
     *
     * Runs literally as configured, with no argument substitution or
     * validation of its own: an invalid command simply produces no
     * output, which the dialog already falls back to a built-in message
     * for.
     *
     * @see @c menu/dialog/fortune.c
     * @see @c STR_FORTUNE_FALLBACK in @c defs/uistr.h
     */
    struct {
        bool is_enabled;
        char command[CONFIG_MAX_LENGTH_COMMAND];
    } fortune;

    char theme[CONFIG_MAX_LENGTH_FILENAME];

    /* Basic main programs: terminal and program launcher */
    struct {
        char terminal[CONFIG_MAX_LENGTH_COMMAND];
        char launcher[CONFIG_MAX_LENGTH_COMMAND];
        char file_manager[CONFIG_MAX_LENGTH_COMMAND];
        char web_browser[CONFIG_MAX_LENGTH_COMMAND];
        char editor[CONFIG_MAX_LENGTH_COMMAND];
    } programs;
};


#endif  /* ! CONFIG_BASE_H */
