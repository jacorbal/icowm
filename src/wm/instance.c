/**
 * @file wm/instance.c
 *
 * @brief Narrow field accessors for the opaque @c wm_td singleton
 *
 * @c wm_td is opaque everywhere outside @c wm.c and this file; every
 * other file in the project reaches its fields only through the
 * functions declared in @c wm.h and implemented here, never through
 * direct member access.
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
#include <xcb/xcb_ewmh.h>
#include <xcb/xcb_keysyms.h>

/* ADT includes */
#include <adt/list.h>

/* Project includes */
#include <config.h>
#include <wm.h>

/* Local includes */
#include <wm/internal.h>


/* XCB connection handle */
xcb_connection_t *wm_connection(const wm_td *wm)
{
    return (wm != NULL) ? wm->connection : NULL;
}


/* EWMH connection handle */
xcb_ewmh_connection_t *wm_ewmh(const wm_td *wm)
{
    return (wm != NULL) ? wm->ewmh : NULL;
}


/* List of managed surfaces */
list_td *wm_surfaces(const wm_td *wm)
{
    return (wm != NULL) ? wm->surfaces : NULL;
}


/* Key symbols table used to translate keycodes to keysyms */
xcb_key_symbols_t *wm_keysyms(const wm_td *wm)
{
    return (wm != NULL) ? wm->keysyms : NULL;
}


/* XRandR extension availability */
bool wm_randr_available(const wm_td *wm)
{
    return (wm != NULL) && wm->is_randr_available;
}


/* XRandR base event code */
uint8_t wm_randr_base_event(const wm_td *wm)
{
    return (wm != NULL) ? wm->randr_base_event : 0u;
}


/* XSync extension availability */
bool wm_sync_available(const wm_td *wm)
{
    return (wm != NULL) && wm->is_sync_available;
}


/* XSync base event code */
uint8_t wm_sync_base_event(const wm_td *wm)
{
    return (wm != NULL) ? wm->sync_base_event : 0u;
}


/* Window manager configuration */
config_td *wm_config(const wm_td *wm)
{
    return (wm != NULL) ? wm->config : NULL;
}


/* Window matching rules table */
rules_td *wm_rules(const wm_td *wm)
{
    return (wm != NULL) ? wm->rules : NULL;
}


/* Session hooks table */
session_td *wm_session(const wm_td *wm)
{
    return (wm != NULL) ? wm->session : NULL;
}


/* Configuration directory passed at startup */
const char *wm_config_dir_prefix(const wm_td *wm)
{
    return (wm != NULL) ? wm->config_dir_prefix : NULL;
}


/* Running state flag */
bool wm_is_running(const wm_td *wm)
{
    return (wm != NULL) && wm->is_running;
}


/* Restricted-memory mode's available-memory ceiling */
uint32_t wm_restricted_memory_mib(const wm_td *wm)
{
    return (wm != NULL) ? wm->restricted_memory_mib : 0u;
}


/* Return the '_NET_SUPPORTING_WM_CHECK' window */
xcb_window_t wm_ewmh_support_win(const wm_td *wm)
{
    return (wm != NULL) ? wm->ewmh_support_win : (xcb_window_t) XCB_NONE;
}


/* Set the '_NET_SUPPORTING_WM_CHECK' window */
void wm_set_ewmh_support_win(wm_td *wm, xcb_window_t win)
{
    if (wm == NULL) {
        return;
    }

    wm->ewmh_support_win = win;
}


/* Set the key symbols table */
void wm_set_keysyms(wm_td *wm, xcb_key_symbols_t *keysyms)
{
    if (wm == NULL) {
        return;
    }

    wm->keysyms = keysyms;
}


/* Set XRandR availability and its base event code together */
void wm_set_randr(wm_td *wm, bool available, uint8_t base_event)
{
    if (wm == NULL) {
        return;
    }

    wm->is_randr_available = available;
    wm->randr_base_event = base_event;
}


/* Set XSync availability and its base event code together */
void wm_set_sync(wm_td *wm, bool available, uint8_t base_event)
{
    if (wm == NULL) {
        return;
    }

    wm->is_sync_available = available;
    wm->sync_base_event = base_event;
}
