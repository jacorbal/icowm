/**
 * @file wm.c
 *
 * @brief Window manager implementation
 */
/*
 * Copyright (c) 2026, J. A. Corbal.
 * All rights reserved.
 *
 * This file is licensed under the 'ISC License'.
 * Read the 'LICENSE' file in the root of this repository for details.
 */

#define _POSIX_C_SOURCE 200112L /* sigaction, sigemptyset, strtok_r */


/* System includes */
#include <errno.h>      /* errno, EINTR */
#include <limits.h>     /* UINT16_MAX */
#include <poll.h>       /* poll, struct pollfd, POLLIN */
#include <signal.h>     /* sigaction, SIGHUP, SIGINT, SIGQUIT, SIGTERM */
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>      /* snprintf */
#include <stdlib.h>     /* NULL, free, malloc */
#include <string.h>     /* memcpy, strerror, strtok_r */
#include <strings.h>    /* strcasecmp */
#include <sys/wait.h>   /* waitpid */
#include <unistd.h>     /* execl, _exit, fork */

/* XCB includes */
#include <xcb/xcb.h>
#include <xcb/xcb_ewmh.h>
#include <xcb/xcb_keysyms.h>

/* ADT includes */
#include <adt/cdlist.h> /* Doubly linked circular list */
#include <adt/list.h>   /* Singly linked list */
#include <adt/ohtbl.h>  /* Open-addressed hash table */

/* Utils includes */
#include <utils/safestr.h>

/* Default initial values */
#include <defs/wm.h>

/* Project includes */
#include <actdata.h>
#include <action.h>
#include <client.h>
#include <config.h>
#include <desktop.h>
#include <event.h>
#include <eventq.h>
#include <logger.h>
#include <priority.h>
#include <render/desktop.h>
#include <render/surface.h>
#include <render/text.h>
#include <surface.h>

/* Local includes */
#include <wm.h>


/* Though variable static dost often lurk near,
 * In shadows of scope, few e’er call thee their own,
 * Thy global existence, to none dost bring fear,
 * A sentinel watching, though thou art alone. */
static wm_td *wm = NULL;    /**< Pointer to the singleton instance of
                                 the window manager */


/* Key binding infrastructure */
/**
 * @brief Action types for key bindings
 */
enum wm_keybind_type_e {
    KEYBIND_NONE,

    /* Desktop cycling */
    KEYBIND_DESKTOP_NEXT,               /**< Switch to next desktop */
    KEYBIND_DESKTOP_PREV,               /**< Switch to previous desktop */

    /* Window operations */
    KEYBIND_CLIENT_ICONIFY,             /**< Iconify focused client */
    KEYBIND_CLIENT_HIDE,                /**< Hide (minimize) focused client */
    KEYBIND_CLIENT_CLOSE,               /**< Close focused client */
    KEYBIND_CLIENT_KILL,                /**< Forcibly kill focused client */
    KEYBIND_CLIENT_MAXIMIZE,            /**< Maximize focused client */
    KEYBIND_CLIENT_CENTER,              /**< Center focused client */
    KEYBIND_CLIENT_SHADE,               /**< Toggle focused client shade */
    KEYBIND_CLIENT_FULLSCREEN,          /**< Toggle foc. client fullscreen */
    KEYBIND_CLIENT_PIN,                 /**< Toggle focused client sticky */
    KEYBIND_CLIENT_INFO,                /**< Show focused client info */
    KEYBIND_CLIENT_TOGGLE_DECORATION,   /**< Toggle decoration on client */
    KEYBIND_CLIENT_CYCLE_NEXT,          /**< Focus next client */
    KEYBIND_CLIENT_CYCLE_PREV,          /**< Focus previous client */
    KEYBIND_DESKTOP_ICON_NEXT,          /**< Cycle to next iconified client */
    KEYBIND_DESKTOP_ICON_PREV,          /**< Cycle to prev iconified client */

    /* Program launcher */
    KEYBIND_LAUNCH_TERMINAL,            /**< Launch terminal */
    KEYBIND_LAUNCH_LAUNCHER,            /**< Launch application launcher */
    KEYBIND_LAUNCH_FILE_MANAGER,        /**< Launch file manager */
    KEYBIND_LAUNCH_WEB_BROWSER,         /**< Launch web browser */
    KEYBIND_LAUNCH_EDITOR,              /**< Launch editor */

    /* Window movement (fixed step, or snap to a screen corner) */
    KEYBIND_CLIENT_MOVE_LEFT,           /**< Move focused client left */
    KEYBIND_CLIENT_MOVE_RIGHT,          /**< Move focused client right */
    KEYBIND_CLIENT_MOVE_UP,             /**< Move focused client up */
    KEYBIND_CLIENT_MOVE_DOWN,           /**< Move focused client down */
    KEYBIND_CLIENT_MOVE_TOP_LEFT,       /**< Snap to top-left corner */
    KEYBIND_CLIENT_MOVE_TOP_RIGHT,      /**< Snap to top-right corner */
    KEYBIND_CLIENT_MOVE_BOTTOM_LEFT,    /**< Snap to bottom-left corner */
    KEYBIND_CLIENT_MOVE_BOTTOM_RIGHT,   /**< Snap to bottom-right corner */

    /* Window resizing (fixed step) */
    KEYBIND_CLIENT_RESIZE_LEFT,         /**< Shrink focused client width */
    KEYBIND_CLIENT_RESIZE_RIGHT,        /**< Grow focused client width */
    KEYBIND_CLIENT_RESIZE_UP,           /**< Shrink focused client height */
    KEYBIND_CLIENT_RESIZE_DOWN,         /**< Grow focused client height */
};

/**
 * @brief Key binding resolved from configuration
 *
 * Stores a keyboard key symbol, its modifier mask, and the action type
 * associated with the binding.
 */
typedef struct {
    xcb_keysym_t keysym;
    uint16_t     modmask;
    enum wm_keybind_type_e type;
} wm_keybinding_td;


 /** Resolved key bindings loaded from configuration */
static wm_keybinding_td s_keybindings[WM_MAX_KEYBINDINGS];

/** Number of active key bindings */
static int s_keybindings_count = 0;


/**
 * @brief Action types for mouse bindings
 *
 * Defines the actions that a mouse binding can trigger.
 */
enum wm_mousebind_type_e {
    MOUSEBIND_NONE,
    MOUSEBIND_MOVE,             /**< Move the clicked client */
    MOUSEBIND_RESIZE,           /**< Resize the clicked client */
    MOUSEBIND_LOWER,            /**< Lower the clicked client */
    MOUSEBIND_DESKTOP_NEXT,     /**< Switch to next desktop (wheel: 5) */
    MOUSEBIND_DESKTOP_PREV,     /**< Switch to previous desktop (wheel: 4) */
};


/**
 * @brief Mouse binding resolved from configuration
 *
 * Stores a mouse button and the action associated with it.
 */
typedef struct {
    xcb_button_index_t button;
    enum wm_mousebind_type_e type;
} wm_mousebinding_td;


/** Mouse bindings registered by the window manager */
static wm_mousebinding_td s_mousebindings[WM_MAX_MOUSEBINDINGS];

/** Number of active mouse bindings */
static int s_mousebindings_count = 0;

/** Window identifier of the currently visible info popup */
static xcb_window_t s_info_popup_window = XCB_WINDOW_NONE;

/** Cached text content of the info popup, used to repaint on exposure */
static char s_info_popup_lines[4][WM_INFO_POPUP_LINE_MAX_LEN];


/**
 * @brief Mouse drag state for move and resize interactions
 *
 * Stores the current drag status, the active client, the pointer
 * position at the beginning of the drag, and the client's geometry at
 * drag start.
 */
static struct {
    bool active;
    enum window_operation_e operation;
    client_td *client;
    int16_t pointer_start_x;
    int16_t pointer_start_y;
    int32_t client_start_x;
    int32_t client_start_y;
    uint16_t client_start_w;
    uint16_t client_start_h;
} s_drag = {
    .active = false,
    .operation = CLIENT_OPERATION_IDLE,
    .client = NULL,
    .pointer_start_x = 0,
    .pointer_start_y = 0,
    .client_start_x = 0,
    .client_start_y = 0,
    .client_start_w = 0,
    .client_start_h = 0
};


/**
 * @brief Clamp a dimension to the supported client geometry bounds
 *
 * Ensures that a requested window dimension stays within the minimum
 * supported size and the maximum value representable by @c uint16_t.
 *
 * @param value Dimension value to clamp
 *
 * @return Clamped dimension value
 *
 * @note Complexity: @e O(1)
 */
static uint16_t s_wm_clamp_dimension(int32_t value)
{
    if (value < (int32_t) WM_MIN_WINDOW_DIMENSION) {
        return WM_MIN_WINDOW_DIMENSION;
    }
    if (value > (int32_t) UINT16_MAX) {
        return UINT16_MAX;
    }
    return (uint16_t) value;
}


/**
 * @brief Determine whether the loaded focus policy follows the pointer
 *
 * @return @c true when focus should follow mouse enter events
 *
 * @note Complexity: @e O(1)
 */
static bool s_wm_is_focus_follows_mouse_policy(void)
{
    if (wm == NULL || wm->config == NULL) {
        return false;
    }

    return wm->config->base.windows.focus_policy ==
        CONFIG_FOCUS_POLICY_FOLLOW_MOUSE;
}


/* Key-string parsing helpers */
/**
 * @brief Resolve configured modifier aliases such as @c modc or @c mods
 *
 * Expands symbolic modifier aliases from the configuration into their
 * actual configured string values.  If the token does not match a known
 * alias, the original token is returned unchanged.
 *
 * @param tok Modifier token to resolve
 *
 * @return Resolved modifier token, or the original token if no alias
 *         matches
 *
 * @note Complexity: @e O(1)
 */
static const char *s_resolve_modifier_token(const char *tok)
{
    if (tok == NULL) {
        return tok;
    }

    if (strcasecmp(tok, "modc") == 0) {
        return wm->config->bindings.modc;
    }
    if (strcasecmp(tok, "mods") == 0) {
        return wm->config->bindings.mods;
    }
    if (strcasecmp(tok, "modl") == 0) {
        return wm->config->bindings.modl;
    }
    if (strcasecmp(tok, "mod1") == 0) {
        return wm->config->bindings.mod1;
    }
    if (strcasecmp(tok, "mod2") == 0) {
        return wm->config->bindings.mod2;
    }
    if (strcasecmp(tok, "mod3") == 0) {
        return wm->config->bindings.mod3;
    }
    if (strcasecmp(tok, "mod4") == 0) {
        return wm->config->bindings.mod4;
    }
    if (strcasecmp(tok, "mod5") == 0) {
        return wm->config->bindings.mod5;
    }

    return tok;
}


/**
 * @brief Map a single modifier token to an XCB modifier mask
 *
 * Converts a textual modifier name into the corresponding XCB modifier
 * mask.  Supports configured aliases, common modifier names, and
 * some alternative spellings.
 *
 * @param tok Modifier token to parse
 *
 * @return Matching XCB modifier mask, or 0 if the token is not
 *         recognized
 *
 * @note Complexity: @e O(1)
 */
