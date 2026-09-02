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
#include <utils/xcb/connection.h>


/** Resolved key bindings loaded from configuration */
static wm_keybinding_td s_keybindings[WM_MAX_KEYBINDINGS];

/** Number of active key bindings */
static int s_keybindings_count = 0;


/**
 * @brief One configured binding string paired with its action
 */
typedef struct {
    const char *binding;
    enum wm_keybind_type_e type;
} s_keybind_def_td;


/**
 * @brief Lock modifiers a grab is installed under as well as bare
 *
 * Caps Lock and Num Lock are reported in a key press's state, so
 * a grab that named neither would simply not fire while either is
 * on.  Installing every combination is what makes a binding work
 * regardless.
 */
static const uint16_t s_lockmods[] = {
    0,
    XCB_MOD_MASK_LOCK,
    XCB_MOD_MASK_2,
    XCB_MOD_MASK_LOCK | XCB_MOD_MASK_2
};


/**
 * @brief Cycling pairs whose two halves must not resolve alike
 */
static const struct {
    enum wm_keybind_type_e a;
    enum wm_keybind_type_e b;
} s_cycle_pairs[] = {
    { KEYBIND_CLIENT_CYCLE_NEXT, KEYBIND_CLIENT_CYCLE_PREV },
    { KEYBIND_DESKTOP_ICON_NEXT, KEYBIND_DESKTOP_ICON_PREV }
};


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
 * only part that actually needs its function; the list traversal
 * and null-surface skip around it do not.
 *
 * @param surfaces  List of surfaces to check
 * @param predicate Called with each non-@c NULL surface in turn;
 *                  returns @c true to stop and report a match
 *
 * @return @c true if @p predicate returned @c true for at least one
 *         surface in @p surfaces
 *
 * @note Answers @c false without a list, since @c list_head reaches
 *       into one unguarded and the contract that every caller passes
 *       the manager's own is nowhere written down
 * @note Complexity: @e O(n), where @e n is the number of surfaces
 */
