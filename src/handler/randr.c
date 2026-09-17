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
#include <adt/cdlist.h>
#include <adt/list.h>

/* Render includes */
#include <render/outdate.h>

/* Input includes */
#include <input/kbd/bind.h>

/* Project includes */
#include <logger.h>
#include <lookup.h>
#include <stage.h>
#include <stage/action.h>
#include <stage/client.h>
#include <stage/desktop.h>
#include <stage/monitor.h>
#include <stage/workarea.h>
#include <systray.h>
#include <systray/handle.h>
#include <wm.h>

/* Local includes */
#include <handler.h>
#include <handler/randr.h>


/**
 * @brief Record what a CRTC change says about its stage
 *
 * @param stages Every managed stage
 * @param change What the server reported
 *
 * @note A change reporting no mode is ignored
 * @note An output going dark says nothing about the geometry to
 *       remember for it
 * @note Complexity: @e O(n), where @e n is the number of stages
 */
static void s_randr_crtc_change_note(list_td *stages,
        const xcb_randr_crtc_change_t *change)
{
    stage_td *stage;

    if (change->mode == XCB_NONE) {
        return;
    }

    stage = lookup_stage_for_root(stages, change->window);
    if (stage == NULL) {
        return;
    }

    stage->randr.is_known = true;
    stage->randr.crtc_id = (uint32_t) change->crtc;
    stage->randr.mode_id = (uint32_t) change->mode;
    stage->randr.rotation = change->rotation;
}


/**
 * @brief Mark one desktop as needing a redraw
 *
 * @param desktop Desktop reached by the walk
 * @param data    Unused
 *
 * @note Complexity: @e O(1)
 */
static void s_desktop_outdate_visit(desktop_td *desktop, void *data)
{
    (void) data;

    wm_outdate_desktop(desktop);
}


/* Mark every desktop in a stage as outdated and refresh workareas */
static void s_handler_randr_refresh_stage(stage_td *stage)
{

    if (stage == NULL) {
        return;
    }

    stage_monitor_refresh_all(stage);
    stage_workarea_refresh_all(stage);
    stage_client_reflow_all(stage);
    wm_outdate_stage(stage);
    stage_desktop_walk_all(stage, s_desktop_outdate_visit, NULL);
}


/**
 * @brief React to an XRandR notification about the display layout
 *
 * @param stages  Every managed stage
 * @param keysyms Key symbol table to reload bindings against
 * @param config  Configuration in force
 * @param event   Notification to act on
 *
 * @note Complexity: @e O(n), where @e n is the number of stages
 */
static void s_randr_notify_apply(list_td *stages,
        xcb_key_symbols_t *keysyms, const config_td *config,
        const xcb_randr_notify_event_t *event)
{
    /* Output/CRTC changes can alter effective workareas and monitor
     * mappings; refresh all known stages conservatively. */
    if (event->subCode == XCB_RANDR_NOTIFY_CRTC_CHANGE ||
            event->subCode == XCB_RANDR_NOTIFY_OUTPUT_CHANGE ||
            event->subCode ==
                XCB_RANDR_NOTIFY_OUTPUT_PROPERTY) {

        /* For 'CRTC_CHANGE' events, update the stage'->randr'
         * fields for the matching stage so 'set_resolution'
         * or 'set_orientation' always have new CRTC metadata */
        if (event->subCode == XCB_RANDR_NOTIFY_CRTC_CHANGE) {
            s_randr_crtc_change_note(stages,
                    &event->u.cc);
        }

        /* Only for 'OUTPUT_CHANGE' (an output actually connected,
         * disconnected, or otherwise changed identity), not for every
         * 'CRTC_CHANGE': applying a profile itself issues
         * 'xcb_randr_set_crtc_config', which raises a 'CRTC_CHANGE' of
         * its own, so reacting to 'CRTC_CHANGE' here too would risk
         * retriggering itself.  Lets a profile for an output that was
         * not yet connected at startup still get applied
         * once it is (e.g., a docked laptop's external monitor). */
        if (event->subCode == XCB_RANDR_NOTIFY_OUTPUT_CHANGE) {
            for (list_item_td *node = list_head(stages);
                    node != NULL; node = list_next(node)) {
                (void) stage_action_randr_apply_profiles(
                        (stage_td *) list_data(node), false);
            }
        }

        for (list_item_td *node = list_head(stages);
                node != NULL; node = list_next(node)) {
            s_handler_randr_refresh_stage(
                    (stage_td *) list_data(node));
        }
        keyboard_load(stages, keysyms, config);

        LOGGER_DEBUG("Processed XRandR notify subcode=%u",
                (unsigned int) event->subCode);
    }
}


/* Handle XRandR extension notifications */
void handler_randr_event(wm_td *wm, xcb_generic_event_t *event)
{
    uint8_t event_type;
    uint8_t screen_change_type;
    uint8_t notify_type;
    list_td *stages = wm_stages(wm);
    xcb_key_symbols_t *keysyms = wm_keysyms(wm);
    const config_td *config = wm_config(wm);

    if (wm == NULL || event == NULL || stages == NULL ||
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
        stage_td *const stage = lookup_stage_for_root(stages,
                randr_event->root);

        if (stage != NULL) {
            stage_resize(stage, randr_event->width,
                    randr_event->height);
            stage->properties.dim_mm.w = randr_event->mwidth;
            stage->properties.dim_mm.h = randr_event->mheight;
            stage->randr.rotation = randr_event->rotation;

            /* Mark CRTC/output identity as known from the event */
            stage->randr.is_known = true;
            stage->randr.crtc_id =
                (uint32_t) randr_event->config_timestamp;
            s_handler_randr_refresh_stage(stage);
            systray_handle_stage_resize(wm);
            keyboard_load(stages, keysyms, config);

            LOGGER_INFO("XRandR screen change on stage %u: %ux%u",
                    stage->id,
                    (unsigned int) randr_event->width,
                    (unsigned int) randr_event->height);
        }

        return;
    }

    if (event_type == notify_type) {
        s_randr_notify_apply(stages, keysyms, config,
                (const xcb_randr_notify_event_t *) event);
    }
}
