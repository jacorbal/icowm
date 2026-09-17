/**
 * @file menu/context/iconmenu.c
 *
 * @brief Icon context menu implementation
 *
 * Builds and manages the right-click context menu for an iconified
 * client's icon.  Actions are dispatched by calling the matching
 * @c enact function directly, taking effect at once.  "Send to
 * desktop", "Send to page", and "Send to monitor" are built through
 * the same shared submodules the window context menu uses, so the
 * two menus can never drift apart on what those submenus offer or
 * how they behave.
 */
/*
 * Copyright (c) 2026, J. A. Corbal.
 * All rights reserved.
 *
 * This file is licensed under the 'ISC License'.
 * Read the 'LICENSE' file in the root of this repository for details.
 */

/* System includes */
#include <stdbool.h>
#include <stdint.h>
#include <string.h>     /* memset */

/* XCB includes */
#include <xcb/xcb.h>

/* Default initial values */
#include <defs/uistr.h>

/* Utils includes */
#include <utils/safe/safestr.h>

/* Render includes */
#include <render/icon.h>

/* Project includes */
#include <client.h>
#include <config.h>
#include <desktop.h>
#include <enact.h>
#include <enact/client.h>
#include <i18n.h>
#include <stage.h>

/* Menu includes */
#include <menu/context/ctxmenu.h>
#include <menu/context/ctxmenu/tree.h>
#include <menu/context/submenu/desktop.h>
#include <menu/context/submenu/monitor.h>
#include <menu/context/submenu/page.h>
#include <menu/dialog/inspect.h>

/* Local includes */
#include <menu/context/iconmenu.h>


/** Singleton root menu state */
static ctxmenu_state_td s_root;

/** Entries for the top-level icon context menu */
static ctxmenu_entry_td s_entries[ICONMENU_FIXED_ENTRIES];

/** Pointer to the stage (valid while the menu is open) */
static stage_td *s_stage = NULL;

/** Active configuration (valid while the menu is open) */
static const config_td *s_config = NULL;

/** Client the menu is currently showing, or NULL when closed */
static client_td *s_target_client = NULL;


/**
 * @brief Fill in one plain command entry
 *
 * @param e           Entry to fill
 * @param label       Entry label, already translated
 * @param cb          Activation callback
 * @param userdata    Passed through to @p cb unchanged
 * @param is_disabled Whether the entry starts disabled
 *
 * @note Complexity: @e O(1)
 */
static void s_entry_command(ctxmenu_entry_td *e, const char *label,
        void (*cb)(xcb_connection_t *, void *), void *userdata,
        bool is_disabled)
{
    memset(e, 0, sizeof(*e));
    e->type = CTXMENU_COMMAND;
    safe_strncpy(e->label, label, sizeof(e->label) - 1u);
    e->on_activate = cb;
    e->userdata = userdata;
    e->is_disabled = is_disabled;
}


/**
 * @brief Callback: restore the target client from its icon
 *
 * @param connection XCB connection (unused)
 * @param userdata   Target @c client_td
 *
 * @note Complexity: @e O(1)
 */
static void s_cb_restore(xcb_connection_t *connection, void *userdata)
{
    (void) connection;

    if (userdata == NULL) {
        return;
    }

    enact_client_restore((client_td *) userdata);
}


/**
 * @brief Callback: hide the target client
 *
 * @param connection XCB connection (unused)
 * @param userdata   Target @c client_td
 *
 * @note Complexity: @e O(1)
 */
static void s_cb_hide(xcb_connection_t *connection, void *userdata)
{
    (void) connection;

    if (userdata == NULL) {
        return;
    }

    enact_client_hide((client_td *) userdata);
}


/**
 * @brief Callback: close the target client
 *
 * @param connection XCB connection (unused)
 * @param userdata   Target @c client_td
 *
 * @note Complexity: @e O(1)
 */
static void s_cb_close(xcb_connection_t *connection, void *userdata)
{
    (void) connection;

    if (userdata == NULL) {
        return;
    }

    enact_client_close((client_td *) userdata);
}


