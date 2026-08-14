/**
 * @file config.h
 *
 * @brief Configuration structures and procedures declaration
 *
 * The default configuration is taken from the configuration files on
 * the default configuration base directory.  This directory depends on
 * the environment variables: @c XDG_CONFIG_HOME/ICOWM_NAME_PROG if the
 * variable @c XDG_CONFIG_HOME is set, otherwise it will default to the
 * classic @c HOME/.ICOWM_NAME_PROG.
 *
 * @defgroup config Configuration loading
 * @ingroup wm
 */
/*
 * Copyright (c) 2026, J. A. Corbal.
 * All rights reserved.
 *
 * This file is licensed under the 'ISC License'.
 * Read the 'LICENSE' file in the root of this repository for details.
 */

#ifndef CONFIG_H
#define CONFIG_H


/* System includes */
#include <stdbool.h>
#include <stdint.h>

/* Type includes */
#include <types/pair.h>

/* Default initial values */
#include <defs/config.h>


/* Theme-related configuration structure */
/**
 * @brief Where a menu appears when it is opened by a means that
 *        has no inherent screen position of its own (e.g.,
 *        a keyboard shortcut); shared by every menu type below
 */
enum config_menu_position_e {
    CONFIG_MENU_POSITION_CENTER = 0,    /**< Always screen-centered */
    CONFIG_MENU_POSITION_UNDER_MOUSE    /**< Under the current
                                             mouse pointer position */
};


/**
 * @brief Base settings configuration structure
 */
struct config_base_s {
    char theme[CONFIG_MAX_LENGTH_FILENAME];

    /* Desktops: number and which on is the default one */
    uint32_t screen_count;                      /**< Number of screens */
    struct {
        uint32_t desktop_count;                 /**< No. of desktops */
        uint32_t desktop_inaugural;             /**< Initial desktop */

        struct {
            char name[CONFIG_MAX_LENGTH_NAME];  /**< Desktop name */
            struct desktop_settings_s {
                union {
                    //Pixmap image;               /**< Background image */
                    uint32_t color;             /**< Background color */
                } background;
            } settings;                         /**< Desktop settings */
        } desktops[CONFIG_MAX_DESKTOPS];        /**< Desktops per screen */
    } screens[CONFIG_MAX_SCREENS];              /**< All screens */

    /* Basic main programs: terminal and program launcher */
    struct {
        char terminal[CONFIG_MAX_LENGTH_COMMAND];
        char launcher[CONFIG_MAX_LENGTH_COMMAND];
        char file_manager[CONFIG_MAX_LENGTH_COMMAND];
        char web_browser[CONFIG_MAX_LENGTH_COMMAND];
        char editor[CONFIG_MAX_LENGTH_COMMAND];
    } programs;

    /* General behavior of environment towards windows */
    struct {
        uint32_t snap;          /**< Snap factor in pixels */
        uint32_t move_step;     /**< Keyboard move step in pixels */
        uint32_t resize_step;   /**< Keyboard resize step in pixels */
        bool show_geom;         /**< Show geometry overlay on move/resize */
        struct {
            bool is_new_focused;
            bool is_raised_on_focus;
        } focus;
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
            CONFIG_PLACEMENT_POLICY_UNDER_MOUSE
        } placement_policy;

        /**
         * Which physical monitor a placement decision is resolved
         * against, on a surface made of more than one sharing the
         * same combined X screen: @c pointer (default) picks whichever
         * monitor the pointer is currently on (not necessarily where
         * on that monitor the pointer actually is; the window can
         * still land far from the cursor within it, depending on the
         * placement policy), @c primary always picks the one RandR
         * reports as primary.
         *
         * @see @c place_apply, @c place_smart
         */
        enum config_placement_monitor_e {
            CONFIG_PLACEMENT_MONITOR_POINTER = 0,
            CONFIG_PLACEMENT_MONITOR_PRIMARY
        } monitor_policy;