static uint16_t s_parse_modifier_token(const char *tok)
{
    const char *resolved = s_resolve_modifier_token(tok);

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
 * Supports printable single-character keys, function keys, and a set of
 * common named keys such as arrows, navigation keys, and editing keys.
 *
 * @param tok Key token to parse
 *
 * @return Matching @e keysym, or @c XCB_NO_SYMBOL if the token is not
 *         recognized
 *
 * @note Complexity: @e O(1)
 */
static xcb_keysym_t s_parse_keysym_token(const char *tok)
{
    /* Single printable character */
    if (tok[1] == '\0') {
        char c = tok[0];
        if (c >= 'a' && c <= 'z') {
            return (xcb_keysym_t) c;
        }
        if (c >= 'A' && c <= 'Z') {
            return (xcb_keysym_t) (c + 32); /* keysym = lowercase */
        }
        if (c >= '0' && c <= '9') {
            return (xcb_keysym_t) c;
        }
    }

    /* Function keys F1-F12 */
    if ((tok[0] == 'F' || tok[0] == 'f') &&
            tok[1] >= '1' && tok[1] <= '9') {
        char *end = NULL;
        long n = strtol(tok + 1, &end, 10);
        if (end != NULL && *end == '\0' && n >= 1 && n <= 12) {
            return (xcb_keysym_t) (0xffbdu + (unsigned long) n);
        }
    }

    /* Named keys */
    if (strcasecmp(tok, "return")    == 0 ||
            strcasecmp(tok, "enter") == 0) { return 0xff0du; }
    if (strcasecmp(tok, "space")     == 0) { return 0x0020u; }
    if (strcasecmp(tok, "tab")       == 0) { return 0xff09u; }
    if (strcasecmp(tok, "escape")    == 0 ||
            strcasecmp(tok, "esc")   == 0) { return 0xff1bu; }
    if (strcasecmp(tok, "backspace") == 0) { return 0xff08u; }
    if (strcasecmp(tok, "delete")    == 0 ||
            strcasecmp(tok, "del")   == 0) { return 0xffffu; }
    if (strcasecmp(tok, "left")      == 0) { return 0xff51u; }
    if (strcasecmp(tok, "up")        == 0) { return 0xff52u; }
    if (strcasecmp(tok, "right")     == 0) { return 0xff53u; }
    if (strcasecmp(tok, "down")      == 0) { return 0xff54u; }
    if (strcasecmp(tok, "home")      == 0) { return 0xff50u; }
    if (strcasecmp(tok, "end")       == 0) { return 0xff57u; }
    if (strcasecmp(tok, "pageup")    == 0 ||
            strcasecmp(tok, "prior") == 0) { return 0xff55u; }
    if (strcasecmp(tok, "pagedown")  == 0 ||
            strcasecmp(tok, "next")  == 0) { return 0xff56u; }

    return XCB_NO_SYMBOL;
}


/**
 * @brief Parse a binding string (such as @c Mod1+Shift+F9)
 *
 * Splits on `+` and classifies each token as a modifier or the key
 * (last token).
 *
 * @param[in]  binding  Binding string from configuration
 * @param[out] modmask  Receives the combined modifier mask
 * @param[out] keysym   Receives the main keysym
 *
 * @return @c true if the binding could be parsed, @c false otherwise
 */
static bool s_parse_binding(const char *binding,
        uint16_t *modmask, xcb_keysym_t *keysym)
{
    char buf[128];
    char *tok;
    char *save;
    char *prev_tok = NULL;
    size_t len;

    if (binding == NULL || binding[0] == '\0') {
        return false;
    }

    /* Safe copy into a local buffer */
    len = safe_strlen(binding);
    if (len >= sizeof(buf)) {
        len = sizeof(buf) - 1;
    }
    memcpy(buf, binding, len);
    buf[len] = '\0';

    *modmask = 0;
    *keysym  = XCB_NO_SYMBOL;

    tok = strtok_r(buf, "+", &save);
    while (tok != NULL) {
        if (prev_tok != NULL) {
            /* Previous token was a modifier */
            uint16_t mod = s_parse_modifier_token(prev_tok);
            if (mod != 0) {
                *modmask |= mod;
            }
        }
        prev_tok = tok;
        tok = strtok_r(NULL, "+", &save);
    }

    /* The last token is the key */
    if (prev_tok != NULL) {
        *keysym = s_parse_keysym_token(prev_tok);
    }

    return *keysym != XCB_NO_SYMBOL;
}


/**
 * @brief Parse a mouse button token such as "button1" into an XCB
 *        button index
 *
 * @param[in] tok Button token from configuration (e.g. "button1"
 *                 through "button5")
 *
 * @return The parsed button index, or @c 0 if @p tok could not be
 *         parsed as a valid button token
 */
static xcb_button_index_t s_parse_button_token(const char *tok)
{
    long n;
    char *end = NULL;

    if (tok == NULL || strncasecmp(tok, "button", 6) != 0) {
        return 0;
    }

    n = strtol(tok + 6, &end, 10);
    if (end == NULL || *end != '\0' || tok[6] == '\0' ||
            n < 1 || n > 5) {
        return 0;
    }

    return (xcb_button_index_t) n;
}


/* Window manager helper functions */
/**
 * @brief Find the surface whose root window matches @p root
 *
 * @param root Root window ID to search for
 *
 * @return Pointer to the matching surface, or @c NULL if not found
 */
static surface_td *s_wm_get_surface_for_root(xcb_window_t root)
{
    for (list_item_td *node = list_head(wm->surfaces);
            node != NULL; node = list_next(node)) {
        surface_td *surface = (surface_td *) list_data(node);
        if (surface != NULL && surface->screen != NULL &&
                surface->screen->root == root) {
            return surface;
        }
    }

    return NULL;
}


/**
 * @brief Return the currently active desktop for a surface
 *
 * @param surface Pointer to the surface
 *
 * @return Pointer to the current desktop, or @c NULL on error
 */
static desktop_td *s_wm_get_current_desktop(surface_td *surface)
{
    if (surface == NULL) {
        return NULL;
    }

    return surface_desktop_get(surface, surface->desktop_cur);
}


/**
 * @brief Test whether an X window belongs to a managed client
 *
 * Checks whether the specified window matches any of the windows
 * associated with the client, including the client window, frame,
 * titlebar, icon window, or the client identifier itself.
 *
 * @param client Pointer to the client to test
 * @param window Window ID to compare against the client
 *
 * @return @c true if the window belongs to the client, or @c false
 *         otherwise
 *
 * @note Complexity: @e O(1)
 */
static bool s_wm_client_matches_window(const client_td *client,
        xcb_window_t window)
{
    if (client == NULL || window == XCB_WINDOW_NONE) {
        return false;
    }

    return client->id == window ||
           client->window == window ||
           client->frame == window ||
           client->titlebar == window ||
           client->icon_window == window;
}


/**
 * @brief Destroy the currently visible info popup window
 *
 * Closes the info popup window if it exists and resets the cached popup
 * window identifier.
 *
 * @note Complexity: @e O(1)
 */
static void s_wm_close_info_popup(void)
{
    if (wm == NULL || wm->connection == NULL ||
            s_info_popup_window == XCB_WINDOW_NONE) {
        return;
    }

    xcb_destroy_window(wm->connection, s_info_popup_window);
    s_info_popup_window = XCB_WINDOW_NONE;
}


/**
 * @brief Search all surfaces and desktops for a client by window ID
 *
 * @param window      X window ID to search for
 * @param out_surface If non-null, receives the owning surface pointer
 * @param out_desktop If non-null, receives the owning desktop pointer
 *
 * @return Pointer to the client, or @c NULL if not found
 */
static client_td *s_wm_find_client(xcb_window_t window,
        surface_td **out_surface, desktop_td **out_desktop)
{
    if (out_surface != NULL) {
        *out_surface = NULL;
    }
    if (out_desktop != NULL) {
        *out_desktop = NULL;
    }

    for (list_item_td *snode = list_head(wm->surfaces);
            snode != NULL; snode = list_next(snode)) {
        surface_td *surface = (surface_td *) list_data(snode);
        cdlist_item_td *dnode;
        cdlist_item_td *dinitial;
        if (surface == NULL || surface->desktops == NULL ||
                cdlist_size(surface->desktops) == 0) {
            continue;
        }

        dnode = cdlist_head(surface->desktops);
        dinitial = dnode;
        if (dnode == NULL) {
            continue;
        }

        do {
            desktop_td *desktop =
                (desktop_td *) cdlist_data(dnode);
            if (desktop != NULL && desktop->clients != NULL) {
                for (size_t i = 0;
                        i < desktop->clients->positions;
                        ++i) {
                    client_td *client;
                    if (desktop->clients->table[i] == NULL ||
                            desktop->clients->table[i] ==
                                desktop->clients->vacated) {
                        continue;
                    }
                    client = (client_td *) desktop->clients->table[i];
                    if (!s_wm_client_matches_window(client, window)) {
                        continue;
                    }

                    if (out_surface != NULL) {
                        *out_surface = surface;
                    }
                    if (out_desktop != NULL) {
                        *out_desktop = desktop;
                    }
                    return client;
                }
            }
            dnode = cdlist_next(dnode);
        } while (dnode != NULL && dnode != dinitial);
    }

    return NULL;
}


/**
 * @brief Select the next or previous focusable visible client
 *
 * Traverses the desktop stacking list starting from the currently
 * active client and returns the next candidate according to the
 * requested cycling direction.
 *
 * @param desktop Pointer to the desktop where cycling is performed
 * @param is_next When @c true, cycle to next; when @c false, cycle to
 *                previous
 *
 * @return Pointer to the selected client, or @c NULL if no suitable
 *         client exists
 *
 * @note Complexity: @e O(n), where @e n is the number of clients in the
 *       desktop stacking list
 */
static client_td *s_wm_cycle_target_client(desktop_td *desktop,
        bool is_next)
{
    cdlist_item_td *node;
    cdlist_item_td *initial;
    cdlist_item_td *active_node = NULL;

    if (desktop == NULL || desktop->stacking == NULL ||
            cdlist_size(desktop->stacking) == 0) {
        return NULL;
    }

    node = cdlist_head(desktop->stacking);
    if (node == NULL) {
        return NULL;
    }

    initial = node;
    do {
        client_td *client = (client_td *) cdlist_data(node);
        if (client != NULL && client->id == desktop->client_active_id) {
            active_node = node;
            break;
        }
        node = cdlist_next(node);
    } while (node != NULL && node != initial);

    if (active_node != NULL) {
        node = (is_next)
            ? cdlist_prev(active_node)
            : cdlist_next(active_node);
    } else {
        node = (is_next)
            ? cdlist_tail(desktop->stacking)
            : cdlist_head(desktop->stacking);
    }

    if (node == NULL) {
        return NULL;
    }

    initial = node;
    do {
        client_td *client = (client_td *) cdlist_data(node);
        if (client != NULL &&
                !client_is_iconified(client) &&
                client_is_focusable(client)) {
            return client;
        }
        node = (is_next) ? cdlist_prev(node) : cdlist_next(node);
    } while (node != NULL && node != initial);

    return NULL;
}


/**
 * @brief Select the next or previous iconified client
 *
 * Traverses the desktop stacking list and returns the next iconified
 * client according to the requested cycling direction.  When found, the
 * client is restored from iconification.
 *
 * @param desktop Pointer to the desktop where cycling is performed
 * @param is_next When @c true, cycle to next; when @c false, cycle to
 *                previous
 *
 * @return Pointer to the selected (and now restored) client, or @c NULL
 *         if no iconified client exists
 *
 * @note Complexity: @e O(n), where @e n is the number of clients in the
 *       desktop stacking list
 */
static client_td *s_wm_cycle_icon_client(desktop_td *desktop,
        bool is_next)
{
    cdlist_item_td *node;
    cdlist_item_td *initial;
    cdlist_item_td *active_node = NULL;

    if (desktop == NULL || desktop->stacking == NULL ||
            cdlist_size(desktop->stacking) == 0) {
        return NULL;
    }

    node = cdlist_head(desktop->stacking);
    if (node == NULL) {
        return NULL;
    }

    initial = node;
    do {
        client_td *client = (client_td *) cdlist_data(node);
        if (client != NULL && client->id == desktop->client_active_id) {
            active_node = node;
            break;
        }
        node = cdlist_next(node);
    } while (node != NULL && node != initial);

    if (active_node != NULL) {
        node = (is_next)
            ? cdlist_prev(active_node)
            : cdlist_next(active_node);
    } else {
        node = (is_next)
            ? cdlist_tail(desktop->stacking)
            : cdlist_head(desktop->stacking);
    }

    if (node == NULL) {
        return NULL;
    }

    initial = node;
    do {
        client_td *client = (client_td *) cdlist_data(node);
        if (client != NULL &&
                client_is_iconified(client) &&
                client_is_focusable(client)) {
            client_send_event_restore(client);
            return client;
        }
        node = is_next ? cdlist_prev(node) : cdlist_next(node);
    } while (node != NULL && node != initial);

    return NULL;
}


/**
 * @brief Focus a client and keep focus-related state in sync
 *
 * Updates the active client for the desktop, sends focus and unfocus
 * events as needed, and optionally raises the client to the front.
 *
 * @param surface Pointer to the surface containing the client
 * @param desktop Pointer to the desktop tracking the active client
 * @param client  Pointer to the client to focus
 * @param raise   Whether the client should be raised immediately
 *
 * @note The previous active client is unfocused before the new client
 *       is marked as active
 * @note Complexity: @e O(1) for focus bookkeeping; up to @e O(n * m)
 *       when immediate surface redraw is triggered after raising, where
 *       @e n is the number of desktops and @e m the number of clients
 *       rendered on the target surface
 */
static void s_wm_focus_client(surface_td *surface, desktop_td *desktop,
        client_td *client, bool raise)
{
    client_td *previous = NULL;
    surface_td *ps = NULL;
    desktop_td *pd = NULL;
    bool should_raise;

    if (surface == NULL || desktop == NULL || client == NULL) {
        return;
    }

    if (desktop->client_active_id != 0 &&
            desktop->client_active_id != client->id) {
        previous = s_wm_find_client(desktop->client_active_id, &ps, &pd);
        if (previous != NULL) {
            (void) client_send_event_unfocus(previous);
        }
    }

    desktop->client_active_id = client->id;
    (void) client_send_event_focus(client);

    /* Mark the surface and desktop as outdated so the next update cycle
     * repaints the titlebars of both the newly focused and the
     * previously focused clients with the correct active/inactive theme
     * colors.  Without this, the 'focus_in' handler skips the update
     * because it sees the active ID already set. */
    desktop->is_outdated = true;
    if (surface != NULL) {
        surface->is_outdated = true;
    }

    should_raise =
        (raise || wm->config->base.windows.focus.is_raised_on_focus);
    if (should_raise) {
        (void) desktop_action_client_send_front(desktop, client);
        (void) client_send_event_raise(client);
    }

    /* Keep focus-related repaint synchronized with the state change so
     * titlebars and themed borders do not update one event later when
     * focus changes via mouse hover, click, or keyboard cycling. */
    if (surface != NULL && surface->is_outdated) {
        if (surface_render_all_desktops(surface) != 0) {
            LOGGER_ERROR("Failed to refresh surface %u after focus" \
                    " switch", surface->id);
        }
    }

    (void) surface;
}


/**
 * @brief Show a small centered popup with focused-client information
 *
 * Creates a popup window centered on the current surface and displays
 * basic information about the focused client, including its name,
 * class, instance, window identifiers, geometry, and state flags.
 *
 * @param surface Pointer to the surface where the popup should be shown
 * @param desktop Pointer to the desktop associated with the client
 * @param client  Pointer to the client to describe in the popup
 *
 * @note Any previously visible popup is closed before the new one is
 *       shown
 *
 * @note Complexity: @e O(1)
 */
static void s_wm_show_client_info(surface_td *surface,
        desktop_td *desktop, client_td *client)
{
    const char *name;
    const char *class_name;
    const char *instance_name;
    const int16_t width = 520;
    const int16_t height = 96;
    int16_t x;
    int16_t y;
    uint32_t mask;
    uint32_t values[3];

    if (surface == NULL || desktop == NULL || client == NULL ||
            surface->screen == NULL) {
        return;
    }
    name = (client->info.name != NULL) ? client->info.name : "";
    class_name = (client->info.class_name[1] != NULL)
        ? client->info.class_name[1] : "";
    instance_name = (client->info.class_name[0] != NULL)
        ? client->info.class_name[0] : "";

    s_wm_close_info_popup();

    x = (int16_t) (((int32_t) surface->properties.dim.w - width) / 2);
    y = (int16_t) (((int32_t) surface->properties.dim.h - height) / 2);
    if (x < 0) {
        x = 0;
    }
    if (y < 0) {
        y = 0;
    }

    s_info_popup_window = xcb_generate_id(wm->connection);
    mask = XCB_CW_BACK_PIXEL | XCB_CW_BORDER_PIXEL | XCB_CW_EVENT_MASK;
    values[0] = wm->config->theme.window.active.background_color;
    values[1] = wm->config->theme.window.active.border_color;
    values[2] = XCB_EVENT_MASK_EXPOSURE | XCB_EVENT_MASK_BUTTON_PRESS |
                XCB_EVENT_MASK_KEY_PRESS;
    xcb_create_window(wm->connection,
            XCB_COPY_FROM_PARENT,
            s_info_popup_window,
            surface->screen->root,
            x, y,
            (uint16_t) width, (uint16_t) height,
            1,
            XCB_WINDOW_CLASS_INPUT_OUTPUT,
            XCB_COPY_FROM_PARENT,
            mask, values);

    snprintf(s_info_popup_lines[0], sizeof(s_info_popup_lines[0]),
            "name=%s class=%s instance=%s",
            name, class_name, instance_name);
    snprintf(s_info_popup_lines[1], sizeof(s_info_popup_lines[1]),
            "window=%#x frame=%#x desktop=%u surface=%u",
            client->window, client->frame, desktop->id, surface->id);
    snprintf(s_info_popup_lines[2], sizeof(s_info_popup_lines[2]),
            "geom=%ux%u+%d+%d",
            client->layout.geometry.cur.dim.w,
            client->layout.geometry.cur.dim.h,
            client->layout.geometry.cur.pos.x,
            client->layout.geometry.cur.pos.y);
    snprintf(s_info_popup_lines[3], sizeof(s_info_popup_lines[3]),
            "flags=%#x state=%#x",
            client->properties.flags, client->properties.state);

    xcb_map_window(wm->connection, s_info_popup_window);
    xcb_flush(wm->connection);
}


/**
 * @brief Send a desktop event to launch a command
 *
 * @param desktop Target desktop
 * @param command Command line to execute
 *
 * @return 0 on success, non-zero on error
 */
static int s_wm_send_desktop_launch_event(desktop_td *desktop,
        const char *command)
{
    action_td action;
    action_data_desktop_td *data;
    event_td *event;

    if (desktop == NULL || command == NULL || command[0] == '\0') {
        LOGGER_WARNING("Cannot launch: command is null or empty", L_NARG);
        return -1;
    }

    action.type = ACTION_TYPE_DESKTOP;
    action.object.desktop = ACTION_DESKTOP_COMMAND_LAUNCH;

    data = action_data_desktop_init(desktop, action.object.desktop);
    if (data == NULL) {
        LOGGER_ERROR("Failed to allocate desktop action data", L_NARG);
        return 1;
    }
    /* Command pointer originates from persistent window manager
     * configuration data and remains valid for the event queue
     * lifecycle */
    data->new_data.str = (char *) command;

    event = event_init((void *) desktop, (void *) data,
            action, PRIORITY_NORMAL);
    if (event == NULL) {
        LOGGER_ERROR("Failed to create desktop launch event", L_NARG);
        action_data_desktop_destroy(data);
        return 1;
    }

    if (eventq_add(event) != 0) {
        LOGGER_ERROR("Failed to queue desktop launch event", L_NARG);
        event_destroy(event);
        return 1;
    }

    return 0;
}


/**
 * @brief Subscribe to SubstructureRedirect and related events on each
 *        root window
 *
 * @return 0 on success, -1 on error
 *
 * @note Fails with a fatal log if another window manager is already
 *       running (@c BadAccess error)
 */
static int s_wm_subscribe_root_events(void)
{
    uint32_t values[1];

    values[0] = XCB_EVENT_MASK_SUBSTRUCTURE_REDIRECT |
                XCB_EVENT_MASK_SUBSTRUCTURE_NOTIFY   |
                XCB_EVENT_MASK_KEY_PRESS             |
                XCB_EVENT_MASK_BUTTON_PRESS          |
                XCB_EVENT_MASK_BUTTON_RELEASE        |
                XCB_EVENT_MASK_PROPERTY_CHANGE;

    for (list_item_td *node = list_head(wm->surfaces);
            node != NULL; node = list_next(node)) {
        surface_td *surface = (surface_td *) list_data(node);
        xcb_void_cookie_t cookie;
        xcb_generic_error_t *err;

        if (surface == NULL || surface->screen == NULL) {
            continue;
        }

        cookie = xcb_change_window_attributes_checked(
                wm->connection, surface->screen->root,
                XCB_CW_EVENT_MASK, values);
        err = xcb_request_check(wm->connection, cookie);
        if (err != NULL) {
            LOGGER_FATAL("Cannot subscribe to root events on" \
                    " surface %u; another window manager may be" \
                    " running (XCB error code %d)",
                    surface->id, err->error_code);
            free(err);
            return -1;
        }

        LOGGER_DEBUG("Subscribed to root events on surface %u" \
                " (root %#x)", surface->id, surface->screen->root);
    }

    xcb_flush(wm->connection);
    return 0;
}


/**
 * @brief Register key grabs for the emergency exit and every
 *        configured key binding
 *
 * @param keysyms Allocated key-symbols table
 */
static void s_wm_grab_keys(xcb_key_symbols_t *keysyms)
{
    /* Binding strings paired with their action type */
    struct {
        const char *binding;
        enum wm_keybind_type_e type;
    } defs[] = {
        { wm->config->bindings.keyboard.terminal,
          KEYBIND_LAUNCH_TERMINAL },
        { wm->config->bindings.keyboard.launcher,
          KEYBIND_LAUNCH_LAUNCHER },
        { wm->config->bindings.keyboard.file_manager,
          KEYBIND_LAUNCH_FILE_MANAGER },
        { wm->config->bindings.keyboard.web_browser,
          KEYBIND_LAUNCH_WEB_BROWSER },
        { wm->config->bindings.keyboard.editor,
          KEYBIND_LAUNCH_EDITOR },
        { wm->config->bindings.keyboard.iconify,
          KEYBIND_CLIENT_ICONIFY },
        { wm->config->bindings.keyboard.hide,
          KEYBIND_CLIENT_HIDE },
        { wm->config->bindings.keyboard.close,
          KEYBIND_CLIENT_CLOSE },
        { wm->config->bindings.keyboard.kill,
          KEYBIND_CLIENT_KILL },
        { wm->config->bindings.keyboard.maximize,
          KEYBIND_CLIENT_MAXIMIZE },
        { wm->config->bindings.keyboard.center,
          KEYBIND_CLIENT_CENTER },
        { wm->config->bindings.keyboard.shade,
          KEYBIND_CLIENT_SHADE },
        { wm->config->bindings.keyboard.fullscreen,
          KEYBIND_CLIENT_FULLSCREEN },
        { wm->config->bindings.keyboard.pin,
          KEYBIND_CLIENT_PIN },
        { wm->config->bindings.keyboard.info,
          KEYBIND_CLIENT_INFO },
        { wm->config->bindings.keyboard.cycle_prev,
          KEYBIND_CLIENT_CYCLE_PREV },
        { wm->config->bindings.keyboard.cycle_next,
          KEYBIND_CLIENT_CYCLE_NEXT },
        { wm->config->bindings.keyboard.toggle_decoration,
          KEYBIND_CLIENT_TOGGLE_DECORATION },
        { wm->config->bindings.keyboard.desktop.cycle_prev,
          KEYBIND_DESKTOP_PREV },
        { wm->config->bindings.keyboard.desktop.cycle_next,
          KEYBIND_DESKTOP_NEXT },
        { wm->config->bindings.keyboard.desktop.cycle_icon_prev,
          KEYBIND_DESKTOP_ICON_PREV },
        { wm->config->bindings.keyboard.desktop.cycle_icon_next,
          KEYBIND_DESKTOP_ICON_NEXT },
        { wm->config->bindings.keyboard.move.relative.left,
          KEYBIND_CLIENT_MOVE_LEFT },
        { wm->config->bindings.keyboard.move.relative.right,
          KEYBIND_CLIENT_MOVE_RIGHT },
        { wm->config->bindings.keyboard.move.relative.up,
          KEYBIND_CLIENT_MOVE_UP },
        { wm->config->bindings.keyboard.move.relative.down,
          KEYBIND_CLIENT_MOVE_DOWN },
        { wm->config->bindings.keyboard.move.absolute.top_left,
          KEYBIND_CLIENT_MOVE_TOP_LEFT },
        { wm->config->bindings.keyboard.move.absolute.top_right,
          KEYBIND_CLIENT_MOVE_TOP_RIGHT },
        { wm->config->bindings.keyboard.move.absolute.bottom_left,
          KEYBIND_CLIENT_MOVE_BOTTOM_LEFT },
        { wm->config->bindings.keyboard.move.absolute.bottom_right,
          KEYBIND_CLIENT_MOVE_BOTTOM_RIGHT },
        { wm->config->bindings.keyboard.resize.left,
          KEYBIND_CLIENT_RESIZE_LEFT },
        { wm->config->bindings.keyboard.resize.right,
          KEYBIND_CLIENT_RESIZE_RIGHT },
        { wm->config->bindings.keyboard.resize.up,
          KEYBIND_CLIENT_RESIZE_UP },
        { wm->config->bindings.keyboard.resize.down,
          KEYBIND_CLIENT_RESIZE_DOWN },
        /* Hardcoded emergency exit */
        { "Ctrl+Mod1+BackSpace", KEYBIND_NONE },
        { NULL, KEYBIND_NONE }
    };

    /* Lock-modifier variants: passive grabs match the modifier mask
     * exactly, so 'Caps_Lock' ('Lock') and/or 'Num_Lock' ('Mod2') being
     * active would otherwise stop the grab from firing */
    static const uint16_t lockmods[] = {
        0,
        XCB_MOD_MASK_LOCK,
        XCB_MOD_MASK_2,
        XCB_MOD_MASK_LOCK | XCB_MOD_MASK_2
    };

    s_keybindings_count = 0;

    for (int i = 0; defs[i].binding != NULL; ++i) {
        xcb_keysym_t  keysym;
        uint16_t      modmask;
        xcb_keycode_t *keycodes;

        if (!s_parse_binding(defs[i].binding, &modmask, &keysym)) {
            continue;
        }

        keycodes = xcb_key_symbols_get_keycode(keysyms, keysym);
        if (keycodes == NULL) {
            continue;
        }

        /* Store in the binding table (avoid overflow) */
        if (s_keybindings_count < WM_MAX_KEYBINDINGS) {
            s_keybindings[s_keybindings_count].keysym  = keysym;
            s_keybindings[s_keybindings_count].modmask = modmask;
            s_keybindings[s_keybindings_count].type    = defs[i].type;
            s_keybindings_count++;
        }

        /* Grab on all root windows */
        for (list_item_td *node = list_head(wm->surfaces);
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
                    ck = xcb_grab_key_checked(wm->connection,
                            1,   /* owner_events */
                            surface->screen->root,
                            (uint16_t) (modmask | lockmods[k]),
                            keycodes[j],
                            XCB_GRAB_MODE_ASYNC,
                            XCB_GRAB_MODE_ASYNC);

                    err = xcb_request_check(wm->connection, ck);
                    if (err != NULL) {
                        LOGGER_WARNING("xcb_grab_key failed for "
                                "keycode=%u modmask=0x%x error=%d",
                                keycodes[j],
                                (unsigned) (modmask | lockmods[k]),
                                err->error_code);
                        free(err);
                    }
                } /* ! for (k) */
            } /* ! for (j) */
        }

        free(keycodes);
    }

    xcb_flush(wm->connection);
    LOGGER_DEBUG("Grabbed %d key binding(s)", s_keybindings_count);
}


