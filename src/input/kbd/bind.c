/**
 * @file input/kbd/bind.c
 *
 * @brief Keyboard binding parsing, grab installation, and lookup
 *
 * Implements modifier-alias resolution, keysym and modifier token
 * parsing, the binding table, passive grab installation on all root
 * windows, binding lookup by action type, and the modifier-keysym test
 * used by the key-release handler.
 */
/*
 * Copyright (c) 2026, J. A. Corbal.
 * All rights reserved.
 *
 * This file is licensed under the 'ISC License'.
 * Read the 'LICENSE' file in the root of this repository for details.
 */

#define _POSIX_C_SOURCE 200112L /* strtok_r */


/* System includes */
#include <stdbool.h>
#include <stdint.h>
#include <stdlib.h>     /* free, strtol, NULL, size_t */
#include <string.h>     /* memcpy */
#include <strings.h>    /* strcasecmp, strncasecmp */

/* XCB includes */
#include <xcb/xcb.h>
#include <xcb/xcb_keysyms.h>

/* Default initial values */
#include <defs/input.h>
#include <defs/kbd.h>

/* ADT includes */
#include <adt/list.h>

/* Utils includes */
#include <utils/safe/safestr.h>

/* Project includes */
#include <config.h>
#include <logger.h>
#include <surface.h>

/* Local includes */
#include <input/kbd/bind.h>
#include <input/modifier.h>


/** Resolved key bindings loaded from configuration */
static wm_keybinding_td s_keybindings[WM_MAX_KEYBINDINGS];

/** Number of active key bindings */
static int s_keybindings_count = 0;


/**
 * @brief Map a key-name token to an X11 keysym
 *
 * Converts a textual key name into its corresponding X11 keysym value.
 * Supports printable single-character keys, function keys, and common
 * named keys such as arrows, navigation keys, and editing keys.
 *
 * @param token Key token to parse
 *
 * @return Matching keysym, or @c XCB_NO_SYMBOL if the token is not
 *         recognized
 *
 * @note Complexity: @e O(1)
 */
static xcb_keysym_t s_parse_keysym_token(const char *token)
{
    /* Single printable character */
    if (token[1] == '\0') {
        char c = token[0];
        if (c >= 'a' && c <= 'z') { return (xcb_keysym_t) c; }
        if (c >= 'A' && c <= 'Z') { return (xcb_keysym_t) (c + 32); }
        if (c >= '0' && c <= '9') { return (xcb_keysym_t) c; }
    }

    /* Function keys F1-F12 */
    if ((token[0] == 'F' || token[0] == 'f') &&
            token[1] >= '1' && token[1] <= '9') {
        char *end = NULL;
        long n = strtol(token + 1, &end, 10);
        if (end != NULL && *end == '\0' && n >= 1 && n <= 12) {
            return (xcb_keysym_t) (KS_FKEY_BASE + (unsigned long) n);
        }
    }

    /* Named keys */
    if (strcasecmp(token, "return") == 0 ||
            strcasecmp(token, "enter") == 0)  { return KS_RETURN; }
    if (strcasecmp(token, "space") == 0)      { return KS_SPACE; }
    if (strcasecmp(token, "tab") == 0)        { return KS_TAB; }
    if (strcasecmp(token, "escape") == 0 ||
            strcasecmp(token, "esc") == 0)    { return KS_ESCAPE; }
    if (strcasecmp(token, "backspace") == 0)  { return KS_BACKSPACE; }
    if (strcasecmp(token, "delete") == 0 ||
            strcasecmp(token, "del") == 0)    { return KS_DELETE; }
    if (strcasecmp(token, "insert") == 0 ||
            strcasecmp(token, "ins") == 0)    { return KS_INSERT; }
    if (strcasecmp(token, "left") == 0)       { return KS_LEFT; }
    if (strcasecmp(token, "up") == 0)         { return KS_UP; }
    if (strcasecmp(token, "right") == 0)      { return KS_RIGHT; }
    if (strcasecmp(token, "down") == 0)       { return KS_DOWN; }
    if (strcasecmp(token, "home") == 0)       { return KS_HOME; }
    if (strcasecmp(token, "end") == 0)        { return KS_END; }
    if (strcasecmp(token, "pageup") == 0 ||
            strcasecmp(token, "pgup") == 0 ||
            strcasecmp(token, "prior") == 0 ||
            strcasecmp(token, "previous") == 0 ||
            strcasecmp(token, "prev") == 0)   { return KS_PAGE_UP; }
    if (strcasecmp(token, "pagedown") == 0 ||
            strcasecmp(token, "pgdn") == 0 ||
            strcasecmp(token, "next") == 0)   { return KS_PAGE_DOWN; }
    if (strcasecmp(token, "pause") == 0)      { return KS_PAUSE; }
    if (strcasecmp(token, "sysreq") == 0 ||
            strcasecmp(token, "sysrq") == 0)  { return KS_SYS_REQ; }
    if (strcasecmp(token, "break") == 0)      { return KS_BREAK; }
    if (strcasecmp(token, "print") == 0 ||
            strcasecmp(token, "prntscr") == 0 ||
            strcasecmp(token, "prntscrn") == 0 ||
            strcasecmp(token, "prtsc") == 0 ||
            strcasecmp(token, "prtscn") == 0 ||
            strcasecmp(token, "prtscrn") == 0 ||
            strcasecmp(token, "ps") == 0)     { return KS_PRINT; }

    return XCB_NO_SYMBOL;
}