        /**
         * Cluster a newly placed window next to others sharing its
         * @c WM_CLIENT_LEADER / @c WM_HINTS group (e.g., several
         * windows of the same application) instead of running the
         * placement policy above for it.
         *
         * @see @c place_apply
         */
        bool group_related;
    } windows;

    /* Icon placement policy settings */
    struct {
        bool show_geom;     /**< Show geometry overlay on move/resize */
        enum config_icon_placement_e {
            CONFIG_ICON_PLACEMENT_BOTTOM = 0, /**< Bottom rpw (default) */
            CONFIG_ICON_PLACEMENT_TOP,        /**< Top row */
            CONFIG_ICON_PLACEMENT_LEFT,       /**< Left column */
            CONFIG_ICON_PLACEMENT_RIGHT,      /**< Right column */
            CONFIG_ICON_PLACEMENT_SMART       /**< First free slot;
                                                   falls back to bottom
                                                   when none available */
        } placement_policy;
    } icons;

    bool enable_emergency_shortcut; /**< Allow 'Ctrl+Mod1+BackSpace' exit */

    /**
     * @brief Coordinated shutdown behavior for the normal quit action
     *
     * When quit is confirmed, every managed client is first asked to
     * close (ICCCM 'WM_DELETE_WINDOW' where supported, so an
     * application with unsaved changes gets the same chance to warn
     * the user it already gets when its own window is closed
     * individually), rather than the window manager simply exiting
     * out from under them.  @c timeout_seconds bounds how long this
     * wait lasts before whichever clients are still open get forced
     * closed regardless (see @c ccmd_client_kill, cmds/client/
     * basic.h) and the window manager exits anyway.
     *
     * @note Deliberately not consulted at all by the emergency exit
     *       shortcut above, which already bypasses even the exit
     *       session hooks for the same reason: it exists as a last
     *       resort that must never wait on anything
     *
     * @see @c wm_request_graceful_stop (wm.h), @c wm/shutdown.c
     */
    struct {
        /** Seconds to wait for clients to close on their own before
         *  forcing the rest closed; @c 0 skips the wait entirely and
         *  force-closes every remaining client right away */
        uint32_t timeout_seconds;
    } shutdown;

    /**
     * @brief The @c fortune easter egg (@c menu/dialog/fortune.c):
     *        whether its own keyboard shortcut is active at all, and
     *        which command it runs
     *
     * @c command is run through a shell (@c popen), so it may be any
     * shell command line, not just a bare executable name -- e.g.
     * @c "fortune -s" for short-only fortunes, @c "fortune -o" for
     * offensive ones, or a specific fortune database/language.  Runs
     * literally as configured, with no argument substitution or
     * validation of its own: an invalid command simply produces no
     * output, which the dialog already falls back to a built-in
     * message for (see @c STR_FORTUNE_FALLBACK, defs/uistr.h).
     */
    struct {
        bool is_enabled;
        char command[CONFIG_MAX_LENGTH_COMMAND];
    } fortune;

    /**
     * @brief Whether launching a program begins a startup-
     *        notification sequence at all, and that sequence's own
     *        timeout
     *
     * See @c sn_begin (its only call site checks @c is_enabled first)
     * and @c sn_set_timeout_seconds/@c SN_TIMEOUT_SECONDS in sn.h for
     * what @c timeout_seconds controls and its built-in default.
     */
    struct {
        bool is_enabled;
        uint32_t timeout_seconds;
    } startup_notification;

    /* Context-menu placement, per menu type */
    struct {
        struct {
            enum config_menu_position_e position;   /**< Desktop (root)
                                                         context menu
                                                         (@c menu.json) */
        } root;

        struct {
            enum config_menu_position_e position;   /**< Window list menu;
                                                         every window on
                                                         every desktop */
        } windows;
    } menus;

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
         *        on a surface made of more than one sharing the same
         *        combined X screen
         *
         * @c anchor picks the strategy: @c surface (default) anchors
         * @c position's corner to the whole combined surface, exactly
         * as if there were only one monitor; @c primary anchors it to
         * the monitor RandR reports as primary; @c index anchors it
         * to @c monitor.index specifically, a zero-based index into
         * that surface's own monitor list (falls back to monitor 0 if
         * it does not exist, logging a warning, the same as
         * @c rules.json's own @c apply.monitor).  Only one tray dock
         * ever exists at a time regardless of this setting: the
         * @c _NET_SYSTEM_TRAY_Sn manager selection this implements is
         * one per screen by its own specification, so more than one
         * independent tray on the same screen is not something any
         * tray implementation can offer, this one included.
         *
         * @see @c systray.c
         */
        struct {
            enum config_systray_monitor_anchor_e {
                CONFIG_SYSTRAY_MONITOR_SURFACE = 0,
                CONFIG_SYSTRAY_MONITOR_PRIMARY,
                CONFIG_SYSTRAY_MONITOR_INDEX
            } anchor;
            uint32_t index; /**< Only meaningful when @c anchor is
                                  @c CONFIG_SYSTRAY_MONITOR_INDEX */
        } monitor;

        enum config_systray_order_e {
            CONFIG_SYSTRAY_ORDER_LEFT_TO_RIGHT = 0, /**< New icons are
                                                         appended after
                                                         the last one */
            CONFIG_SYSTRAY_ORDER_RIGHT_TO_LEFT,     /**< New icons are
                                                         inserted before
                                                         the first one */
            CONFIG_SYSTRAY_ORDER_ASCENDING,         /**< Kept sorted by
                                                         icon class name,
                                                         'A-Z' */
            CONFIG_SYSTRAY_ORDER_DESCENDING         /**< Kept sorted by
                                                         icon class name,
                                                         'Z-A' */
        } order;            /**< Where newly docked icons are placed
                                 relative to already-docked ones */
        bool is_enabled;    /**< Enable the built-in systray dock */

        /**
         * @brief Whether the tray ever acquires the
         *        @c _NET_SYSTEM_TRAY_S0 selection at all, letting
         *        third-party applications dock an icon in it
         *
         * Never loaded from any configuration file: an ordinary
         * session's own @a config_set_default_values always sets this
         * @c true, and restricted-memory mode's own @a
         * config_set_default_values_memguard (config/memguard.h)
         * always sets it @c false, as a fixed part of that mode's own
         * profile rather than something @c memguard.json itself is
         * allowed to configure.  With this @c false, the tray still
         * shows its own clock and battery text when @c is_enabled is
         * also @c true; only docking a third party's own icon is
         * ever affected (see @c systray_init and @c systray_reload).
         */
        bool is_embedding_enabled;

        /**
         * @brief Whether the tray publishes its own
         *        @c _NET_WM_STRUT_PARTIAL/@c _NET_WM_STRUT, reserving
         *        its own on-screen area so maximized windows and
         *        placement leave it alone (see @c
         *        systray_get_reserved_strut and @c
         *        desktop_update_workarea)
         *
         * @c false by default: nothing reserved, an explicit
         * @c {0, 0, 0, 0} strut published, the same as if the tray
         * were not there at all for placement purposes.  Setting this
         * @c true instead makes the tray reserve its own on-screen
         * area, per the specification's own recommendation for a
         * docking area, a taskbar, or a panel.
         */
        bool reserve_space;

        /**
         * @brief Extra space added on top of whatever @c reserve_space
         *        already reserves for the tray itself, on each of the
         *        four screen edges
         *
         * Mirrors @c config_desktop_s's own @c margins exactly: added
         * to the tray's own computed strut (see @c
         * s_systray_update_strut in systray/layout.c) rather than
         * replacing it, so a taller reservation than the tray's own
         * exact visual footprint is possible without having to fake
         * it by inflating @c height instead.  All zero by default,
         * same as no extra margin at all.  Has no effect when
         * @c reserve_space is @c false: an all-zero strut plus a
         * margin is still all zero from @c desktop_update_workarea's
         * own point of view, so there is nothing meaningful to add
         * to.
         */
        struct {
            uint32_t top;
            uint32_t right;
            uint32_t bottom;
            uint32_t left;
        } margins;

        /**
         * @brief Where the tray dock window sits in the stacking order
         *        relative to normal client windows and fullscreen ones
         */
        enum config_systray_layer_e {
            CONFIG_SYSTRAY_LAYER_BELOW = 0,     /**< Always behind every
                                                     normal client window
                                                     (default) */
            CONFIG_SYSTRAY_LAYER_ABOVE,         /**< Above normal
                                                     windows; a
                                                     fullscreen window
                                                     still covers it */
            CONFIG_SYSTRAY_LAYER_OVERLAY        /**< Above everything,
                                                     including fullscreen
                                                     windows */
        } layer;

        /**
         * @brief Optional clock drawn inside the systray dock
         *
         * Where it is positioned/aligned is shared with @c battery
         * below; see @c text.
         */
        struct {
            bool is_enabled;    /**< Draw the clock at all */
            char format[CONFIG_MAX_LENGTH_NAME]; /**< 'strftime(3)'
                                                       format string */
        } clock;

        /**
         * @brief Optional battery status drawn inside the systray
         *        dock, compatible with either the Linux ACPI or the
         *        older APM battery interface
         *
         * The status text itself follows @c threshold: @c charged is
         * the percentage (typically 100) at or above which it reads
         * "Full" instead of a percentage; @c low and @c critical each
         * append one or two '!' to the percentage while running on
         * battery power (never while on AC), e.g., "20%!" or "5%!!".
         * "AC" is appended whenever AC power is connected, whether or
         * not the battery itself is present or charged.  "N/A" is
         * shown when no battery matching @c backend can be read at
         * all.  Where it is positioned/aligned is shared with @c clock
         * above; see @c text.
         */
        struct {
            bool is_enabled;    /**< Draw the battery status at all */

            struct {
                uint32_t charged;   /**< Percentage at/above which the
                                          status reads "Full" */
                uint32_t low;       /**< Percentage at/below which a
                                          single '!' is appended */
                uint32_t critical;  /**< Percentage at/below which two
                                          '!' are appended instead of
                                          one */
            } threshold;

            /**
             * @brief Which kernel battery interface to read, and
             *        which battery to read from it
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

                /** Which battery to read when a system has more than
                 *  one, 0-indexed (e.g., 1 for @c BAT1 under ACPI);
                 *  meaningless for APM, which only ever exposes one
                 *  aggregate battery */
                uint32_t number;
            } backend;

            /** Seconds between re-reading the battery status; see
             *  @c WM_SYSTRAY_BATTERY_POLL_SECONDS in defs/loop.h for
             *  the built-in default this overrides */
            uint32_t poll_seconds;
        } battery;

        /**
         * @brief Shared placement and alignment for the clock and
         *        battery status text, when either or both are enabled
         */
        struct {
            /** Which of the two to show, and in what left-to-right
             *  order; an item absent from this list does not show
             *  even if its own @c is_enabled is true, and one with
             *  @c is_enabled false is skipped even if listed here */
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

    /**
     * @brief Configuration for the scratchpad: a single dedicated
     *        client, launched on demand from @c command, toggled
     *        visible/hidden by its own keybind or IPC command
     *        instead of iconified/restored
     *
     * @c is_enabled just gates whether the toggle action does
     * anything at all; @c width and @c height are always applied
     * regardless of whatever geometry the client itself requests,
     * against whichever edge @c edge names, centered along that
     * edge's own other axis.
     *
     * @see @c scratchpad.c
     */
    struct {
        bool is_enabled;    /**< Enable the scratchpad toggle action */
        char command[CONFIG_MAX_LENGTH_COMMAND]; /**< Launched the
                                                        first time the
                                                        toggle runs
                                                        with no
                                                        scratchpad
                                                        client yet */
        enum config_scratchpad_edge_e {
            CONFIG_SCRATCHPAD_EDGE_TOP = 0,
            CONFIG_SCRATCHPAD_EDGE_BOTTOM,
            CONFIG_SCRATCHPAD_EDGE_LEFT,
            CONFIG_SCRATCHPAD_EDGE_RIGHT
        } edge;              /**< Screen edge it slides out from */

        /**
         * @brief Either dimension, given as a fixed pixel count or
         *        as the string @c "max", meaning "however much of
         *        that axis is actually available", so a user is
         *        never forced to hard-code a resolution that may
         *        change later
         *
         * @c pixels is only meaningful when @c mode is
         * @c CONFIG_SCRATCHPAD_SIZE_FIXED; under @c CONFIG_
         * SCRATCHPAD_SIZE_MAX the scratchpad's own placement code
         * computes it fresh every time instead, against
         * @c desktop->workarea (or the full monitor extent, when
         * @c ignore_margins is @c true), the same as a numeric
         * value would be measured against.
         */
        struct config_scratchpad_size_s {
            enum config_scratchpad_size_e {
                CONFIG_SCRATCHPAD_SIZE_FIXED = 0,
                CONFIG_SCRATCHPAD_SIZE_MAX
            } mode;
            uint32_t pixels;
        } width;             /**< Always-applied width */
        struct config_scratchpad_size_s height; /**< Always-applied
                                                       height */

        /**
         * @brief Whether the scratchpad's own placement skips
         *        'desktops.margins' and the systray's own reserved
         *        space
         *
         * @c false (the default) places it the same way an ordinary
         * client already respects that reserved space; @c true lets
         * it use the full edge regardless, e.g. a top-edge scratchpad
         * sliding out from underneath an external panel that already
         * reserves that same space rather than starting just below
         * it.
         */
        bool ignore_margins;
    } scratchpad;
};


