/**
 * @file config/theme.h
 *
 * @brief Theme configuration
 *
 * Fonts, colors, borders and geometry for every themeable surface:
 * window decorations, icons, menus, dialogs and the systray, as loaded
 * from the theme file a @c config.json names.
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

#ifndef CONFIG_THEME_H
#define CONFIG_THEME_H


/* System includes */
#include <stdbool.h>
#include <stdint.h>

/* Default initial values */
#include <defs/config.h>


/* Theme-related configuration structure */
/**
 * @brief Maximum titlebar buttons on one side (left or right)
 *
 * One per button kind, so that a theme may put every one of them on the
 * same side if it likes.  Repeats never take a slot, the loader
 * dropping a name it has already seen. 
 */
#define CONFIG_MAX_TITLEBAR_BUTTONS (9u)


/**
 * @brief A single titlebar button kind, as named in a theme's
 *        @p window.titlebar.buttons.left / @p .right lists
 *
 * A button not present in either list is simply never drawn and never
 * clickable; there is no separate "hidden" flag.  Omission from both
 * lists IS how a theme turns a button off.
 */
enum config_titlebar_button_e {
    CONFIG_TITLEBAR_BUTTON_PIN = 0,
    CONFIG_TITLEBAR_BUTTON_LAYER,
    CONFIG_TITLEBAR_BUTTON_ICONIZE,
    CONFIG_TITLEBAR_BUTTON_HIDE,
    CONFIG_TITLEBAR_BUTTON_SHADE,
    CONFIG_TITLEBAR_BUTTON_MAXIMIZE,
    CONFIG_TITLEBAR_BUTTON_FULLSCREEN,
    CONFIG_TITLEBAR_BUTTON_CLOSE,
    CONFIG_TITLEBAR_BUTTON_STICKY
};


