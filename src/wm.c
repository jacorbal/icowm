/**
 * @file wm.c
 *
 * @brief Window manager singleton lifecycle and query helpers
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
#include <stdio.h>      /* snprintf */
#include <stdint.h>
#include <stdlib.h>     /* NULL, free, malloc */
#include <string.h>     /* memcpy */

/* XCB includes */
#include <xcb/xcb.h>
#include <xcb/xcb_ewmh.h>

/* ADT includes */
#include <adt/list.h>

/* Session includes */
#include <session.h>

/* IPC includes */
#include <ipc.h>

/* Rules includes */
#include <rules.h>

/* Render includes */
#include <render/text.h>

/* Default initial values */
#include <defs/dialog.h>
#include <defs/ewmh.h>
#include <defs/uistr.h>
#include <i18n.h>

/* Project includes */
#include <config.h>
#include <config/memguard.h>
#include <desktop.h>
#include <logger.h>
#include <lookup.h>
#include <loop.h>
#include <memguard.h>
#include <startup.h>
#include <surface.h>
#include <sn.h>
#include <systray.h>
#include <xsettings.h>

/* Utils includes */
#include <utils/config/json.h>
#include <utils/sysmem.h>

/* Menu includes */
#include <menu/context/rootmenu.h>
#include <menu/dialog/message.h>

/* Input includes */
#include <input/mouse.h>

/* Local includes */
#include <wm.h>
#include <wm/internal.h>
#include <wm/shutdown.h>


/* Though variable static dost often lurk near,
 * In shadows of scope, few e'er call thee their own,
 * Thy global existence, to none dost bring fear,
 * A sentinel watching, though thou art alone. */
wm_td *wm = NULL;   /**< Singleton window manager instance */


/**
 * @brief Release every initialized window-manager subsystem
 *
 * Frees only the members that were successfully initialized so it can
 * be used from partial startup failure paths as well as normal
 * teardown.
 *
 * @note Complexity: @e O(n), where @e n is the number of managed
 *       surfaces currently stored in @c wm->surfaces
 */
static void s_wm_cleanup(void)
{
    if (wm == NULL) {
        return;
    }

    systray_shutdown(wm);
    xsettings_shutdown(wm);
    ipc_destroy();
    mouse_destroy_resize_cursors(wm->connection);

    if (wm->session != NULL) {
        if (wm->is_emergency_exit) {
            LOGGER_NOTICE("Emergency exit in effect;" \
                    " pending session hooks will not be run", L_NARG);
        } else {
            session_run_hook(wm->session, wm->connection,
                    SESSION_HOOK_EXIT);
        }
    }

    text_renderer_destroy();

    if (wm->surfaces != NULL) {
        list_destroy(wm->surfaces);
        wm->surfaces = NULL;
    }

    if (wm->rules != NULL) {
        rules_destroy(wm->rules);
        wm->rules = NULL;
    }

    if (wm->session != NULL) {
        session_destroy(wm->session);
        wm->session = NULL;
    }

    rootmenu_free_menu_json();

    if (wm->config != NULL) {
        config_destroy(wm->config);
        wm->config = NULL;
    }

    if (wm->ewmh != NULL) {
        xcb_ewmh_connection_wipe(wm->ewmh);
        free(wm->ewmh);
        wm->ewmh = NULL;
    }

    if (wm->connection != NULL) {
        if (wm->ewmh_support_win != XCB_NONE) {
            xcb_destroy_window(wm->connection, wm->ewmh_support_win);
            wm->ewmh_support_win = XCB_NONE;
        }
        xcb_disconnect(wm->connection);
        wm->connection = NULL;
    }

    free(wm);
    wm = NULL;
}