/**
 * @brief Register mouse button grabs for move/resize interactions
 */
static void s_wm_grab_buttons(void)
{
    /* Binding strings paired with their action type, read from
     * configuration instead of hardcoding specific button numbers.
     * All mouse actions are gated behind the same 'mod1' (Alt)
     * modifier as the rest of the mouse section, for consistency
     * with the existing move/resize grabs. */
    struct {
        const char *binding;
        enum wm_mousebind_type_e type;
        bool requires_mod1;
    } defs[] = {
        { wm->config->bindings.mouse.move, MOUSEBIND_MOVE, true },
        { wm->config->bindings.mouse.resize, MOUSEBIND_RESIZE, true },
        { wm->config->bindings.mouse.lower, MOUSEBIND_LOWER, true },
        { wm->config->bindings.mouse.desktop.cycle_prev,
            MOUSEBIND_DESKTOP_PREV, false },
        { wm->config->bindings.mouse.desktop.cycle_next,
            MOUSEBIND_DESKTOP_NEXT, false },
        { NULL, MOUSEBIND_NONE, false }
    };

    /* Lock-modifier variants: passive grabs match the modifier mask
     * exactly, so 'Caps_Lock' ('Lock') and/or 'Num_Lock' ('Mod2') being
     * active would otherwise stop the grab from firing */
    static const uint16_t lockmods[] = {
        0,
        XCB_MOD_MASK_LOCK,
        XCB_MOD_MASK_2,
        XCB_MOD_MASK_LOCK | XCB_MOD_MASK_2
    };

    s_mousebindings_count = 0;

    for (int i = 0; defs[i].binding != NULL; ++i) {
        xcb_button_index_t button = s_parse_button_token(defs[i].binding);

        if (button == 0) {
            LOGGER_WARNING("Ignoring unparseable mouse binding '%s'",
                    defs[i].binding);
            continue;
        }

        if (s_mousebindings_count < WM_MAX_MOUSEBINDINGS) {
            s_mousebindings[s_mousebindings_count].button = button;
            s_mousebindings[s_mousebindings_count].type   = defs[i].type;
            s_mousebindings_count++;
        }

        for (list_item_td *node = list_head(wm->surfaces);
                node != NULL; node = list_next(node)) {
            surface_td *surface = (surface_td *) list_data(node);
            if (surface == NULL || surface->screen == NULL) {
                continue;
            }

            for (size_t k = 0;
                    k < sizeof(lockmods) / sizeof(lockmods[0]);
                    ++k) {
                xcb_grab_button(wm->connection,
                        0,  /* owner_events */
                        surface->screen->root,
                        XCB_EVENT_MASK_BUTTON_PRESS |
                        XCB_EVENT_MASK_BUTTON_RELEASE |
                        XCB_EVENT_MASK_POINTER_MOTION,
                        XCB_GRAB_MODE_ASYNC,
                        XCB_GRAB_MODE_ASYNC,
                        XCB_NONE,
                        XCB_NONE,
                        (uint8_t) button,
                        (uint16_t) (((defs[i].requires_mod1)
                                ? XCB_MOD_MASK_1
                                : 0u) | lockmods[k]));
            }
        }
    }

    xcb_flush(wm->connection);
    LOGGER_DEBUG("Grabbed %d mouse binding(s)", s_mousebindings_count);
}


/**
 * @brief Adopt all pre-existing mapped windows at window manager startup
 *
 * Queries the window tree for each screen and calls
 * @c client_manage on any already-mapped, non-override-redirect child.
 */
static void s_wm_scan_existing_windows(void)
{
    for (list_item_td *node = list_head(wm->surfaces);
            node != NULL; node = list_next(node)) {
        surface_td *surface = (surface_td *) list_data(node);
        xcb_query_tree_cookie_t qt_cookie;
        xcb_query_tree_reply_t *qt_reply;
        xcb_window_t *children;
        int nchildren;

        if (surface == NULL || surface->screen == NULL) {
            continue;
        }

        qt_cookie = xcb_query_tree(wm->connection,
                surface->screen->root);
        qt_reply  = xcb_query_tree_reply(wm->connection,
                qt_cookie, NULL);
        if (qt_reply == NULL) {
            continue;
        }

        children  = xcb_query_tree_children(qt_reply);
        nchildren = xcb_query_tree_children_length(qt_reply);

        for (int i = 0; i < nchildren; ++i) {
            xcb_get_window_attributes_cookie_t ac =
                xcb_get_window_attributes(wm->connection, children[i]);
            xcb_get_window_attributes_reply_t *ar =
                xcb_get_window_attributes_reply(wm->connection, ac, NULL);

            if (ar == NULL) {
                continue;
            }

            if (!ar->override_redirect &&
                    ar->map_state == XCB_MAP_STATE_VIEWABLE) {
                desktop_td *desktop = s_wm_get_current_desktop(surface);
                if (desktop != NULL) {
                    client_td *client = client_manage(
                            wm->connection, wm->ewmh,
                            children[i], &wm->config->theme);
                    if (client != NULL) {
                        client->screen_id  = surface->id;
                        client->desktop_id = desktop->id;
                        desktop_action_client_add(desktop, client);
                        surface->is_outdated = true;
                    }
                }
            }

            free(ar);
        }

        free(qt_reply);
    }

    xcb_flush(wm->connection);
}


/**
 * @brief Update a managed client's name from the X server
 *
 * @param client Client whose @c WM_NAME should be re-read
 */
static void s_wm_refresh_client_name(client_td *client)
{
    xcb_get_property_cookie_t cookie;
    xcb_get_property_reply_t *reply;

    if (client == NULL) {
        return;
    }

    cookie = xcb_get_property(client->connection, 0, client->window,
            XCB_ATOM_WM_NAME, XCB_ATOM_STRING, 0, 255);
    reply  = xcb_get_property_reply(client->connection, cookie, NULL);

    if (reply != NULL && reply->value_len > 0) {
        size_t len = reply->value_len < 255u
            ? reply->value_len
            : 254u;
        char *value = (char *) xcb_get_property_value(reply);
        memcpy(client->info.name, value, len);
        memcpy(client->info.visible_name, value, len);
        client->info.name[len] = '\0';
        client->info.visible_name[len] = '\0';
    }

    if (reply != NULL) {
        free(reply);
    }
}


/**
 * @brief Handle key press events from the X server
 *
 * Processes keyboard input events by converting XCB keycodes to
 * keysyms and performing appropriate window manager actions based
 * on configured key bindings.
 *
 * @param keysyms Pointer to XCB key symbols structure
 * @param event   Pointer to the key press event
 *
 * @note Complexity: @e O(1)
 */
