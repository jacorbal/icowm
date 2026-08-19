/**
 * @file wm/actions.c
 *
 * @brief Window manager action dispatchers implementation
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

/* XCB includes */
#include <xcb/xcb.h>

/* ADT includes */
#include <adt/list.h>
#include <adt/ohtbl.h>

/* Utils includes */
#include <utils/config/json.h>

/* Session includes */
#include <session.h>

/* Rules includes */
#include <rules.h>

/* Input includes */
#include <input/kbd/bind.h>
#include <input/mouse.h>

/* Default initial values */
#include <defs/icon.h>

/* Project includes */
#include <client.h>
#include <config.h>
#include <config/memguard.h>
#include <desktop.h>
#include <logger.h>
#include <surface.h>
#include <cctl/sn.h>
#include <systray.h>
#include <xsettings.h>
#include <enact.h>
#include <lookup.h>

/* Policy includes */
#include <policy/placement.h>

/* Menu includes */
#include <menu/context/rootmenu.h>
#include <menu/dialog/rrsafe.h>

/* Local includes */
#include <wm.h>
#include <wm/internal.h>


/**
 * @brief Resynchronize every already-managed surface, desktop, and
 *        client after a configuration reload
 *
 * A configuration reload updates @p wm's own configuration in place,
 * but anything already derived from it before the reload (a desktop's
 * resolved background color, a client's cached frame dimensions) has
 * to be explicitly recomputed or repainted; nothing else does that on
 * its own just because the underlying configuration changed
 * underneath it.
 *
 * @param wm Window manager instance
 *
 * @note Complexity: @e O(s * d * c), where @e s is the number of
 *       surfaces, @e d the number of desktops per surface, and @e c
 *       the number of clients per desktop
 */