/* Initialize a window manager instance */
int wm_start(const char *display_name, const char *config_dir_prefix,
        uint32_t restricted_memory_mib, bool ipc_disabled)
{
    uint32_t screens_detected;
    uint32_t screens_managed;
    xcb_screen_iterator_t it;

    LOGGER_DEBUG("Initializing window manager", L_NARG);

    if (wm != NULL) {
        return -1;
    }

    /* Checked before allocating anything at all, so refusing to start
     * costs as little as possible: restricted-memory mode promises a
     * ceiling on this process's own future usage (see
     * 'memguard.h'), and that promise is meaningless if the system
     * does not even have that much memory free right now for this
     * process to grow into in the first place. */
    if (restricted_memory_mib > 0u) {
        uint32_t available_mib;

        LOGGER_NOTICE("Entering mode of restricted memory" \
                " (ceiling=%u MiB)", (unsigned int) restricted_memory_mib);

        if (sysmem_available_mib(&available_mib) &&
                available_mib < restricted_memory_mib) {
            LOGGER_FATAL("Restricted-memory mode: only %u MiB of" \
                    " system memory is available, less than the" \
                    " configured %u MiB ceiling; refusing to start",
                    (unsigned int) available_mib,
                    (unsigned int) restricted_memory_mib);
            return 11;
        }
    }

    wm = malloc(sizeof(wm_td));
    if (wm == NULL) {
        LOGGER_FATAL("Failed to allocate memory for window manager",
                L_NARG);
        return 1;
    }

    /* Zero-initialize all pointer fields so s_wm_cleanup can check
     * each one safely during any subsequent error path */
    wm->connection = NULL;
    wm->ewmh = NULL;
    wm->ewmh_support_win = XCB_NONE;
    wm->config = NULL;
    wm->rules = NULL;
    wm->session = NULL;
    wm->surfaces = NULL;
    wm->keysyms = NULL;
    wm->randr_available = false;
    wm->randr_base_event = 0u;
    wm->sync_available = false;
    wm->sync_base_event = 0u;
    wm->is_emergency_exit = false;
    wm->restricted_memory_mib = restricted_memory_mib;

    /* Called this early, before any surface or desktop gets set up
     * below, specifically so 'memguard_max_clients' already has a
     * real answer by the time 'desktop_init' asks it how many
     * positions to size a desktop's own client hash table to; see
     * 'desktop_init' itself in desktop.c. */
    memguard_init(wm->restricted_memory_mib);

    /* Restricted-memory mode already forces every theme font to some
     * variant of "fixed" ('config/memguard.h'), which always resolves
     * as an X core font on its own, so the heavier 'xcb-render'/
     * FreeType2/fontconfig backend is never actually needed for the
     * rest of this process's own life; see
     * 'text_renderer_disable_glyph_backend''s comment for why this
     * closes a gap that font-name matching alone could not. */
    if (wm->restricted_memory_mib > 0u) {
        text_renderer_disable_glyph_backend();
    }

    LOGGER_DEBUG("Opening X display", L_NARG);
    wm->connection = xcb_connect(display_name,
            (int *) &(wm->screenp));

    if (xcb_connection_has_error(wm->connection)) {
        if (display_name == NULL) {
            LOGGER_FATAL("Failed to open X display", L_NARG);
        } else {
            LOGGER_FATAL("Failed to open X display '%s'",
                    display_name);
        }
        s_wm_cleanup();
        return 2;
    }

    LOGGER_DEBUG("Allocating memory for EWMH connection", L_NARG);
    wm->ewmh = malloc(sizeof(xcb_ewmh_connection_t));
    if (wm->ewmh == NULL) {
        LOGGER_FATAL("Failed to allocate memory for EWMH connection",
                L_NARG);
        s_wm_cleanup();
        return 1;
    }

    if (!xcb_ewmh_init_atoms_replies(wm->ewmh,
                xcb_ewmh_init_atoms(wm->connection, wm->ewmh),
                NULL)) {
        LOGGER_FATAL("Failed to initialize EWMH atoms", L_NARG);
        s_wm_cleanup();
        return 10;
    }

    /* Two entirely separate configuration paths, not one with
     * restricted-memory branches woven through it: 'config_init'/
     * 'config_load' know nothing about restricted-memory mode at all,
     * and 'config_init_memguard'/'config_load_memguard' (config/
     * memguard.h) know nothing about an ordinary session's own
     * 'config.json'.  Only the choice of which pair to call lives
     * here. */
    wm->config = (wm->restricted_memory_mib > 0u)
        ? config_init_memguard()
        : config_init();
    if (wm->config == NULL) {
        s_wm_cleanup();
        return 3;
    }

    LOGGER_DEBUG("Loading configuration into window manager", L_NARG);
    wm->config_dir_prefix = config_dir_prefix;
    json_syntax_errors_reset();
    config_missing_theme_reset();
    if (wm->restricted_memory_mib > 0u) {
        config_load_memguard(wm->config, wm->config_dir_prefix);
    } else {
        config_load(wm->config, wm->config_dir_prefix);
    }

    wm->rules = rules_init();
    if (wm->rules != NULL) {
        (void) rules_load(wm->rules, wm->config_dir_prefix);
    }

    wm->session = session_init();
    if (wm->session != NULL) {
        (void) session_load(wm->session, wm->config_dir_prefix);
    }

    /* menu.json has no dedicated 'struct' of its own the way rules
     * and session do (nothing else in the window manager needs it
     * outside of the root menu itself), so it is owned by
     * menu/context/rootmenu.c and loaded directly rather than through
     * an 'init' handle here. */
    rootmenu_load_menu_json(wm->config_dir_prefix);

    it = xcb_setup_roots_iterator(xcb_get_setup(wm->connection));
    screens_detected = 0;
    for (; it.rem > 0; xcb_screen_next(&it)) {
        screens_detected++;
    }

    if (screens_detected == 0) {
        LOGGER_FATAL("No screens detected", L_NARG);
        s_wm_cleanup();
        return 5;
    } else {
        LOGGER_INFO("Detected screen %u as preferred", wm->screenp);
    }

    LOGGER_DEBUG("Allocating memory for surface structures", L_NARG);
    wm->surfaces = list_init((void(*)(void *)) surface_destroy);
    if (wm->surfaces == NULL) {
        LOGGER_FATAL("Failed to allocate memory for surfaces array",
                L_NARG);
        s_wm_cleanup();
        return 6;
    }

    LOGGER_DEBUG("Initializing surface structures", L_NARG);
    if (wm->config->base.screen_count != screens_detected) {
        LOGGER_NOTICE("Detected %u screen(s); %u" \
                " specified in the configuration file",
                screens_detected, wm->config->base.screen_count);
        if (wm->config->base.screen_count >= screens_detected ||
                wm->config->base.screen_count == 0) {
            wm->config->base.screen_count = screens_detected;
        }
        LOGGER_DEBUG("Setting number of screens to %u",
                wm->config->base.screen_count);
    } else {
        LOGGER_DEBUG("Setting number of screens to %u",
                wm->config->base.screen_count);
    }

    if (wm->config->base.screen_count > CONFIG_MAX_SCREENS) {
        LOGGER_NOTICE("Configured %u screen(s), but this build" \
                " supports up to %u; clamping",
                wm->config->base.screen_count, CONFIG_MAX_SCREENS);
        wm->config->base.screen_count = CONFIG_MAX_SCREENS;
    }
    screens_managed = wm->config->base.screen_count;

    for (unsigned int i = 0; i < screens_managed; ++i) {
        uint32_t desktops_count =
            wm->config->base.screens[i].desktop_count;
        surface_td *surface =
            surface_init(wm->connection, wm->ewmh,
                    (uint32_t) i, desktops_count, wm->config);
        if (surface == NULL) {
            LOGGER_FATAL("Failed to initialize surface %u", i);
            s_wm_cleanup();
            return 7;
        }

        LOGGER_TRACE("Inserting surface %u into surface list", i);
        if (list_ins_next(wm->surfaces, list_tail(wm->surfaces),
                    (const void *) surface) != 0) {
            LOGGER_FATAL("Failed to insert surface %u" \
                    " into surface list", i);
            surface_destroy(surface);
            s_wm_cleanup();
            return 8;
        }

        surface->desktop_count =
            wm->config->base.screens[i].desktop_count;
        surface->desktop_cur =
            wm->config->base.screens[i].desktop_inaugural;

        LOGGER_TRACE("Setting desktop %u as the startup desktop" \
                " on surface %u", surface->desktop_cur, i);
    }

    if (startup_subscribe_root_events(wm) != 0) {
        s_wm_cleanup();
        return 9;
    }

    mouse_create_resize_cursors(wm->connection);
    (void) startup_randr_init(wm);
    (void) startup_subscribe_randr_events(wm);
    (void) startup_sync_init(wm);

    /* Not fatal if it fails, the same reasoning as EWMH root
     * metadata just below: a working window manager without its
     * own control socket is still a working window manager, just
     * one external tools cannot script against for this run.
     * 'ipc_disabled' skips it outright instead, for anyone who
     * would rather it never come up at all than have it come up and
     * be reachable by any other local process running as the same
     * user. */
    if (ipc_disabled) {
        LOGGER_INFO("IPC control socket disabled ('-s')", L_NARG);
    } else if (ipc_init() != 0) {
        LOGGER_WARNING("Failed to initialize IPC control socket",
                L_NARG);
    }

    if (wm_ewmh_init() != 0) {
        LOGGER_WARNING("Failed to initialize EWMH root metadata",
                L_NARG);
    }
    wm_ewmh_sync();

    systray_init(wm);
    xsettings_init(wm);
    sn_set_timeout_seconds(
            wm->config->base.startup_notification.timeout_seconds);

    LOGGER_DEBUG("Setting running status flag to" \
            " an unquestionable 'true'", L_NARG);
    wm->is_running = true;
    if (wm->session != NULL) {
        session_run_hook(wm->session, wm->connection, SESSION_HOOK_START);
    }

    /* Any JSON file that failed to parse during the load just above
     * (config.json, bindings.json, a theme file, randr.json, rules.
     * json, session.json, menu.json) gets a combined warning dialog
     * here, ahead of restricted-memory mode's own announcement right
     * below: a configuration silently reverted to defaults is more
     * urgent to know about than which mode is active.
     * menu_message_dialog_show only ever shows one dialog at a time,
     * so if this one fires, the one below simply does not, for this
     * run; nothing else about that mode's own announcement is lost by
     * that, only delayed to whenever it is checked again (another
     * load or reload). */
    wm_warn_json_syntax_errors();

    /* Restricted-memory mode's own presence is announced once, right
     * before entering the main loop, so it is never a silent surprise
     * to whoever is sitting at the keyboard: only the log otherwise
     * says anything about it. */
    if (wm->restricted_memory_mib > 0u && wm->surfaces != NULL &&
            !list_is_empty(wm->surfaces)) {
        menu_message_dialog_show(wm->connection,
                (surface_td *) list_data(list_head(wm->surfaces)),
                wm->config,
                _(STR_WM_RESTRICTED_MEMORY_MODE_ANNOUNCE),
                MENU_MSG_LEVEL_INFO);
    }

    loop_run(wm);

    return 0;
}