/**
 * @brief Parse a binding string such as @c Mod1+Shift+F9
 *
 * Splits on '+' and classifies each token as a modifier (all but the
 * last) or the key (last token).
 *
 * @param config  Configuration for alias resolution
 * @param binding Binding string from configuration
 * @param modmask Receives the combined modifier mask
 * @param keysym  Receives the main keysym
 *
 * @return @c true if the binding could be parsed, @c false otherwise
 *
 * @note Complexity: @e O(n), where @e n is the length of @p binding
 */
static bool s_parse_binding(const config_td *config,
        const char *binding, uint16_t *modmask, xcb_keysym_t *keysym)
{
    char buf[128];
    char *token;
    char *save;
    const char *prev_tok = NULL;
    size_t len;

    if (binding == NULL || binding[0] == '\0') {
        return false;
    }

    len = safe_strlen(binding);
    if (len >= sizeof(buf)) {
        len = sizeof(buf) - 1;
    }
    memcpy(buf, binding, len);
    buf[len] = '\0';

    *modmask = 0;
    *keysym = XCB_NO_SYMBOL;

    token = strtok_r(buf, "+", &save);
    while (token != NULL) {
        if (prev_tok != NULL) {
            uint16_t mod = im_parse_modifier_token(config, prev_tok);
            if (mod != 0) {
                *modmask |= mod;
            }
        }
        prev_tok = token;
        token = strtok_r(NULL, "+", &save);
    }

    if (prev_tok != NULL) {
        *keysym = s_parse_keysym_token(prev_tok);
    }

    return *keysym != XCB_NO_SYMBOL;
}


/**
 * @brief Check whether any surface in a list matches a per-surface
 *        predicate
 *
 * Shared by every "is this keybind even meaningful given the current
 * setup" check below (multiple monitors, multiple desktops, and any
 * future one of the same shape): each only differs in which single
 * field of a surface it looks at, so that one field comparison is the
 * only part that actually needs its own function; the list traversal
 * and null-surface skip around it do not.
 *
 * @param surfaces  List of surfaces to check
 * @param predicate Called with each non-@c NULL surface in turn;
 *                  returns @c true to stop and report a match
 *
 * @return @c true if @p predicate returned @c true for at least one
 *         surface in @p surfaces
 *
 * @note Complexity: @e O(n), where @e n is the number of surfaces
 */
static bool s_any_surface_matches(list_td *surfaces,
        bool (*predicate)(const surface_td *surface))
{
    for (list_item_td *node = list_head(surfaces);
            node != NULL; node = list_next(node)) {
        surface_td *const surface = (surface_td *) list_data(node);

        if (surface != NULL && predicate(surface)) {
            return true;
        }
    }
    return false;
}


/**
 * @brief Predicate: does this surface have more than one physical
 *        monitor?
 *
 * @param surface Surface to check
 *
 * @return @c true if @p surface has more than one monitor
 *
 * @note Complexity: @e O(1)
 */
static bool s_surface_has_multiple_monitors(const surface_td *surface)
{
    return surface->monitor_count > 1u;
}


/**
 * @brief Predicate: does this surface have more than one virtual
 *        desktop?
 *
 * @param surface Surface to check
 *
 * @return @c true if @p surface has more than one desktop
 *
 * @note Complexity: @e O(1)
 */