static void s_wm_handle_key_press(xcb_key_symbols_t *keysyms,
        xcb_key_press_event_t *event)
{
    xcb_keysym_t keysym;
    uint16_t state;
    surface_td *surface;

    if (keysyms == NULL || event == NULL) {
        LOGGER_ERROR("Received 'NULL' pointer in key press handler",
                L_NARG);
        return;
    }

    if (s_info_popup_window != XCB_WINDOW_NONE) {
        s_wm_close_info_popup();
    }

    /* Translate keycode to keysym using the key symbols table */
    keysym = xcb_key_symbols_get_keysym(keysyms, event->detail, 0);

    /* Strip locking modifiers ('Num_Lock=Mod2', 'Caps_Lock=Lock') so
     * comparisons against configured masks are clean */
    state = (uint16_t) ((unsigned int) event->state &
            ~((unsigned int) XCB_MOD_MASK_LOCK |
                (unsigned int) XCB_MOD_MASK_2));

    LOGGER_TRACE("Key press event: keysym=0x%x, state=0x%x",
            keysym, state);

    /* Hardcoded emergency exit: 'Ctrl+Mod1+Shift+BackSpace' (the
     * classic X11 'panic' combination; 'Shift' is intentionally not
     * required so it matches what users conventionally expect/try) */
    if (keysym == 0xff08 &&     // == XK_BackSpace &&
            (event->state & XCB_MOD_MASK_CONTROL) &&
            (event->state & XCB_MOD_MASK_1)) {
        LOGGER_NOTICE("Emergency exit key combination detected", L_NARG);
        (void) wm_request_stop();
        return;
    }

    /* Identify the surface that generated this event */
    surface = s_wm_get_surface_for_root(event->root);
    if (surface == NULL && !list_is_empty(wm->surfaces)) {
        surface = (surface_td *) list_data(list_head(wm->surfaces));
    }

    /* Iterate the binding table and dispatch on first match */
    for (int i = 0; i < s_keybindings_count; ++i) {
        uint16_t bind_state =
            (uint16_t) ((unsigned int) s_keybindings[i].modmask &
                    ~((unsigned int) XCB_MOD_MASK_LOCK |
                        (unsigned int) XCB_MOD_MASK_2));

        if (keysym != s_keybindings[i].keysym || state != bind_state) {
            continue;
        }

        switch (s_keybindings[i].type) {
            case KEYBIND_DESKTOP_NEXT:
                if (surface != NULL) {
                    event_td *ev;
                    action_td action;
                    action.type = ACTION_TYPE_SURFACE;
                    action.object.surface =
                        ACTION_SURFACE_DESKTOP_SWITCH_NEXT;
                    ev = event_init((void *) surface, NULL,
                            action, PRIORITY_NORMAL);
                    if (ev != NULL) {
                        eventq_add(ev);
                    }
                }
                return;

            case KEYBIND_DESKTOP_PREV:
                if (surface != NULL) {
                    event_td *ev;
                    action_td action;
                    action.type = ACTION_TYPE_SURFACE;
                    action.object.surface =
                        ACTION_SURFACE_DESKTOP_SWITCH_PREV;
                    ev = event_init((void *) surface, NULL,
                            action, PRIORITY_NORMAL);
                    if (ev != NULL) {
                        eventq_add(ev);
                    }
                }
                return;

            case KEYBIND_CLIENT_CYCLE_NEXT:
            case KEYBIND_CLIENT_CYCLE_PREV:
                if (surface != NULL) {
                    desktop_td *desktop =
                        s_wm_get_current_desktop(surface);
                    if (desktop != NULL) {
                        const bool is_next =
                            (s_keybindings[i].type ==
                                KEYBIND_CLIENT_CYCLE_NEXT);
                        client_td *target =
                            s_wm_cycle_target_client(desktop, is_next);
                        if (target != NULL) {
                            s_wm_focus_client(surface, desktop, target,
                                    true);
                        }
                    }
                }
                return;

            case KEYBIND_DESKTOP_ICON_NEXT:
            case KEYBIND_DESKTOP_ICON_PREV:
                if (surface != NULL) {
                    desktop_td *desktop =
                        s_wm_get_current_desktop(surface);
                    if (desktop != NULL) {
                        const bool is_next =
                            (s_keybindings[i].type ==
                                KEYBIND_DESKTOP_ICON_NEXT);
                        client_td *target =
                            s_wm_cycle_icon_client(desktop, is_next);
                        if (target != NULL) {
                            s_wm_focus_client(surface, desktop, target,
                                    true);
                        }
                    }
                }
                return;

            case KEYBIND_CLIENT_ICONIFY:
            case KEYBIND_CLIENT_HIDE:
            case KEYBIND_CLIENT_CLOSE:
            case KEYBIND_CLIENT_KILL:
            case KEYBIND_CLIENT_MAXIMIZE:
            case KEYBIND_CLIENT_CENTER:
            case KEYBIND_CLIENT_SHADE:
            case KEYBIND_CLIENT_FULLSCREEN:
            case KEYBIND_CLIENT_PIN:
            case KEYBIND_CLIENT_INFO:
            case KEYBIND_CLIENT_TOGGLE_DECORATION:
                /* Determine the focused/top client on current desktop */
                if (surface != NULL) {
                    desktop_td *desktop =
                        s_wm_get_current_desktop(surface);
                    if (desktop != NULL &&
                            desktop->client_active_id != 0) {
                        client_td *client = NULL;
                        surface_td *cs = NULL;
                        desktop_td *cd = NULL;
                        client = s_wm_find_client(
                                desktop->client_active_id, &cs, &cd);
                        if (client != NULL) {
                            enum action_client_e act =
                                ACTION_CLIENT_ICONIFY;

                            if (s_keybindings[i].type ==
                                    KEYBIND_CLIENT_INFO) {
                                s_wm_show_client_info(surface,
                                        desktop, client);
                                return;
                            }

                            if (s_keybindings[i].type ==
                                    KEYBIND_CLIENT_HIDE) {
                                act = ACTION_CLIENT_HIDE;
                            } else if (s_keybindings[i].type ==
                                    KEYBIND_CLIENT_CLOSE) {
                                act = ACTION_CLIENT_CLOSE;
                            } else if (s_keybindings[i].type ==
                                    KEYBIND_CLIENT_KILL) {
                                act = ACTION_CLIENT_KILL;
                            } else if (s_keybindings[i].type ==
                                    KEYBIND_CLIENT_MAXIMIZE) {
                                act = ACTION_CLIENT_MAXIMIZE;
                            } else if (s_keybindings[i].type ==
                                    KEYBIND_CLIENT_CENTER) {
                                act = ACTION_CLIENT_CENTER;
                            } else if (s_keybindings[i].type ==
                                    KEYBIND_CLIENT_SHADE) {
                                act = ACTION_CLIENT_TOGGLE_SHADE;
                            } else if (s_keybindings[i].type ==
                                    KEYBIND_CLIENT_FULLSCREEN) {
                                act = ACTION_CLIENT_TOGGLE_FULLSCREEN;
                            } else if (s_keybindings[i].type ==
                                    KEYBIND_CLIENT_PIN) {
                                act = ACTION_CLIENT_TOGGLE_STICKY;
                            } else if (s_keybindings[i].type ==
                                    KEYBIND_CLIENT_TOGGLE_DECORATION) {
                                act = ACTION_CLIENT_TOGGLE_DECORATION;
                            }

                            client_send_event(client, act,
                                    PRIORITY_NORMAL);
                        }
                    }
                }
                return;

            case KEYBIND_LAUNCH_TERMINAL:
                if (surface != NULL) {
                    desktop_td *desktop =
                        s_wm_get_current_desktop(surface);
                    if (desktop != NULL) {
                        (void) s_wm_send_desktop_launch_event(
                                desktop,
                                wm->config->base.programs.terminal);
                    }
                }
                return;

            case KEYBIND_LAUNCH_LAUNCHER:
                if (surface != NULL) {
                    desktop_td *desktop =
                        s_wm_get_current_desktop(surface);
                    if (desktop != NULL) {
                        (void) s_wm_send_desktop_launch_event(
                                desktop,
                                wm->config->base.programs.launcher);
                    }
                }
                return;


            case KEYBIND_LAUNCH_FILE_MANAGER:
                if (surface != NULL) {
                    desktop_td *desktop =
                        s_wm_get_current_desktop(surface);
                    if (desktop != NULL) {
                        (void) s_wm_send_desktop_launch_event(
                                desktop,
                                wm->config->base.programs.file_manager);
                    }
                }
                return;

            case KEYBIND_LAUNCH_WEB_BROWSER:
                if (surface != NULL) {
                    desktop_td *desktop =
                        s_wm_get_current_desktop(surface);
                    if (desktop != NULL) {
                        (void) s_wm_send_desktop_launch_event(
                                desktop,
                                wm->config->base.programs.web_browser);
                    }
                }
                return;

            case KEYBIND_LAUNCH_EDITOR:
                if (surface != NULL) {
                    desktop_td *desktop =
                        s_wm_get_current_desktop(surface);
                    if (desktop != NULL) {
                        (void) s_wm_send_desktop_launch_event(
                                desktop,
                                wm->config->base.programs.editor);
                    }
                }
                return;

            case KEYBIND_CLIENT_MOVE_LEFT:
            case KEYBIND_CLIENT_MOVE_RIGHT:
            case KEYBIND_CLIENT_MOVE_UP:
            case KEYBIND_CLIENT_MOVE_DOWN:
            case KEYBIND_CLIENT_MOVE_TOP_LEFT:
            case KEYBIND_CLIENT_MOVE_TOP_RIGHT:
            case KEYBIND_CLIENT_MOVE_BOTTOM_LEFT:
            case KEYBIND_CLIENT_MOVE_BOTTOM_RIGHT:
                /* Move the focused/top client on current desktop by
                 * a fixed step, or snap it to a screen corner */
                if (surface != NULL) {
                    desktop_td *desktop =
                        s_wm_get_current_desktop(surface);
                    if (desktop != NULL &&
                            desktop->client_active_id != 0) {
                        client_td *client = NULL;
                        surface_td *cs = NULL;
                        desktop_td *cd = NULL;
                        client = s_wm_find_client(
                                desktop->client_active_id, &cs, &cd);
                        if (client != NULL) {
                            enum wm_keybind_type_e type =
                                s_keybindings[i].type;
                            int32_t new_x =
                                client->layout.geometry.cur.pos.x;
                            int32_t new_y =
                                client->layout.geometry.cur.pos.y;
                            int32_t max_x = (cs != NULL)
                                ? (int32_t) cs->properties.dim.w -
                                    (int32_t) client->layout.
                                        geometry.cur.dim.w
                                : new_x;
                            int32_t max_y = (cs != NULL)
                                ? (int32_t) cs->properties.dim.h -
                                    (int32_t) client->layout.
                                        geometry.cur.dim.h
                                : new_y;

                            if (type == KEYBIND_CLIENT_MOVE_LEFT) {
                                new_x -= WM_KEYBOARD_MOVE_STEP;
                            } else if (type ==
                                    KEYBIND_CLIENT_MOVE_RIGHT) {
                                new_x += WM_KEYBOARD_MOVE_STEP;
                            } else if (type ==
                                    KEYBIND_CLIENT_MOVE_UP) {
                                new_y -= WM_KEYBOARD_MOVE_STEP;
                            } else if (type ==
                                    KEYBIND_CLIENT_MOVE_DOWN) {
                                new_y += WM_KEYBOARD_MOVE_STEP;
                            } else if (type ==
                                    KEYBIND_CLIENT_MOVE_TOP_LEFT) {
                                new_x = 0;
                                new_y = 0;
                            } else if (type ==
                                    KEYBIND_CLIENT_MOVE_TOP_RIGHT) {
                                new_x = max_x;
                                new_y = 0;
                            } else if (type ==
                                    KEYBIND_CLIENT_MOVE_BOTTOM_LEFT) {
                                new_x = 0;
                                new_y = max_y;
                            } else if (type ==
                                    KEYBIND_CLIENT_MOVE_BOTTOM_RIGHT) {
                                new_x = max_x;
                                new_y = max_y;
                            }

                            (void) client_send_event_move(client,
                                    new_x, new_y);
                        }
                    }
                }
                return;

            case KEYBIND_CLIENT_RESIZE_LEFT:
            case KEYBIND_CLIENT_RESIZE_RIGHT:
            case KEYBIND_CLIENT_RESIZE_UP:
            case KEYBIND_CLIENT_RESIZE_DOWN:
                /* Resize the focused/top client on current desktop
                 * by a fixed step */
                if (surface != NULL) {
                    desktop_td *desktop =
                        s_wm_get_current_desktop(surface);
                    if (desktop != NULL &&
                            desktop->client_active_id != 0) {
                        client_td *client = NULL;
                        surface_td *cs = NULL;
                        desktop_td *cd = NULL;
                        client = s_wm_find_client(
                                desktop->client_active_id, &cs, &cd);
                        if (client != NULL &&
                                client_is_resizable(client)) {
                            enum wm_keybind_type_e type =
                                s_keybindings[i].type;
                            int32_t new_w = (int32_t)
                                client->layout.geometry.cur.dim.w;
                            int32_t new_h = (int32_t)
                                client->layout.geometry.cur.dim.h;

                            if (type == KEYBIND_CLIENT_RESIZE_LEFT) {
                                new_w -= WM_KEYBOARD_RESIZE_STEP;
                            } else if (type ==
                                    KEYBIND_CLIENT_RESIZE_RIGHT) {
                                new_w += WM_KEYBOARD_RESIZE_STEP;
                            } else if (type ==
                                    KEYBIND_CLIENT_RESIZE_UP) {
                                new_h -= WM_KEYBOARD_RESIZE_STEP;
                            } else if (type ==
                                    KEYBIND_CLIENT_RESIZE_DOWN) {
                                new_h += WM_KEYBOARD_RESIZE_STEP;
                            }

                            (void) client_send_event_resize(client,
                                    s_wm_clamp_dimension(new_w),
                                    s_wm_clamp_dimension(new_h));
                        }
                    }
                }
                return;

            case KEYBIND_NONE:
                LOGGER_TRACE("Ignoring 'KEYBIND_NONE' entry", L_NARG);
                return;
        }
    }
}


/**
 * @brief Handle @c BUTTON_PRESS events for mouse-driven interactions
 *
 * Processes mouse-driven window manager actions triggered with @c Mod1,
 * including focusing clients, restoring iconified windows, cycling
 * desktops, lowering windows, and starting move or resize drags
 * according to the configured mouse bindings.
 *
 * @param event Pointer to the button press event
 *
 * @note This handler may walk up the window tree when @c event->child
 *       refers to a subwindow, so the actual managed client resolved
 *       for move or resize can differ from the original click target
 */
