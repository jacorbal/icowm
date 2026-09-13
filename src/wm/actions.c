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
#include <adt/cdlist.h>
#include <adt/list.h>
#include <adt/ohtbl.h>

/* Utils includes */
#include <utils/config/json.h>

/* Types includes */
#include <types/pair.h>

/* Input includes */
#include <input/kbd/bind.h>
#include <input/mouse/bind.h>

/* Control includes */
#include <cctl/sn.h>

/* Commands includes */
#include <cmds/surface.h>

/* Default initial values */
#include <defs/icon.h>

/* Policy includes */
#include <policy/placement/icon.h>

/* Menu includes */
#include <menu/context/rootmenu.h>
#include <menu/dialog/rrsafe.h>

/* Render includes */
#include <render/viewport/mesh.h>

/* Project includes */
#include <client.h>
#include <config.h>
#include <config/memguard.h>
#include <desktop.h>
#include <enact.h>
#include <logger.h>
#include <lookup.h>
#include <rules.h>
#include <session.h>
#include <surface.h>
#include <systray.h>
#include <xsettings.h>

/* Local includes */
#include <wm.h>
#include <wm/internal.h>


/**
 * @brief What @a s_desktop_reload_visit is applying
 */
struct s_reload_ctx_s {
    const wm_td *wm;                /**< Window manager instance */
    const struct config_base_s *config_base;
                                    /**< Configuration just reloaded */
    surface_td *surface;            /**< Surface being reloaded */
    const struct geometry_s *tray;  /**< Tray rectangle on it */
    bool is_tray_visible;           /**< Whether the tray is showing */
    uint32_t index;                 /**< Desktop index reached */
};


/**
 * @brief Apply the reloaded configuration to one client
 *
 * @param ctx     What the reload is carrying
 * @param desktop Desktop the client is on
 * @param client  Client to apply it to
 *
 * @note Complexity: @e O(1)
 */
static void s_client_reload_apply(const struct s_reload_ctx_s *ctx,
        const desktop_td *desktop, client_td *client)
{
        client_theme_layout_resync(client,
                desktop->client_active_id == client->id);
        /* 'client_theme_layout_resync' above only marks 'c' outdated
         * (which is what actually makes the render pass repaint its
         * border and titlebar, see 'desktop_render_clients') when the
         * border width or titlebar height numerically changed; a reload
         * that only changed a color or font, with every dimension
         * unchanged, would otherwise never repaint anything already on
         * screen even though 'client->config->theme' itself already
         * points at the freshly reloaded values. */
        wm_request_client_redraw(client);

        /* An icon left sitting exactly where this same reload has just
         * moved the tray is never otherwise revisited on its own:
         * nothing else here (or anywhere else) re-checks an
         * already-placed icon's position against the tray's, only
         * a fresh 'place_icon_apply' call or a drag ever does (see
         * 'place_icon_avoid_systray_overlap''s comment). */
        if (ctx->is_tray_visible && client->is_icon_mapped &&
                client->icon_window != 0u) {
            int16_t icon_x = client->icon_pos.x;
            int16_t icon_y = client->icon_pos.y;
            uint16_t icon_h = (uint16_t)
                WM_ICON_SQUARE_SIZE;

            if (client->config != NULL &&
                    client->config->theme.icon.is_captioned) {
                icon_h = (uint16_t) (icon_h +
                        (uint16_t)
                        WM_ICON_CAPTION_HEIGHT);
            }

            if (place_icon_avoid_systray_overlap(
                        &icon_x, &icon_y,
                        (struct dimensions_s) {
                            WM_ICON_SQUARE_SIZE, icon_h },
                        *ctx->tray,
                        &desktop->workarea)) {
                uint32_t vals[2];

                /* 'client->icon_pos.x = icon_x' is a no-op */
                client->icon_pos.y = icon_y;
                vals[0] = (uint32_t) icon_x;
                vals[1] = (uint32_t) icon_y;
                xcb_configure_window(wm_connection(ctx->wm),
                        client->icon_window,
                        XCB_CONFIG_WINDOW_X |
                        XCB_CONFIG_WINDOW_Y,
                        vals);
            }
        }
}


