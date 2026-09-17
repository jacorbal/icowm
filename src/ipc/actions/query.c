/**
 * @file ipc/actions/query.c
 *
 * @brief The read-only IPC commands implementation
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
#include <stdint.h>

/* JSON includes */
#include <cjson/cJSON.h>

/* Default initial values */
#include <defs/ipc.h>

/* ADT includes */
#include <adt/cdlist.h>
#include <adt/list.h>

/* Project includes */
#include <client.h>
#include <desktop.h>
#include <lookup.h>
#include <stage.h>
#include <stage/desktop.h>
#include <wm.h>

/* Local includes */
#include <ipc/response.h>
#include <ipc/actions/query.h>


/**
 * @brief Append one client's summary object to a "clients" array
 *
 * @param array   Destination JSON array
 * @param client  Client to describe
 * @param desktop Desktop it belongs to (for the response's
 *                @c "desktop_id" field)
 * @param stage Stage it belongs to (for the response's
 *                @c "stage_id" field)
 *
 * @note Complexity: @e O(1)
 */
static void s_append_client_summary(cJSON *array, const client_td *client,
        const desktop_td *desktop, const stage_td *stage)
{
    cJSON *const entry = cJSON_CreateObject();
    const char *name = (client->info.name != NULL &&
            client->info.name[0] != '\0') ? client->info.name : "";
    const struct geometry_s *geom = &client->layout.geometry.cur;

    if (entry == NULL) {
        return;
    }
    cJSON_AddNumberToObject(entry, "id", (double) client->id);
    cJSON_AddStringToObject(entry, "name", name);
    cJSON_AddNumberToObject(entry, "desktop_id", (double) desktop->id);
    cJSON_AddNumberToObject(entry, "stage_id", (double) stage->id);
    cJSON_AddNumberToObject(entry, "x", (double) geom->pos.x);
    cJSON_AddNumberToObject(entry, "y", (double) geom->pos.y);
    cJSON_AddNumberToObject(entry, "w", (double) geom->dim.w);
    cJSON_AddNumberToObject(entry, "h", (double) geom->dim.h);
    cJSON_AddBoolToObject(entry, "iconified",
            client_is_iconified(client) ? 1 : 0);
    cJSON_AddBoolToObject(entry, "urgent",
            client_is_urgent(client) ? 1 : 0);
    cJSON_AddBoolToObject(entry, "pinned",
            client_is_pinned(client) ? 1 : 0);
    cJSON_AddItemToArray(array, entry);
}


/**
 * @brief What the two summary visitors below are filling in
 */
struct s_query_ctx_s {
    cJSON *array;                   /**< Array being appended to */
    const stage_td *stage;      /**< Stage the walk is on */
};


/**
 * @brief Append one desktop's summary
 *
 * @param desktop Desktop reached by the walk
 * @param data    The @c s_query_ctx_s being filled
 *
 * @note Complexity: @e O(1)
 */
static void s_desktop_summary_visit(desktop_td *desktop, void *data)
{
    const struct s_query_ctx_s *const ctx = data;
    cJSON *entry;

    if (ctx == NULL) {
        return;
    }

    entry = cJSON_CreateObject();
    if (entry == NULL) {
        return;
    }

    cJSON_AddNumberToObject(entry, "id", (double) desktop->id);
    cJSON_AddStringToObject(entry, "name", desktop->name);
    cJSON_AddNumberToObject(entry, "stage_id",
            (double) ctx->stage->id);
    cJSON_AddBoolToObject(entry, "current",
            (ctx->stage->desktop_cur == desktop->id) ? 1 : 0);
    cJSON_AddItemToArray(ctx->array, entry);
}


/**
 * @brief Append a summary of every client one desktop shows
 *
 * @param desktop Desktop reached by the walk
 * @param data    The @c s_query_ctx_s being filled
 *
 * @note Complexity: @e O(n), where @e n is the number of clients on
 *       @p desktop
 */