static void s_wm_handle_button_press(xcb_button_press_event_t *event)
{
    xcb_window_t window;
    client_td *client;
    desktop_td *desktop;
    surface_td *surface;
    uint16_t state;
    enum wm_mousebind_type_e type = MOUSEBIND_NONE;

    /* Used for sub-window ancestor walk */
    xcb_window_t w;
    xcb_query_tree_cookie_t qt_c;
    xcb_query_tree_reply_t *qt_r;
    xcb_window_t qt_parent;
    xcb_window_t qt_root;

    xcb_grab_pointer_cookie_t grab_c;
    xcb_grab_pointer_reply_t *grab_r;

    if (event == NULL) {
        return;
    }

    if (s_info_popup_window != XCB_WINDOW_NONE) {
        if (event->event == s_info_popup_window ||
                event->child == s_info_popup_window) {
            s_wm_close_info_popup();
            xcb_flush(wm->connection);
            return;
        }
        s_wm_close_info_popup();
    }

    window = (event->child != XCB_NONE) ? event->child : event->event;
    client = s_wm_find_client(window, NULL, &desktop);
    if (client != NULL && window == client->icon_window) {
        (void) client_send_event_restore(client);
        if (desktop != NULL) {
            surface = s_wm_get_surface_for_root(event->root);
            if (surface != NULL) {
                s_wm_focus_client(surface, desktop, client, true);
            }
        }

        /* Unfreeze the pointer; safe even when no passive grab is
         * active (generates a silent 'NoCurrentGrab', per spec) */
        xcb_allow_events(wm->connection, XCB_ALLOW_ASYNC_POINTER,
                event->time);
        xcb_flush(wm->connection);
        return;
    }

    state = (uint16_t) ((unsigned int) event->state &
            ~((unsigned int) XCB_MOD_MASK_LOCK |
                (unsigned int) XCB_MOD_MASK_2));

    for (int i = 0; i < s_mousebindings_count; ++i) {
        if (s_mousebindings[i].button ==
                (xcb_button_index_t) event->detail) {
            type = s_mousebindings[i].type;
            break;
        }
    }

    if ((state & XCB_MOD_MASK_1) == 0 &&
            (type == MOUSEBIND_DESKTOP_NEXT ||
             type == MOUSEBIND_DESKTOP_PREV)) {
        surface = s_wm_get_surface_for_root(event->root);
        if (surface != NULL) {
            event_td *ev;
            action_td action;
            action.type = ACTION_TYPE_SURFACE;
            action.object.surface = (type == MOUSEBIND_DESKTOP_NEXT)
                ? ACTION_SURFACE_DESKTOP_SWITCH_NEXT
                : ACTION_SURFACE_DESKTOP_SWITCH_PREV;
            ev = event_init((void *) surface, NULL, action,
                    PRIORITY_NORMAL);
            if (ev != NULL) {
                eventq_add(ev);
            }
        }
        return;
    }

    if (client != NULL && (state & XCB_MOD_MASK_1) == 0) {
        surface = s_wm_get_surface_for_root(event->root);
        if (surface != NULL && desktop != NULL) {
            s_wm_focus_client(surface, desktop, client, true);
        }

        /* Check if click landed on a titlebar decoration button.
         * Buttons are in the titlebar ('y < frame_extents.top') and we
         * test event_x against each button's x-extent. */
        if (event->child == client->titlebar && client->titlebar != 0) {
            int16_t  ex  = event->event_x;
            int16_t  ey = event->event_y;
            uint16_t left = (uint16_t) client->layout.frame_extents.left;
            uint16_t right = (uint16_t) client->layout.frame_extents.right;
            uint16_t frame_w = (uint16_t) client->layout.geometry.cur.dim.w;
            uint16_t fw = (frame_w > left + right)
                ? (uint16_t) (frame_w - left - right)
                : 1u;
            uint16_t btn = (uint16_t) WM_DECOR_BTN_SIZE;
            uint16_t gap = (uint16_t) WM_DECOR_BTN_GAP;
            uint16_t pad = (uint16_t) WM_DECOR_BTN_PAD;
            uint16_t step = (uint16_t) (btn + gap);
            uint16_t title_h = client->title_height;
            int16_t  btn_y = (title_h > btn)
                ? (int16_t) ((title_h - btn) / 2u)
                : 0;

            if (ey >= btn_y && ey < btn_y + (int16_t) btn) {
                /* Check Pin button (left-aligned) */
                if (ex >= (int16_t) pad &&
                        ex < (int16_t) (pad + btn)) {
                    client_send_event(client,
                            ACTION_CLIENT_TOGGLE_STICKY,
                            PRIORITY_NORMAL);
                }

                /* Check right-aligned buttons (right-to-left):
                 * Values of 'i':
                 * 0=Close, 1=Fullscreen, 2=Maximize, 3=Shade, 4=Hide,
                 * 5=Iconify */
                else {
                    int bi;
                    static const enum action_client_e btn_actions[6] = {
                        ACTION_CLIENT_CLOSE,
                        ACTION_CLIENT_TOGGLE_FULLSCREEN,
                        ACTION_CLIENT_MAXIMIZE,
                        ACTION_CLIENT_TOGGLE_SHADE,
                        ACTION_CLIENT_HIDE,
                        ACTION_CLIENT_ICONIFY
                    };

                    for (bi = 0; bi < 6; ++bi) {
                        int16_t bx = (int16_t)(fw - pad - btn -
                                (int16_t) (bi * step));
                        if (ex >= bx && ex < bx + (int16_t) btn) {
                            client_send_event(client, btn_actions[bi],
                                    PRIORITY_NORMAL);
                            break;
                        }
                    } /* ! for (bi) */
                }
            }
        }

        /* Unfreeze the pointer (frame's SYNC passive grab is active).
         * Replay the click to the application if it landed on the
         * client content window; consume it silently for frame or
         * titlebar clicks (WM-decoration actions only). */
        if (event->child == client->window) {
            xcb_allow_events(wm->connection, XCB_ALLOW_REPLAY_POINTER,
                    event->time);
        } else {
            xcb_allow_events(wm->connection, XCB_ALLOW_ASYNC_POINTER,
                    event->time);
        }
        xcb_flush(wm->connection);
        return;
    }

    if ((state & XCB_MOD_MASK_1) == 0) {
        /* Click on root background or orphaned frame.  If a SYNC
         * passive grab is somehow active (orphaned frame), unfreeze; if
         * not, this is a no-op (silent 'NoCurrentGrab' per spec). */
        if (event->event != event->root) {
            xcb_allow_events(wm->connection, XCB_ALLOW_ASYNC_POINTER,
                    event->time);
            xcb_flush(wm->connection);
        }
        return;
    }

    if (type == MOUSEBIND_NONE) {
        return;
    }

    /* Desktop cycling without 'Alt' was already handled above.  If the
     * configured wheel binding also happens to be used while 'Alt' is
     * held, dispatch it here too before touching the drag state
     * machine */
    if (type == MOUSEBIND_DESKTOP_NEXT || type == MOUSEBIND_DESKTOP_PREV) {
        surface = s_wm_get_surface_for_root(event->root);
        if (surface != NULL) {
            event_td *ev;
            action_td action;
            action.type = ACTION_TYPE_SURFACE;
            action.object.surface = (type == MOUSEBIND_DESKTOP_NEXT)
                ? ACTION_SURFACE_DESKTOP_SWITCH_NEXT
                : ACTION_SURFACE_DESKTOP_SWITCH_PREV;
            ev = event_init((void *) surface, NULL, action,
                    PRIORITY_NORMAL);
            if (ev != NULL) {
                eventq_add(ev);
            }
        }
        return;
    }

    window = (event->child != XCB_NONE) ? event->child : event->event;
    client = s_wm_find_client(window, NULL, &desktop);
    if (client == NULL && event->child != XCB_NONE) {
        /* 'event->child' may be a sub-window; so walk up the window
         * tree until we find a managed ancestor or reach root */
        w = event->child;
        while (client == NULL) {
            qt_c = xcb_query_tree(wm->connection, w);
            qt_r = xcb_query_tree_reply(wm->connection, qt_c, NULL);
            if (qt_r == NULL) {
                break;
            }
            qt_parent = qt_r->parent;
            qt_root   = qt_r->root;
            free(qt_r);
            if (qt_parent == XCB_NONE || qt_parent == qt_root) {
                break;
            }
            w = qt_parent;
            client = s_wm_find_client(w, NULL, &desktop);
        }
    }

    if (client == NULL) {
        xcb_allow_events(wm->connection, XCB_ALLOW_ASYNC_POINTER,
                event->time);
        xcb_flush(wm->connection);
        return;
    }

    if (type == MOUSEBIND_RESIZE && !client_is_resizable(client)) {
        return;
    }

    /* Lowering a window is a single-shot action: perform it
     * immediately and return without starting a drag */
    if (type == MOUSEBIND_LOWER) {
        if (desktop != NULL) {
            surface = s_wm_get_surface_for_root(event->root);
            if (surface != NULL) {
                s_wm_focus_client(surface, desktop, client, true);
            } else {
                desktop->client_active_id = client->id;
            }
        }
        (void) client_send_event_lower(client);
        return;
    }

    s_drag.active = true;
    s_drag.client = client;
    s_drag.pointer_start_x = event->root_x;
    s_drag.pointer_start_y = event->root_y;
    s_drag.client_start_x = client->layout.geometry.cur.pos.x;
    s_drag.client_start_y = client->layout.geometry.cur.pos.y;
    s_drag.client_start_w = (uint16_t) client->layout.geometry.cur.dim.w;
    s_drag.client_start_h = (uint16_t) client->layout.geometry.cur.dim.h;
    s_drag.operation = (type == MOUSEBIND_MOVE)
        ? CLIENT_OPERATION_MOVING
        : CLIENT_OPERATION_RESIZING;

    client->properties.operation = (uint16_t) s_drag.operation;
    if (desktop != NULL) {
        desktop->client_active_id = client->id;
        (void) desktop_action_client_send_front(desktop, client);
    }

    grab_c = xcb_grab_pointer(wm->connection,
            0,  /* owner events */
            event->root,
            XCB_EVENT_MASK_BUTTON_RELEASE |
            XCB_EVENT_MASK_POINTER_MOTION,
            XCB_GRAB_MODE_ASYNC,
            XCB_GRAB_MODE_ASYNC,
            XCB_NONE,
            XCB_NONE,
            event->time);
    grab_r = xcb_grab_pointer_reply(wm->connection, grab_c, NULL);
    if (grab_r != NULL) {
        /* ALREADY_GRABBED is expected: the root's passive button grab
         * (registered in s_wm_grab_buttons) is the current active grab
         * and cannot be replaced mid-event.  Motion and release events
         * are still delivered through that grab so the drag works
         * regardless.  Any other non-SUCCESS status is unexpected and
         * worth logging. */
        if (grab_r->status != XCB_GRAB_STATUS_SUCCESS &&
                grab_r->status != XCB_GRAB_STATUS_ALREADY_GRABBED) {
            LOGGER_WARNING(
                    "Pointer grab for drag operation failed: status %u",
                    (unsigned int) grab_r->status);
        }
        free(grab_r);
    }

    xcb_flush(wm->connection);
}


/**
 * @brief Handle @c MOTION_NOTIFY events during move/resize drags
 *
 * @param event Pointer to the motion event
 */
static void s_wm_handle_motion_notify(xcb_motion_notify_event_t *event)
{
    client_td *client;
    int32_t dx;
    int32_t dy;

    if (event == NULL || !s_drag.active || s_drag.client == NULL) {
        return;
    }

    client = s_drag.client;
    dx = (int32_t) event->root_x - (int32_t) s_drag.pointer_start_x;
    dy = (int32_t) event->root_y - (int32_t) s_drag.pointer_start_y;

    if (s_drag.operation == CLIENT_OPERATION_MOVING) {
        (void) client_send_event_move(client,
                s_drag.client_start_x + dx,
                s_drag.client_start_y + dy);
    } else if (s_drag.operation == CLIENT_OPERATION_RESIZING) {
        uint16_t width;
        uint16_t height;
        int32_t new_w = (int32_t) s_drag.client_start_w + dx;
        int32_t new_h = (int32_t) s_drag.client_start_h + dy;
        width = s_wm_clamp_dimension(new_w);
        height = s_wm_clamp_dimension(new_h);

        (void) client_send_event_resize(client, width, height);
    }
}


/**
 * @brief Handle @c BUTTON_RELEASE events for move/resize drags
 *
 * @param event Pointer to the button release event
 */
static void s_wm_handle_button_release(xcb_button_release_event_t *event)
{
    (void) event;

    if (!s_drag.active) {
        return;
    }

    if (s_drag.client != NULL) {
        s_drag.client->properties.operation = CLIENT_OPERATION_IDLE;
    }

    s_drag.active = false;
    s_drag.operation = CLIENT_OPERATION_IDLE;
    s_drag.client = NULL;

    xcb_ungrab_pointer(wm->connection, XCB_CURRENT_TIME);
    xcb_flush(wm->connection);
}


/**
 * @brief Handle @c CONFIGURE_REQUEST events from the X server
 *
 * Applies geometry and stacking requests directly through XCB and keeps
 * the managed client's cached geometry synchronized when applicable.
 *
 * @param event Pointer to the configure request event
 *
 * @note Complexity: @e O(1)
 */
static void s_wm_handle_configure_request(
        xcb_configure_request_event_t *event)
{
    client_td *client;
    surface_td *surface;
    desktop_td *desktop;
    uint16_t mask;
    uint32_t values[7];
    int i = 0;

    if (event == NULL) {
        LOGGER_ERROR("Received 'NULL' pointer in configure request" \
                " handler", L_NARG);
        return;
    }

    LOGGER_TRACE("Configure request event: window=0x%x, mask=0x%x",
            event->window, event->value_mask);

    mask = event->value_mask &
        (XCB_CONFIG_WINDOW_X            |
         XCB_CONFIG_WINDOW_Y            |
         XCB_CONFIG_WINDOW_WIDTH        |
         XCB_CONFIG_WINDOW_HEIGHT       |
         XCB_CONFIG_WINDOW_BORDER_WIDTH |
         XCB_CONFIG_WINDOW_SIBLING      |
         XCB_CONFIG_WINDOW_STACK_MODE);

    /* Managed windows update cached geometry; unmanaged windows still
     * receive the XCB configure request verbatim */
    client = s_wm_find_client(event->window, &surface, &desktop);

    if (mask & XCB_CONFIG_WINDOW_X) {
        values[i++] = (uint32_t) event->x;
        if (client != NULL) {
            client->layout.geometry.cur.pos.x = event->x;
        }
    }
    if (mask & XCB_CONFIG_WINDOW_Y) {
        values[i++] = (uint32_t) event->y;
        if (client != NULL) {
            client->layout.geometry.cur.pos.y = event->y;
        }
    }
    if (mask & XCB_CONFIG_WINDOW_WIDTH) {
        values[i++] = (uint32_t) event->width;
        if (client != NULL) {
            client->layout.geometry.cur.dim.w = event->width;
        }
    }
    if (mask & XCB_CONFIG_WINDOW_HEIGHT) {
        values[i++] = (uint32_t) event->height;
        if (client != NULL) {
            client->layout.geometry.cur.dim.h = event->height;
        }
    }
    if (mask & XCB_CONFIG_WINDOW_BORDER_WIDTH) {
        values[i++] = (uint32_t) event->border_width;
    }
    if (mask & XCB_CONFIG_WINDOW_SIBLING) {
        values[i++] = event->sibling;
    }
    if (mask & XCB_CONFIG_WINDOW_STACK_MODE) {
        values[i++] = (uint32_t) event->stack_mode;
    }

    if (mask != 0) {
        xcb_configure_window(wm->connection, event->window, mask, values);
        xcb_flush(wm->connection);
    }

    if (surface != NULL) {
        surface->is_outdated = true;
    }
    if (desktop != NULL) {
        desktop->is_outdated = true;
    }
}


/**
 * @brief Handle @c CONFIGURE_NOTIFY events from the X server
 *
 * Processes window configuration change notifications.  These events
 * indicate that a window's geometry or stacking order has changed.
 *
 * @param event Pointer to the configure notify event
 *
 * @note Complexity: @e O(1)
 */
static void s_wm_handle_configure_notify(
        xcb_configure_notify_event_t *event)
{
    client_td *client;

    if (event == NULL) {
        LOGGER_ERROR("Received 'NULL' pointer in configure handler",
                L_NARG);
        return;
    }

    LOGGER_TRACE("Configure notify event: window=0x%x," \
            " geom=%ux%u+%d+%d",
            event->window, event->width, event->height,
            event->x, event->y);

    /* Update the client's cached geometry */
    /* NOTE: Only trust the event when it comes from the outermost
     *       window that the WM actually positions on the screen.  For
     *       decorated clients that outer window is the frame; for
     *       undecorated clients it is the client window itself.
     *
     *      Ignoring configure_notify from an inner (reparented) client
     *      window is essential: its x/y are frame-relative, not
     *      screen-relative, so blindly copying them would corrupt the
     *      cached position (e.g., replacing the screen coordinates with
     *      the small frame-inset offsets "left" and "top"), which in
     *      turn causes the frame to jump to the wrong location on the
     *      next render pass. */
    client = s_wm_find_client(event->window, NULL, NULL);
    if (client != NULL) {
        bool is_frame_event = (client->frame != 0)
            ? (event->window == client->frame)
            : (event->window == client->window ||
                    event->window == client->id);
        if (is_frame_event) {
            client->layout.geometry.cur.pos.x = event->x;
            client->layout.geometry.cur.pos.y = event->y;
            client->layout.geometry.cur.dim.w = event->width;
            client->layout.geometry.cur.dim.h = event->height;
        }
    }
}


/**
 * @brief Apply the configured placement policy to a newly mapped client
 *
 * Computes an initial frame position for @p client according to the
 * window placement policy stored in the configuration and moves the
 * frame window to that position.  The following policies are handled:
 *
 * - @c "cascade": each successive window is offset by a fixed step so
 *   that windows fan out diagonally.  The sequence wraps when it would
 *   push the frame off the right or bottom edge of the screen.
 * - @c "centered": the frame is centred on the screen;
 * - @c "under-mouse": the frame appears around current pointer position.
 *
 * If @c config->base.windows.placement.is_centered is set it takes
 * precedence over the policy string.  Any unrecognized policy leaves
 * the frame at its initial X-server-assigned position.
 *
 * @param surface Pointer to the surface (screen) on which @p client
 *                will be displayed
 * @param client  Pointer to the newly managed client whose frame is to
 *                be repositioned
 *
 * @note This function must be called after @c client_manage so that
 *       both @c client->frame and @c client->layout.geometry are valid.
 * @note Complexity: @e O(1)
 */