static bool s_surface_has_multiple_desktops(const surface_td *surface)
{
    return surface->desktop_count > 1u;
}


/* Parse configured key bindings and install passive grabs */
void keyboard_load(list_td *surfaces, xcb_key_symbols_t *keysyms,
        const config_td *config)
{
    /* Binding definitions: string from config paired with action type */
    struct {
        const char *binding;
        enum wm_keybind_type_e type;
    } defs[] = {
        { config->bindings.keyboard.wm.menus.root,
          KEYBIND_WM_ROOT_MENU },
        { config->bindings.keyboard.wm.menus.windows,
          KEYBIND_WM_WINDOWS_MENU },
        { config->bindings.keyboard.wm.search,
          KEYBIND_WM_SEARCH_WINDOWS },
        { config->bindings.keyboard.wm.redraw,
          KEYBIND_WM_REDRAW },
        { config->bindings.keyboard.wm.reload,
          KEYBIND_WM_RELOAD },
        { config->bindings.keyboard.wm.quit,
          KEYBIND_WM_QUIT },
        { config->bindings.keyboard.wm.shortcuts,
          KEYBIND_WM_SHORTCUTS_LIST },
        { config->bindings.keyboard.launch.terminal,
          KEYBIND_LAUNCH_TERMINAL },
        { config->bindings.keyboard.launch.launcher,
          KEYBIND_LAUNCH_LAUNCHER },
        { config->bindings.keyboard.launch.file_manager,
          KEYBIND_LAUNCH_FILE_MANAGER },
        { config->bindings.keyboard.launch.web_browser,
          KEYBIND_LAUNCH_WEB_BROWSER },
        { config->bindings.keyboard.launch.editor,
          KEYBIND_LAUNCH_EDITOR },
        { config->bindings.keyboard.window.iconify,
          KEYBIND_CLIENT_ICONIFY },
        { config->bindings.keyboard.window.iconify_all,
          KEYBIND_DESKTOP_CLIENTS_ICONIFY_ALL },
        { config->bindings.keyboard.window.deiconify_all,
          KEYBIND_DESKTOP_CLIENTS_DEICONIFY_ALL },
        { config->bindings.keyboard.window.arrange,
          KEYBIND_DESKTOP_CLIENTS_REARRANGE },
        { config->bindings.keyboard.window.hide,
          KEYBIND_CLIENT_HIDE },
        { config->bindings.keyboard.window.close,
          KEYBIND_CLIENT_CLOSE },
        { config->bindings.keyboard.window.kill,
          KEYBIND_CLIENT_KILL },
        { config->bindings.keyboard.window.maximize,
          KEYBIND_CLIENT_MAXIMIZE },
        { config->bindings.keyboard.window.next_monitor,
          KEYBIND_CLIENT_MOVE_NEXT_MONITOR },
        { config->bindings.keyboard.window.shade,
          KEYBIND_CLIENT_SHADE },
        { config->bindings.keyboard.window.fullscreen,
          KEYBIND_CLIENT_FULLSCREEN },
        { config->bindings.keyboard.window.pin,
          KEYBIND_CLIENT_PIN },
        { config->bindings.keyboard.window.layer,
          KEYBIND_CLIENT_CYCLE_LAYER },
        { config->bindings.keyboard.window.info,
          KEYBIND_CLIENT_INFO },
        { config->bindings.keyboard.window.decorate,
          KEYBIND_CLIENT_TOGGLE_DECORATION },
        { config->bindings.keyboard.window.move.absolute.center,
          KEYBIND_CLIENT_CENTER },
        { config->bindings.keyboard.window.move.relative.left,
          KEYBIND_CLIENT_MOVE_LEFT },
        { config->bindings.keyboard.window.move.relative.right,
          KEYBIND_CLIENT_MOVE_RIGHT },
        { config->bindings.keyboard.window.move.relative.up,
          KEYBIND_CLIENT_MOVE_UP },
        { config->bindings.keyboard.window.move.relative.down,
          KEYBIND_CLIENT_MOVE_DOWN },
        { config->bindings.keyboard.window.move.absolute.top_left,
          KEYBIND_CLIENT_MOVE_TOP_LEFT },
        { config->bindings.keyboard.window.move.absolute.top_right,
          KEYBIND_CLIENT_MOVE_TOP_RIGHT },
        { config->bindings.keyboard.window.move.absolute.bottom_left,
          KEYBIND_CLIENT_MOVE_BOTTOM_LEFT },
        { config->bindings.keyboard.window.move.absolute.bottom_right,
          KEYBIND_CLIENT_MOVE_BOTTOM_RIGHT },
        { config->bindings.keyboard.window.resize.left,
          KEYBIND_CLIENT_RESIZE_LEFT },
        { config->bindings.keyboard.window.resize.right,
          KEYBIND_CLIENT_RESIZE_RIGHT },
        { config->bindings.keyboard.window.resize.up,
          KEYBIND_CLIENT_RESIZE_UP },
        { config->bindings.keyboard.window.resize.down,
          KEYBIND_CLIENT_RESIZE_DOWN },
        { config->bindings.keyboard.wm.show_desktop,
          KEYBIND_DESKTOP_SHOW },
        { config->bindings.keyboard.wm.scratchpad,
          KEYBIND_WM_SCRATCHPAD_TOGGLE },
        { config->bindings.keyboard.wm.go_to.desktop[0],
          KEYBIND_DESKTOP_GOTO_0 },
        { config->bindings.keyboard.wm.go_to.desktop[1],
          KEYBIND_DESKTOP_GOTO_1 },
        { config->bindings.keyboard.wm.go_to.desktop[2],
          KEYBIND_DESKTOP_GOTO_2 },
        { config->bindings.keyboard.wm.go_to.desktop[3],
          KEYBIND_DESKTOP_GOTO_3 },
        { config->bindings.keyboard.wm.go_to.desktop[4],
          KEYBIND_DESKTOP_GOTO_4 },
        { config->bindings.keyboard.wm.go_to.desktop[5],
          KEYBIND_DESKTOP_GOTO_5 },
        { config->bindings.keyboard.wm.go_to.desktop[6],
          KEYBIND_DESKTOP_GOTO_6 },
        { config->bindings.keyboard.wm.go_to.desktop[7],
          KEYBIND_DESKTOP_GOTO_7 },
        { config->bindings.keyboard.wm.go_to.desktop[8],
          KEYBIND_DESKTOP_GOTO_8 },
        { config->bindings.keyboard.wm.go_to.desktop[9],
          KEYBIND_DESKTOP_GOTO_9 },
        { config->bindings.keyboard.wm.desktop.add,
          KEYBIND_DESKTOP_ADD },
        { config->bindings.keyboard.wm.desktop.remove,
          KEYBIND_DESKTOP_REMOVE },
        { config->bindings.keyboard.wm.toggle_strutless_maximize,
          KEYBIND_WM_TOGGLE_STRUTLESS_MAXIMIZE },
        { config->bindings.keyboard.cycle.window.prev,
          KEYBIND_CLIENT_CYCLE_PREV },
        { config->bindings.keyboard.cycle.window.next,
          KEYBIND_CLIENT_CYCLE_NEXT },
        { config->bindings.keyboard.cycle.desktop.prev,
          KEYBIND_DESKTOP_PREV },
        { config->bindings.keyboard.cycle.desktop.next,
          KEYBIND_DESKTOP_NEXT },
        { config->bindings.keyboard.cycle.icon.prev,
          KEYBIND_DESKTOP_ICON_PREV },
        { config->bindings.keyboard.cycle.icon.next,
          KEYBIND_DESKTOP_ICON_NEXT },
        /* Hardcoded 'Alt+Space': always opens the per-window context
         * menu (right-click on a titlebar); fixed, not configurable,
         * matching the common desktop-environment convention for this
         * exact key combination, the same way 'Ctrl+Mod1+Backspace'
         * below is a fixed emergency-exit shortcut.  Unrelated to
         * either 'keyboard.wm.menus.root' or '.windows': those two
         * open the desktop menu and the all-desktops window list;
         * this opens the context menu of one specific window. */
        { "Mod1+space", KEYBIND_CLIENT_WINDOW_MENU },
        /* Fortune easter egg (grabbed only if enabled); default
         * mirrors emergency exit's own combination below but with
         * 'Mod4' in place of 'Mod1', keeping the two visually and
         * mnemonically distinct while both stay clear of
         * 'Ctrl+Mod1+F10', which is commonly reserved by the system
         * for switching to a text console */
        { config->bindings.keyboard.wm.fortune, KEYBIND_WM_FORTUNE },
        /* Hardcoded emergency exit (grabbed only if enabled) */
        { "Ctrl+Mod1+Backspace", KEYBIND_WM_EMERGENCY_EXIT },
        { NULL, KEYBIND_NONE }
    };

    /* Lock-modifier variants so grabs fire even with Caps/Num Lock on */
    static const uint16_t lockmods[] = {
        0,
        XCB_MOD_MASK_LOCK,
        XCB_MOD_MASK_2,
        XCB_MOD_MASK_LOCK | XCB_MOD_MASK_2
    };

    /* Cycle-next/prev pairs: warn if both have identical binding */
    static const struct {
        enum wm_keybind_type_e a;
        enum wm_keybind_type_e b;
    } pairs[] = {
        { KEYBIND_CLIENT_CYCLE_NEXT, KEYBIND_CLIENT_CYCLE_PREV },
        { KEYBIND_DESKTOP_ICON_NEXT, KEYBIND_DESKTOP_ICON_PREV }
    };

    /* Resolved up front (only when the emergency exit is actually
     * enabled, since there is nothing to protect when it is off), so
     * every other binding below can be checked against it and skipped
     * on a collision, guaranteeing the emergency exit always wins its
     * own key combination no matter what a user's 'bindings.json'
     * happens to say. */
    xcb_keysym_t emergency_keysym = XCB_NO_SYMBOL;
    uint16_t emergency_modmask = 0;

    /* Resolved up front too: whether the "move to next monitor" grab
     * below is worth installing at all.  This is a global check (any
     * surface with more than one monitor unlocks it everywhere), not
     * a genuinely per-surface one, since every grab in this function
     * is already installed on every surface uniformly; a surface with
     * only one monitor sitting alongside another with several is rare
     * enough not to be worth restructuring the grab loop over. */
    bool has_multi_monitor_surface =
        s_any_surface_matches(surfaces, s_surface_has_multiple_monitors);

    /* Same reasoning as 'has_multi_monitor_surface' just above, for
     * the desktop-cycling and go-to-desktop-N grabs instead of the
     * move-to-next-monitor one. */
    bool has_multi_desktop_surface =
        s_any_surface_matches(surfaces, s_surface_has_multiple_desktops);

    s_keybindings_count = 0;

    /* Release every key grab this window manager previously made on
     * each root window before re-grabbing below.  Without this, calling
     * 'keyboard_load' again after a configuration reload (vid.
     * 'wm_action_config_reload') would leave a binding's OLD key
     * combination still grabbed and firing in addition to its new one
     * whenever a binding actually changed, since 'xcb_grab_key' only
     * adds a grab and there was previously nothing here that removed
     * a stale one. */
    for (list_item_td *node = list_head(surfaces);
            node != NULL; node = list_next(node)) {
        surface_td *surface = (surface_td *) list_data(node);

        if (surface == NULL || surface->screen == NULL) {
            continue;
        }

        xcb_ungrab_key(surface->connection, XCB_GRAB_ANY,
                surface->screen->root, XCB_MOD_MASK_ANY);
    }

    /* Resolve the emergency exit combo now that all declarations are
     * in place above */
    if (config->base.shutdown.enable_emergency_shortcut) {
        (void) s_parse_binding(config, "Ctrl+Mod1+Backspace",
                &emergency_modmask, &emergency_keysym);
    }

    for (int i = 0; defs[i].binding != NULL; ++i) {
        xcb_keysym_t keysym;
        uint16_t modmask;
        xcb_keycode_t *keycodes;

        /* Skip emergency exit grab when disabled in configuration.
         * ('defs[i].type == KEYBIND_NONE' would never match here:
         * that value only ever appears on the array's own NULL
         * terminator, which the loop condition above already stops
         * before reaching, so this has to name the emergency exit
         * entry's own type directly or the grab happens regardless
         * of the flag below.) */
        if (defs[i].type == KEYBIND_WM_EMERGENCY_EXIT &&
                !config->base.shutdown.enable_emergency_shortcut) {
            continue;
        }

        /* Skip the fortune easter egg grab the same way, when
         * disabled in configuration */
        if (defs[i].type == KEYBIND_WM_FORTUNE &&
                !config->base.fortune.is_enabled) {
            continue;
        }

        /* Skip the "move to next monitor" grab the same way, when no
         * surface actually has more than one monitor to move to; see
         * 'has_multi_monitor_surface' above */
        if (defs[i].type == KEYBIND_CLIENT_MOVE_NEXT_MONITOR &&
                !has_multi_monitor_surface) {
            continue;
        }

        /* Skip every desktop-cycling and go-to-desktop-N grab the
         * same way, when no surface actually has more than one
         * desktop to switch to; see 'has_multi_desktop_surface'
         * above */
        if ((defs[i].type == KEYBIND_DESKTOP_NEXT ||
                    defs[i].type == KEYBIND_DESKTOP_PREV ||
                    (defs[i].type >= KEYBIND_DESKTOP_GOTO_0 &&
                     defs[i].type <= KEYBIND_DESKTOP_GOTO_9)) &&
                !has_multi_desktop_surface) {
            continue;
        }

        if (!s_parse_binding(config, defs[i].binding,
                    &modmask, &keysym)) {
            continue;
        }

        /* Any OTHER binding that happens to resolve to the exact same
         * key combination as the (enabled) emergency exit shortcut is
         * ignored in its favor: two bindings sharing one combo would
         * otherwise both end up grabbed, and whichever happened to be
         * checked first at dispatch time would silently win, which
         * for this one combination must always be the emergency exit
         * and never something a configuration file could override,
         * intentionally or by accident. */
        if (defs[i].type != KEYBIND_WM_EMERGENCY_EXIT &&
                emergency_keysym != XCB_NO_SYMBOL &&
                keysym == emergency_keysym &&
                modmask == emergency_modmask) {
            LOGGER_WARNING("Binding '%s' collides with the emergency" \
                    " exit shortcut (Ctrl+Mod1+Backspace); ignoring" \
                    " it so the emergency exit keeps that combination" \
                    " to itself", defs[i].binding);
            continue;
        }

        keycodes = xcb_key_symbols_get_keycode(keysyms, keysym);
        if (keycodes == NULL) {
            continue;
        }

        if (s_keybindings_count < WM_MAX_KEYBINDINGS) {
            s_keybindings[s_keybindings_count].keysym = keysym;
            s_keybindings[s_keybindings_count].modmask = modmask;
            s_keybindings[s_keybindings_count].type = defs[i].type;
            s_keybindings_count++;
        }

        for (list_item_td *node = list_head(surfaces);
                node != NULL; node = list_next(node)) {
            surface_td *surface = (surface_td *) list_data(node);

            if (surface == NULL || surface->screen == NULL) {
                continue;
            }

            for (int j = 0; keycodes[j] != 0; ++j) {
                for (size_t k = 0;
                        k < sizeof(lockmods) / sizeof(lockmods[0]);
                        ++k) {
                    xcb_void_cookie_t ck;
                    xcb_generic_error_t *err;
                    ck = xcb_grab_key_checked(
                            surface->connection,
                            1,
                            surface->screen->root,
                            (uint16_t) (modmask | lockmods[k]),
                            keycodes[j],
                            XCB_GRAB_MODE_ASYNC,
                            XCB_GRAB_MODE_ASYNC);
                    err = xcb_request_check(surface->connection, ck);
                    if (err != NULL) {
                        LOGGER_WARNING(
                                "xcb_grab_key failed for" \
                                " keycode=%u modmask=0x%x error=%d",
                                keycodes[j],
                                (unsigned) (modmask | lockmods[k]),
                                err->error_code);
                        free(err);
                    }
                }
            }
        }

        free(keycodes);
    }

    /* Warn on identical next/prev bindings */
    for (int i = 0;
            i < (int) (sizeof(pairs) / sizeof(pairs[0]));
            ++i) {
        xcb_keysym_t aks = XCB_NO_SYMBOL;
        xcb_keysym_t bks = XCB_NO_SYMBOL;
        uint16_t amm = 0;
        uint16_t bmm = 0;
        if (keyboard_find(pairs[i].a, &aks, &amm) &&
                keyboard_find(pairs[i].b, &bks, &bmm)) {
            if (aks == bks && amm == bmm) {
                LOGGER_WARNING("Cycle next/prev bindings are" \
                        " identical (next type %u); 'prev' will" \
                        " never fire", (unsigned int) pairs[i].a);
            }
        }
    }

    if (surfaces != NULL && !list_is_empty(surfaces)) {
        surface_td *const s0 = (surface_td *) list_data(list_head(surfaces));
        if (s0 != NULL) {
            xcb_flush(s0->connection);
        }
    }

    LOGGER_DEBUG("Grabbed %d key binding(s)", s_keybindings_count);
}