static bool s_any_surface_matches(list_td *surfaces,
        bool (*predicate)(const surface_td *surface))
{
    if (surfaces == NULL || predicate == NULL) {
        return false;
    }

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


/**
 * @brief Fill in every binding the configuration names
 *
 * The array is terminated by an entry whose @c binding is @c NULL,
 * the same way the loop reading it expects.
 *
 * @param config Configuration the binding strings come from
 * @param defs   Array to fill, of at least @p max entries
 * @param max    How many entries @p defs can hold
 *
 * @return Number of entries written, terminator excluded
 *
 * @note Complexity: @e O(n), where @e n is the number of bindings
 */
static size_t s_keyboard_binding_defs(const config_td *config,
        s_keybind_def_td *defs, size_t max)
{
    const s_keybind_def_td all[] = {
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
        { config->bindings.keyboard.window.send_to.monitor.north,
          KEYBIND_CLIENT_MOVE_MONITOR_NORTH },
        { config->bindings.keyboard.window.send_to.monitor.south,
          KEYBIND_CLIENT_MOVE_MONITOR_SOUTH },
        { config->bindings.keyboard.window.send_to.monitor.east,
          KEYBIND_CLIENT_MOVE_MONITOR_EAST },
        { config->bindings.keyboard.window.send_to.monitor.west,
          KEYBIND_CLIENT_MOVE_MONITOR_WEST },
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
        { config->bindings.keyboard.window.inspect,
          KEYBIND_CLIENT_INSPECT },
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
        { config->bindings.keyboard.window.send_to.desktop.north,
          KEYBIND_CLIENT_SEND_TO_DESKTOP_NORTH },
        { config->bindings.keyboard.window.send_to.desktop.south,
          KEYBIND_CLIENT_SEND_TO_DESKTOP_SOUTH },
        { config->bindings.keyboard.window.send_to.desktop.east,
          KEYBIND_CLIENT_SEND_TO_DESKTOP_EAST },
        { config->bindings.keyboard.window.send_to.desktop.west,
          KEYBIND_CLIENT_SEND_TO_DESKTOP_WEST },
        { config->bindings.keyboard.desktop.show,
          KEYBIND_DESKTOP_SHOW },
        { config->bindings.keyboard.wm.scratchpad,
          KEYBIND_WM_SCRATCHPAD_TOGGLE },
        { config->bindings.keyboard.desktop.go_to.desktop[0],
          KEYBIND_DESKTOP_GOTO_0 },
        { config->bindings.keyboard.desktop.go_to.desktop[1],
          KEYBIND_DESKTOP_GOTO_1 },
        { config->bindings.keyboard.desktop.go_to.desktop[2],
          KEYBIND_DESKTOP_GOTO_2 },
        { config->bindings.keyboard.desktop.go_to.desktop[3],
          KEYBIND_DESKTOP_GOTO_3 },
        { config->bindings.keyboard.desktop.go_to.desktop[4],
          KEYBIND_DESKTOP_GOTO_4 },
        { config->bindings.keyboard.desktop.go_to.desktop[5],
          KEYBIND_DESKTOP_GOTO_5 },
        { config->bindings.keyboard.desktop.go_to.desktop[6],
          KEYBIND_DESKTOP_GOTO_6 },
        { config->bindings.keyboard.desktop.go_to.desktop[7],
          KEYBIND_DESKTOP_GOTO_7 },
        { config->bindings.keyboard.desktop.go_to.desktop[8],
          KEYBIND_DESKTOP_GOTO_8 },
        { config->bindings.keyboard.desktop.go_to.desktop[9],
          KEYBIND_DESKTOP_GOTO_9 },
        { config->bindings.keyboard.desktop.add,
          KEYBIND_DESKTOP_ADD },
        { config->bindings.keyboard.desktop.remove,
          KEYBIND_DESKTOP_REMOVE },
        { config->bindings.keyboard.wm.toggle_strutless_maximize,
          KEYBIND_WM_TOGGLE_STRUTLESS_MAXIMIZE },
        { config->bindings.keyboard.cycle.window.prev,
          KEYBIND_CLIENT_CYCLE_PREV },
        { config->bindings.keyboard.cycle.window.next,
          KEYBIND_CLIENT_CYCLE_NEXT },
        { config->bindings.keyboard.cycle.desktop.north,
          KEYBIND_DESKTOP_NORTH },
        { config->bindings.keyboard.cycle.desktop.south,
          KEYBIND_DESKTOP_SOUTH },
        { config->bindings.keyboard.cycle.desktop.east,
          KEYBIND_DESKTOP_EAST },
        { config->bindings.keyboard.cycle.desktop.west,
          KEYBIND_DESKTOP_WEST },
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
         * mirrors emergency exit's combination below but with
         * 'Mod4' in place of 'Mod1', keeping the two visually and
         * mnemonically distinct while both stay clear of
         * 'Ctrl+Mod1+F10', which is commonly reserved by the system
         * for switching to a text console */
        { config->bindings.keyboard.wm.fortune, KEYBIND_WM_FORTUNE },
        /* Hardcoded emergency exit, grabbed only when enabled */
        { "Ctrl+Mod1+Backspace", KEYBIND_WM_EMERGENCY_EXIT }
    };
    const size_t count = sizeof(all) / sizeof(all[0]);
    size_t written = 0u;

    while (written < count && written + 1u < max) {
        defs[written] = all[written];
        written++;
    }

    defs[written].binding = NULL;
    defs[written].type = KEYBIND_NONE;

    return written;
}


/**
 * @brief Release every key grab made on each root window
 *
 * Called before re-grabbing, so a reload leaves no stale grab
 * behind: @c xcb_grab_key only ever adds one, and a binding whose
 * combination changed would otherwise keep firing on the old one as
 * well as the new.
 *
 * @param surfaces Every surface to release grabs on
 *
 * @note Complexity: @e O(n), where @e n is the number of surfaces
 */
static void s_keyboard_ungrab_all(list_td *surfaces)
{
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

    xcb_ungrab_key(xcb_connection_get(), XCB_GRAB_ANY,
            surface->screen->root, XCB_MOD_MASK_ANY);
}
}


/**
 * @brief Whether a binding is disabled and must not be grabbed
 *
 * @param type                Binding to judge
 * @param config              Active configuration
 * @param has_multi_monitor   Whether any surface has several
 *                            monitors
 * @param has_multi_desktop   Whether any surface has several
 *                            desktops
 *
 * @return @c true when the binding must be skipped
 *
 * @note Complexity: @e O(1)
 */