/**
 * @brief Keyboard and mouse bindings configuration structure
 */
struct config_bindings_s {
    /* Modifiers */
    char modc[CONFIG_MAX_LENGTH_BINDING];
    char mods[CONFIG_MAX_LENGTH_BINDING];
    char modl[CONFIG_MAX_LENGTH_BINDING];
    char mod1[CONFIG_MAX_LENGTH_BINDING];
    char mod2[CONFIG_MAX_LENGTH_BINDING];
    char mod3[CONFIG_MAX_LENGTH_BINDING];
    char mod4[CONFIG_MAX_LENGTH_BINDING];
    char mod5[CONFIG_MAX_LENGTH_BINDING];

    /* Keyboard bindings */
    struct keyboard_s {
        struct {
            /**
             * Keyboard shortcuts that open a menu with no inherent
             * screen position of their own; see 'config.menus.*' for
             * where each one appears
             */
            struct {
                /**
                 * Keyboard shortcuts that open a menu with no inherent
                 * screen position of their own
                 *
                 * @see @c config.menus.* for where each one appears
                 */
                char root[CONFIG_MAX_LENGTH_BINDING];
                char windows[CONFIG_MAX_LENGTH_BINDING];
            } menus;

            /** Opens the fuzzy window-search widget; see
             *  'menu/search.h' */
            char search[CONFIG_MAX_LENGTH_BINDING];

            char redraw[CONFIG_MAX_LENGTH_BINDING];
            char reload[CONFIG_MAX_LENGTH_BINDING];
            char quit[CONFIG_MAX_LENGTH_BINDING];

            /** Opens a dialog listing every currently active keyboard
             *  shortcut; see 'ctxmenu_show_shortcuts_list' in
             *  menu/dialog/shortcuts.h */
            char shortcuts[CONFIG_MAX_LENGTH_BINDING];

            /** Opens the 'fortune' easter egg dialog; only active
             *  when 'base.fortune.is_enabled' is also true (see
             *  above) -- meaningless on its own otherwise, the same
             *  way every binding in this struct already is when the
             *  feature it triggers is itself off or unavailable */
            char fortune[CONFIG_MAX_LENGTH_BINDING];

            char show_desktop[CONFIG_MAX_LENGTH_BINDING];