/* Warn through a message dialog if any JSON file loaded during the
 * last configuration load or reload failed to parse */
void wm_warn_json_syntax_errors(void)
{
    uint32_t count;
    char message[DIALOG_MSG_RAW_MAX_LENGTH];
    size_t offset;

    count = json_syntax_errors_count();
    if (count == 0u || wm == NULL || wm->connection == NULL ||
            wm->surfaces == NULL || list_is_empty(wm->surfaces)) {
        return;
    }

    LOGGER_WARNING("Configuration: %u file(s) failed to parse;" \
            " reverted to default values for each", count);

    offset = (size_t) snprintf(message, sizeof(message),
            (count == 1u)
                ? _(STR_WM_JSON_SYNTAX_ERROR_SINGLE_FMT)
                : _(STR_WM_JSON_SYNTAX_ERROR_MULTIPLE_FMT),
            json_syntax_errors_get(0u));
    for (uint32_t i = 1u; i < count && offset < sizeof(message); ++i) {
        int written = snprintf(message + offset, sizeof(message) - offset,
                ", '%s'", json_syntax_errors_get(i));
        if (written < 0) {
            break;
        }
        offset += (size_t) written;
    }

    /* The 'count == 1' message above already ends in its own
     * sentence-closing period; the 'count > 1' one does not, since
     * its file list (built by the loop just above) has no fixed end
     * to attach one to ahead of time.  Closing it here, only in that
     * second case, keeps whatever gets appended after this point (the
     * missing-theme note below) starting a properly new sentence
     * rather than running directly into the last filename. */
    if (count > 1u && offset < sizeof(message)) {
        int written = snprintf(message + offset, sizeof(message) - offset,
                ".");
        if (written > 0) {
            offset += (size_t) written;
        }
    }

    /* A theme file config.json names but that turns out not to exist
     * at all is never worth a dialog on its own (an ordinary, silent
     * reason to fall back to the built-in default, the same as any
     * other missing file), but is worth mentioning here as an extra
     * line, since a dialog is already being shown for some other
     * parse failure regardless.  Deliberately not added when 'count'
     * itself is 0: this function would not even reach this point in
     * that case (see the early return above), so this is really just
     * documenting why nothing needs to special-case that here. */
    if (offset < sizeof(message)) {
        const char *missing_theme = config_missing_theme_get();

        if (missing_theme != NULL) {
            snprintf(message + offset, sizeof(message) - offset,
                    _(STR_WM_MISSING_THEME_FMT), missing_theme);
        }
    }

    menu_message_dialog_show(wm->connection,
            (surface_td *) list_data(list_head(wm->surfaces)),
            wm->config, message, MENU_MSG_LEVEL_WARNING);

    /* Cleared once shown, so reopening the root menu (or reloading
     * configuration) with the same still-broken file does not show
     * the exact same dialog again on every attempt; a fresh problem,
     * in this file or another, starts a fresh list of its own the
     * next time something calls 'json_syntax_errors_reset' before
     * loading. */
    json_syntax_errors_reset();
    config_missing_theme_reset();
}