/**
 * @brief Apply the reloaded configuration to one desktop
 *
 * @param desktop Desktop reached by the walk
 * @param data    The @c s_reload_ctx_s being applied
 *
 * @note Complexity: @e O(n), where @e n is the number of clients on
 *       @p desktop
 */
static void s_desktop_reload_visit(desktop_td *desktop, void *data)
{
    struct s_reload_ctx_s *const ctx = data;
    const uint32_t this_index = (ctx != NULL) ? ctx->index : 0u;

    if (ctx == NULL) {
        return;
    }

    /* A surface-wide setting, unlike the per-desktop 'background'/
     * 'desktops' array entries below, so this runs for every desktop
     * still on 'ctx->surface' regardless of whether its own index still
     * has a matching entry in the just-reloaded config: a desktop
     * panned to a page a shrunk viewport no longer has is pulled back
     * to the nearest one that still exists, translating whichever
     * clients were left stranded off it back into view, the same as an
     * ordinary pan already would. */
    scmd_surface_viewport_reclamp(ctx->surface, desktop);

    ctx->index++;
    if (this_index >=
            ctx->config_base->screens[ctx->surface->id].desktop_count) {
        return;
    }

    /* A desktop that set its 'background-color' in the just-reloaded
     * 'config.json' keeps it; one that did not (still holding
     * 'WM_DESKTOP_BG_COLOR_UNSET', the same sentinel every entry starts
     * with) falls back to the just-reloaded theme's
     * 'desktop.color.background' instead, mirroring 'desktop_init''s
     * fallback exactly.  Assigning the sentinel value itself as though
     * it were a real color renders as black, its low 24 bits are all
     * zero: only the top byte, some other flag, is
     * actually set. */
    if (!desktop->background.is_image &&
            !desktop->background.use_root_pixmap) {
        uint32_t new_color =
            ctx->config_base->screens[ctx->surface->id]
            .desktops[this_index].settings.background.color;

        desktop->background.bg.color =
            (new_color == WM_DESKTOP_BG_COLOR_UNSET)
            ? wm_get_config()->theme.desktop.color.background
            : new_color;
    }

    /* The just-reloaded 'config.json' may have switched the mesh off,
     * resized it, or restyled it, and the background color resolved
     * just above is what its dots are derived from, so whichever tile
     * is cached was built against stale inputs */
    viewport_mesh_cache_invalidate();

    /* Resize every already-decorated client's frame to match whatever
     * 'window.titlebar.height' and border width the just-reloaded theme
     * now specifies.  'client->config' is a shared pointer into
     * 'wm_config(wm)' that 'config_load' above already updated in
     * place, so colors, fonts, and button lists all take effect on
     * their own the next time each client repaints; only the cached
     * 'title_height'/'frame_extents' (and the frame size that has to
     * match them) need this explicit resync, since nothing else
     * re-derives those from the theme on its own
     * once a client is already mapped. */
    if (desktop->clients != NULL) {
        void *elem;

        ohtbl_foreach(desktop->clients, elem) {
            client_td *const client = (client_td *) elem;

            if (client != NULL) {
                s_client_reload_apply(ctx, desktop, client);
            }
        }
    }

    desktop->is_outdated = true;
}


/**
 * @brief Resynchronize every already-managed surface, desktop, and
 *        client after a configuration reload
 *
 * A configuration reload updates @p wm's configuration in place,
 * but anything already derived from it before the reload (a desktop's
 * resolved background color, a client's cached frame dimensions) has
 * to be explicitly recomputed or repainted; nothing else does that on
 * its just because the underlying configuration changed
 * underneath it.
 *
 * @param wm Window manager instance
 *
 * @note Complexity: @e O(s * d * c), where @e s is the number of
 *       surfaces, @e d the number of desktops per surface, and @e c
 *       the number of clients per desktop
 */