            /** Toggles the scratchpad's own visibility; see
             *  'scratchpad_toggle' in 'scratchpad.h' */
            char scratchpad[CONFIG_MAX_LENGTH_BINDING];

            /* Direct desktop goto shortcuts (indices 0-9) */
            struct {
                char desktop[10][CONFIG_MAX_LENGTH_BINDING];
            } go_to;
        } wm;

        struct {
            char terminal[CONFIG_MAX_LENGTH_BINDING];
            char launcher[CONFIG_MAX_LENGTH_BINDING];
            char file_manager[CONFIG_MAX_LENGTH_BINDING];
            char web_browser[CONFIG_MAX_LENGTH_BINDING];
            char editor[CONFIG_MAX_LENGTH_BINDING];
        } launch;

        struct {
            char close[CONFIG_MAX_LENGTH_BINDING];
            char decorate[CONFIG_MAX_LENGTH_BINDING];
            char fullscreen[CONFIG_MAX_LENGTH_BINDING];
            char hide[CONFIG_MAX_LENGTH_BINDING];
            char iconify[CONFIG_MAX_LENGTH_BINDING];

            /** Iconify (minimize) every client on the current
             *  desktop */
            char iconify_all[CONFIG_MAX_LENGTH_BINDING];

            /** Restore every iconified client on the current
             *  desktop */
            char deiconify_all[CONFIG_MAX_LENGTH_BINDING];

            /** Re-apply the configured placement policy to every
             *  client on the current desktop; see ICCCM §4.1.2.6
             *  for the one exception, a transient dialog among them */
            char arrange[CONFIG_MAX_LENGTH_BINDING];

            char info[CONFIG_MAX_LENGTH_BINDING];
            char layer[CONFIG_MAX_LENGTH_BINDING];
            char kill[CONFIG_MAX_LENGTH_BINDING];
            char maximize[CONFIG_MAX_LENGTH_BINDING];
            char next_monitor[CONFIG_MAX_LENGTH_BINDING]; /**< Move
                                        focused client to the next
                                        monitor */
            char pin[CONFIG_MAX_LENGTH_BINDING];
            char shade[CONFIG_MAX_LENGTH_BINDING];

            /* Window movement, absolute and relative positions */
            struct {
                struct relative_s {
                    char right[CONFIG_MAX_LENGTH_BINDING];
                    char left[CONFIG_MAX_LENGTH_BINDING];
                    char up[CONFIG_MAX_LENGTH_BINDING];
                    char down[CONFIG_MAX_LENGTH_BINDING];
                } relative;

                struct {
                    char center[CONFIG_MAX_LENGTH_BINDING];
                    char top_left[CONFIG_MAX_LENGTH_BINDING];
                    char top_right[CONFIG_MAX_LENGTH_BINDING];
                    char bottom_left[CONFIG_MAX_LENGTH_BINDING];
                    char bottom_right[CONFIG_MAX_LENGTH_BINDING];
                } absolute;
            } move;

            struct {
                char right[CONFIG_MAX_LENGTH_BINDING];
                char left[CONFIG_MAX_LENGTH_BINDING];
                char up[CONFIG_MAX_LENGTH_BINDING];
                char down[CONFIG_MAX_LENGTH_BINDING];
            } resize;
        } window;

        struct {
            struct {
                char prev[CONFIG_MAX_LENGTH_BINDING];
                char next[CONFIG_MAX_LENGTH_BINDING];
            } desktop;
            struct {
                char prev[CONFIG_MAX_LENGTH_BINDING];
                char next[CONFIG_MAX_LENGTH_BINDING];
            } icon;
            struct {
                char prev[CONFIG_MAX_LENGTH_BINDING];
                char next[CONFIG_MAX_LENGTH_BINDING];
            } window;
        } cycle;
    } keyboard;

    /* Mouse bindings */
    struct {
        struct {
            char move[CONFIG_MAX_LENGTH_BINDING];
            char lower[CONFIG_MAX_LENGTH_BINDING];
            char resize[CONFIG_MAX_LENGTH_BINDING];
        } window;

        struct {
            struct {
                char prev[CONFIG_MAX_LENGTH_BINDING];
                char next[CONFIG_MAX_LENGTH_BINDING];
            } desktop;
        } cycle;
    } mouse;
};


/* Theme-related configuration structure */
/**
 * @brief Maximum titlebar buttons on one side (left or right)
 *
 * Generous headroom over the eight buttons the built-in default theme
 * uses across both sides combined.
 */
#define CONFIG_MAX_TITLEBAR_BUTTONS (8u)


/**
 * @brief A single titlebar button kind, as named in a theme's
 *        @c window.titlebar.buttons.left / @c .right lists
 *
 * A button not present in either list is simply never drawn and never
 * clickable; there is no separate "hidden" flag; omission from both
 * lists *is* how a theme turns a button off.
 */
enum config_titlebar_button_e {
    CONFIG_TITLEBAR_BUTTON_PIN = 0,
    CONFIG_TITLEBAR_BUTTON_LAYER,
    CONFIG_TITLEBAR_BUTTON_ICONIZE,
    CONFIG_TITLEBAR_BUTTON_HIDE,
    CONFIG_TITLEBAR_BUTTON_SHADE,
    CONFIG_TITLEBAR_BUTTON_MAXIMIZE,
    CONFIG_TITLEBAR_BUTTON_FULLSCREEN,
    CONFIG_TITLEBAR_BUTTON_CLOSE
};


/**
 * @brief Font, color, and border shared shape used by every themeable
 *        surface (window active/inactive, icon active/inactive,
 *        systray)
 */
struct config_theme_style_s {
    char font[CONFIG_MAX_LENGTH_FONTNAME];

    struct {
        uint32_t background;
        uint32_t foreground;
    } color;

    struct {
        uint32_t color;
        uint32_t width;
    } border;

    /**
     * @brief Desired opacity, 0 to 100, published through
     *        '_NET_WM_WINDOW_OPACITY'
     *
     * Purely advisory: IcoWM never composites anything itself, it only
     * publishes the atom on the relevant window, converted to the
     * 32-bit range that atom expects (see 'config_theme_opacity_to_
     * raw', config.h).  Without a compositing manager, e.g.,
     * picom, running, this has no visible effect at all
     */
    uint8_t opacity;
};


/**
 * @brief Theme configuration structure
 */
struct config_theme_s {
    /* Name of this theme, just for future reference if necessary */
    char name[80];

    /* Window theme */
    struct window_theme_s {
        /**
         * Whether windows get window-manager decoration at all; also
         * implied by 'titlebar.height' being 0
         *
         * @see @c client_is_decorated / @c ci_set_decoration_defaults
         *      for where that equivalence is applied, since a theme
         *      only needs to specify one or the other
         */
        bool is_decorated;

