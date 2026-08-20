/**
 * @file handler/randr.c
 *
 * @brief XRandR extension event handler
 */
/*
 * Copyright (c) 2026, J. A. Corbal.
 * All rights reserved.
 *
 * This file is licensed under the 'ISC License'.
 * Read the 'LICENSE' file in the root of this repository for details.
 */

/* System includes */
#include <stddef.h>     /* NULL */

/* XCB includes */
#include <xcb/xcb.h>
#include <xcb/randr.h>

/* ADT includes */
#include <adt/list.h>

/* Render includes */
#include <render/outdate.h>

/* Project includes */
#include <logger.h>
#include <lookup.h>
#include <surface.h>
#include <systray.h>
#include <wm.h>

/* Local includes */
#include <handler.h>
#include <input/kbd/bind.h>


/* Mark every desktop in a surface as outdated and refresh workareas */
static void s_handler_randr_refresh_surface(surface_td *surface)
{
    if (surface == NULL) {
        return;
    }

    surface_refresh_monitors(surface);
    surface_refresh_workareas(surface);
    surface_clients_reflow(surface);
    wm_outdate_surface(surface);
    for (uint32_t did = 0u; did < surface->desktop_count; ++did) {
        wm_outdate_desktop(surface_desktop_get(surface, did));
    }
}


/* Handle XRandR extension notifications */
void handler_randr_event(wm_td *wm, xcb_generic_event_t *event)
{
    uint8_t event_type;
    uint8_t screen_change_type;
    uint8_t notify_type;
    list_td *surfaces = wm_surfaces(wm);
    xcb_key_symbols_t *keysyms = wm_keysyms(wm);
    const config_td *config = wm_config(wm);

    if (wm == NULL || event == NULL || surfaces == NULL ||
            !wm_randr_available(wm)) {
        return;
    }

    event_type = (uint8_t) (event->response_type & ~0x80u);
    screen_change_type = (uint8_t)
        (wm_randr_base_event(wm) + XCB_RANDR_SCREEN_CHANGE_NOTIFY);
    notify_type = (uint8_t) (wm_randr_base_event(wm) + XCB_RANDR_NOTIFY);

    if (event_type == screen_change_type) {
        xcb_randr_screen_change_notify_event_t *randr_event =
            (xcb_randr_screen_change_notify_event_t *) event;
        surface_td *const surface = lookup_surface_for_root(surfaces,
                randr_event->root);

        if (surface != NULL) {
            surface_resize(surface, randr_event->width,
                    randr_event->height);
            surface->properties.dim_mm.w = randr_event->mwidth;
            surface->properties.dim_mm.h = randr_event->mheight;
            surface->randr.rotation = randr_event->rotation;

            /* Mark CRTC/output identity as known from the event */
            surface->randr.is_known = true;
            surface->randr.crtc_id =
                (uint32_t) randr_event->config_timestamp;
            s_handler_randr_refresh_surface(surface);
            systray_handle_surface_resize(wm);
            keyboard_load(surfaces, keysyms, config);

            LOGGER_INFO("XRandR screen change on surface %u: %ux%u",
                    surface->id,
                    (unsigned int) randr_event->width,
                    (unsigned int) randr_event->height);
        }

        return;
    }

    if (event_type == notify_type) {
        xcb_randr_notify_event_t *randr_event =
            (xcb_randr_notify_event_t *) event;

        /* Output/CRTC changes can alter effective workareas and monitor
         * mappings; refresh all known surfaces conservatively. */
        if (randr_event->subCode == XCB_RANDR_NOTIFY_CRTC_CHANGE ||
                randr_event->subCode == XCB_RANDR_NOTIFY_OUTPUT_CHANGE ||
                randr_event->subCode ==
                    XCB_RANDR_NOTIFY_OUTPUT_PROPERTY) {

            /* For 'CRTC_CHANGE' events, update the surface'->randr'
             * fields for the matching surface so
             * 'set_resolution'/'set_orientation' always have new CRTC
             * metadata */
            if (randr_event->subCode == XCB_RANDR_NOTIFY_CRTC_CHANGE) {
                xcb_randr_crtc_change_t *const cc = &randr_event->u.cc;
                if (cc->mode != XCB_NONE) {
                    surface_td *const s = lookup_surface_for_root(
                            surfaces, cc->window);
                    if (s != NULL) {
                        s->randr.is_known = true;
                        s->randr.crtc_id = (uint32_t) cc->crtc;
                        s->randr.mode_id = (uint32_t) cc->mode;
                        s->randr.rotation = cc->rotation;
                    }
                }
            }

            /* Only for 'OUTPUT_CHANGE' (an output actually connected,
             * disconnected, or otherwise changed identity), not for
             * every 'CRTC_CHANGE': applying a profile itself issues
             * 'xcb_randr_set_crtc_config', which raises a CRTC_CHANGE
             * of its own, so reacting to CRTC_CHANGE here too would
             * risk retriggering itself.  Lets a profile for an output
             * that was not yet connected at startup still get applied
             * once it is (e.g., a docked laptop's external monitor). */
            if (randr_event->subCode == XCB_RANDR_NOTIFY_OUTPUT_CHANGE) {
                for (list_item_td *node = list_head(surfaces);
                        node != NULL; node = list_next(node)) {
                    (void) surface_action_apply_randr_profiles(
                            (surface_td *) list_data(node), false);
                }
            }

            for (list_item_td *node = list_head(surfaces);
                    node != NULL; node = list_next(node)) {
                s_handler_randr_refresh_surface(
                        (surface_td *) list_data(node));
            }
            keyboard_load(surfaces, keysyms, config);

            LOGGER_DEBUG("Processed XRandR notify subcode=%u",
                    (unsigned int) randr_event->subCode);
        }
    } /* ! if (event_type) */
}