/**
 * @brief Callback: open the inspector for the client this menu was
 *        raised over
 *
 * @param connection XCB connection
 * @param userdata   Target @c client_td
 *
 * @note Complexity: @e O(t), where @e t is the client's number of
 *       transient children, which the inspector counts
 */
static void s_cb_inspect(xcb_connection_t *connection, void *userdata)
{
    if (userdata == NULL) {
        return;
    }

    dialog_inspect_show(connection, s_stage, s_config,
            (client_td *) userdata);
}


void iconmenu_show(xcb_connection_t *connection,
        stage_td *stage, desktop_td *desktop, client_td *client,
        struct position_s pos, const config_td *config)
{
    int n;
    int desk_count;
    int page_count;
    int monitor_count;
    ctxmenu_entry_td *desk_entries = NULL;
    ctxmenu_state_td *desk_state = NULL;
    ctxmenu_entry_td *page_entries = NULL;
    ctxmenu_state_td *page_state = NULL;
    ctxmenu_entry_td *monitor_entries = NULL;
    ctxmenu_state_td *monitor_state = NULL;

    if (connection == NULL || stage == NULL || desktop == NULL ||
            client == NULL || config == NULL ||
            client_is_locked(client)) {
        return;
    }

    /* Close any previously open icon context menu */
    iconmenu_close();

    s_stage = stage;
    s_config = config;
    s_target_client = client;

    page_count = ctxmenu_submenu_page_build(stage, desktop, client,
            &page_entries, &page_state);
    desk_count = ctxmenu_submenu_desktop_build(stage, desktop, client,
            &desk_entries, &desk_state);

    /* Build "Send to monitor" submenu, only meaningful (and only shown
     * at all, see below) on a stage with more than one monitor */
    monitor_count = ctxmenu_submenu_monitor_build(stage, desktop,
            client, &monitor_entries, &monitor_state);

    memset(s_entries, 0, sizeof(s_entries));
    n = 0;

    /* Send to desktop (submenu); omitted entirely, not just disabled,
     * on a stage with only one desktop */
    if (desk_count > 0) {
        s_entries[n].type = CTXMENU_SUBMENU;
        safe_strncpy(s_entries[n].label,
                _(STR_WINCMENU_SEND_TO_DESKTOP),
                sizeof(s_entries[n].label) - 1u);
        s_entries[n].items = desk_entries;
        s_entries[n].item_count = desk_count;
        s_entries[n].userdata = desk_state;
        ++n;
    }

    /* Send to page (submenu); omitted entirely on a viewport that
     * cannot pan, and for a sticky client, which is on screen from
     * every origin and so belongs to no one page */
    if (page_count > 0) {
        s_entries[n].type = CTXMENU_SUBMENU;
        safe_strncpy(s_entries[n].label,
                _(STR_WINCMENU_SEND_TO_PAGE),
                sizeof(s_entries[n].label) - 1u);
        s_entries[n].items = page_entries;
        s_entries[n].item_count = page_count;
        s_entries[n].userdata = page_state;
        ++n;
    }

    /* Send to monitor (submenu); omitted entirely, not just disabled,
     * on a stage with only one monitor */
    if (monitor_count > 0) {
        s_entries[n].type = CTXMENU_SUBMENU;
        safe_strncpy(s_entries[n].label,
                _(STR_WINCMENU_SEND_TO_MONITOR),
                sizeof(s_entries[n].label) - 1u);
        s_entries[n].items = monitor_entries;
        s_entries[n].item_count = monitor_count;
        s_entries[n].userdata = monitor_state;
        ++n;
    }

    /* Separator before Restore/Hide, only when at least one of the
     * three submenus above is actually present */
    if (n > 0) {
        s_entries[n].type = CTXMENU_SEPARATOR;
        ++n;
    }

    s_entry_command(&s_entries[n], _(STR_WINCMENU_RESTORE),
            s_cb_restore, client, false);
    ++n;

    s_entry_command(&s_entries[n], _(STR_WINCMENU_HIDE),
            s_cb_hide, client, false);
    ++n;