        struct {
            uint32_t height;    /**< Setting it to zero is equivalent
                                     to 'window.is-decorated: false' */
            enum config_titlebar_alignment_e {
                CONFIG_TITLEBAR_ALIGN_LEFT = 0,
                CONFIG_TITLEBAR_ALIGN_CENTER,
                CONFIG_TITLEBAR_ALIGN_RIGHT
            } alignment;

            struct {
                uint32_t horizontal;
                uint32_t vertical;
            } padding;

            struct {
                enum config_titlebar_button_e
                    left[CONFIG_MAX_TITLEBAR_BUTTONS];
                uint8_t left_count;
                enum config_titlebar_button_e
                    right[CONFIG_MAX_TITLEBAR_BUTTONS];
                uint8_t right_count;

                /** Button glyph colors, independent of the titlebar's
                 *  own text foreground: @c on for a button whose
                 *  state is currently engaged (pinned, a non-normal
                 *  layer, or simply the focused-window state every
                 *  other button reflects), @c off otherwise.  A
                 *  button that cannot currently do anything (e.g.,
                 *  maximize on a non-resizable client) is not drawn
                 *  at all rather than needing a third color for that
                 *  case. */
                struct {
                    uint32_t on;
                    uint32_t off;
                } color;
            } buttons;
        } titlebar;

        struct config_theme_style_s active;
        struct config_theme_style_s inactive;
    } window;

    /* Icons theme when windows are iconified */
    struct {
        bool is_captioned;

        /** Draw the client's own '_NET_WM_ICON' image, centered in
         *  the icon window's square icon-graphic area, above the
         *  caption text (see 'is_captioned'); the two never overlap,
         *  since the caption has its own separate strip below that
         *  square (see 'WM_ICON_SQUARE_SIZE' and
         *  'WM_ICON_CAPTION_HEIGHT' in defs/icon.h) */
        bool show_pixmaps;

        /** Draw the small state-hint indicators in the icon's own top
         *  corners: a filled square in the top-left when the client
         *  is sticky/pinned, and a single letter in the top-right for
         *  whichever maximize/fullscreen state it was in right before
         *  being iconified ('f'/'m'/'h'/'v'; none for plain normal;
         *  see 'client_properties_s.pre_iconify_state' in client.h
         *  and 'ri_draw_icon_hints' in render/icon.c) */
        bool show_hints;

        struct config_theme_style_s active;
        struct config_theme_style_s inactive;
    } icon;

    /* Systray dock theme */
    struct {
        struct config_theme_style_s style;
        uint32_t height;    /**< Tray dock height in pixels; icons and
                                  the clock/battery text (if enabled,
                                  see 'config_base_s.systray.text' for
                                  which are shown and in what order)
                                  are vertically centered or aligned
                                  within it, per 'text.valign' for the
                                  text and centered for icons; must be
                                  at least tall enough to fit an icon,
                                  see 'pixmap.size' below, or icons
                                  get clipped */

        /**
         * @brief Sizing for each docked icon's own embed window
         *
         * Every docked icon is forced to exactly @c size by @c size
         * pixels regardless of whatever size it originally requested
         * (see 'systray_enforce_icon_size' in systray.c), with
         * @c padding pixels of breathing room around and between
         * icons.
         */
        struct {
            uint32_t size;      /**< Side length in pixels of each
                                      docked icon's embed window */
            uint32_t padding;   /**< Padding in pixels around and
                                      between icons */
        } pixmap;

        /**
         * @brief Appearance-only placement for the clock/battery
         *        text, as opposed to which items show and in what
         *        order (a behavior setting, see
         *        'config_base_s.systray.text')
         */
        struct {
            /** Horizontal gap, in pixels, between adjacent text
             *  items (e.g., between the battery status and the
             *  clock) when more than one is shown; has no effect on
             *  the inset between the text block as a whole and the
             *  tray's own edges, which is fixed (see 'pixmap.padding'
             *  above) */
            uint32_t gap;

            enum config_systray_text_valign_e {
                CONFIG_SYSTRAY_TEXT_VALIGN_CENTER = 0, /**< Centered in
                                                             the tray's
                                                             full
                                                             height */
                CONFIG_SYSTRAY_TEXT_VALIGN_TOP,
                CONFIG_SYSTRAY_TEXT_VALIGN_BOTTOM
            } valign;
        } text;
    } systray;

    /**
     * @brief Default desktop background color
     *
     * Used only as the fallback for a desktop whose own entry in
     * @c screens.settings.desktops (config.json) does not set its own
     * @c background-color; a desktop that does set one always keeps
     * it regardless of this.  Also only ever used when no external
     * tool (xsetbg, feh, nitrogen, and so on) has painted the root
     * window with its own wallpaper pixmap, exactly like an explicit
     * per-desktop color (see @c desktop_render_background in
     * render/desktop.c).  Named @c color.background, matching every
     * other themed section (@c systray.color.background and so on),
     * rather than @c background.color, even though a desktop has no
     * corresponding foreground to pair it with today.
     */
    struct {
        struct {
            uint32_t background;
        } color;
    } desktop;

    /**
     * @brief Context/cycle menu theme
     *
     * Applies to every context menu (root menu, per-window menu, the
     * all-desktops window list, and their submenus) and to the
     * Alt+Tab-style cycle menu's own window chrome.  The cycle menu's
     * individual icon cells keep using @c icon.active / @c
     * icon.inactive above instead of this, since that is what already
     * themes "the icon currently selected while cycling" specifically.
     */
    struct {
        /** Style for a menu entry that is neither hovered nor the
         *  keyboard-navigated selection */
        struct config_theme_style_s unselected;

        /** Style for the hovered or keyboard-navigated entry */
        struct config_theme_style_s selected;

        /** Style for a non-interactive heading row (@c CTXMENU_LABEL):
         *  never highlighted or activated, so it never borrows
         *  @c unselected or @c selected even though it can look
         *  similar by default */
        struct config_theme_style_s label;

        /** Text color for an entry that cannot currently be activated
         *  (e.g., "maximize" on a client that cannot be resized) */
        uint32_t disabled_foreground;

        /** Line color for a separator between groups of entries */
        uint32_t separator_color;

        /**
         * @brief The menu window's own outer frame, entries aside
         *
         * Distinct from any entry's own @c border (see @c unselected,
         * @c selected, @c label above): those draw a rectangle around
         * one row; this is the single window border XCB itself draws
         * around the whole menu.  The Alt+Tab-style cycle menu shares
         * this same field for its own window frame, so a context menu
         * and the cycle menu always present the same outer border
         * regardless of whatever an entry's own border happens to be
         * set to (including entries having none at all).
         */
        struct {
            uint32_t color;
            uint32_t width;
        } border;