static void s_resync_after_reload(const wm_td *wm)
{
    struct s_reload_ctx_s reload_ctx;
    config_td *const config = wm_config(wm);

    for (list_item_td *snode = list_head(wm_surfaces(wm));
            snode != NULL; snode = list_next(snode)) {
        surface_td *const s = (surface_td *) list_data(snode);
        struct config_base_s *const cb = &(config->base);
        struct geometry_s tray;
        /* Queried once per surface here, ahead of the desktop/client
         * loop below, rather than once per icon inside it.  This is
         * a synchronous round trip to the X server, as
         * 'systray_get_geometry''s comment notes, and every icon on
         * this same surface shares the identical tray rectangle
         * regardless. */
        bool tray_visible = systray_get_geometry(s, &tray);

        if (s->id >= cb->screen_count) {
            continue;
        }

        reload_ctx.wm = wm;
        reload_ctx.config_base = cb;
        reload_ctx.surface = s;
        reload_ctx.tray = &tray;
        reload_ctx.is_tray_visible = tray_visible;
        reload_ctx.index = 0u;
        surface_desktops_walk(s, s_desktop_reload_visit,
                &reload_ctx);

        s->is_outdated = true;

        /* 'config->desktops.margins' just reloaded above (config_load,
         * called from 'wm_action_config_reload' before this function
         * runs) is not something anything else here re-derives on its
         * own: 'desktop->workarea' (what maximize and placement
         * actually use) is only otherwise recomputed on its own trigger
         * (a client mapping/unmapping, a RandR change...), none of
         * which a reload is.  Without this, a changed margin would stay
         * invisible until one of those unrelated triggers happened to
         * fire, e.g., by switching desktops (switching away and back
         * hides and shows clients, an unmap/map pair that reaches
         * 'surface_refresh_workareas' as a side effect of something
         * else entirely). */
        surface_refresh_workareas(s);
    }
}


/* Rearrange every visible window on the current desktop */
void wm_action_rearrange(const wm_td *wm, surface_td *surface)
{
    const desktop_td *desktop;

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
int wm_action_config_reload(const wm_td *wm)
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
     * with, since 'wm_restricted_memory_mib(wm)' never changes for the
     * life of the process. */
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
     * itself just changed it.  Every surface's own changes are
     * snapshotted, not only the first surface to actually change any:
     * 'surface_action_randr_snapshot_begin' clears the slate once,
     * ahead of the whole loop, and every 'surface_action_apply_randr_
     * profiles' call below appends to that same snapshot rather than
     * starting a fresh one of its own, so the one dialog shown, after
     * every surface has had its own profiles applied, its cancel (or
     * the countdown elapsing) reverts every surface that changed, not
     * only whichever happened to be first. */
    if (wm_surfaces(wm) != NULL) {
        bool any_changed = false;
        surface_td *first_changed = NULL;

        surface_action_randr_snapshot_begin();

        for (list_item_td *node = list_head(wm_surfaces(wm));
                node != NULL; node = list_next(node)) {
            surface_td *const s = (surface_td *) list_data(node);
            bool changed = surface_action_apply_randr_profiles(s, true);

            if (changed) {
                any_changed = true;
                if (first_changed == NULL) {
                    first_changed = s;
                }
            }
        }

        if (any_changed) {
            dialog_rrsafe_show(wm_connection(wm), first_changed, config);
        }
    }

    /* Re-establish keyboard/mouse binding grabs from the just-reloaded
     * configuration's bindings: 'config_load' above already refreshed
     * that in-memory data (it loads 'bindings.json' too, not just
     * 'config.json'), but the X server grabs 'keyboard_load' and
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

    /* A root menu on screen shares each submenu's 'items' array and
     * child state with the entries 'rootmenu_menu_json_load' frees just
     * below, so it has to go first.  Activating an entry closes the
     * menu already, which leaves a reload over IPC as the one way to
     * reach this with one still up */
    rootmenu_close();
    rootmenu_menu_json_load(config_dir_prefix);

    s_resync_after_reload(wm);

    LOGGER_INFO("Configuration reloaded successfully", L_NARG);
    wm_json_syntax_errors_warn();
    if (wm_session(wm) != NULL) {
        session_run_hook(wm_session(wm),
                SESSION_HOOK_RELOAD);
    }

    return 0;
}