    /* Separator before the two entries that end the menu */
    s_entries[n].type = CTXMENU_SEPARATOR;
    ++n;

    s_entry_command(&s_entries[n], _(STR_WINCMENU_INSPECT),
            s_cb_inspect, client, false);
    ++n;

    s_entry_command(&s_entries[n], _(STR_WINCMENU_CLOSE),
            s_cb_close, client, false);
    ++n;

    memset(&s_root, 0, sizeof(s_root));
    s_root.window = XCB_WINDOW_NONE;
    s_root.entries = s_entries;
    s_root.entry_count = n;

    ctxmenu_show(connection, stage, &s_root, pos, config);

    /* Nothing else marks 'client' outdated or otherwise revisits its
     * icon on its own here, so without this the icon would go on
     * showing whatever it last displayed until some unrelated event
     * happened to repaint it, the exact same "menu itself does not draw
     * until the pointer happens to cross it" gap 'handler_expose'
     * ('menu/context/iconmenu.h' wiring, 'handler/expose.c') already
     * exists to close for the menu window, just not previously extended
     * to the icon's own selected styling.  'force' is true: nothing
     * about the icon's own geometry or decoration actually changed,
     * only which icon 'iconmenu_target_is' now answers for, which the
     * skip-check inside 'ri_render_client_icon' has no way to know
     * without this. */
    ri_render_client_icon(client, true, true, true);
}


/* Close the icon context menu */
void iconmenu_close(void)
{
    client_td *const was_target = s_target_client;

    ctxmenu_close(&s_root);
    s_stage = NULL;
    s_config = NULL;
    s_target_client = NULL;

    /* The client whose menu just closed needs the same forced repaint
     * 'iconmenu_show' above gives it on the way in, or its icon would
     * go on showing selected styling for a menu that no longer exists,
     * until some unrelated event happened to notice. */
    if (was_target != NULL) {
        ri_render_client_icon(was_target, true, true, true);
    }
}


/* Repaint the icon context menu */
void iconmenu_repaint(xcb_window_t win)
{
    ctxmenu_tree_redraw_window(&s_root, win);
}


/* Handle a button-press event inside the icon context menu */
bool iconmenu_handle_click(xcb_connection_t *connection,
        stage_td *stage, xcb_window_t win, int y,
        const config_td *config)
{
    return ctxmenu_tree_handle_click_window(connection, stage,
            &s_root, win, y, config);
}


/* Query whether the icon context menu is currently open */
bool iconmenu_is_open(void)
{
    return ctxmenu_is_open(&s_root);
}


/* Query whether the icon context menu is currently open for 'client' */
bool iconmenu_target_is(const client_td *client)
{
    return client != NULL && client == s_target_client &&
        ctxmenu_is_open(&s_root);
}


/* Check whether 'win' belongs to the icon context menu hierarchy */
bool iconmenu_owns_window(xcb_window_t win)
{
    return ctxmenu_tree_state_find_for_window(&s_root, win) != NULL;
}


/* Close the icon context menu if it is currently open for 'client' */
void iconmenu_notice_client_destroyed(const client_td *client)
{
    if (client == NULL || s_target_client != client) {
        return;
    }

    /* Not 'iconmenu_close': that also repaints 'client's icon to
     * confirm the menu no longer applies to it, wasted work, on
     * a client already on its way out, and one whose fields nothing
     * here promises are still consistent enough mid-teardown for that
     * repaint to read safely. */
    ctxmenu_close(&s_root);
    s_stage = NULL;
    s_config = NULL;
    s_target_client = NULL;
}


/* Handle a key-press event while the icon context menu is open */
bool iconmenu_handle_keypress(xcb_connection_t *connection,
        stage_td *stage, xcb_keysym_t keysym,
        const config_td *config)
{
    return ctxmenu_tree_handle_keypress_deepest(connection, stage,
            &s_root, keysym, config);
}


/* Handle a pointer-motion event over the icon context menu */
void iconmenu_handle_motion(xcb_window_t win, int x, int y)
{
    ctxmenu_tree_handle_motion_window(&s_root, win, x, y);
}
