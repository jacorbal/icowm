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
#include <stdint.h>

/* ADT includes */
#include <adt/list.h>
#include <adt/ohtbl.h>

/* Session includes */
#include <session.h>

/* Rules includes */
#include <rules.h>

/* Input includes */
#include <input/kbd/bind.h>
#include <input/mouse.h>

/* Project includes */
#include <client.h>
#include <config.h>
#include <desktop.h>
#include <logger.h>
#include <surface.h>
#include <sn.h>
#include <systray.h>
#include <xsettings.h>

/* Local includes */
#include <wm.h>
#include <wm/internal.h>


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

    /* Re-establish keyboard/mouse binding grabs from the just-reloaded
     * 'wm->config->bindings': 'config_load' above already refreshed
     * that in-memory data (it loads 'bindings.json' too, not just
     * 'config.json'), but the X server grabs 'keyboard_load' and
     * 'mouse_load' set up at startup are a separate, one-time action
     * that nothing was re-running on reload, so a changed binding had
     * no actual effect until the window manager was restarted.
     *
     * Both functions release every grab they previously made before
     * re-grabbing, so a binding that changed does not end up with both
     * its old and new key/button combination active at once. */
    if (wm->keysyms != NULL) {
        keyboard_load(wm->surfaces, wm->keysyms, wm->config);
    }
    mouse_load(wm->surfaces, wm->config);

    /* Reload the systray reacting to config. reload */
    systray_reload(wm);

    /* Reload also XSETTINGS */
    xsettings_reload(wm);

    sn_set_timeout_seconds(
            wm->config->base.startup_notification.timeout_seconds);

    if (wm->rules != NULL) {
        (void) rules_load(wm->rules, wm->config_dir_prefix);
    }
    if (wm->session != NULL) {
        (void) session_load(wm->session, wm->config_dir_prefix);
    }

    for (list_item_td *snode = list_head(wm->surfaces);
            snode != NULL; snode = list_next(snode)) {
        surface_td *s = (surface_td *) list_data(snode);
        struct config_base_s *cb = &(wm->config->base);

        if (s->id >= cb->screen_count) {
            continue;
        }

        for (uint32_t i = 0; i < s->desktop_count; ++i) {
            desktop_td *d = surface_desktop_get(s, i);

            if (d == NULL || i >= cb->screens[s->id].desktop_count) {
                continue;
            }

            if (!d->background.is_image &&
                    !d->background.use_root_pixmap) {
                d->background.bg.color =
                    cb->screens[s->id].desktops[i].settings.background.color;
            }

            /* Resize every already-decorated client's frame to match
             * whatever 'window.titlebar.height' and border width the
             * just-reloaded theme now specifies.  'client->theme' is
             * a shared pointer into 'wm->config->theme' that
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
                    client_td *c = (client_td *) elem;

                    if (c != NULL) {
                        client_resync_theme_layout(c,
                                d->client_active_id == c->id);
                        /* 'client_resync_theme_layout' above only
                         * marks 'c' outdated (which is what actually
                         * makes the render pass repaint its border
                         * and titlebar, see 'desktop_render_clients')
                         * when the border width or titlebar height
                         * numerically changed; a reload that only
                         * changed a color or font, with every
                         * dimension unchanged, would otherwise never
                         * repaint anything already on screen even
                         * though 'client->theme' itself already
                         * points at the freshly reloaded values. */
                        wm_request_client_redraw(c);
                    }
                }
            }

            d->is_outdated = true;
        }

        s->is_outdated = true;
    }

    LOGGER_INFO("Configuration reloaded successfully", L_NARG);
    if (wm->session != NULL) {
        session_run_hook(wm->session, wm->connection,
                SESSION_HOOK_RELOAD);
    }

    return 0;
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
