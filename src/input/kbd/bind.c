/**
 * @file input/keyboard.c
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
#include <stdlib.h>     /* free */
#include <string.h>     /* memcpy */
#include <strings.h>    /* strcasecmp, strncasecmp */

/* XCB includes */
#include <xcb/xcb.h>
#include <xcb/xcb_keysyms.h>

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
#include <input/kbd/keycodes.h>


/** Resolved key bindings loaded from configuration */
static wm_keybinding_td s_keybindings[WM_MAX_KEYBINDINGS];

/** Number of active key bindings */
static int s_keybindings_count = 0;


/**
 * @brief Resolve configured modifier aliases such as @c modc or @c mods
 *
 * Expands symbolic modifier aliases from the configuration into their
 * actual configured string values.  If the token does not match a known
 * alias, the original token is returned unchanged.
 *
 * @param config Configuration holding the alias strings
 * @param token  Modifier token to resolve
 *
 * @return Resolved modifier string, or the original token if no alias
 *         matches
 *
 * @note Complexity: @e O(1)
 */
static const char *s_resolve_modifier_token(const config_td *config,
        const char *token)
{
    if (token == NULL || config == NULL) {
        return token;
    }

    if (strcasecmp(token, "modc") == 0) { return config->bindings.modc; }
    if (strcasecmp(token, "mods") == 0) { return config->bindings.mods; }
    if (strcasecmp(token, "modl") == 0) { return config->bindings.modl; }
    if (strcasecmp(token, "mod1") == 0) { return config->bindings.mod1; }
    if (strcasecmp(token, "mod2") == 0) { return config->bindings.mod2; }
    if (strcasecmp(token, "mod3") == 0) { return config->bindings.mod3; }
    if (strcasecmp(token, "mod4") == 0) { return config->bindings.mod4; }
    if (strcasecmp(token, "mod5") == 0) { return config->bindings.mod5; }

    return token;
}


/**
 * @brief Map a single modifier token to an XCB modifier mask
 *
 * Converts a textual modifier name into the corresponding XCB modifier
 * mask.  Supports configured aliases, common modifier names, and some
 * alternative spellings.
 *
 * @param config Configuration holding the alias strings
 * @param token  Modifier token to parse
 *
 * @return Matching XCB modifier mask, or 0 if not recognized
 *
 * @note Complexity: @e O(1)
 */
static uint16_t s_parse_modifier_token(const config_td *config,
        const char *token)
{
    const char *resolved = s_resolve_modifier_token(config, token);

    if (resolved == NULL || resolved[0] == '\0') {
        return 0;
    }

    if (strcasecmp(resolved, "mod1") == 0 ||
            strcasecmp(resolved, "alt") == 0) {
        return XCB_MOD_MASK_1;
    }
    if (strcasecmp(resolved, "mod2") == 0 ||
            strcasecmp(resolved, "num_lock") == 0 ||
            strcasecmp(resolved, "num-lock") == 0) {
        return XCB_MOD_MASK_2;
    }
    if (strcasecmp(resolved, "mod3") == 0) {
        return XCB_MOD_MASK_3;
    }
    if (strcasecmp(resolved, "mod4") == 0 ||
            strcasecmp(resolved, "super") == 0 ||
            strcasecmp(resolved, "win") == 0) {
        return XCB_MOD_MASK_4;
    }
    if (strcasecmp(resolved, "mod5") == 0 ||
            strcasecmp(resolved, "hyper") == 0) {
        return XCB_MOD_MASK_5;
    }
    if (strcasecmp(resolved, "ctrl") == 0 ||
            strcasecmp(resolved, "control") == 0) {
        return XCB_MOD_MASK_CONTROL;
    }
    if (strcasecmp(resolved, "shift") == 0) {
        return XCB_MOD_MASK_SHIFT;
    }
    if (strcasecmp(resolved, "lock") == 0 ||
            strcasecmp(resolved, "caps_lock") == 0 ||
            strcasecmp(resolved, "caps-lock") == 0) {
        return XCB_MOD_MASK_LOCK;
    }

    return 0;
}


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
            return (xcb_keysym_t) (0xffbdu + (unsigned long) n);
        }
    }

    /* Named keys */
    if (strcasecmp(token, "return") == 0 ||
            strcasecmp(token, "enter") == 0)  { return 0xff0du; }
    if (strcasecmp(token, "space") == 0)      { return 0x0020u; }
    if (strcasecmp(token, "tab") == 0)        { return 0xff09u; }
    if (strcasecmp(token, "escape") == 0 ||
            strcasecmp(token, "esc") == 0)    { return 0xff1bu; }
    if (strcasecmp(token, "backspace") == 0)  { return 0xff08u; }
    if (strcasecmp(token, "delete") == 0 ||
            strcasecmp(token, "del") == 0)    { return 0xffffu; }
    if (strcasecmp(token, "left") == 0)       { return 0xff51u; }
    if (strcasecmp(token, "up") == 0)         { return 0xff52u; }
    if (strcasecmp(token, "right") == 0)      { return 0xff53u; }
    if (strcasecmp(token, "down") == 0)       { return 0xff54u; }
    if (strcasecmp(token, "home") == 0)       { return 0xff50u; }
    if (strcasecmp(token, "end") == 0)        { return 0xff57u; }
    if (strcasecmp(token, "pageup") == 0 ||
            strcasecmp(token, "prior") == 0)  { return 0xff55u; }
    if (strcasecmp(token, "pagedown") == 0 ||
            strcasecmp(token, "next") == 0)   { return 0xff56u; }

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
    char *prev_tok = NULL;
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
            uint16_t mod = s_parse_modifier_token(config, prev_tok);
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


