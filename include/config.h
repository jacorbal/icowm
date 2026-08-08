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
            CONFIG_FOCUS_POLICY_FOLLOW_MOUSE
        } focus_policy;
        enum config_placement_policy_e {
            CONFIG_PLACEMENT_POLICY_SMART = 0,
            CONFIG_PLACEMENT_POLICY_CASCADE,
            CONFIG_PLACEMENT_POLICY_CENTERED,
            CONFIG_PLACEMENT_POLICY_UNDER_MOUSE
        } placement_policy;

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

    bool show_desktop_overlay;       /**< Show desktop name on switch */
    bool enable_emergency_shortcut; /**< Allow 'Ctrl+Mod1+BackSpace' exit */
    bool enable_fortune_shortcut;   /**< Allow 'Ctrl+Mod4+BackSpace'
                                          fortune dialog */

    /**
     * @brief Startup-notification sequence timeout
     *
     * See @c sn_set_timeout_seconds and @c SN_TIMEOUT_SECONDS in
     * sn.h for what this controls and its built-in default.
     */
    struct {
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
         * @brief Where the tray dock window sits in the stacking order
         *        relative to normal client windows and fullscreen ones
         */
        enum config_systray_layer_e {
            CONFIG_SYSTRAY_LAYER_BELOW = 0,     /**< Always behind every
                                                     normal client window */
            CONFIG_SYSTRAY_LAYER_ABOVE,         /**< Above normal windows
                                                     (default); a
                                                     fullscreen window
                                                     still covers it */
            CONFIG_SYSTRAY_LAYER_ABOVE_ALL      /**< Above everything,
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
                    CONFIG_BATTERY_BACKEND_ACPI = 0, /**< Linux sysfs
                                                           @c
                                                           /sys/class/power_supply,
                                                           the modern,
                                                           near-universal
                                                           interface */
                    CONFIG_BATTERY_BACKEND_APM       /**< Legacy
                                                           @c /proc/apm,
                                                           for older
                                                           hardware or
                                                           kernels
                                                           without ACPI
                                                           */
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

            char redraw[CONFIG_MAX_LENGTH_BINDING];
            char reload[CONFIG_MAX_LENGTH_BINDING];
            char quit[CONFIG_MAX_LENGTH_BINDING];

            char show_desktop[CONFIG_MAX_LENGTH_BINDING];

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
            char info[CONFIG_MAX_LENGTH_BINDING];
            char layer[CONFIG_MAX_LENGTH_BINDING];
            char kill[CONFIG_MAX_LENGTH_BINDING];
            char maximize[CONFIG_MAX_LENGTH_BINDING];
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
         *      only needs to specify one or the other)
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
                                  see 'SYSTRAY_ICON_SIZE' in systray.c,
                                  or icons get clipped */

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
             *  tray's own edges, which is fixed (see
             *  'SYSTRAY_ICON_PAD' in systray.c) */
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

        /** Inset, in pixels, between the menu window's own edges and
         *  every row's text (and, for a submenu, its arrow indicator);
         *  applies equally to @c unselected, @c selected, and
         *  @c label rows */
        struct {
            uint32_t horizontal;
            uint32_t vertical;
        } padding;
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
        unsigned int cursor_theme_size; /**< Cursor size in pixels */
        unsigned int dpi;               /**< Display resolution, in dots
                                             per inch; published as
                                             'Xft/DPI' (times 1024, per
                                             the XSETTINGS convention) */
        char gtk_theme_name[CONFIG_MAX_LENGTH_NAME];
        char icon_theme_name[CONFIG_MAX_LENGTH_NAME];
        char cursor_theme_name[CONFIG_MAX_LENGTH_NAME];
    } xsettings;
};


/**
 * @brief Per-output RandR profile configuration
 *
 * Stores the user-defined settings for a single physical output.
 * When @p enabled is @c true the output profile is applied at startup
 * and whenever the output is reconnected.
 */
struct config_randr_output_s {
    char name[CONFIG_RANDR_OUTPUT_NAME_LEN];    /**< Output name
                                                     ("HDMI-1",
                                                     "VESA-1",...) */

    bool is_enabled;    /**< Whether this profile is active */
    bool is_primary;    /**< Mark output as primary */

    struct dimensions_s preferred_res;  /**< Preferred resolution */
    struct position_s position;         /**< Output position (x, y) */
    uint16_t rotation;                  /**< Preferred rotation
                                             (XRandR mask) */
};


/**
 * @brief XRandR layout configuration
 *
 * Holds a list of per-output profiles and a global on/off switch.
 */
struct config_randr_s {
    bool is_enabled;        /**< Enable RandR profile management */
    uint32_t output_count;  /**< Number of populated output profiles */
    struct config_randr_output_s outputs[CONFIG_RANDR_MAX_OUTPUTS];
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
} config_td;


/* Public interface */
/**
 * @brief Initialize a new structure for the configuration
 *
 * Allocates memory for a new @c config_td structure and initializes its
 * fields to default values.
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
 * @brief Populate the configuration structure with default values
 *
 * Sets default values for all fields in the given @c config_td
 * structure.
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
 * @brief Load all the configuration
 *
 * Loads configuration settings into the provided @c config_td structure
 * from predefined sources (configuration files) by invoking the
 * functions @a config_load_base, @a config_load_bindings and
 * @a config_load_theme.
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
 * @brief Load base configuration settings from a JSON file
 *
 * Loads base configuration settings into the provided @c config_base_s
 * structure from the specified file.
 *
 * @param filename    The path to the configuration file
 * @param config_base Pointer to the base configuration structure to
 *                    populate
 *
 * @return 0 on success, or otherwise
 *
 * @note Complexity: @e O(n), where @e n is the size of the
 *       configuration file being read
 *
 * @see @c config_base_s
 */
int config_load_base(const char *filename,
        struct config_base_s *config_base);

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
