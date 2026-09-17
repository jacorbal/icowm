/**
 * @file menu/context/submenu/monitor.c
 *
 * @brief Shared "Send to monitor" context menu submenu implementation
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
#include <stdio.h>      /* snprintf */
#include <string.h>     /* memset */

/* XCB includes */
#include <xcb/xcb.h>

/* Project includes */
#include <client.h>
#include <desktop.h>
#include <enact.h>
#include <enact/client.h>
#include <stage.h>
#include <stage/monitor.h>

/* Menu includes */
#include <menu/context/ctxmenu.h>

/* Local includes */
#include <menu/context/submenu/monitor.h>


/**
 * @brief Most monitors this submenu ever lists
 *
 * Tied to @c WM_STAGE_MAX_MONITORS itself, the one real source of
 * truth for how many a stage can ever report, rather than an
 * independent number of its own that could silently drift out of step
 * with it.
 */
#define S_MAX_MONITORS WM_STAGE_MAX_MONITORS


/**
 * @brief Userdata structure passed to the "Send to monitor" callback
 */
typedef struct {
    client_td *client;      /**< Target client */
    uint32_t monitor_index; /**< Destination monitor index */
} s_send_data_td;


/** Entries for the "Send to monitor" submenu */
static ctxmenu_entry_td s_monitor_entries[S_MAX_MONITORS];

/** State for the "Send to monitor" child menu */
static ctxmenu_state_td s_monitor_state;

/** Per-monitor userdata pool for "Send to monitor" callbacks */
static s_send_data_td s_send_data[S_MAX_MONITORS];


/**
 * @brief Callback: send client to a specific monitor
 *
 * @param connection XCB connection (unused)
 * @param userdata   Pointer to this entry's own @c s_send_data_td
 *
 * @note Complexity: @e O(1)
 */
static void s_cb_send_to_monitor(xcb_connection_t *connection,
        void *userdata)
{
    s_send_data_td *d;

    (void) connection;

    if (userdata == NULL) {
        return;
    }
    d = (s_send_data_td *) userdata;
    if (d->client == NULL) {
        return;
    }

    enact_client_move_to_monitor(d->client, d->monitor_index);
}


/* Build the "Send to monitor" submenu entries for 'client' */
int ctxmenu_submenu_monitor_build(stage_td *stage,
        desktop_td *desktop, client_td *client,
        ctxmenu_entry_td **out_entries, ctxmenu_state_td **out_state)
{
    monitor_td cur_monitor;
    struct position_s center_pos;
    int n = 0;

    (void) desktop;

    if (stage == NULL || client == NULL || out_entries == NULL ||
            out_state == NULL || stage->monitor_count <= 1u) {
        return 0;
    }

    memset(s_monitor_entries, 0, sizeof(s_monitor_entries));

    center_pos.x = client->layout.geometry.cur.pos.x +
        (int32_t) (client->layout.geometry.cur.dim.w / 2u);
    center_pos.y = client->layout.geometry.cur.pos.y +
        (int32_t) (client->layout.geometry.cur.dim.h / 2u);
    cur_monitor = stage_monitor_for_point(stage, center_pos);

    for (uint32_t m_idx = 0; m_idx < stage->monitor_count &&
            n < S_MAX_MONITORS; ++m_idx) {
        bool is_cur;
        const monitor_td *m = &stage->monitors[m_idx];

        is_cur = (m->x == cur_monitor.x && m->y == cur_monitor.y);

        (void) snprintf(s_monitor_entries[n].label,
                sizeof(s_monitor_entries[n].label),
                "%s[%u] %ux%u @ %d,%d%s%s",
                MENU_CONTEXT_CTXMENU_LABEL_PREFIX,
                m_idx, m->w, m->h, m->x, m->y,
                (m_idx == stage->primary_monitor_index)
                    ? " (primary)" : "",
                MENU_CONTEXT_CTXMENU_LABEL_SUFFIX);

        s_monitor_entries[n].type = CTXMENU_COMMAND;
        s_monitor_entries[n].is_disabled = is_cur;
        s_send_data[n].client = client;
        s_send_data[n].monitor_index = m_idx;
        s_monitor_entries[n].on_activate = s_cb_send_to_monitor;
        s_monitor_entries[n].userdata = &s_send_data[n];
        ++n;
    }

    memset(&s_monitor_state, 0, sizeof(s_monitor_state));
    s_monitor_state.window = XCB_WINDOW_NONE;
    s_monitor_state.entries = s_monitor_entries;
    s_monitor_state.entry_count = n;

    *out_entries = s_monitor_entries;
    *out_state = &s_monitor_state;
    return n;
}