        /**
         * @brief Desired opacity, 0 to 100, published on the menu
         *        window itself through '_NET_WM_WINDOW_OPACITY'
         *
         * The same window-versus-row distinction as @c border above:
         * '_NET_WM_WINDOW_OPACITY' is a per-window property, so it
         * cannot vary row by row the way @c unselected/@c selected/
         * @c label's own colors do, and lives here, a sibling of
         * @c border, rather than inside any one of those.  The cycle
         * menu's own window shares this same field, the same way it
         * already shares @c border.  Purely advisory: IcoWM never
         * composites anything itself, so this has no visible effect
         * at all unless a compositing manager, e.g., picom, is also
         * running and reading the property back off the window.
         */
        uint8_t opacity;

        /** Inset, in pixels, between the menu window's own edges and
         *  every row's text (and, for a submenu, its arrow indicator);
         *  applies equally to @c unselected, @c selected, and
         *  @c label rows */
        struct {
            uint32_t horizontal;
            uint32_t vertical;
        } padding;

        /**
         * @brief Draw the application's own icon to the left of the
         *        name, for whichever rows represent an actual client
         *        window
         *
         * Applies to the Alt+Tab-style cycle menu (both the window
         * and the icon variant; see @c menu/cycledraw.c) and to the
         * all-desktops window list (see @c menu/context/winlist.c);
         * a no-op for context menu entries that do not represent a
         * client at all (the root menu, ordinary command entries),
         * which never reserve icon space regardless of this setting.
         * The icon is sized to fit within the row, minus
         * @c WM_MENU_ICON_INSET (see defs/ctxmenu.h) on top and
         * bottom.  A row whose client has no icon of its own to draw
         * (see @c wmicon_draw_at) still reserves that same square of
         * blank space, so every row's own text stays aligned in the
         * same column regardless of which rows happen to have one.
         */
        bool show_pixmaps;
    } menu;

    /**
     * @brief Dialog theme (the quit-confirmation and generic message
     *        dialogs)
     */
    struct {
        /** Dialog window background color */
        uint32_t background;

        struct {
            uint32_t color;
            uint32_t width;
        } border;

        /**
         * @brief Desired opacity, 0 to 100, published on the dialog
         *        window itself through '_NET_WM_WINDOW_OPACITY'
         *
         * A sibling of @c background/@c border above, not of
         * @c button.unselected/@c button.selected below: the dialog
         * window is one single window regardless of which button (if
         * any) currently has the keyboard-navigated selection, and
         * '_NET_WM_WINDOW_OPACITY' is a per-window property, so it
         * cannot vary per button.  Purely advisory: IcoWM never
         * composites anything itself, so this has no visible effect
         * at all unless a compositing manager, e.g., picom, is also
         * running and reading the property back off the window.
         */
        uint8_t opacity;

        /** Prompt text (e.g., "Are you sure you want to exit IcoWM?") */
        struct {
            char font[CONFIG_MAX_LENGTH_FONTNAME];
            uint32_t foreground;

            /** Inset, in pixels, between the dialog window's edges
             *  and the prompt text */
            struct {
                uint32_t horizontal;
                uint32_t vertical;
            } padding;
        } label;

        /** The dialog's own buttons (e.g., "Cancel" / "Exit"), styled
         *  the same way as a menu entry: not selected, or the
         *  keyboard-navigated choice */
        struct {
            struct config_theme_style_s unselected;
            struct config_theme_style_s selected;

            /** Horizontal gap, in pixels, between adjacent buttons */
            uint32_t gap;

            /** Inset, in pixels, between a button's own edges and its
             *  label; shared by @c unselected and @c selected on
             *  purpose, since a button growing or shrinking when it
             *  becomes the selection would shift every other button
             *  beside it */
            struct {
                uint32_t horizontal;
                uint32_t vertical;
            } padding;
        } button;
    } dialog;

    /**
     * @brief Transient informational overlay theme
     *
     * Applies to the client-info popup and the desktop-switch
     * notification: both are single-style, non-interactive overlays
     * with no selected/unselected state to distinguish, unlike
     * @c menu and @c dialog above.
     */
    struct config_theme_style_s overlay;

    /**
     * @brief Theme settings published through the built-in XSETTINGS
     *        manager
     *
     * Implements the freedesktop.org XSETTINGS specification (a
     * @c _XSETTINGS_Sn manager selection publishing a
     * @c _XSETTINGS_SETTINGS property) so that GTK/Qt applications
     * requesting to "use theme colors"/system settings pick up a real
     * theme, icon theme, cursor theme, and DPI instead of falling back
     * to their own built-in defaults.  This is purely a theme concern,
     * not a behavior one, hence living here rather than in
     * @c config_base_s: whether the manager runs at all is still
     * controlled by @c is_enabled below, but everything it publishes
     * is an appearance choice.
     */
    struct {
        bool is_enabled;                /**< Enable the built-in
                                             XSETTINGS manager */
        unsigned int dpi;               /**< Display resolution, in dots
                                             per inch; published as
                                             'Xft/DPI' (times 1024, per
                                             the XSETTINGS convention) */
        struct {
            char gtk_theme_name[CONFIG_MAX_LENGTH_NAME];
            char icon_theme_name[CONFIG_MAX_LENGTH_NAME];
            char cursor_theme_name[CONFIG_MAX_LENGTH_NAME];
            unsigned int cursor_theme_size; /**< Cursor size in
                                                 pixels */
        } theme;
    } xsettings;

    /**
     * @brief The scratchpad's own border, since it never has any
     *        other decoration to theme (always undecorated; see
     *        @c scratchpad.h)
     *
     * @c width of @c 0 disables the border entirely, the same way
     * @c window.titlebar.height of @c 0 disables the titlebar.
     */
    struct {
        struct {
            uint32_t color;
            uint32_t width;
        } border;
    } scratchpad;
};


/**
 * @brief Per-output RandR profile configuration
 *
 * Stores the user-defined settings for a single physical output,
 * applied via 'surface_action_apply_randr_profiles' (see
 * 'surface.h') at startup and whenever that output is (re)connected.
 */
struct config_randr_output_s {
    char name[CONFIG_RANDR_OUTPUT_NAME_LENGTH];    /**< Output name
                                                     ("HDMI-1",
                                                     "VESA-1",...) */