/**
 * @brief Font, color and border shape shared by every themeable
 *        surface, active and inactive windows and icons and the
 *        systray alike
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
     *        @c _NET_WM_WINDOW_OPACITY
     *
     * @note IcoWM composites nothing itself and only publishes the
     *       atom on the window concerned, converted to the 32-bit
     *       range that atom expects
     * @note Without a compositing manager running, @c picom among
     *       them, this has no visible effect whatever
     *
     * @see @a config_theme_opacity_to_raw in @c config.h
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
         * @brief Whether windows get window-manager decoration at all;
         *        also implied by @p titlebar.height being zero
         *
         * @see @a client_is_decorated / @a ci_set_decoration_defaults
         *      for where that equivalence is applied, since a theme
         *      only needs to specify one or the other
         */
        bool is_decorated;

        struct {
            /** Setting it to zero is equivalent to setting
             *  @c window.is-decorated to false */
            uint32_t height;

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
                /**
                 * @brief Button glyph colors, independent of the
                 *        titlebar's text foreground
                 *
                 * - @p on for a button whose state is currently engaged
                 *   (pinned, a non-normal layer, or simply the
                 *   focused-window state every other button reflects);
                 * - @p off otherwise.
                 *
                 * A button that cannot currently do anything (e.g.,
                 * maximize on a non-resizable client) is not drawn at
                 * all rather than needing a third color for that
                 * case.
                 */
                struct {
                    uint32_t on;
                    uint32_t off;
                } color;

                enum config_titlebar_button_e
                    left[CONFIG_MAX_TITLEBAR_BUTTONS];
                enum config_titlebar_button_e
                    right[CONFIG_MAX_TITLEBAR_BUTTONS];
                uint8_t left_count;
                uint8_t right_count;

                /**
                 * @brief Side of one button, in pixels
                 *
                 * Chosen by the theme rather than derived from the
                 * titlebar height: making the bar taller is a
                 * decision about the bar, and should not silently
                 * resize the buttons in it.
                 *
                 * @see @c WM_DECOR_BTN_SIZE_DEFAULT and
                 *      @c WM_DECOR_BTN_SIZE_MIN (@c defs/client.h)
                 *      for the default and the floor; the ceiling is
                 *      the titlebar height less two, so a button
                 *      always leaves a pixel of bar above and below
                 *      it
                 */
                uint16_t size;

                /**
                 * @brief Whether each button draws the symbol of its
                 *        own action
                 *
                 * Crossed diagonals for @c close, a hollow square for
                 * @c maximize, and so on.  With this off every button
                 * is the plain filled square the state-reporting ones
                 * always were, which is what a theme wants when its
                 * own colors are doing the telling apart, or at a
                 * @p size too small for a symbol to survive.
                 *
                 * @note @c pin, @c sticky and @c layer stay plain
                 *       squares either way: their color already
                 *       reports their state, and a symbol on top of
                 *       that would compete with it
                 */
                bool use_symbols;
            } buttons;
        } titlebar;

        struct config_theme_style_s active;
        struct config_theme_style_s inactive;
    } window;

    /* Icons theme when windows are iconified */
    struct {
        bool is_captioned;

        /**
         * @brief Draw the client's @c _NET_WM_ICON image
         *
         * The image is centered in the icon window's square
         * icon-graphic area, above the caption text; the two never
         * overlap, since the caption has its separate strip below
         * that square.
         *
         * @see @p is_captioned
         * @see @c WM_ICON_SQUARE_SIZE and @c WM_ICON_CAPTION_HEIGHT in
         *      @c defs/icon.h
         */
        bool show_pixmaps;

        /**
         * @brief Draw the small state-hint indicators in the icon's
         *        top corners
         *
         * A filled square in the top-left when the client is pinned,
         * and a single letter in the top-right for the outermost
         * maximize or full screen state it still holds while
         * iconified ('f'/'m'/'h'/'v'; none when it holds neither).
         *
         * @see @c client_properties_s.state in @c client.h and
         *      @a ri_icon_hints_draw in @c render/icon.c
         */
        bool show_hints;

        struct config_theme_style_s active;
        struct config_theme_style_s inactive;
    } icon;

    /* Systray dock theme */
    struct {
        struct config_theme_style_s style;
        uint32_t height;    /**< Tray dock height in pixels; icons and
                                 the clock/battery text (if enabled,
                                 see @p config_base_s.systray.text for
                                 which are shown and in what order) are
                                 vertically centered or aligned within
                                 it, per @p text.valign for the text and
                                 centered for icons; must be at least
                                 tall enough to fit an icon, see
                                 @p pixmap.size below, or icons get
                                 clipped */

        /**
         * @brief Sizing for each docked icon's embed window
         *
         * Every docked icon is forced to exactly @p size by @p size
         * pixels regardless of whatever size it originally requested,
         * with @p padding pixels of breathing room around and between
         * icons.
         *
         * @see @p systray_icon_size_enforce in @c systray.c
         */
        struct {
            uint32_t size;      /**< Side length in pixels of each
                                     docked icon's embed window */
            uint32_t padding;   /**< Padding in pixels around and
                                     between icons */
        } pixmap;

        /**
         * @brief Appearance-only placement for the clock/battery text,
         *        as opposed to which items show and in what order (a
         *        behavior setting)
         *
         * @see @p config_base_s.systray.text
         */
        struct {
            /**
             * @brief Horizontal gap, in pixels, between adjacent text
             *        items
             *
             * Gap in between the battery status and the clock when more
             * than one is shown.  Has no effect on the inset between
             * the text block as a whole and the tray's edges, which
             * is fixed.
             *
             * @see @p pixmap.padding' above
             */
            uint32_t gap;

            enum config_systray_text_valign_e {
                /** Centered in the tray's full height */
                CONFIG_SYSTRAY_TEXT_VALIGN_CENTER = 0,
                CONFIG_SYSTRAY_TEXT_VALIGN_TOP,
                CONFIG_SYSTRAY_TEXT_VALIGN_BOTTOM
            } valign;
        } text;
    } systray;

    /**
     * @brief Default desktop background color
     *
     * Used only as the fallback for a desktop whose entry in
     * @p screens.settings.desktops (@c config.json) does not set its
     * own @p background-color; a desktop that does set one always keeps
     * it regardless of this.
     *
     * Also only ever used when no external tool (@c xsetbg, @c feh,
     * @c nitrogen, &c.) has painted the root window with its
     * wallpaper pixmap, exactly like an explicit per-desktop color.
     * Named @p color.background, matching every other themed section
     * (@p systray.color.background and so on), rather than
     * @p background.color, even though a desktop has no corresponding
     * foreground to pair it with today.
     *
     * @see @a render_desktop_background_render in
     *      @c render/desktop/background.c
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
     * @c Alt+Tab style cycle menu's window chrome.  The cycle
     * menu's individual icon cells keep using @p icon.active /
     * @p icon.inactive above instead of this, since that is what
     * already themes "the icon currently selected while cycling"
     * specifically.
     */
    struct {
        /**
         * @brief Text color for an entry that cannot currently be
         *        activated
         *
         * For example @c maximize on a client that cannot be resized.
         */
        uint32_t disabled_foreground;

        /** Line color for a separator between groups of entries */
        uint32_t separator_color;

        /**
         * @brief The menu window's outer frame, entries aside
         *
         * Distinct from any entry's @p border (see @p unselected,
         * @p selected, @p label above): those draw a rectangle around
         * one row; this is the single window border XCB itself draws
         * around the whole menu.
         *
         * The @c Alt+Tab style cycle menu shares this same field for
         * its window frame, so a context menu and the cycle menu
         * always present the same outer border regardless of whatever
         * an entry's border happens to be set to (including entries
         * having none at all).
         */
        struct {
            uint32_t color;
            uint32_t width;
        } border;

        /**
         * @brief Inset, in pixels, between the menu window's edges
         *        and every row's text, and, for a submenu, its arrow
         *        indicator
         *
         * Applies equally to @p unselected, @p selected, and @p label
         * rows.
         */
        struct {
            uint32_t horizontal;
            uint32_t vertical;
        } padding;

        /**
         * @brief Style for a menu entry that is neither hovered nor the
         *        keyboard-navigated selection
         */
        struct config_theme_style_s unselected;

        /**
         * @brief Style for the hovered or keyboard-navigated entry
         */
        struct config_theme_style_s selected;

        /**
         * @brief Style for a non-interactive heading row, that is,
         *        a @c CTXMENU_LABEL
         *
         * Never highlighted or activated, so it never borrows
         * @p unselected or @p selected even though it can look similar
         * by default.
         */
        struct config_theme_style_s label;

        /**
         * @brief Desired opacity, 0 to 100, published on the menu
         *        window itself through @c _NET_WM_WINDOW_OPACITY
         *
         * The same window-versus-row distinction as @p border above:
         * @c _NET_WM_WINDOW_OPACITY is a per-window property, so it
         * cannot vary row by row the way @p unselected / @p selected /
         * @p label's colors do, and lives here, a sibling of
         * @p border, rather than inside any one of those.  The cycle
         * menu's window shares this same field, the same way it
         * already shares @p border.
         *
         * @note IcoWM never composites anything itself, so this has no
         *       visible effect at all unless a compositing manager is
         *       also running and reading the property back off the
         *       window
         */
        uint8_t opacity;

        /**
         * @brief Draw the application's icon to the left of the
         *        name, for whichever rows represent an actual client
         *        window
         *
         * Applies to the @c Alt+Tab style cycle menu (both the window
         * and the icon variant) and to the all-desktops window list
         * a no-op for context menu entries that do not represent
         * a client at all (the root menu, ordinary command entries),
         * which never reserve icon space regardless of this setting.
         *
         * The icon is sized to fit within the row, minus
         * @c WM_MENU_ICON_INSET on top and bottom.  A row whose client
         * has no icon of its own to draw still reserves that same
         * square of blank space, so every row's text stays aligned
         * in the same column regardless of which rows happen to have
         * one.
         *
         * @see @c menu/context/winlist.c, @c menu/cycle/draw.c,
         *      @c defs/ctxmenu.h and @a wmicon_draw_at
         */
        bool show_pixmaps;
    } menu;

    /**
     * @brief Fuzzy window-search widget theme (@c menu/search.h)
     *
     * Its dedicated section rather than reusing @p menu above.  The
     * two happened to share identical values when the widget was
     * first built, but nothing ties them together architecturally, and
     * a user may want the widget to stand out from ordinary context
     * menus.
     */
    struct {
        /** Style for the query bar itself (the text actually typed) */
        struct config_theme_style_s input;

        /** Style for a result row that is neither hovered nor the
         *  keyboard-navigated selection */
        struct config_theme_style_s unselected;

        /** Style for the hovered or keyboard-navigated result row */
        struct config_theme_style_s selected;

        /** The widget window's outer frame, the same
         *  window-versus-row distinction @p menu.border's doc
         *  comment gives */
        struct {
            uint32_t color;
            uint32_t width;
        } border;
    } search;

    /**
     * @brief Built-in run-box theme (@c menu/dialog/run.h)
     */
    struct {
        /** Style for the "Run:" prompt itself */
        struct config_theme_style_s label;

        /** Style for the typed command */
        struct config_theme_style_s input;

        /** The box's outer frame */
        struct {
            uint32_t color;
            uint32_t width;
        } border;
    } prompt;

    /**
     * @brief Dialog theme (the quit-confirmation and generic message
     *        dialogs)
     */
    struct {
        uint32_t background;    /**< Dialog window background color */

        struct {
            uint32_t color;
            uint32_t width;
        } border;

        /**
         * @brief Desired opacity, 0 to 100, published on the dialog
         *        window itself through @c _NET_WM_WINDOW_OPACITY
         *
         * A sibling of @p background / @p border above, not of
         * @p button.unselected / @p button.selected below.  The dialog
         * window is one single window regardless of which button (if
         * any) currently has the keyboard-navigated selection, and
         * @c _NET_WM_WINDOW_OPACITY is a per-window property, so it
         * cannot vary per button.
         *
         * @note IcoWM never composites anything itself, so this has no
         *       visible effect at all unless a compositing manager
         *       is also running and reading the property back off the
         *       window
         */
        uint8_t opacity;

        /**
         * @brief Prompt text
         *
         * For example, "Are you sure you want to exit IcoWM?" or
         * "lupDujHomwIj lubuy'moH gharghmey", if that makes sense
         * apart from Monty Python.
         */
        struct {
            char font[CONFIG_MAX_LENGTH_FONTNAME];
            uint32_t foreground;

            /**
             * @brief Inset, in pixels, between the dialog window's
             *        a edges and the prompt text
             */
            struct {
                uint32_t horizontal;
                uint32_t vertical;
            } padding;
        } label;

        /**
         * @brief The dialog's buttons ("Cancel", "Exit", &c.),
         *        styled the same way as a menu entry
         *
         * @note Not selected, or the keyboard-navigated choice
         */
        struct {
            struct config_theme_style_s unselected;
            struct config_theme_style_s selected;

            /** Horizontal gap, in pixels, between adjacent buttons */
            uint32_t gap;

            /**
             * @brief Inset, in pixels, between a button's edges and
             *        its label
             *
             * Shared by @p unselected and @p selected on purpose, since
             * a button growing or shrinking when it becomes the
             * selection would shift every other button beside it.
             */
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
     * notification.  Both are single-style, non-interactive overlays
     * with no selected/unselected state to distinguish, unlike @p menu
     * and @p dialog above.
     */
    struct config_theme_style_s overlay;

    /**
     * @brief Theme settings published through the built-in XSETTINGS
     *        manager
     *
     * Implements the freedesktop.org XSETTINGS specification (a
     * @c _XSETTINGS_Sn manager selection publishing a
     * @c _XSETTINGS_SETTINGS property) so that GTK/Qt applications
     * requesting to "use theme colors"/"system settings" pick up a real
     * theme, icon theme, cursor theme, and DPI instead of falling back
     * to their built-in defaults.
     *
     * This is purely a theme concern, not a behavior one, hence living
     * here rather than in @p config_base_s.  Whether it runs at
     * all is still controlled by @p is_enabled below, but everything it
     * publishes is an appearance choice.
     */
    struct {
        bool is_enabled;                /**< Enable the built-in
                                             XSETTINGS manager */
        /**
         * @brief Display resolution, in dots per inch
         *
         * Published as @c Xft/DPI, multiplied by 1024 as the XSETTINGS
         * convention requires.
         */
        unsigned int dpi;
        struct {
            char gtk_theme_name[CONFIG_MAX_LENGTH_NAME];
            char icon_theme_name[CONFIG_MAX_LENGTH_NAME];
            char cursor_theme_name[CONFIG_MAX_LENGTH_NAME];
            /** Cursor size in pixels */
            unsigned int cursor_theme_size;
        } theme;
    } xsettings;

    /**
     * @brief Scratchpad's border, since it never has any other
     *        decoration to theme
     *
     * Always undecorated.  @p width of @c 0 disables the border
     * entirely, the same way @p window.titlebar.height of @c 0 disables
     * the titlebar.
     *
     * @see @c scratchpad.h
     */
    struct {
        struct {
            uint32_t color;
            uint32_t width;
        } border;
    } scratchpad;

    /**
     * @brief Border shown around whichever window or icon is
     *        currently selected while cycling (@c Alt+Tab and its
     *        icon-menu counterpart)
     *
     * Deliberately its field, not a reuse of @p window.active's
     * own border: the two answer different questions ("is this
     * client focused" vs. "is this the one the cycle is pointing at
     * right now"), and a client already focused before cycling
     * began can otherwise end up displayed with the exact same
     * color as the one currently selected, the only difference
     * being a few pixels of width, easy to miss at a glance.
     * Applied through @a render_outline_show/_move/_hide
     * (render/outline.h), never through the target's native
     * border width, so cycling never shifts the target by however
     * many pixels @p width itself happens to be, regardless of
     * @p window.active/inactive's configured width.
     *
     * @note A theme changing @p window.active/inactive away from
     *       this project's default color family should
     *       reconsider this field too, for the same reason a theme
     *       changing @p active without also changing @p inactive
     *       risks leaving the two indistinguishable from one
     *       another
     */
    struct {
        struct {
            uint32_t color;
            uint32_t width;
        } border;
    } cycle;
};


#endif  /* ! CONFIG_THEME_H */