/* Destroy window manager instance */
int wm_stop(void)
{
    LOGGER_DEBUG("Deallocating structure for window manager", L_NARG);

    if (wm == NULL) {
        return 1;
    }

    LOGGER_DEBUG("Running window manager cleanup", L_NARG);
    s_wm_cleanup();

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


/* Request a coordinated stop, giving managed clients a chance to
 * close first */
void wm_request_graceful_stop(void)
{
    wm_shutdown_begin();
}


/* Retrieve the desktop that currently contains the given client */
desktop_td *wm_get_client_desktop(const client_td *client)
{
    desktop_td *desktop = NULL;

    if (client == NULL || wm == NULL) {
        return NULL;
    }

    (void) lookup_find_client(wm->surfaces, client->id, NULL, &desktop);
    return desktop;
}


/* Return the managed surface with the given identifier */
surface_td *wm_get_surface_by_id(uint32_t surface_id)
{
    if (wm == NULL || wm->surfaces == NULL) {
        return NULL;
    }

    for (list_item_td *snode = list_head(wm->surfaces);
            snode != NULL; snode = list_next(snode)) {
        surface_td *surface = (surface_td *) list_data(snode);
        if (surface != NULL && surface->id == surface_id) {
            return surface;
        }
    }

    return NULL;
}


/* Query whether the XSync extension is available on this server */
bool wm_sync_available(void)
{
    return (wm != NULL) && (wm->sync_available);
}


/* Return the list of surfaces managed by the singleton window manager */
list_td *wm_get_surfaces(void)
{
    return (wm != NULL) ? wm->surfaces : NULL;
}


/* Find which managed surface a given desktop belongs to */
surface_td *wm_get_desktop_surface(const desktop_td *desktop)
{
    if (desktop == NULL || wm == NULL || wm->surfaces == NULL) {
        return NULL;
    }

    for (list_item_td *snode = list_head(wm->surfaces); snode != NULL;
            snode = list_next(snode)) {
        surface_td *surface = (surface_td *) list_data(snode);

        if (surface == NULL) {
            continue;
        }
        for (uint32_t i = 0; i < surface->desktop_count; ++i) {
            if (surface_desktop_get(surface, i) == desktop) {
                return surface;
            }
        }
    }

    return NULL;
}


/* Return the active configuration of the singleton window manager
 * instance */
config_td *wm_get_config(void)
{
    return (wm != NULL) ? wm->config : NULL;
}


/* Return the configuration directory prefix */
const char *wm_get_config_dir(void)
{
    if (wm == NULL) {
        return NULL;
    }

    return wm->config_dir_prefix;
}


/* Mark the client owner desktop and surface as outdated */
void wm_request_client_redraw(client_td *client)
{
    desktop_td *desktop;

    if (client == NULL || wm == NULL || wm->surfaces == NULL) {
        return;
    }

    /* Mark the individual client so the render pass applies the heavy
     * geometry configure and expose only to this client, avoiding
     * spurious redraws (and visible flicker) in other windows */
    client->is_outdated = true;

    desktop = wm_get_client_desktop(client);
    if (desktop != NULL) {
        desktop->is_outdated = true;
    }

    for (list_item_td *snode = list_head(wm->surfaces);
            snode != NULL; snode = list_next(snode)) {
        surface_td *surface = (surface_td *) list_data(snode);

        if (surface == NULL) {
            continue;
        }
        if (surface->id == client->screen_id) {
            surface->is_outdated = true;
            break;
        }
    } /* ! for (snode) */
}


/* Mark all surfaces and desktops as outdated */
void wm_request_full_redraw(void)
{
    if (wm == NULL || wm->surfaces == NULL) {
        return;
    }

    for (list_item_td *snode = list_head(wm->surfaces);
            snode != NULL; snode = list_next(snode)) {
        surface_td *surface = (surface_td *) list_data(snode);

        if (surface == NULL) {
            continue;
        }

        surface->is_outdated = true;

        for (uint32_t did = 0; did < surface->desktop_count; ++did) {
            desktop_td *desktop = surface_desktop_get(surface, did);
            if (desktop != NULL) {
                desktop->is_outdated = true;
            }
        }
    } /* ! for (snode) */
}


/* Set the emergency exit flag to true */
void wm_enable_emergency_exit(void)
{
    if (wm == NULL) {
        return;
    }
    wm->is_emergency_exit = true;
}