static void s_resync_after_reload(wm_td *wm)
{
    config_td *config = wm_config(wm);

    for (list_item_td *snode = list_head(wm_surfaces(wm));
            snode != NULL; snode = list_next(snode)) {
        surface_td *const s = (surface_td *) list_data(snode);
        struct config_base_s *const cb = &(config->base);
        int32_t tray_x;
        int32_t tray_y;
        uint16_t tray_w;
        uint16_t tray_h;
        /* Queried once per surface here, ahead of the desktop/client
         * loop below, rather than once per icon inside it.  This is
         * a synchronous round trip to the X server (see 'systray_get_
         * geometry''s comment), and every icon on this same surface
         * shares the identical tray rectangle regardless. */
        bool tray_visible = systray_get_geometry(s, &tray_x, &tray_y,
                &tray_w, &tray_h);

        if (s->id >= cb->screen_count) {
            continue;
        }

        for (uint32_t i = 0; i < s->desktop_count; ++i) {
            desktop_td *d = surface_desktop_get(s, i);

            if (d == NULL || i >= cb->screens[s->id].desktop_count) {
                continue;
            }

            /* A desktop that set its own 'background-color' in the
             * just-reloaded 'config.json' keeps it; one that did not
             * (still holding 'WM_DESKTOP_BG_COLOR_UNSET', the same
             * sentinel every entry starts with) falls back to the
             * just-reloaded theme's own 'desktop.color.background'
             * instead, mirroring 'desktop_init''s own fallback
             * exactly.  Assigning the sentinel value itself as though
             * it were a real color (as this block used to, before this
             * check existed) renders as black, since its low 24 bits
             * are all zero: only the top byte, some other flag, is
             * actually set. */
            if (!d->background.is_image &&
                    !d->background.use_root_pixmap) {
                uint32_t new_color = cb->screens[s->id].desktops[i]
                    .settings.background.color;

                d->background.bg.color =
                    (new_color == WM_DESKTOP_BG_COLOR_UNSET)
                    ? config->theme.desktop.color.background
                    : new_color;
            }

            /* Resize every already-decorated client's frame to match
             * whatever 'window.titlebar.height' and border width the
             * just-reloaded theme now specifies.  'client->theme' is
             * a shared pointer into 'wm_config(wm)->theme' that
             * 'config_load' above already updated in place, so colors,
             * fonts, and button lists all take effect on their own the
             * next time each client repaints; only the cached
             * 'title_height'/'frame_extents' (and the frame size that
             * has to match them) need this explicit resync, since
             * nothing else re-derives those from the theme on its own
             * once a client is already mapped. */
            if (d->clients != NULL) {
                void *elem;

                ohtbl_foreach(d->clients, elem) {
                    client_td *const c = (client_td *) elem;

                    if (c != NULL) {
                        client_theme_layout_resync(c,
                                d->client_active_id == c->id);
                        /* 'client_theme_layout_resync' above only marks
                         * 'c' outdated (which is what actually makes
                         * the render pass repaint its border and
                         * titlebar, see 'desktop_render_clients') when
                         * the border width or titlebar height
                         * numerically changed; a reload that only
                         * changed a color or font, with every dimension
                         * unchanged, would otherwise never repaint
                         * anything already on screen even though
                         * 'client->theme' itself already points at the
                         * freshly reloaded values. */
                        wm_request_client_redraw(c);

                        /* An icon left sitting exactly where the tray
                         * used to be, before this same reload just
                         * moved it there, is never otherwise revisited
                         * on its own: nothing else here (or anywhere
                         * else) re-checks an already-placed icon's own
                         * position against the tray's, only a fresh
                         * 'place_icon' call or a drag ever does (see
                         * 'icon_avoid_systray_overlap''s comment). */
                        if (tray_visible && c->is_icon_mapped &&
                                c->icon_window != 0u) {
                            int16_t icon_x = c->icon_x;
                            int16_t icon_y = c->icon_y;
                            uint16_t icon_h = (uint16_t)
                                WM_ICON_SQUARE_SIZE;

                            if (c->theme != NULL &&
                                    c->theme->icon.is_captioned) {
                                icon_h = (uint16_t) (icon_h +
                                        (uint16_t)
                                        WM_ICON_CAPTION_HEIGHT);
                            }

                            if (icon_avoid_systray_overlap(&icon_x,
                                        &icon_y,
                                        (uint16_t) WM_ICON_SQUARE_SIZE,
                                        icon_h, tray_x, tray_y, tray_w,
                                        tray_h, &d->workarea)) {
                                uint32_t vals[2];

                                //c->icon_x = icon_x;   /* 'tis a no-op */
                                c->icon_y = icon_y;
                                vals[0] = (uint32_t) icon_x;
                                vals[1] = (uint32_t) icon_y;
                                xcb_configure_window(wm_connection(wm),
                                        c->icon_window,
                                        XCB_CONFIG_WINDOW_X |
                                        XCB_CONFIG_WINDOW_Y,
                                        vals);
                            }
                        }
                    }
                }
            }

            d->is_outdated = true;
        }

        s->is_outdated = true;

        /* 'config->desktops.margins' just reloaded above (config_load,
         * called from 'wm_action_config_reload' before this function
         * runs) is not something anything else here re-derives on its
         * own: 'desktop->workarea' (what maximize and placement
         * actually use) is only otherwise recomputed on its own trigger
         * (a client mapping/unmapping, a RandR change...), none of
         * which a reload is. Without this, a changed margin would stay
         * invisible until one of those unrelated triggers happened to
         * fire, e.g., by switching desktops (switching away and back
         * hides and shows clients, an unmap/map pair that reaches
         * 'surface_refresh_workareas' as a side effect of something
         * else entirely). */
        surface_refresh_workareas(s);
    }
}


/* Rearrange every visible window on the current desktop */
void wm_action_rearrange(wm_td *wm, surface_td *surface)
{
    desktop_td *desktop;

    if (surface == NULL) {
        return;
    }

    desktop = lookup_current_desktop(surface);
    if (desktop == NULL) {
        return;
    }

    enact_desktop_clients_rearrange(wm, surface, desktop);
}