    /** Whether this output is used at all.  @c true applies
     *  'preferred_res', 'position', and 'rotation' below to the
     *  output's own CRTC (see 'surface_action_apply_randr_profiles'),
     *  and also lets IcoWM manage windows on it (see
     *  'surface_refresh_monitors').  @c false instead turns the
     *  output's own CRTC off if it has one, blanking it, and
     *  excludes it from window management entirely, as if physically
     *  disconnected -- useful for a permanently-connected output (a
     *  projector for mirroring, say) that should never receive
     *  windows. */
    bool is_enabled;

    /** Mark this output as RandR's primary one, applied as a separate
     *  request right after the rest of this profile; only meaningful
     *  when @c is_enabled is @c true. */
    bool is_primary;

    /** Preferred resolution; matched against the screen's own mode
     *  list, falling back to whatever mode the output's CRTC already
     *  has (or its first preferred mode, if none) when left at @c 0
     *  or when no mode matches exactly.  Only applied when @c
     *  is_enabled is @c true. */
    struct dimensions_s preferred_res;

    /** Output position (x, y) in the virtual screen; only applied
     *  when @c is_enabled is @c true. */
    struct position_s position;

    uint16_t rotation;                  /**< Preferred rotation
                                             (XRandR mask); see
                                             @c is_enabled */
};


/**
 * @brief XRandR layout configuration
 *
 * Holds a list of per-output profiles and a global on/off switch.
 *
 * @note One instance per @c config_td, shared by every managed X
 *       screen (@c surface_td), not scoped per-screen: matching in
 *       @c surface_action_apply_randr_profiles is by @c
 *       config_randr_output_s.name alone, queried independently
 *       against each screen's own RandR resources. On a multi-GPU
 *       setup with two X screens exposing an output of the same
 *       name, the matching profile applies to both identically.
 */
struct config_randr_s {
    /** Master switch for the whole per-output profile system (see
     *  'config_randr_output_s'); false ignores every profile in
     *  'outputs' and every detected RandR output is used, same as
     *  before this system existed. */
    bool is_enabled;

    uint32_t output_count;  /**< Number of populated output profiles */
    struct config_randr_output_s outputs[CONFIG_RANDR_MAX_OUTPUTS];
};


/**
 * @brief Global desktop-navigation and reserved-space behavior
 *
 * Unlike @c config_base_s (screen and desktop topology: how many
 * screens and desktops exist, and their own names/colors), none of
 * this describes topology at all -- only how navigation between
 * whatever desktops @c config_base_s already defines behaves, and how
 * much of each desktop's own area stays reserved regardless of what
 * any client itself publishes via @c _NET_WM_STRUT_PARTIAL (see @c
 * desktop_update_workarea).  Loaded from @c config.json's own top-
 * level @c "desktops" object, a sibling of @c "topology", not nested
 * inside it: unlike topology, every field here does take effect on a
 * configuration reload.
 */
struct config_desktop_s {
    /** Whether the current desktop's own name briefly overlays the
     *  screen after switching to it. */
    bool show_overlay;

    /** Whether dragging a window past a screen edge, held there past
     *  @c WM_DESKTOP_WARP_DELAY_MS (defs/desktop.h), switches to the
     *  adjacent desktop with the drag still held.  Meaningless with
     *  only one desktop. */
    bool enable_edge_warp;

    /** Whether switching past the first or last desktop wraps around
     *  to the other end, rather than stopping there.  Meaningless
     *  with only one desktop. */
    bool is_circular;

    /** Extra space reserved on each edge of every desktop's own
     *  workarea, on top of whatever @c _NET_WM_STRUT_PARTIAL clients
     *  already reserve there (see @c desktop_update_workarea);
     *  useful for a program that does not publish that property
     *  itself (e.g. Conky).  Applies identically to every desktop on
     *  every screen; there is no per-desktop or per-screen override. */
    struct {
        uint32_t top;
        uint32_t right;
        uint32_t bottom;
        uint32_t left;
    } margins;
};


/**
 * @brief Main configuration structure
 *
 * Encapsulates the main configuration, including base settings,
 * bindings, and theme.
 */
typedef struct {
    struct config_base_s base;
    struct config_bindings_s bindings;
    struct config_theme_s theme;
    struct config_randr_s randr;
    struct config_desktop_s desktops;
} config_td;


/* Public interface */
/**
 * @brief Initialize a new structure for the configuration
 *
 * Allocates memory for a new @c config_td structure and initializes
 * its fields to an ordinary session's own default values.
 * Restricted-memory mode never calls this: see @a
 * config_init_memguard in config/memguard.h for its own completely
 * separate path, which this function knows nothing about.
 *
 * @return Pointer to the initialized configuration structure, or @c NULL
 *         on failure
 *
 * @note Complexity: @e O(1), as it only involves memory allocation and
 *       initialization
 *
 * @see @c config_td
 */
config_td *config_init(void);

/**
 * @brief Destroy a configuration structure and free resources
 *
 * Frees the memory associated with a @c config_td structure.
 *
 * @param config Pointer to the configuration structure to destroy
 *
 * @note Complexity: @e O(1), as it only involves freeing memory
 *
 * @see @c config_td
 */
void config_destroy(config_td *config);

/**
 * @brief Populate the configuration structure with an ordinary
 *        session's own default values
 *
 * Sets default values for all fields in the given @c config_td
 * structure.  Restricted-memory mode never calls this: see @a
 * config_set_default_values_memguard in config/memguard.h for its own
 * completely separate profile, which this function knows nothing
 * about.
 *
 * @param config Pointer to the configuration structure to set the
 *               default values for
 *
 * @note This function is loaded before user configuration, as
 *       a fail-safe for fields not yet configured manually
 * @note Complexity: @e O(n), where @e n is the number of fields that
 *       need to be set
 */
void config_set_default_values(config_td *config);

/**
 * @brief Populate default values for one theme structure
 *
 * Used both as the compiled-in fallback theme (via @a
 * config_set_default_values and @a config_set_default_values_memguard
 * in config/memguard.h) and, before applying any theme file found, as
 * the known-good starting point that file's own fields then overlay:
 * @a config_load_theme only ever overwrites whichever fields a theme
 * file specifies, never resets the rest on its own, so a caller that
 * skips this first and reuses whatever @c theme already held from a
 * previous load would leave a field the new file no longer specifies
 * (e.g. a boolean like @c window.is-decorated) stuck at its old value
 * instead of falling back to this default.  @a config_load calls this
 * itself before loading a theme file on every call, not just the
 * first, for exactly that reason.
 *
 * @param theme Theme structure to populate
 *
 * @note Complexity: @e O(n), where @e n is the number of fields that
 *       need to be set
 */