static void s_wm_apply_placement_policy(surface_td *surface,
        client_td *client)
{
    uint32_t sw;
    uint32_t sh;
    uint32_t fw;
    uint32_t fh;
    int32_t new_x;
    int32_t new_y;
    xcb_window_t target;

    if (wm == NULL || wm->config == NULL ||
            surface == NULL || client == NULL) {
        return;
    }

    sw = surface->properties.dim.w;
    sh = surface->properties.dim.h;
    fw = client->layout.geometry.cur.dim.w;
    fh = client->layout.geometry.cur.dim.h;

    if (wm->config->base.windows.placement.is_centered) {
        new_x = ((int32_t) sw - (int32_t) fw) / 2;
        new_y = ((int32_t) sh - (int32_t) fh) / 2;
        if (new_x < 0) { new_x = 0; }
        if (new_y < 0) { new_y = 0; }
    } else {
        if (wm->config->base.windows.placement_policy ==
                CONFIG_PLACEMENT_POLICY_CASCADE ||
                wm->config->base.windows.placement_policy ==
                CONFIG_PLACEMENT_POLICY_SMART) {
            static uint32_t s_cascade_seq = 0;
            const uint32_t cascade_step = 24u;
            uint32_t max_steps;

            max_steps = (sw > fw) ? (sw - fw) / cascade_step : 1u;
            if (sh > fh) {
                uint32_t my = (sh - fh) / cascade_step;
                if (my < max_steps) {
                    max_steps = my;
                }
            }
            if (max_steps == 0u) {
                max_steps = 1u;
            }

            new_x = (int32_t) ((s_cascade_seq % max_steps) * cascade_step);
            new_y = (int32_t) ((s_cascade_seq % max_steps) * cascade_step);
            s_cascade_seq++;
        } else if (wm->config->base.windows.placement_policy ==
                CONFIG_PLACEMENT_POLICY_CENTERED) {
            new_x = ((int32_t) sw - (int32_t) fw) / 2;
            new_y = ((int32_t) sh - (int32_t) fh) / 2;
            if (new_x < 0) { new_x = 0; }
            if (new_y < 0) { new_y = 0; }
        } else if (wm->config->base.windows.placement_policy ==
                CONFIG_PLACEMENT_POLICY_UNDER_MOUSE) {
            xcb_query_pointer_cookie_t pointer_cookie;
            xcb_query_pointer_reply_t *pointer_reply;

            pointer_cookie = xcb_query_pointer(wm->connection,
                    surface->screen->root);
            pointer_reply = xcb_query_pointer_reply(wm->connection,
                    pointer_cookie, NULL);
            if (pointer_reply == NULL) {
                LOGGER_NOTICE("Failed to query pointer for"
                        " 'under-mouse' placement; keeping"
                        " X-server-assigned position", L_NARG);
                return;
            }

            new_x = (int32_t) pointer_reply->root_x - (int32_t) (fw / 2u);
            new_y = (int32_t) pointer_reply->root_y - (int32_t) (fh / 2u);
            if (new_x < 0) {
                new_x = 0;
            } else if ((uint32_t) new_x + fw > sw) {
                new_x = (sw > fw) ? (int32_t) (sw - fw) : 0;
            }
            if (new_y < 0) {
                new_y = 0;
            } else if ((uint32_t) new_y + fh > sh) {
                new_y = (sh > fh) ? (int32_t) (sh - fh) : 0;
            }

            free(pointer_reply);
        } else {
            /* "none" or unknown: keep the X-server-assigned position */
            return;
        }
    }

    target = (client_is_decorated(client) && client->frame != 0)
        ? client->frame
        : client->window;

    xcb_configure_window(wm->connection, target,
            XCB_CONFIG_WINDOW_X | XCB_CONFIG_WINDOW_Y,
            (const uint32_t[]) {(uint32_t) new_x, (uint32_t) new_y});
    client->layout.geometry.cur.pos.x = new_x;
    client->layout.geometry.cur.pos.y = new_y;
}


/**
 * @brief Handle @c MAP_REQUEST events from the X server
 *
 * Processes requests to map (display) windows.  This event is sent when
 * a window wants to become visible on the screen.
 *
 * @param event Pointer to the map request event
 *
 * @note Complexity: @e O(1)
 */
static void s_wm_handle_map_request(
        xcb_map_request_event_t *event)
{
    surface_td *surface;
    desktop_td *desktop;
    client_td  *client;

    if (event == NULL) {
        LOGGER_ERROR("Received 'NULL' pointer in map request handler",
                L_NARG);
        return;
    }

    LOGGER_TRACE("Map request event: window=0x%x, parent=0x%x",
            event->window, event->parent);

    /* Do not re-manage already-known windows */
    if (s_wm_find_client(event->window, NULL, NULL) != NULL) {
        LOGGER_TRACE("Window %#x already managed; mapping directly",
                event->window);
        xcb_map_window(wm->connection, event->window);
        xcb_flush(wm->connection);
        return;
    }

    /* Find the surface that owns this root window */
    surface = s_wm_get_surface_for_root(event->parent);
    if (surface == NULL && !list_is_empty(wm->surfaces)) {
        surface = (surface_td *) list_data(list_head(wm->surfaces));
    }
    if (surface == NULL) {
        LOGGER_ERROR("No surface found for 'MAP_REQUEST' on root %#x",
                event->parent);
        return;
    }

    desktop = s_wm_get_current_desktop(surface);
    if (desktop == NULL) {
        LOGGER_ERROR("No current desktop on surface %u; mapping" \
                " without management", surface->id);
        xcb_map_window(wm->connection, event->window);
        xcb_flush(wm->connection);
        return;
    }

    /* Adopt the window */
    client = client_manage(wm->connection, wm->ewmh,
            event->window, &wm->config->theme);
    if (client == NULL) {
        /* Override-redirect or allocation failure; just map it */
        xcb_map_window(wm->connection, event->window);
        xcb_flush(wm->connection);
        return;
    }

    /* Bind to the current desktop */
    client->screen_id  = surface->id;
    client->desktop_id = desktop->id;

    if (desktop_action_client_add(desktop, client) != 0) {
        LOGGER_ERROR("Failed to add client %#x to desktop %u",
                event->window, desktop->id);
        client->window = 0;  /* Prevent double-destroy below */
        client_destroy(client);
        xcb_map_window(wm->connection, event->window);
        xcb_flush(wm->connection);
        return;
    }

    /* Apply the placement policy before the window becomes visible */
    s_wm_apply_placement_policy(surface, client);

    /* Show the managed object */
    if (client->titlebar != 0) {
        xcb_map_window(wm->connection, client->titlebar);
    }
    if (client->frame != 0) {
        xcb_map_window(wm->connection, client->frame);
        xcb_map_window(wm->connection, client->window);
    } else {
        xcb_map_window(wm->connection, event->window);
    }

    if (wm->config->base.windows.focus.is_new_focused) {
        s_wm_focus_client(surface, desktop, client, true);
    }

    surface->is_outdated = true;
    desktop->is_outdated = true;
    xcb_flush(wm->connection);

    LOGGER_DEBUG("Mapped and adopted window %#x ('%s') on desktop %u",
            event->window, client->info.name, desktop->id);
}


/**
 * @brief Handle @c UNMAP_NOTIFY events from the X server
 *
 * Processes notifications that a window has been unmapped (hidden).
 * This typically means the window is no longer visible on screen.
 *
 * @param event Pointer to the unmap notify event
 *
 * @note Complexity: @e O(1)
 */
static void s_wm_handle_unmap_notify(
        xcb_unmap_notify_event_t *event)
{
    client_td  *client;
    surface_td *surface;
    desktop_td *desktop;

    if (event == NULL) {
        LOGGER_ERROR("Received 'NULL' pointer in unmap handler",
                L_NARG);
        return;
    }

    LOGGER_TRACE("Unmap notify event: window=0x%x", event->window);

    /* Keep the client managed; do not force the 'hidden' flag here.
     * Explicit user actions (iconify/hide, see 'wcmd_client_iconify'
     * and 'wcmd_client_hide' in 'cmds/ccmd.c') already set
     * 'CLIENT_FLAG_HIDDEN' themselves right before unmapping the
     * window. Desktop switching (see 'surface_clients_hide' in
     * 'surface.c') also unmaps windows but must NOT be treated as
     * hidden, or the client would never be remapped when switching back
     * to its desktop ('surface_clients_show' skips clients with
     * 'CLIENT_FLAG_HIDDEN' set).  Setting the flag unconditionally on
     * every unmap notification broke exactly that case. */
    client = s_wm_find_client(event->window, &surface, &desktop);
    if (client != NULL) {
        if (event->window != client->window &&
                event->window != client->frame) {
            return;
        }
        if (desktop != NULL &&
                desktop->client_active_id == client->id) {
            desktop->client_active_id = 0;
        }
    }
}


/**
 * @brief Handle @c DESTROY_NOTIFY events from the X server
 *
 * Processes notifications that a window has been destroyed.  When this
 * event is received, the window is no longer valid and any references
 * to it should be cleaned up.
 *
 * @param event Pointer to the destroy notify event
 *
 * @note Complexity: @e O(1)
 */
static void s_wm_handle_destroy_notify(
        xcb_destroy_notify_event_t *event)
{
    client_td  *client;
    surface_td *surface;
    desktop_td *desktop;

    if (event == NULL) {
        LOGGER_ERROR("Received 'NULL' pointer in destroy handler",
                L_NARG);
        return;
    }

    LOGGER_TRACE("Destroy notify event: window=0x%x", event->window);

    client = s_wm_find_client(event->window, &surface, &desktop);
    if (client == NULL) {
        return;
    }

    if (event->window != client->window) {
        return;
    }

    if (s_drag.active && s_drag.client == client) {
        s_drag.active = false;
        s_drag.operation = CLIENT_OPERATION_IDLE;
        s_drag.client = NULL;
        xcb_ungrab_pointer(wm->connection, XCB_CURRENT_TIME);
        xcb_flush(wm->connection);
    }

    if (desktop != NULL &&
            desktop->client_active_id == client->id) {
        desktop->client_active_id = 0;
    }

    /* Remove from data structures */
    if (desktop != NULL) {
        desktop_action_client_rem(desktop, client);
    }

    /* The X window is already gone; clear the handle so
     * 'client_destroy' does not attempt 'xcb_destroy_window' on a dead
     * window */
    client->window = 0;
    client_destroy(client);

    if (surface != NULL) {
        surface->is_outdated = true;
    }
    if (desktop != NULL) {
        desktop->is_outdated = true;
    }

    LOGGER_DEBUG("Removed destroyed window %#x", event->window);
}


/**
 * @brief Handle @c PROPERTY_NOTIFY events from the X server
 *
 * Processes notifications that window properties have changed.
 * Properties may include @c WM_NAME, @c WM_CLASS, @c WM_HINTS, and
 * others which affect how the window manager displays or manages the
 * window.
 *
 * @param event Pointer to the property notify event
 *
 * @note Complexity: @e O(1)
 */
static void s_wm_handle_property_notify(
        xcb_property_notify_event_t *event)
{
    client_td  *client;
    surface_td *surface;

    if (event == NULL) {
        LOGGER_ERROR("Received 'NULL' pointer in property handler",
                L_NARG);
        return;
    }

    LOGGER_TRACE("Property notify event: window=0x%x, atom=%u",
            event->window, event->atom);

    if (event->state == XCB_PROPERTY_DELETE) {
        return;     /* Deleted properties do not need refresh */
    }

    /* Re-read 'WM_NAME' when it changes */
    if (event->atom == XCB_ATOM_WM_NAME) {
        client = s_wm_find_client(event->window, &surface, NULL);
        if (client != NULL) {
            s_wm_refresh_client_name(client);
            if (surface != NULL) {
                surface->is_outdated = true;
            }
        }
    }
}


/**
 * @brief Handle @c XCB_FOCUS_IN events from the X server
 *
 * Synchronizes the desktop active-client identifier with the real X11
 * input focus when a managed client receives focus.
 *
 * @param event Pointer to the focus-in event
 *
 * @note Complexity: @e O(n), where @e n is the number of managed
 *       clients searched by @a s_wm_find_client
 */
static void s_wm_handle_focus_in(xcb_focus_in_event_t *event)
{
    client_td  *client;
    surface_td *surface;
    desktop_td *desktop;

    if (event == NULL) {
        LOGGER_ERROR("Received 'NULL' pointer in focus handler", L_NARG);
        return;
    }

    client = s_wm_find_client(event->event, &surface, &desktop);
    if (client == NULL || desktop == NULL) {
        return;
    }

    if (desktop->client_active_id != client->id) {
        desktop->client_active_id = client->id;
        desktop->is_outdated = true;
        if (surface != NULL) {
            surface->is_outdated = true;
        }
    }
}


/**
 * @brief Handle @c XCB_MAPPING_NOTIFY events from the X server
 *
 * Refreshes the cached keyboard-mapping table and re-establishes all
 * passive key grabs with updated keycodes.  Without this, any keyboard
 * layout change (e.g. via @c setxkbmap or an input-method switch) that
 * moves keycodes causes existing grabs to stop firing silently.
 *
 * When the @e modifier mapping changes the button grabs are also
 * rebuilt, because modifier-key reassignments can break the
 * @c XCB_MOD_MASK_1 match used for every mouse action.
 *
 * @param keysyms Pointer to the XCB key symbols table to refresh
 * @param event   Pointer to the mapping notify event
 *
 * @note Complexity: @e O(k * s), where @e k is the number of key
 *       bindings and @e s is the number of surfaces
 */
static void s_wm_handle_mapping_notify(xcb_key_symbols_t *keysyms,
        xcb_mapping_notify_event_t *event)
{
    list_item_td *node;
    surface_td *surface;
    if (keysyms == NULL || event == NULL) {
        LOGGER_ERROR("Received 'NULL' pointer in mapping notify handler",
                L_NARG);
        return;
    }
    /* Pointer-only remaps do not affect any grab we registered */
    if (event->request == XCB_MAPPING_POINTER) {
        return;
    }

    LOGGER_TRACE("Mapping notify: request=%u; refreshing grabs",
            (unsigned int) event->request);

    /* Update the cached keycode→keysym table */
    xcb_refresh_keyboard_mapping(keysyms, event);

    /* Remove every passive key grab from every root window so that the
     * re-grab below can install fresh ones with the new keycodes */
    for (node = list_head(wm->surfaces);
            node != NULL; node = list_next(node)) {
        surface = (surface_td *) list_data(node);
        if (surface == NULL || surface->screen == NULL) {
            continue;
        }

        xcb_ungrab_key(wm->connection,
                (xcb_keycode_t) XCB_GRAB_ANY,
                surface->screen->root,
                (uint16_t) XCB_MOD_MASK_ANY);
    }

    xcb_flush(wm->connection);
    s_wm_grab_keys(keysyms);

    /* A modifier-map change can also invalidate button grabs because
     * the physical key producing XCB_MOD_MASK_1 may have moved */
    if (event->request == XCB_MAPPING_MODIFIER) {
        for (node = list_head(wm->surfaces);
                node != NULL; node = list_next(node)) {
            surface = (surface_td *) list_data(node);
            if (surface == NULL || surface->screen == NULL) {
                continue;
            }

            xcb_ungrab_button(wm->connection,
                    (uint8_t) XCB_BUTTON_INDEX_ANY,
                    surface->screen->root,
                    (uint16_t) XCB_MOD_MASK_ANY);
        }

        xcb_flush(wm->connection);
        s_wm_grab_buttons();
    }
}


/**
 * @brief Handle @c XCB_EXPOSE events for decoration repaints
 *
 * Repaints decoration windows after an expose sequence completes.
 * Only the final event in a sequence @c (count == 0) triggers a repaint
 * to avoid redundant draws.
 *
 * Two targets are handled:
 * - info popup window that redraws the cached text lines; and
 * - managed client's titlebar, that repaints the background and title
 *   text in the appropriate active/inactive theme colors.
 *
 * @param event Pointer to the expose event
 *
 * @note Complexity: @e O(n) for the titlebar case, where @e n is the
 *       number of managed clients searched by @a s_wm_find_client
 */