/* Reload the configuration */
int wm_action_config_reload(wm_td *wm)
{
    int load_result;
    config_td *config;
    const char *config_dir_prefix;

    LOGGER_DEBUG("Reloading configuration", L_NARG);

    config = wm_config(wm);
    if (config == NULL) {
        LOGGER_ERROR("Window manager is not initialized", L_NARG);
        return 1;
    }

    config_dir_prefix = wm_config_dir_prefix(wm);

    json_syntax_errors_reset();
    config_missing_theme_reset();
    /* Same two entirely separate paths 'wm_start' chooses between
     * ('config/memguard.h'); a reload takes the same one it started
     * with, since 'wm_restricted_memory_mib(wm)' never changes for
     * the life of the process. */
    load_result = (wm_restricted_memory_mib(wm) > 0u)
        ? config_load_memguard(config, config_dir_prefix)
        : config_load(config, config_dir_prefix);
    if (load_result != 0) {
        LOGGER_ERROR("Failed to reload configuration", L_NARG);
        /* Whatever caused the load to fail outright is far more likely
         * to be a syntax error introduced while editing an
         * already-working configuration file than the file simply not
         * existing at all, unlike at first startup; worth surfacing
         * here even on this early-failure path, not just after
         * a successful reload below. */
        wm_json_syntax_errors_warn();
        return 1;
    }

    /* Apply any 'randr.json' output profile that changed since the last
     * load, offering a chance to revert it (see 'dialog_rrsafe_ show')
     * before 's_resync_after_reload' below, so any surface or client
     * resync there already reflects the new screen geometry if RandR
     * itself just changed it.  Every surface still gets its own
     * profiles applied even when more than one changes, but only the
     * first one to actually change is snapshotted and offered the
     * confirm dialog: 'menu_confirm_dialog' allows only one instance
     * open at a time, and 'surface_action_revert_randr_profiles' itself
     * remembers only the single most recent snapshot, so a second
     * surface changing in the same reload has no dialog of its own to
     * revert through regardless. */
    if (wm_surfaces(wm) != NULL) {
        bool dialog_shown = false;

        for (list_item_td *node = list_head(wm_surfaces(wm));
                node != NULL; node = list_next(node)) {
            surface_td *const s = (surface_td *) list_data(node);
            bool changed = surface_action_apply_randr_profiles(s,
                    !dialog_shown);

            if (changed && !dialog_shown) {
                dialog_rrsafe_show(wm_connection(wm), s, config);
                dialog_shown = true;
            }
        }
    }

    /* Re-establish keyboard/mouse binding grabs from the just-reloaded
     * configuration's own bindings: 'config_load' above already
     * refreshed that in-memory data (it loads 'bindings.json' too, not
     * just 'config.json'), but the X server grabs 'keyboard_load' and
     * 'mouse_load' set up at startup are a separate, one-time action
     * that nothing was re-running on reload, so a changed binding had
     * no actual effect until the window manager was restarted.
     *
     * Both functions release every grab they previously made before
     * re-grabbing, so a binding that changed does not end up with both
     * its old and new key/button combination active at once. */
    if (wm_keysyms(wm) != NULL) {
        keyboard_load(wm_surfaces(wm), wm_keysyms(wm), config);
    }
    mouse_load(wm_surfaces(wm), config);

    /* Reload the systray reacting to config. reload */
    systray_reload(wm);

    /* Reload also XSETTINGS */
    xsettings_reload(wm);

    cctl_sn_set_timeout_seconds(
            config->base.startup_notification.timeout_seconds);

    if (wm_rules(wm) != NULL) {
        (void) rules_load(wm_rules(wm), config_dir_prefix);
    }
    if (wm_session(wm) != NULL) {
        (void) session_load(wm_session(wm), config_dir_prefix);
    }
    rootmenu_menu_json_load(config_dir_prefix);

    s_resync_after_reload(wm);

    LOGGER_INFO("Configuration reloaded successfully", L_NARG);
    wm_json_syntax_errors_warn();
    if (wm_session(wm) != NULL) {
        session_run_hook(wm_session(wm), wm_connection(wm),
                SESSION_HOOK_RELOAD);
    }

    return 0;
}


/* Perform exit actions before stopping the window manager */
int wm_action_exit(wm_td *wm)
{
    LOGGER_DEBUG("Executing exit actions", L_NARG);

    if (wm == NULL) {
        return 1;
    }

    wm_request_stop();
    return 0;
}