void config_set_default_theme_values(struct config_theme_s *theme);

/**
 * @brief Load all of an ordinary session's own configuration
 *
 * Loads configuration settings into the provided @c config_td
 * structure from predefined sources (@c config.json, @c
 * bindings.json, the named theme file, and @c randr.json), by
 * invoking @a config_load_base, @a config_load_bindings, @a
 * config_load_theme, and @a config_load_randr in that order.
 * Restricted-memory mode never calls this: see @a config_load_memguard
 * in config/memguard.h for its own completely separate path (@c
 * memguard.json instead of @c config.json, @c randr.json never read
 * at all), which this function knows nothing about.
 *
 * @param config            Pointer to the configuration structure where
 *                          to load the data
 * @param config_dir_prefix Configuration directory, or @c NULL to use
 *                          default value
 *
 * @return 0 on success, or otherwise
 *
 * @note This function does not take into account default values,
 *       because it's invoked after calling @a config_set_default_values
 * @note Complexity: @e O(n), where @e n is the number of parameters
 *       loaded because it involves reading from the configuration file
 *
 * @see @a config_load_base,
 *      @a config_load_bindings,
 *      @a config_load_theme
 */
int config_load(config_td *config, const char *config_dir_prefix);

/**
 * @brief Clear whichever theme file @c config_load last recorded as
 *        specified by @c config.json but not actually found
 *
 * Called once at the start of a full configuration-loading sequence
 * (see @c wm_start and @c wm_action_config_reload), the same as
 * @c json_syntax_errors_reset (see @c utils/config/json.h), so a note
 * from a previous load or reload is never repeated for a theme that
 * has since been fixed, or attributed to the wrong one.
 *
 * @note Complexity: @e O(1)
 */
void config_missing_theme_reset(void);

/**
 * @brief The theme file path @c config_load most recently found
 *        specified by @c config.json but missing, if any
 *
 * Distinct from a theme file that was found but failed to parse as
 * JSON (that case is already covered by @c json_syntax_errors_get,
 * since @c config_load_theme goes through @c json_load_config the
 * same as any other configuration file); this is specifically for
 * @c config.json parsing successfully, naming a theme, and that
 * theme's own file simply not existing.
 *
 * @return The path, or @c NULL if no theme is currently missing
 *
 * @note Complexity: @e O(1)
 */
const char *config_missing_theme_get(void);

/**
 * @brief Resolve the configuration directory from a prefix, or from
 *        environment variables when none is given
 *
 * Resolution order: @p config_dir_prefix, if given; otherwise
 * @c "${XDG_CONFIG_HOME}/icowm"; otherwise @c "${HOME}/.icowm";
 * otherwise @c "./.icowm" in the current working directory.
 *
 * @param config_dir_prefix Configuration directory, or @c NULL to
 *                          resolve it from the environment instead
 * @param config_dir_base   Buffer to receive the resolved path
 *
 * @note @p config_dir_base should be at least
 *       @c CONFIG_MAX_LENGTH_PATH_BASE bytes
 * @note Complexity: @e O(1)
 */
void config_resolve_dir(const char *config_dir_prefix,
        char *config_dir_base);

/**
 * @brief Load base configuration settings from a JSON file
 *
 * Loads base configuration settings into the provided @c config_base_s
 * structure from the specified file, and desktop-navigation/reserved-
 * space behavior (@c "desktops" -- @c warp, @c cycle, @c margins; see
 * @c config_desktop_s) into @p config_desktop from that same file,
 * since both live in @c config.json.
 *
 * @param filename       The path to the configuration file
 * @param config_base    Pointer to the base configuration structure
 *                       to populate
 * @param config_desktop Pointer to the desktop-behavior structure to
 *                       populate
 *
 * @return 0 on success, or otherwise
 *
 * @note Complexity: @e O(n), where @e n is the size of the
 *       configuration file being read
 *
 * @see @c config_base_s
 * @see @c config_desktop_s
 */
int config_load_base(const char *filename,
        struct config_base_s *config_base,
        struct config_desktop_s *config_desktop);

/**
 * @brief Load key bindings from a JSON file
 *
 * Loads key bindings into the provided @c config_bindings_s structure
 * from the specified file.
 *
 * @param filename        Path to the key bindings configuration file
 * @param config_bindings Pointer to the bindings configuration
 *                        structure to populate
 *
 * @return 0 on success, or otherwise
 *
 * @note Complexity: @e O(n), where @e n is the size of the key bindings
 *       file being read
 *
 * @see @c config_bindings_s
 */
int config_load_bindings(const char *filename,
        struct config_bindings_s *config_bindings);

/**
 * @brief Load theme settings from a JSON file
 *
 * Loads theme settings into the provided @c config_theme_s structure
 * from the specified file.
 *
 * @param filename     The path to the theme configuration file.
 * @param config_theme Pointer to the theme configuration structure to
 *                     populate
 *
 * @return 0 on success, or otherwise
 *
 * @note Complexity: @e O(n), where @e n is the size of the theme file
 *       being read
 *
 * @see @c config_theme_s
 */
int config_load_theme(const char *filename,
        struct config_theme_s *config_theme);

/**
 * @brief Convert a theme's own 0 to 100 opacity percentage to the
 *        32-bit value '_NET_WM_WINDOW_OPACITY' itself expects
 *
 * The property's own valid range is 0 (fully transparent) to
 * 0xffffffff (fully opaque); this scales @p percent linearly onto
 * that range, matching the same formula every compositing manager
 * already assumes for its own atom.
 *
 * @param percent Opacity percentage, 0 to 100; a value above 100 is
 *                treated as 100, since 'config_theme_style_s.opacity'
 *                is meant to already be within range by the time this
 *                runs, this is only a last defensive clamp
 *
 * @return The 32-bit value to publish on '_NET_WM_WINDOW_OPACITY'
 *
 * @note Complexity: @e O(1)
 */
uint32_t config_theme_opacity_to_raw(uint8_t percent);

/**
 * @brief Load XRandR output profiles from a JSON file
 *
 * Loads per-output profile settings into the provided
 * @c config_randr_s structure from the specified file.
 *
 * @param filename     Path to the RandR configuration file
 * @param config_randr Pointer to the RandR configuration structure to
 *                     populate
 *
 * @return 0 on success, or otherwise
 *
 * @note Complexity: @e O(n), where @e n is the number of output entries
 *       in the file
 *
 * @see @c config_randr_s
 */
int config_load_randr(const char *filename,
        struct config_randr_s *config_randr);


#endif  /* ! CONFIG_H */