static void s_wm_handle_expose(xcb_expose_event_t *event)
{
    client_td *client;
    desktop_td *desktop;
    bool is_focused;
    uint16_t left;
    uint16_t right;
    uint16_t title_h;
    uint16_t inner_w;

    if (event == NULL || event->count != 0) {
        return;
    }

    /* Info popup: repaint from the cached text lines */
    if (s_info_popup_window != XCB_WINDOW_NONE &&
            event->window == s_info_popup_window) {
        text_renderer_init(wm->connection,
                wm->config->theme.window.active.font);
        text_draw_string(wm->connection, s_info_popup_window, XCB_NONE,
                8, 16, s_info_popup_lines[0]);
        text_draw_string(wm->connection, s_info_popup_window, XCB_NONE,
                8, 34, s_info_popup_lines[1]);
        text_draw_string(wm->connection, s_info_popup_window, XCB_NONE,
                8, 52, s_info_popup_lines[2]);
        text_draw_string(wm->connection, s_info_popup_window, XCB_NONE,
                8, 70, s_info_popup_lines[3]);
        xcb_flush(wm->connection);
        return;
    }

    client = s_wm_find_client(event->window, NULL, &desktop);
    if (client == NULL) {
        return;
    }

    /* Icon window: repaint caption below the square */
    if (client->icon_window == event->window) {
        xcb_change_window_attributes(wm->connection, client->icon_window,
                XCB_CW_BACK_PIXEL | XCB_CW_BORDER_PIXEL,
                (const uint32_t[]) {
                    wm->config->theme.icon.background_color,
                    wm->config->theme.icon.border_color
                });
        xcb_clear_area(wm->connection, 0, client->icon_window,
                0, 0, 0, 0);

        if (wm->config->theme.icon.is_captioned &&
                client->info.name != NULL) {
            text_renderer_init(wm->connection,
                    wm->config->theme.icon.font);
            text_renderer_set_color(
                    wm->config->theme.icon.foreground_color,
                    wm->config->theme.icon.background_color);
            text_draw_string(wm->connection, client->icon_window,
                    XCB_NONE,
                    2,
                    (int16_t) (WM_ICON_SQUARE_SIZE +
                               WM_ICON_CAPTION_HEIGHT - 2u),
                    client->info.name);
        }

        xcb_flush(wm->connection);

        return;
    }

    /* Titlebar: repaint background, title text, and decoration buttons */
    if (client->titlebar != event->window || client->info.name == NULL) {
        return;
    }

    is_focused = (desktop != NULL &&
                  desktop->client_active_id == client->id);

    if (client->frame == event->window) {
        xcb_change_window_attributes(wm->connection, client->frame,
                XCB_CW_BACK_PIXEL | XCB_CW_BORDER_PIXEL,
                (const uint32_t[]) {
                    (is_focused)
                    ? wm->config->theme.window.active.border_color
                    : wm->config->theme.window.inactive.border_color,
                    (is_focused)
                    ? wm->config->theme.window.active.border_color
                    : wm->config->theme.window.inactive.border_color
                });
        xcb_clear_area(wm->connection, 0, client->frame, 0, 0, 0, 0);
        xcb_flush(wm->connection);
        return;
    }

    left = (uint16_t) client->layout.frame_extents.left;
    right = (uint16_t) client->layout.frame_extents.right;
    title_h = client->title_height;
    inner_w = (client->layout.geometry.cur.dim.w > left + right)
        ? (uint16_t) (client->layout.geometry.cur.dim.w - left - right)
        : 1u;
    xcb_change_window_attributes(wm->connection, client->frame,
            XCB_CW_BACK_PIXEL | XCB_CW_BORDER_PIXEL,
            (const uint32_t[]) {
                (is_focused)
                    ? wm->config->theme.window.active.border_color
                    : wm->config->theme.window.inactive.border_color,
                (is_focused)
                    ? wm->config->theme.window.active.border_color
                    : wm->config->theme.window.inactive.border_color
            });
    xcb_clear_area(wm->connection, 0, client->frame, 0, 0, 0, 0);
    xcb_change_window_attributes(wm->connection, client->titlebar,
            XCB_CW_BACK_PIXEL,
            (const uint32_t[]) {
                (is_focused)
                ? wm->config->theme.window.active.background_color
                : wm->config->theme.window.inactive.background_color
            });
    xcb_clear_area(wm->connection, 0, client->titlebar, 0, 0, 0, 0);

    text_renderer_init(wm->connection,
            (is_focused)
                ? wm->config->theme.window.active.font
                : wm->config->theme.window.inactive.font);
    text_renderer_set_color(
            (is_focused)
                ? wm->config->theme.window.active.foreground_color
                : wm->config->theme.window.inactive.foreground_color,
            (is_focused)
                ? wm->config->theme.window.active.background_color
                : wm->config->theme.window.inactive.background_color);
    text_draw_string(wm->connection, client->titlebar, XCB_NONE,
            (int16_t) (WM_DECOR_BTN_PAD + WM_DECOR_BTN_SIZE +
                WM_DECOR_BTN_PAD),
            (int16_t) ((title_h > (uint16_t) WM_TITLEBAR_TEXT_BOTTOM_PAD)
                    ? title_h - (uint16_t) WM_TITLEBAR_TEXT_BOTTOM_PAD
                    : title_h),
            client->info.name);

    desktop_draw_titlebar_buttons(wm->connection, client->titlebar,
            inner_w, title_h,
            is_focused, (bool) client_is_sticky(client),
            &wm->config->theme);

    xcb_flush(wm->connection);
}


/**
 * @brief Soft window manager update
 *
 * Performs a minimal update of the window manager state, redrawing only
 * those surfaces that were actually marked as outdated (e.g., by
 * a background color change, a configuration reload, or a property
 * change on a client window).  This is called on every iteration of the
 * main event loop, so surfaces that have nothing pending are left
 * untouched to avoid unnecessary rendering.
 *
 * @note Complexity: @e O(n), where @e n is the number of surfaces
 */
static void s_wm_update(void)
{
    /* Update only the surfaces that actually need it */
    for (list_item_td *surface_node = list_head(wm->surfaces);
            surface_node != NULL;
            surface_node = list_next(surface_node)) {
        surface_td *surface_cur = (surface_td *) list_data(surface_node);

        if (surface_cur->is_outdated) {
            /* Render all desktops on this surface */
            if (surface_render_all_desktops(surface_cur) != 0) {
                LOGGER_ERROR("Failed to render surface %u",
                        surface_cur->id);
            }
        }
    } /* ! for (surface_node) */
}


/**
 * @brief Full window manager update
 *
 * Forces a complete visual refresh by marking every surface as outdated
 * before delegating to @a s_wm_update.  This is called once, right
 * before entering the main event loop, so that surfaces (and any
 * windows adopted from a previous session) are drawn from scratch
 * regardless of their current outdated state.
 *
 * @note Complexity: @e O(n * m), where @e n is the number of surfaces
 *       and @e m is the number of desktops on the surface
 */
static void s_wm_update_full(void)
{
    LOGGER_TRACE("Fully updating window manager", L_NARG);

    /* Force every surface to be redrawn, regardless of its current
     * outdated state */
    for (list_item_td *surface_node = list_head(wm->surfaces);
            surface_node != NULL;
            surface_node = list_next(surface_node)) {
        surface_td *surface_cur = (surface_td *) list_data(surface_node);
        surface_cur->is_outdated = true;
    } /* ! for (surface_node) */

    /* Perform the actual rendering of every (now outdated) surface */
    s_wm_update();
}


/**
 * @brief Flag set (in an async-signal-safe manner) when a termination
 *        signal has been received
 *
 * Holds the number of the received signal, or 0 if none has been
 * received yet.  It is only ever written from @a s_wm_handle_signal and
 * only ever read from the main event loop (@a s_wm_loop), so its
 * @c volatile @c sig_atomic_t type is sufficient without any further
 * synchronization.
 */
static volatile sig_atomic_t s_stop_signal_received = 0;


/**
 * @brief Signal handler for termination signals
 *
 * Only records the signal number; the actual shutdown request is
 * performed later, from normal execution context inside @a s_wm_loop,
 * since @a wm_request_stop is not guaranteed to be async-signal-safe.
 *
 * @param signum Number of the received signal
 */
static void s_wm_handle_signal(int signum)
{
    s_stop_signal_received = signum;
}


/**
 * @brief Handle @c XCB_ENTER_NOTIFY events for configurable focus policy
 *
 * When the focus policy is set to @c follow-mouse the window under the
 * pointer is focused automatically as soon as the pointer enters it,
 * without requiring a button click.  The window is focused but not
 * raised, so stacking order is preserved and cascading raise events are
 * avoided.
 *
 * Only @c XCB_NOTIFY_MODE_NORMAL events are acted upon; events
 * generated by grab/ungrab transitions (@c XCB_NOTIFY_MODE_GRAB,
 * @c XCB_NOTIFY_MODE_UNGRAB) are ignored to prevent spurious focus
 * changes during drag operations.  Detail @c XCB_NOTIFY_DETAIL_INFERIOR
 * (pointer moving into a child window within the same frame) is also
 * ignored to suppress redundant re-focus of the already-focused client.
 *
 * @param event Pointer to the enter-notify event; must not be @c NULL
 *
 * @note Complexity: @e O(n) where n is the total number of managed
 *       clients (one hash-table lookup via @c s_wm_find_client)
 */
static void s_wm_handle_enter_notify(xcb_enter_notify_event_t *event)
{
    client_td *client;
    desktop_td *desktop;
    surface_td *surface;

    if (event == NULL) {
        return;
    }

    /* Ignore grab/ungrab-generated events to avoid interfering with
     * ongoing drag operations or other pointer grabs */
    if (event->mode != XCB_NOTIFY_MODE_NORMAL) {
        return;
    }

    /* Ignore sub-window transitions within the same frame hierarchy */
    if (event->detail == XCB_NOTIFY_DETAIL_INFERIOR) {
        return;
    }

    /* Act only when focus-follows-mouse policy is in effect */
    if (!s_wm_is_focus_follows_mouse_policy()) {
        return;
    }

    client = s_wm_find_client(event->event, NULL, &desktop);
    if (client == NULL) {
        return;
    }

    surface = s_wm_get_surface_for_root(event->root);
    if (surface == NULL || desktop == NULL) {
        return;
    }

    /* Focus without raise to preserve stacking order */
    s_wm_focus_client(surface, desktop, client, false);
    xcb_flush(wm->connection);
}


/**
 * @brief Install handlers for common termination signals so the window
 *        manager shuts down gracefully instead of being killed abruptly
 *
 * @return Status of the operation
 * @retval  0 Success
 * @retval -1 Failed to install one of the handlers
 */
static int s_wm_install_signal_handlers(void)
{
    struct sigaction sa;

    memset(&sa, 0, sizeof(sa));
    sa.sa_handler = s_wm_handle_signal;
    sa.sa_flags = 0;
    sigemptyset(&sa.sa_mask);

    if (sigaction(SIGHUP, &sa, NULL) != 0 ||
            sigaction(SIGINT, &sa, NULL) != 0 ||
            sigaction(SIGQUIT, &sa, NULL) != 0 ||
            sigaction(SIGTERM, &sa, NULL) != 0) {
        LOGGER_ERROR("Failed to install termination signal handlers",
                L_NARG);
        return -1;
    }

    return 0;
}


/**
 * @brief Enters the main event handling loop of the window manager
 *
 * Runs continuously while the window manager is active, listening for
 * XCB events and passing them to appropriate handlers.  It blocks on
 * @a poll waiting for activity on the X connection's file descriptor,
 * then drains all pending events with @a xcb_poll_for_event, so the
 * process consumes no CPU while idle instead of spinning.
 *
 * @note The event loop will stop when the @p is_running flag is set to
 *       @c false, typically in response to user actions or during
 *       window manager termination
 * @note Complexity: @e O(1) for each event; overall complexity depends
 *       on the number of events processed
 */
static void s_wm_loop(void)
{
    xcb_key_symbols_t *keysyms;
    xcb_generic_event_t *event;
    struct pollfd pfd;
    int poll_status;

    if (wm == NULL || !wm->is_running) {
        LOGGER_TRACE("Window manager is not initialized" \
                " or set to not run", L_NARG);
        return;
    }

    /* Install signal handlers so common termination signals (e.g.,
     * 'SIGHUP', 'SIGINT', 'SIGQUIT', 'SIGTERM') trigger a graceful
     * shutdown instead of an abrupt termination that would skip
     * 'wm_stop' */
    if (s_wm_install_signal_handlers() != 0) {
        LOGGER_WARNING("Continuing without termination signal handling",
                L_NARG);
    }

    /* Allocate key symbols table for keyboard event processing */
    keysyms = xcb_key_symbols_alloc(wm->connection);
    if (keysyms == NULL) {
        LOGGER_ERROR("Failed to allocate key symbols table", L_NARG);
        return;
    }

    /* Grab configured key bindings (must be after keysyms alloc) */
    s_wm_grab_keys(keysyms);
    s_wm_grab_buttons();

    /* Adopt any windows already on screen before we started, then do
     * the first full render so pre-existing windows are included */
    s_wm_scan_existing_windows();

    /* Update window manager before starting the event loop */
    s_wm_update_full();

#ifdef DEBUG
    /* TEST: Create dummy windows to test rendering */
    if (wm->surfaces != NULL) {
        list_item_td *surface_node = list_head(wm->surfaces);
        if (surface_node != NULL) {
            surface_td *surface = (surface_td *) list_data(surface_node);

            if (surface != NULL && surface->desktops != NULL) {
                cdlist_item_td *desktop_node =
                    cdlist_head(surface->desktops);
                /* Iterate to the current desktop */
                for (uint32_t i = 0; i < surface->desktop_cur; ++i) {
                    desktop_node = cdlist_next(desktop_node);
                    if (desktop_node == NULL) {
                        break;
                    }
                }

                if (desktop_node != NULL) {
                    desktop_td *desktop =
                        (desktop_td *) cdlist_data(desktop_node);
                    if (desktop != NULL) {
                        /* Create a test window for rendering */
                        client_td *test_client = client_init(
                                wm->connection,
                                wm->ewmh,
                                surface->screen->root,  /* parent window */
                                100, 100,               /* width, height */
                                50, 50,                 /* x, y */
                                &(wm->config->theme));

                        if (test_client != NULL) {
                            /* Remove 'HIDDEN' flag to show the window */
                            safeflg_unset(&(test_client)->properties.flags,
                                    CLIENT_FLAG_HIDDEN, CLIENT_FLAG_MAX);

                            desktop_action_client_add(desktop, test_client);
                            desktop->is_outdated = true;
                            LOGGER_TRACE("Created test client for rendering",
                                    L_NARG);
                        } /* ! if (test_client) */
                    } /* ! if (desktop) */
                } /* ! if (desktop_node) */
            } /* ! if (surface) */
        } /* ! if (surface_node) */
    }
#endif  /* ! DEBUG */

    LOGGER_DEBUG("Entering main event loop", L_NARG);
    while (wm->is_running) {
        /* Notice signals received asynchronously and request a
         * graceful shutdown from this normal execution context */
        if (s_stop_signal_received != 0) {
            LOGGER_INFO("Termination signal %d received;" \
                    " requesting shutdown",
                    (int) s_stop_signal_received);
            wm_request_stop();
            break;
        }

        /* Defensively verify the X connection is still alive. If it
         * were ever to break (e.g. X server crash/disconnect), reading
         * from a broken connection never blocks and never yields new
         * events, so 'poll()' below would return immediately forever,
         * spinning this loop at 100% CPU without making progress.
         * Shut down gracefully instead. */
        if (xcb_connection_has_error(wm->connection) != 0) {
            LOGGER_ERROR("X connection error detected;" \
                    " requesting shutdown", L_NARG);
            wm_request_stop();
            break;
        }

        /* Block until the X connection has data to read (or a signal
         * interrupts the call).  Without this, 'xcb_poll_for_event'
         * alone would spin the loop as fast as possible, keeping a CPU
         * core permanently busy even while completely idle */
        pfd.fd = xcb_get_file_descriptor(wm->connection);
        pfd.events = POLLIN;
        pfd.revents = 0;

        poll_status = poll(&pfd, 1, -1);
        if (poll_status < 0 && errno != EINTR) {
            LOGGER_ERROR("Failed waiting on X connection: %s",
                    strerror(errno));
            break;
        }

        /* Process X events.
         * 'xcb_poll_for_event' is non-blocking and returns 'NULL' when
         * no events are available */
        while ((event = xcb_poll_for_event(wm->connection))) {
            /* Dispatch event to appropriate handler based on type.
             * Mask '& ~0x80' clears the synthetic event bit */
            switch (event->response_type & ~0x80) { /* Ignore error bits */
                case XCB_KEY_PRESS:
                    s_wm_handle_key_press(
                            keysyms,
                            (xcb_key_press_event_t *) event);
                    break;

                case XCB_BUTTON_PRESS:
                    s_wm_handle_button_press(
                            (xcb_button_press_event_t *) event);
                    break;

                case XCB_BUTTON_RELEASE:
                    s_wm_handle_button_release(
                            (xcb_button_release_event_t *) event);
                    break;

                case XCB_MOTION_NOTIFY:
                    s_wm_handle_motion_notify(
                            (xcb_motion_notify_event_t *) event);
                    break;

                case XCB_ENTER_NOTIFY:
                    s_wm_handle_enter_notify(
                            (xcb_enter_notify_event_t *) event);
                    break;

                case XCB_CONFIGURE_NOTIFY:
                    s_wm_handle_configure_notify(
                            (xcb_configure_notify_event_t *) event);
                    break;

                case XCB_UNMAP_NOTIFY:
                    s_wm_handle_unmap_notify(
                            (xcb_unmap_notify_event_t *) event);
                    break;

                case XCB_DESTROY_NOTIFY:
                    s_wm_handle_destroy_notify(
                            (xcb_destroy_notify_event_t *) event);
                    break;

                case XCB_PROPERTY_NOTIFY:
                    s_wm_handle_property_notify(
                            (xcb_property_notify_event_t *) event);
                    break;

                case XCB_FOCUS_IN:
                    s_wm_handle_focus_in(
                            (xcb_focus_in_event_t *) event);
                    break;

                case XCB_EXPOSE:
                    s_wm_handle_expose(
                            (xcb_expose_event_t *) event);
                    break;

                case XCB_CONFIGURE_REQUEST:
                    s_wm_handle_configure_request(
                            (xcb_configure_request_event_t *) event);
                    break;

                case XCB_MAP_REQUEST:
                    s_wm_handle_map_request(
                            (xcb_map_request_event_t *) event);
                    break;

                case XCB_MAPPING_NOTIFY:
                    s_wm_handle_mapping_notify(keysyms,
                            (xcb_mapping_notify_event_t *) event);
                    break;

                default:
                    LOGGER_TRACE("Unhandled X event type: %d",
                            event->response_type & ~0x80);
                    break;
            }

            /* Free the event structure after processing */
            free(event);
        } /* ! while (event) */

        /* Update the window manager after processing all events */
        s_wm_update();
    } /* ! while(wm->is_running) */

    LOGGER_DEBUG("Exiting event loop", L_NARG);

    /* Free key symbols table */
    xcb_key_symbols_free(keysyms);
}