/* Parse configured key bindings and install passive grabs */
void keyboard_load(list_td *surfaces, xcb_key_symbols_t *keysyms,
        const config_td *config)
{
    /* Binding definitions: string from config paired with action type */
    struct {
        const char *binding;
        enum wm_keybind_type_e type;
    } defs[] = {
        { config->bindings.keyboard.wm.redraw,
          KEYBIND_WM_REDRAW },
        { config->bindings.keyboard.wm.reload,
          KEYBIND_WM_RELOAD },
        { config->bindings.keyboard.wm.quit,
          KEYBIND_WM_QUIT },
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
        { config->bindings.keyboard.window.hide,
          KEYBIND_CLIENT_HIDE },
        { config->bindings.keyboard.window.close,
          KEYBIND_CLIENT_CLOSE },
        { config->bindings.keyboard.window.kill,
          KEYBIND_CLIENT_KILL },
        { config->bindings.keyboard.window.maximize,
          KEYBIND_CLIENT_MAXIMIZE },
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
        /* Hardcoded emergency exit */
        { "Ctrl+Mod1+BackSpace", KEYBIND_NONE },
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

    s_keybindings_count = 0;

    for (int i = 0; defs[i].binding != NULL; ++i) {
        xcb_keysym_t keysym;
        uint16_t modmask;
        xcb_keycode_t *keycodes;

        if (!s_parse_binding(config, defs[i].binding,
                    &modmask, &keysym)) {
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
        surface_td *s0 = (surface_td *) list_data(list_head(surfaces));
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
            (keysym == 0xffe1u || keysym == 0xffe2u)) {
        return true;
    }

    /* Control_L (0xffe3), Control_R (0xffe4) */
    if ((mask & XCB_MOD_MASK_CONTROL) &&
            (keysym == 0xffe3u || keysym == 0xffe4u)) {
        return true;
    }

    /* Meta/Alt: 0xffe7 to 0xffea */
    if ((mask & XCB_MOD_MASK_1) &&
            keysym >= 0xffe7u && keysym <= 0xffeau) {
        return true;
    }

    /* Num_Lock (0xff7f) */
    if ((mask & XCB_MOD_MASK_2) && keysym == 0xff7fu) {
        return true;
    }

    /* Super_L (0xffeb), Super_R (0xffec) */
    if ((mask & XCB_MOD_MASK_4) &&
            (keysym == 0xffebu || keysym == 0xffecu)) {
        return true;
    }

    /* Hyper_L (0xffed), Hyper_R (0xffee) */
    if ((mask & XCB_MOD_MASK_5) &&
            (keysym == 0xffedu || keysym == 0xffeeu)) {
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