static bool s_keyboard_is_disabled(enum wm_keybind_type_e type,
        const config_td *config, bool has_multi_monitor,
        bool has_multi_desktop)
{
    /* Named directly rather than through @c KEYBIND_NONE: that value
     * only ever appears on the array's terminator, which the loop
     * reading it stops before reaching, so the emergency exit has to
     * be named itself or its grab happens regardless of the flag. */
    if (type == KEYBIND_WM_EMERGENCY_EXIT &&
            !config->base.shutdown.enable_emergency_shortcut) {
        return true;
    }

    /* The fortune easter egg, the same way */
    if (type == KEYBIND_WM_FORTUNE &&
            !config->base.fortune.is_enabled) {
        return true;
    }

    /* Every "move to monitor" direction, when no surface has more
     * than one monitor to move to.  All four are named.  An earlier
     * two-direction version of this guard named only 'next', which
     * silently left 'prev' grabbed, and so reachable as a no-op, on
     * a genuinely single-monitor surface. */
    if ((type == KEYBIND_CLIENT_MOVE_MONITOR_NORTH ||
                type == KEYBIND_CLIENT_MOVE_MONITOR_SOUTH ||
                type == KEYBIND_CLIENT_MOVE_MONITOR_EAST ||
                type == KEYBIND_CLIENT_MOVE_MONITOR_WEST) &&
            !has_multi_monitor) {
        return true;
    }

    /* Every desktop-cycling and go-to-desktop-N binding, when no
     * surface has more than one desktop to switch to */
    if ((type == KEYBIND_DESKTOP_NORTH ||
                type == KEYBIND_DESKTOP_SOUTH ||
                type == KEYBIND_DESKTOP_EAST ||
                type == KEYBIND_DESKTOP_WEST ||
                (type >= KEYBIND_DESKTOP_GOTO_0 &&
                 type <= KEYBIND_DESKTOP_GOTO_9)) &&
            !has_multi_desktop) {
        return true;
    }

    return false;
}


/**
 * @brief Install one binding's grab on every surface
 *
 * Grabbed once per lock-modifier combination, so the binding fires
 * whatever the lock state happens to be.
 *
 * @param surfaces Every surface to grab on
 * @param keycodes Null-terminated keycodes the key symbol resolved
 *                 to
 * @param modmask  Modifiers the binding itself names
 *
 * @note Complexity: @e O(n * k), where @e n is the number of
 *       surfaces and @e k the number of keycodes
 */
static void s_keyboard_grab_on_surfaces(list_td *surfaces,
        const xcb_keycode_t *keycodes, uint16_t modmask)
{
    for (list_item_td *node = list_head(surfaces);
            node != NULL; node = list_next(node)) {
        surface_td *surface = (surface_td *) list_data(node);

        if (surface == NULL || surface->screen == NULL) {
            continue;
        }

        for (int j = 0; keycodes[j] != 0; ++j) {
            for (size_t k = 0;
                    k < sizeof(s_lockmods) / sizeof(s_lockmods[0]);
                    ++k) {
                xcb_void_cookie_t ck;
                xcb_generic_error_t *err;
                ck = xcb_grab_key_checked(
                        xcb_connection_get(),
                        1,
                        surface->screen->root,
                        (uint16_t) (modmask | s_lockmods[k]),
                        keycodes[j],
                        XCB_GRAB_MODE_ASYNC,
                        XCB_GRAB_MODE_ASYNC);
                err = xcb_request_check(xcb_connection_get(), ck);
                if (err != NULL) {
                    LOGGER_WARNING(
                            "xcb_grab_key failed for" \
                            " keycode=%u modmask=0x%x error=%d",
                            keycodes[j],
                            (unsigned) (modmask | s_lockmods[k]),
                            err->error_code);
                    free(err);
                }
            }
        }
    }
}


/**
 * @brief Warn when a cycling pair's two halves resolve alike
 *
 * A next and a prev sharing one combination leaves the second
 * unreachable, since dispatch stops at the first match.
 *
 * @note Complexity: @e O(n), where @e n is the number of pairs
 */
static void s_keyboard_warn_cycle_pairs(void)
{
    /* Warn on identical next/prev bindings */
    const int count =
        (int) (sizeof(s_cycle_pairs) / sizeof(s_cycle_pairs[0]));

    for (int i = 0; i < count; ++i) {
        xcb_keysym_t aks = XCB_NO_SYMBOL;
        xcb_keysym_t bks = XCB_NO_SYMBOL;
        uint16_t amm = 0;
        uint16_t bmm = 0;
        if (keyboard_find(s_cycle_pairs[i].a, &aks, &amm) &&
                keyboard_find(s_cycle_pairs[i].b, &bks, &bmm)) {
            if (aks == bks && amm == bmm) {
                LOGGER_WARNING("Cycle next/prev bindings are" \
                        " identical (next type %u); 'prev' will" \
                        " never fire",
                        (unsigned int) s_cycle_pairs[i].a);
            }
        }
}
}