static void s_client_summary_visit(desktop_td *desktop, void *data)
{
    const struct s_query_ctx_s *const ctx = data;
    void *elem;

    if (ctx == NULL || desktop->clients == NULL) {
        return;
    }

    ohtbl_foreach(desktop->clients, elem) {
        const client_td *const client = (const client_td *) elem;

        if (client != NULL && !client_is_locked(client)) {
            s_append_client_summary(ctx->array, client, desktop,
                    ctx->stage);
        }
    }
}


/* "get_version": report the wire protocol version, not a program
 * version this project does not otherwise track */
cJSON *ipc_action_get_version(const wm_td *wm, const cJSON *args)
{
    cJSON *resp;

    (void) wm;
    (void) args;
    resp = ipc_response_ok();
    if (resp == NULL) {
        return NULL;
    }
    cJSON_AddNumberToObject(resp, "protocol_version",
            (double) IPC_PROTOCOL_VERSION);
    return resp;
}


/* "list_desktops": every desktop on every managed stage */
cJSON *ipc_action_list_desktops(const wm_td *wm, const cJSON *args)
{
    struct s_query_ctx_s desktop_ctx;
    cJSON *resp;
    cJSON *array;

    (void) args;
    resp = ipc_response_ok();
    if (resp == NULL) {
        return NULL;
    }
    array = cJSON_AddArrayToObject(resp, "desktops");

    for (list_item_td *node = list_head(wm_stages(wm)); node != NULL;
            node = list_next(node)) {
        stage_td *const stage = (stage_td *) list_data(node);

        if (stage == NULL) {
            continue;
        }
        desktop_ctx.array = array;
        desktop_ctx.stage = stage;
        stage_desktop_walk_all(stage, s_desktop_summary_visit,
                &desktop_ctx);
    }

    return resp;
}


/* "list_clients": every managed, focusable client on every desktop
 * of every managed stage */
cJSON *ipc_action_list_clients(const wm_td *wm, const cJSON *args)
{
    struct s_query_ctx_s client_ctx;
    cJSON *resp;
    cJSON *array;

    (void) args;
    resp = ipc_response_ok();
    if (resp == NULL) {
        return NULL;
    }
    array = cJSON_AddArrayToObject(resp, "clients");

    for (list_item_td *node = list_head(wm_stages(wm)); node != NULL;
            node = list_next(node)) {
        const stage_td *const stage = (stage_td *) list_data(node);

        if (stage == NULL) {
            continue;
        }
        client_ctx.array = array;
        client_ctx.stage = stage;
        stage_desktop_walk_all(stage, s_client_summary_visit,
                &client_ctx);
    }

    return resp;
}


/* "get_focused": the active client of every managed stage */
cJSON *ipc_action_get_focused(const wm_td *wm, const cJSON *args)
{
    cJSON *resp;
    cJSON *array;

    (void) args;
    resp = ipc_response_ok();
    if (resp == NULL) {
        return NULL;
    }
    array = cJSON_AddArrayToObject(resp, "focused");

    for (list_item_td *node = list_head(wm_stages(wm)); node != NULL;
            node = list_next(node)) {
        stage_td *const stage = (stage_td *) list_data(node);
        desktop_td *desktop;
        cJSON *entry;

        if (stage == NULL) {
            continue;
        }
        desktop = lookup_current_desktop(stage);
        if (desktop == NULL) {
            continue;
        }
        entry = cJSON_CreateObject();
        if (entry == NULL) {
            continue;
        }
        cJSON_AddNumberToObject(entry, "stage_id",
                (double) stage->id);
        if (desktop->client_active_id != XCB_WINDOW_NONE) {
            cJSON_AddNumberToObject(entry, "client_id",
                    (double) desktop->client_active_id);
        } else {
            cJSON_AddItemToObject(entry, "client_id",
                    cJSON_CreateNull());
        }
        cJSON_AddItemToArray(array, entry);
    }

    return resp;
}