/* Initialize a window manager instance */
int wm_start(const char *display_name, const char *config_dir_prefix)
{
    LOGGER_DEBUG("Initializing window manager", L_NARG);
    if (wm == NULL) {
        uint32_t screens_detected;
        xcb_screen_iterator_t it;

        /* Allocate memory for the window manager */
        wm = malloc(sizeof(wm_td));
        if (wm == NULL) {
            LOGGER_FATAL("Failed to allocate memory for window manager",
                    L_NARG);
            return 1;
        }

        LOGGER_DEBUG("Opening X display", L_NARG);
        /* If 'display_name' is 'NULL', then it defaults to the value of
         * the 'DISPLAY' environment variable */
        wm->connection = xcb_connect(display_name,
                (int *) &(wm->screenp));

        if (xcb_connection_has_error(wm->connection)) {
            if (display_name == NULL) {
                LOGGER_FATAL("Failed to open X display", L_NARG);
            } else {
                LOGGER_FATAL("Failed to open X display '%s'",
                        display_name);
            }
            free(wm);
            wm = NULL;
            return 2;
        }

        /* Establish EWMH connection */
        LOGGER_TRACE("Allocating memory for EWMH connection", L_NARG);
        wm->ewmh = malloc(sizeof(xcb_ewmh_connection_t));
        if (wm->ewmh == NULL) {
            LOGGER_FATAL("Error allocating memory for EWMH connection",
                    L_NARG);
            xcb_disconnect(wm->connection);
            free(wm);
            wm = NULL;
            return 1;
        }

        /* Initializate EWMH atoms */
        if (!xcb_ewmh_init_atoms_replies(wm->ewmh,
                    xcb_ewmh_init_atoms(wm->connection, wm->ewmh),
                    NULL)) {
               LOGGER_ERROR("Error initializating EWMH atoms", L_NARG);
        }

        /* Set and load configuration */
        wm->config = config_init();
        if (wm->config == NULL) {
            xcb_disconnect(wm->connection);
            xcb_ewmh_connection_wipe(wm->ewmh);
            free(wm->ewmh);
            free(wm);
            wm = NULL;
            return 3;
        }

        LOGGER_TRACE("Loading configuration into window manager", L_NARG);
        wm->config_dir_prefix = config_dir_prefix;
        config_load(wm->config, wm->config_dir_prefix);

        /* Events: priority queue as min-heap (bottom-heavy heap) */
        if (eventq_start() != 0) {
            LOGGER_FATAL("Failed to initialize event priority queue",
                    L_NARG);
            config_destroy(wm->config);
            xcb_disconnect(wm->connection);
            xcb_ewmh_connection_wipe(wm->ewmh);
            free(wm->ewmh);
            free(wm);
            wm = NULL;
            return 4;
        }

        /* Count the number of screens detected by the X server */
        /* NOTE: Yes,... I can use 'xcb_setup_roots_length', but this
         *       way it's more suitable for *my* purposes.
         *       "bIjatlh 'e' yImev!" */
        it = xcb_setup_roots_iterator(xcb_get_setup(wm->connection));
        screens_detected = 0;
        for (; it.rem > 0; xcb_screen_next(&it)) {
            screens_detected++;
        }

        /* Verify 'screens_detected' for it could be zero on some
         * strange error */
        if (screens_detected == 0) {
            LOGGER_FATAL("No screens detected", L_NARG);
            config_destroy(wm->config);
            eventq_stop();
            xcb_disconnect(wm->connection);
            xcb_ewmh_connection_wipe(wm->ewmh);
            free(wm->ewmh);
            free(wm);
            wm = NULL;
            return 5;
        } else {
            LOGGER_INFO("Detected screen %u as preferred", wm->screenp);
        }

        /* Handle surfaces */
        LOGGER_TRACE("Allocating memory for surface structures", L_NARG);
        wm->surfaces = list_init((void(*)(void *)) surface_destroy);
        if (wm->surfaces == NULL) {
            LOGGER_FATAL("Failed to allocate memory for surfaces array",
                    L_NARG);
            eventq_stop();
            config_destroy(wm->config);
            xcb_disconnect(wm->connection);
            xcb_ewmh_connection_wipe(wm->ewmh);
            free(wm->ewmh);
            free(wm);
            wm = NULL;
            return 5;
        }

        /* Initialize surfaces */
        LOGGER_TRACE("Initializing surface structures", L_NARG);
        /* Maybe there are more surfaces defined in the configuration
         * file, but only those detected will be initialized, hence the
         * 'screen_count' instead of taking the JSON information as
         * true.  In the same way, maybe there are 'n' surfaces, but
         * only want to use the 'm < n' defined in the JSON file. */
        if (wm->config->base.screen_count != screens_detected) {
            LOGGER_NOTICE("Detected %u screen(s); %u" \
                    " specified in the configuration file",
                    screens_detected, wm->config->base.screen_count);
            if (wm->config->base.screen_count >= screens_detected ||
                wm->config->base.screen_count == 0) {
                wm->config->base.screen_count = screens_detected;
            }
            LOGGER_INFO("Setting number of screens to %u",
                    wm->config->base.screen_count);
        } else {
            LOGGER_DEBUG("Setting number of screens to %u",
                    wm->config->base.screen_count);
        }

        /* Not interested in using XCB iterator because it's needed to
         * transverse the screens from 0 to go in the same order as in
         * the configuration file */
        for (unsigned int i = 0; i < screens_detected; ++i) {
            /* Get number of desktops for this surface */
            uint32_t desktops_count =
                wm->config->base.screens[i].desktop_count;
            surface_td *surface =
                surface_init(wm->connection,
                        wm->ewmh, (uint32_t) i,
                        desktops_count, wm->config);
            if (surface == NULL) {
                LOGGER_FATAL("Failed to initialize surface %u", i);
                list_destroy(wm->surfaces);
                eventq_stop();
                config_destroy(wm->config);
                xcb_disconnect(wm->connection);
                xcb_ewmh_connection_wipe(wm->ewmh);
                free(wm->ewmh);
                free(wm);
                wm = NULL;
                return 6;
            }

            LOGGER_TRACE("Inserting surface %u into surface list", i);
            if (list_ins_next(wm->surfaces,
                        list_tail(wm->surfaces),
                        (const void *) surface) != 0) {
                LOGGER_FATAL("Failed to insert surface " \
                        "%u into surface list", i);
                surface_destroy(surface);
                list_destroy(wm->surfaces);
                eventq_stop();
                config_destroy(wm->config);
                xcb_disconnect(wm->connection);
                xcb_ewmh_connection_wipe(wm->ewmh);
                free(wm->ewmh);
                free(wm);
                wm = NULL;
                return 7;
            }

            surface->desktop_count =
                wm->config->base.screens[i].desktop_count;
            surface->desktop_cur =
                wm->config->base.screens[i].desktop_inaugural;

            LOGGER_TRACE("Setting desktop %u as the startup desktop" \
                    " on surface %u", surface->desktop_cur, i);
        }

        /* Subscribe to root window events (MUST be done before loop) */
        /* NOTE: Fails with FATAL log if another win. manager is running */
        if (s_wm_subscribe_root_events() != 0) {
            list_destroy(wm->surfaces);
            eventq_stop();
            config_destroy(wm->config);
            xcb_disconnect(wm->connection);
            xcb_ewmh_connection_wipe(wm->ewmh);
            free(wm->ewmh);
            free(wm);
            wm = NULL;
            return 8;
        }

        /* Begin! */
        LOGGER_TRACE("Setting 'is_running' status flag to 'true'",
                L_NARG);
        wm->is_running = true;
        s_wm_loop();

        return 0;
    }

    return -1;
}


/* Destroy window manager instance */
int wm_stop(void)
{
    LOGGER_DEBUG("Deallocating structure for window manager", L_NARG);
    if (wm == NULL) {
        return 1;
    }

    /* Stop event priority queue FIRST */
    /* NOTE: The event thread processes events that hold raw pointers to
     *       client and desktop objects.  Destroying surfaces (and their
     *       clients) while the event thread is still running would
     *       cause use-after-free errors detected by 'AddressSanitizer' */
    eventq_stop();

    /* Deallocate every surface and its contents */
    LOGGER_TRACE("Deallocating surfaces in window manager", L_NARG);
    list_destroy(wm->surfaces);

    /* Deallocate EWMH structure */
    LOGGER_TRACE("Deallocating EWMH structure", L_NARG);
    xcb_ewmh_connection_wipe(wm->ewmh);
    free(wm->ewmh);

    /* Destroy configuration structure */
    config_destroy(wm->config);

    /* Free the text renderer */
    text_renderer_destroy();

    /* Close the X display connection */
    LOGGER_TRACE("Closing X display", L_NARG);
    xcb_disconnect(wm->connection);

    /* Free the window manager structure itself */
    LOGGER_TRACE("Destroying window manager", L_NARG);
    free(wm);
    wm = NULL;

    LOGGER_DEBUG("Window manager has been destroyed", L_NARG);

    return 0;
}


/* Request a graceful stop of the main window manager loop */
int wm_request_stop(void)
{
    if (wm == NULL) {
        return 1;
    }

    wm->is_running = false;
    return 0;
}


/* Reload the configuration */
int wm_action_config_reload(void)
{
    LOGGER_DEBUG("Reloading configuration", L_NARG);

    if (wm == NULL || wm->config == NULL) {
        LOGGER_ERROR("Window manager is not initialized", L_NARG);
        return 1;
    }

    if (config_load(wm->config, wm->config_dir_prefix) != 0) {
        LOGGER_ERROR("Failed to reload configuration", L_NARG);
        return 1;
    }

    /* 'config_load' only refreshes 'wm->config'.  Already-existing
     * desktops cached their background color once, when they were
     * created (vid. 'desktop_init', in 'src/desktop.c'), so they have
     * to be re-synchronized here with the freshly reloaded values and
     * marked as outdated to actually get redrawn on the next update */
    for (list_item_td *surface_node = list_head(wm->surfaces);
            surface_node != NULL;
            surface_node = list_next(surface_node)) {
        surface_td *surface_cur = (surface_td *) list_data(surface_node);
        struct config_base_s *config_base = &(wm->config->base);

        if (surface_cur->id >= config_base->screen_count) {
            continue;   /* Screen no longer present in reloaded config */
        }

        for (uint32_t i = 0; i < surface_cur->desktop_count; ++i) {
            desktop_td *desktop_cur =
                surface_desktop_get(surface_cur, i);

            if (desktop_cur == NULL ||
                    i >= config_base->screens[surface_cur->id]
                        .desktop_count) {
                continue;   /* Desktop no longer present */
            }

            desktop_cur->background.is_image = false;
            desktop_cur->background.bg.color =
                config_base->screens[surface_cur->id]
                    .desktops[i].settings.background.color;
            desktop_cur->is_outdated = true;
        }

        surface_cur->is_outdated = true;
    } /* ! for (surface_node) */

    LOGGER_INFO("Configuration reloaded successfully", L_NARG);

    return 0;
}


/* Save the current configuration */
int wm_action_config_save(void)
{
    LOGGER_DEBUG("Saving configuration", L_NARG);

    if (wm == NULL || wm->config == NULL) {
        LOGGER_ERROR("Window manager is not initialized", L_NARG);
        return 1;
    }

    /* Configuration saving is not implemented */
    LOGGER_NOTICE("Configuration saving is not yet implemented", L_NARG);

    return 1;
}


/* Insert a surface into the surface list */
int wm_action_surface_ins(void)
{
    LOGGER_DEBUG("Inserting new surface", L_NARG);

    if (wm == NULL) {
        LOGGER_ERROR("Window manager is not initialized", L_NARG);
        return 1;
    }

    /* Dynamic surface insertion requires multi-monitor detection */
    LOGGER_NOTICE("Dynamic surface insertion is not yet implemented",
            L_NARG);

    return 1;
}


/* Remove a surface from the surface list */
int wm_action_surface_rem(void)
{
    LOGGER_DEBUG("Removing surface", L_NARG);

    if (wm == NULL) {
        LOGGER_ERROR("Window manager is not initialized", L_NARG);
        return 1;
    }

    /* Dynamic surface removal requires multi-monitor detection */
    LOGGER_NOTICE("Dynamic surface removal is not yet implemented",
            L_NARG);

    return 1;
}


/* Perform exit actions before stopping the window manager */
int wm_action_exit(void)
{
    LOGGER_DEBUG("Executing exit actions", L_NARG);

    if (wm == NULL) {
        return 1;
    }

    wm_request_stop();

    return 0;
}