/* Load and grab every configured key binding */
void keyboard_load(list_td *surfaces, xcb_key_symbols_t *keysyms,
        const config_td *config)
{
    s_keybind_def_td defs[WM_MAX_KEYBINDINGS + 1];
    xcb_keysym_t emergency_keysym = XCB_NO_SYMBOL;
    uint16_t emergency_modmask = 0;
    bool has_multi_monitor_surface;
    bool has_multi_desktop_surface;

    has_multi_monitor_surface = s_any_surface_matches(surfaces,
            s_surface_has_multiple_monitors);
    has_multi_desktop_surface = s_any_surface_matches(surfaces,
            s_surface_has_multiple_desktops);

    s_keybindings_count = 0;
    (void) s_keyboard_binding_defs(config, defs,
            sizeof(defs) / sizeof(defs[0]));
    s_keyboard_ungrab_all(surfaces);

    if (config->base.shutdown.enable_emergency_shortcut) {
        (void) s_parse_binding(config, "Ctrl+Mod1+Backspace",
                &emergency_modmask, &emergency_keysym);
    }

    for (int i = 0; defs[i].binding != NULL; ++i) {
        xcb_keysym_t keysym;
        uint16_t modmask;
        xcb_keycode_t *keycodes;

        if (s_keyboard_is_disabled(defs[i].type, config,
                    has_multi_monitor_surface,
                    has_multi_desktop_surface)) {
            continue;
        }

        if (!s_parse_binding(config, defs[i].binding,
                    &modmask, &keysym)) {
            continue;
        }

        /* Any other binding resolving to the same combination as the
         * enabled emergency exit is ignored in its favor.  Both would
         * otherwise end up grabbed, and whichever dispatch happened
         * to check first would silently win, which for this one
         * combination must always be the emergency exit and never
         * something a configuration file could override, by
         * intention or by accident. */
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

        s_keyboard_grab_on_surfaces(surfaces, keycodes, modmask);
        free(keycodes);
    }

    s_keyboard_warn_cycle_pairs();

    if (xcb_connection_get() != NULL) {
    }
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


/* Test whether a keysym is any modifier key at all, regardless of which
 * one; unlike 'keyboard_is_modifier_for_mask' above, this does not
 * restrict the check to a specific target mask */
bool keyboard_keysym_is_modifier(xcb_keysym_t keysym)
{
    const uint16_t all_mods = (uint16_t) (
            (unsigned int) XCB_MOD_MASK_SHIFT |
            (unsigned int) XCB_MOD_MASK_CONTROL |
            (unsigned int) XCB_MOD_MASK_1 |
            (unsigned int) XCB_MOD_MASK_2 |
            (unsigned int) XCB_MOD_MASK_4 |
            (unsigned int) XCB_MOD_MASK_5);

    return keyboard_is_modifier_for_mask(keysym, all_mods);
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


/* The symbol a key actually produces, modifiers included */
xcb_keysym_t keyboard_keysym_for_state(xcb_key_symbols_t *keysyms,
        xcb_keycode_t keycode, uint16_t state)
{
    const bool has_shift =
        (state & (uint16_t) XCB_MOD_MASK_SHIFT) != 0u;
    const bool has_lock = (state & (uint16_t) XCB_MOD_MASK_LOCK) != 0u;
    const bool has_altgr = (state & (uint16_t) XCB_MOD_MASK_5) != 0u;
    xcb_keysym_t plain;
    xcb_keysym_t shifted;
    int column;

    if (keysyms == NULL) {
        return XCB_NO_SYMBOL;
    }

    /* The group AltGr selects, then Shift within it: X lays the four
     * out as plain, shifted, alternate and shifted alternate */
    column = (has_altgr ? 2 : 0) + (has_shift ? 1 : 0);

    /* Caps Lock uppercases letters and leaves everything else alone,
     * so it cannot simply be treated as another Shift.  On a Spanish
     * layout that would turn 7 into a slash for as long as the lock
     * was on.  Whether this key is a letter is asked of the key
     * itself, by seeing whether its two columns differ only in case. */
    if (has_lock) {
        plain = xcb_key_symbols_get_keysym(keysyms, keycode,
                has_altgr ? 2 : 0);
        shifted = xcb_key_symbols_get_keysym(keysyms, keycode,
                has_altgr ? 3 : 1);
        if (plain >= (xcb_keysym_t) 'a' &&
                plain <= (xcb_keysym_t) 'z' &&
                shifted == plain - 0x20u) {
            /* The lock inverts the case rather than forcing it, so
             * holding Shift with it typed lowercase again */
            column = (has_altgr ? 2 : 0) + (has_shift ? 0 : 1);
        }
    }

    return xcb_key_symbols_get_keysym(keysyms, keycode, column);
}
