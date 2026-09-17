/**
 * @file config/bindings.h
 *
 * @brief Keyboard and mouse binding configuration
 *
 * The modifier aliases and the command bound to each key or button
 * combination, as loaded from @c bindings.json.
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

#ifndef CONFIG_BINDINGS_H
#define CONFIG_BINDINGS_H


/* Default initial values */
#include <defs/config.h>


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
             * @brief Keyboard shortcuts that open a menu with no
             *        inherent screen position of their own
             *
             * @see @p config.menus.* for where each one appears
             */
            struct {
                /**
                 * @brief Keyboard shortcuts that open a menu with no
                 *        inherent screen position of their own
                 *
                 * @see @p config.menus.* for where each one appears
                 */
                char root[CONFIG_MAX_LENGTH_BINDING];
                char windows[CONFIG_MAX_LENGTH_BINDING];
            } menus;

            /**
             * @brief Opens the fuzzy window-search widget
             *
             * @see @c menu/search.h
             */
            char search[CONFIG_MAX_LENGTH_BINDING];

            char redraw[CONFIG_MAX_LENGTH_BINDING];
            char reload[CONFIG_MAX_LENGTH_BINDING];
            char quit[CONFIG_MAX_LENGTH_BINDING];

            /**
             * @brief Opens a dialog listing every currently active
             *        keyboard shortcut
             *
             * @see @p ctxmenu_show_shortcuts_list in
             *      @c menu/dialog/shortcuts.h
             */
            char shortcuts[CONFIG_MAX_LENGTH_BINDING];

            /**
             * @brief Opens the 'fortune' easter egg dialog
             *
             * Only active when @p base.fortune.is_enabled' is also
             * @c true (see above), as it would be meaningless on its
             * own otherwise, the same way every binding in this struct
             * already is when the feature it triggers is itself off or
             * unavailable.
             */
            char fortune[CONFIG_MAX_LENGTH_BINDING];

            /**
             * @brief Toggles the scratchpad's visibility
             *
             * @see @p scratchpad_toggle in @c scratchpad.h
             */
            char scratchpad[CONFIG_MAX_LENGTH_BINDING];

            /**
             * @brief Toggles whether panel/tray struts are set aside
             *        when maximizing or placing a window on this
             *        stage (strutless maximization)
             *
             * Empty by default, unlike every other binding in this
             * struct: no key combination is bound to it out of the
             * box, only IPC (@c toggle_strutless_maximize) and the
             * root menu's entry reach it until a user opts in
             * with their binding here.
             *
             * @see @a stage_action_maximize_toggle_strutless
             *      (stage.h)
             */
            char toggle_strutless_maximize[CONFIG_MAX_LENGTH_BINDING];
        } wm;

        /**
         * @brief Desktop-level actions: switching, adding/removing,
         *        and the show-desktop toggle
         *
         * Its top-level section, a sibling of @p window rather
         * than nested under @p wm.  None of
         * these act on any one particular client the way everything
         * under @p window does, but they are just as much their
         * coherent, frequently reached-for group as that one is, not
         * really a good fit for @p wm's remaining, much more
         * disparate set of window-manager-lifecycle actions (@p quit,
         * @p reload, @p redraw, and the like) either.
         */
        struct {
            /**
             * @brief Adds or removes the stage's last desktop
             *
             * @see @a enact_stage_desktop_add,
             *      @a enact_stage_desktop_remove (enact.h)
             */
            char add[CONFIG_MAX_LENGTH_BINDING];
            char remove[CONFIG_MAX_LENGTH_BINDING];

            /**
             * @brief Hides all windows and shows the empty desktop,
             *        toggling back on a second press
             *
             * @see @a enact_desktop_show (enact.h)
             */
            char show[CONFIG_MAX_LENGTH_BINDING];

            /* Direct desktop goto shortcuts (indices 0-9) */
            struct {
                char desktop[10][CONFIG_MAX_LENGTH_BINDING];
            } go_to;
        } desktop;

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

            /**
             * @brief Re-apply the configured placement policy to every
             *        client on the current desktop
             *
             * @see ICCCM §4.1.2.6 for the one exception, a transient
             *      dialog among them
             */
            char arrange[CONFIG_MAX_LENGTH_BINDING];

            char info[CONFIG_MAX_LENGTH_BINDING];
            char inspect[CONFIG_MAX_LENGTH_BINDING];
            char layer[CONFIG_MAX_LENGTH_BINDING];
            char kill[CONFIG_MAX_LENGTH_BINDING];
            char maximize[CONFIG_MAX_LENGTH_BINDING];
            char pin[CONFIG_MAX_LENGTH_BINDING];

            /** Not to be confused with @a pin above; see
             *  @c CLIENT_FLAG_STICKY's comment in @c client/state.h
             *  for the full distinction between the two */
            char sticky[CONFIG_MAX_LENGTH_BINDING];

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

            /**
             * @brief Carry the focused client somewhere else instead
             *        of just moving it in place
             */
            struct {
                /**
                 * @brief Carry the focused client to the desktop
                 *        north/south/east/west of the current one,
                 *        following it there
                 *
                 * Parallels the @c cycle.desktop siblings below,
                 * @c north, @c south, @c east and @c west, which
                 * only switch the view itself without moving any
                 * client along.  A silent no-op when there is no
                 * different desktop to move to in that direction,
                 * which is what restricted-memory mode naturally
                 * becomes, always locked to exactly one desktop.
                 *
                 * @see @a enact_client_send_to_desktop_north in
                 *      @c enact.h, and its three siblings, for the
                 *      fuller reasoning
                 */
                struct {
                    char north[CONFIG_MAX_LENGTH_BINDING];
                    char south[CONFIG_MAX_LENGTH_BINDING];
                    char east[CONFIG_MAX_LENGTH_BINDING];
                    char west[CONFIG_MAX_LENGTH_BINDING];
                } desktop;

                /**
                 * @brief Move the focused client to the monitor
                 *        north/south/east/west of the current one
                 *        on its own stage
                 *
                 * Unlike @c desktop just above, never wraps around
                 * at all, and has no equivalent of
                 * @c desktops.wrap-at-bounds to make that
                 * configurable.  See
                 * @a ccmd_client_move_to_monitor_north in
                 * @c cmds/client/geom.h, and its three siblings,
                 * for the fuller reasoning.  A no-op on a
                 * stage with one monitor or none, or when no
                 * monitor lies in the given direction at all.
                 */
                struct {
                    char north[CONFIG_MAX_LENGTH_BINDING];
                    char south[CONFIG_MAX_LENGTH_BINDING];
                    char east[CONFIG_MAX_LENGTH_BINDING];
                    char west[CONFIG_MAX_LENGTH_BINDING];
                } monitor;
            } send_to;
        } window;

        struct {
            struct {
                char north[CONFIG_MAX_LENGTH_BINDING];
                char south[CONFIG_MAX_LENGTH_BINDING];
                char east[CONFIG_MAX_LENGTH_BINDING];
                char west[CONFIG_MAX_LENGTH_BINDING];
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

        /**
         * @brief Pan the current desktop's own viewport by one
         *        screen in a direction, clamped rather than cyclic
         *
         * Parallels @c cycle.desktop above, which switches to a
         * different desktop entirely; these four only move where
         * within the current one the physical screen is looking, a
         * no-op on any desktop whose @c viewport is a single screen.
         *
         * @see @a enact_stage_viewport_pan_north in @c enact.h, and
         *      its three siblings
         */
        struct {
            struct {
                char north[CONFIG_MAX_LENGTH_BINDING];
                char south[CONFIG_MAX_LENGTH_BINDING];
                char east[CONFIG_MAX_LENGTH_BINDING];
                char west[CONFIG_MAX_LENGTH_BINDING];
            } pan;

            /**
             * @brief Move the viewport a whole page in each compass
             *        direction
             *
             * The discrete counterpart to @c pan above, which slides
             * by @c viewport.pan-step pixels instead: these jump to
             * the neighboring page outright, clamped at the grid's
             * own bounds rather than wrapping around, and a no-op
             * where there is no page that way.  Named @c page here
             * rather than after the enumerators it feeds
             * (@c KEYBIND_VIEWPORT_SWITCH_NORTH and its three
             * siblings) because @c switch is a reserved word and
             * cannot name a member.
             *
             * @see @a enact_stage_viewport_switch_north in
             *      @c enact.h, and its three siblings
             */
            struct {
                char north[CONFIG_MAX_LENGTH_BINDING];
                char south[CONFIG_MAX_LENGTH_BINDING];
                char east[CONFIG_MAX_LENGTH_BINDING];
                char west[CONFIG_MAX_LENGTH_BINDING];
            } page;

            /**
             * @brief Direct viewport page go-to shortcuts, zero-based
             *        indices 0-9 addressing the grid's first ten
             *        pages in row-major order
             *
             * Parallels @c desktop.go_to above, which switches to a
             * different desktop entirely by index; these instead only
             * move where within the current desktop the physical
             * screen is looking, a no-op past the configured
             * @c viewport grid's own page count.
             *
             * @see @a enact_stage_viewport_goto in @c enact.h
             */
            struct {
                char page[10][CONFIG_MAX_LENGTH_BINDING];
            } go_to;
        } viewport;
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
                char north[CONFIG_MAX_LENGTH_BINDING];
                char south[CONFIG_MAX_LENGTH_BINDING];
                char east[CONFIG_MAX_LENGTH_BINDING];
                char west[CONFIG_MAX_LENGTH_BINDING];
            } desktop;
        } cycle;
    } mouse;
};


#endif  /* ! CONFIG_BINDINGS_H */