/* Look up the first binding registered for a given action type */
bool keyboard_find(enum wm_keybind_type_e type,
        xcb_keysym_t *keysym_out, uint16_t *modmask_out)
{
    if (keysym_out == NULL || modmask_out == NULL) {
        return false;
    }

    *keysym_out = XCB_NO_SYMBOL;
    *modmask_out = 0;

    for (int i = 0; i < s_keybindings_count; ++i) {
        if (s_keybindings[i].type == type) {
            *keysym_out = s_keybindings[i].keysym;
            *modmask_out = s_keybindings[i].modmask;
            return true;
        }
    }

    return false;
}


/* Translate a raw key-press event into a binding action */
bool keyboard_find_action(xcb_key_symbols_t *keysyms,
        xcb_key_press_event_t *event,
        enum wm_keybind_type_e *type_out,
        uint16_t *raw_modmask_out)
{
    xcb_keysym_t keysym;
    uint16_t state;

    if (keysyms == NULL || event == NULL ||
            type_out == NULL || raw_modmask_out == NULL) {
        return false;
    }

    keysym = xcb_key_symbols_get_keysym(keysyms, event->detail, 0);
    state = INPUT_STRIP_LOCK_MASK(event->state);

    for (int i = 0; i < s_keybindings_count; ++i) {
        uint16_t bind_state = INPUT_STRIP_LOCK_MASK(
                s_keybindings[i].modmask);

        if (keysym == s_keybindings[i].keysym &&
                state == bind_state) {
            *type_out = s_keybindings[i].type;
            *raw_modmask_out = s_keybindings[i].modmask;
            return true;
        }
    }

    return false;
}


/* Test whether a keysym maps to a modifier in the given mask */
bool keyboard_is_modifier_for_mask(xcb_keysym_t keysym, uint16_t mask)
{
    /* Shift_L (0xffe1), Shift_R (0xffe2) */
    if ((mask & XCB_MOD_MASK_SHIFT) &&
            (keysym == KS_SHIFT_L || keysym == KS_SHIFT_R)) {
        return true;
    }

    /* Control_L (0xffe3), Control_R (0xffe4) */
    if ((mask & XCB_MOD_MASK_CONTROL) &&
            (keysym == KS_CONTROL_L || keysym == KS_CONTROL_R)) {
        return true;
    }

    /* Meta/Alt: 0xffe7 to 0xffea */
    if ((mask & XCB_MOD_MASK_1) &&
            keysym >= KS_META_L && keysym <= KS_ALT_R) {
        return true;
    }

    /* Num_Lock (0xff7f) */
    if ((mask & XCB_MOD_MASK_2) && keysym == KS_NUM_LOCK) {
        return true;
    }

    /* Super_L (0xffeb), Super_R (0xffec) */
    if ((mask & XCB_MOD_MASK_4) &&
            (keysym == KS_SUPER_L || keysym == KS_SUPER_R)) {
        return true;
    }

    /* Hyper_L (0xffed), Hyper_R (0xffee) */
    if ((mask & XCB_MOD_MASK_5) &&
            (keysym == KS_HYPER_L || keysym == KS_HYPER_R)) {
        return true;
    }

    return false;
}


/* Return the number of loaded keyboard bindings */
int keyboard_binding_count(void)
{
    return s_keybindings_count;
}


/* Access a binding entry by index */
enum wm_keybind_type_e keyboard_binding_at(int idx,
        xcb_keysym_t *keysym_out, uint16_t *modmask_out)
{
    if (idx < 0 || idx >= s_keybindings_count) {
        if (keysym_out != NULL) { *keysym_out = XCB_NO_SYMBOL; }
        if (modmask_out != NULL) { *modmask_out = 0; }
        return KEYBIND_NONE;
    }

    if (keysym_out != NULL) {
        *keysym_out = s_keybindings[idx].keysym;
    }
    if (modmask_out != NULL) {
        *modmask_out = s_keybindings[idx].modmask;
    }

    return s_keybindings[idx].type;
}
