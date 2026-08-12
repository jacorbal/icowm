/**
 * @file ipc/commands.c
 *
 * @brief IPC command dispatch table implementation
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
#include <stddef.h>     /* NULL */
#include <stdint.h>
#include <string.h>     /* strcmp */

/* JSON includes */
#include <cjson/cJSON.h>

/* Default initial values */
#include <defs/ipc.h>

/* Project includes */
#include <client.h>
#include <desktop.h>
#include <enact.h>
#include <lookup.h>
#include <surface.h>
#include <wm.h>

/* Policy includes */
#include <policy/focus.h>

/* Local includes */
#include <ipc/commands.h>


/**
 * @brief One named command's own handler
 *
 * @param wm   Window manager instance
 * @param args The request object itself (its other fields besides
 *             @c "cmd" are this command's own arguments); never
 *             @c NULL, though it may have no fields of its own for
 *             a command that takes none
 *
 * @return A newly allocated JSON object to use as the full response
 *         (including its own @c "ok" field), or @c NULL to have the
 *         caller (see @c s_dispatch_by_name) fall back to a generic
 *         failure response, when the handler could not build one of
 *         its own (allocation failure)
 */
typedef cJSON *(*s_ipc_cmd_fn)(wm_td *wm, const cJSON *args);


/** One dispatch table entry: a command's own name and handler */
struct s_ipc_cmd_def_s {
    const char *name;
    s_ipc_cmd_fn handler;
};


/**
 * @brief Build a standard failure response
 *
 * @param message Human-readable reason, copied into the response's
 *                own @c "error" field
 *
 * @return A newly allocated @c {"ok": false, "error": message}
 *         object, or @c NULL on allocation failure
 *
 * @note Complexity: @e O(1)
 */
static cJSON *s_error_response(const char *message)
{
    cJSON *resp = cJSON_CreateObject();

    if (resp == NULL) {
        return NULL;
    }
    cJSON_AddBoolToObject(resp, "ok", 0);
    cJSON_AddStringToObject(resp, "error", message);
    return resp;
}


/**
 * @brief Read a required numeric field out of a request's own
 *        arguments
 *
 * @param args  The request object
 * @param field Field name to read
 * @param out   Receives the value on success; untouched on failure
 *
 * @return @c true when @p field was present and a number, @c false
 *         otherwise
 *
 * @note Complexity: @e O(1)
 */
static bool s_get_uint_arg(const cJSON *args, const char *field,
        uint32_t *out)
{
    cJSON *item = cJSON_GetObjectItem(args, field);

    if (item == NULL || !cJSON_IsNumber(item)) {
        return false;
    }
    *out = (uint32_t) item->valuedouble;
    return true;
}


/**
 * @brief Resolve which surface a request refers to
 *
 * Reads an optional numeric @c "surface_id" field from @p args; when
 * absent, falls back to the first surface in @p wm's own list, the
 * only reasonable default on a single-monitor setup and still a
 * usable one (arbitrary, but not wrong) on a multi-monitor one.
 *
 * @param wm   Window manager instance
 * @param args The request object
 *
 * @return The resolved surface, or @c NULL when @c "surface_id" was
 *         given but does not match any currently managed surface,
 *         or when there is no surface to fall back to at all
 *
 * @note Complexity: @e O(1)
 */
static surface_td *s_resolve_surface(wm_td *wm, const cJSON *args)
{
    uint32_t surface_id;

    if (s_get_uint_arg(args, "surface_id", &surface_id)) {
        return wm_get_surface_by_id(surface_id);
    }

    if (wm->surfaces == NULL || list_is_empty(wm->surfaces)) {
        return NULL;
    }
    return (surface_td *) list_data(list_head(wm->surfaces));
}


/**
 * @brief Append one client's own summary object to a "clients" array
 *
 * @param array   Destination JSON array
 * @param client  Client to describe
 * @param desktop Desktop it belongs to (for the response's own
 *                @c "desktop_id" field)
 * @param surface Surface it belongs to (for the response's own
 *                @c "surface_id" field)
 *
 * @note Complexity: @e O(1)
 */
static void s_append_client_summary(cJSON *array, const client_td *client,
        const desktop_td *desktop, const surface_td *surface)
{
    cJSON *entry = cJSON_CreateObject();
    const char *name = (client->info.name != NULL &&
            client->info.name[0] != '\0') ? client->info.name : "";

    if (entry == NULL) {
        return;
    }
    cJSON_AddNumberToObject(entry, "id", (double) client->id);
    cJSON_AddStringToObject(entry, "name", name);
    cJSON_AddNumberToObject(entry, "desktop_id", (double) desktop->id);
    cJSON_AddNumberToObject(entry, "surface_id", (double) surface->id);
    cJSON_AddBoolToObject(entry, "iconified",
            client_is_iconified(client) ? 1 : 0);
    cJSON_AddBoolToObject(entry, "urgent",
            client_is_urgent(client) ? 1 : 0);
    cJSON_AddBoolToObject(entry, "sticky",
            client_is_sticky(client) ? 1 : 0);
    cJSON_AddItemToArray(array, entry);
}


/* "get_version": report the wire protocol version, not a program
 * version this project does not otherwise track */
static cJSON *s_cmd_get_version(wm_td *wm, const cJSON *args)
{
    cJSON *resp = cJSON_CreateObject();

    (void) wm;
    (void) args;
    if (resp == NULL) {
        return NULL;
    }
    cJSON_AddBoolToObject(resp, "ok", 1);
    cJSON_AddNumberToObject(resp, "protocol_version",
            (double) IPC_PROTOCOL_VERSION);
    return resp;
}


/* "list_desktops": every desktop on every managed surface */
static cJSON *s_cmd_list_desktops(wm_td *wm, const cJSON *args)
{
    cJSON *resp = cJSON_CreateObject();
    cJSON *array;

    (void) args;
    if (resp == NULL) {
        return NULL;
    }
    array = cJSON_AddArrayToObject(resp, "desktops");

    for (list_item_td *node = list_head(wm->surfaces); node != NULL;
            node = list_next(node)) {
        surface_td *surface = (surface_td *) list_data(node);

        if (surface == NULL) {
            continue;
        }
        for (uint32_t i = 0; i < surface->desktop_count; ++i) {
            desktop_td *desktop = surface_desktop_get(surface, i);
            cJSON *entry;

            if (desktop == NULL) {
                continue;
            }
            entry = cJSON_CreateObject();
            if (entry == NULL) {
                continue;
            }
            cJSON_AddNumberToObject(entry, "id", (double) desktop->id);
            cJSON_AddStringToObject(entry, "name", desktop->name);
            cJSON_AddNumberToObject(entry, "surface_id",
                    (double) surface->id);
            cJSON_AddBoolToObject(entry, "current",
                    (surface->desktop_cur == desktop->id) ? 1 : 0);
            cJSON_AddItemToArray(array, entry);
        }
    }

    cJSON_AddBoolToObject(resp, "ok", 1);
    return resp;
}


/* "list_clients": every managed, focusable client on every desktop
 * of every managed surface */
static cJSON *s_cmd_list_clients(wm_td *wm, const cJSON *args)
{
    cJSON *resp = cJSON_CreateObject();
    cJSON *array;

    (void) args;
    if (resp == NULL) {
        return NULL;
    }
    array = cJSON_AddArrayToObject(resp, "clients");

    for (list_item_td *node = list_head(wm->surfaces); node != NULL;
            node = list_next(node)) {
        surface_td *surface = (surface_td *) list_data(node);

        if (surface == NULL) {
            continue;
        }
        for (uint32_t i = 0; i < surface->desktop_count; ++i) {
            desktop_td *desktop = surface_desktop_get(surface, i);
            void *elem;

            if (desktop == NULL || desktop->clients == NULL) {
                continue;
            }
            ohtbl_foreach(desktop->clients, elem) {
                client_td *c = (client_td *) elem;

                if (c != NULL) {
                    s_append_client_summary(array, c, desktop, surface);
                }
            }
        }
    }

    cJSON_AddBoolToObject(resp, "ok", 1);
    return resp;
}


/* "get_focused": the active client of every managed surface */
static cJSON *s_cmd_get_focused(wm_td *wm, const cJSON *args)
{
    cJSON *resp = cJSON_CreateObject();
    cJSON *array;

    (void) args;
    if (resp == NULL) {
        return NULL;
    }
    array = cJSON_AddArrayToObject(resp, "focused");

    for (list_item_td *node = list_head(wm->surfaces); node != NULL;
            node = list_next(node)) {
        surface_td *surface = (surface_td *) list_data(node);
        desktop_td *desktop;
        cJSON *entry;

        if (surface == NULL) {
            continue;
        }
        desktop = lookup_current_desktop(surface);
        if (desktop == NULL) {
            continue;
        }
        entry = cJSON_CreateObject();
        if (entry == NULL) {
            continue;
        }
        cJSON_AddNumberToObject(entry, "surface_id", (double) surface->id);
        if (desktop->client_active_id != XCB_WINDOW_NONE) {
            cJSON_AddNumberToObject(entry, "client_id",
                    (double) desktop->client_active_id);
        } else {
            cJSON_AddItemToObject(entry, "client_id", cJSON_CreateNull());
        }
        cJSON_AddItemToArray(array, entry);
    }

    cJSON_AddBoolToObject(resp, "ok", 1);
    return resp;
}


/* "goto_desktop": {"desktop_id": N, "surface_id": M (optional)} */
static cJSON *s_cmd_goto_desktop(wm_td *wm, const cJSON *args)
{
    uint32_t desktop_id;
    surface_td *surface;

    if (!s_get_uint_arg(args, "desktop_id", &desktop_id)) {
        return s_error_response("missing or invalid 'desktop_id'");
    }
    surface = s_resolve_surface(wm, args);
    if (surface == NULL) {
        return s_error_response("no such surface");
    }
    if (desktop_id >= surface->desktop_count) {
        return s_error_response("no such desktop on that surface");
    }

    enact_surface_desktop_switch(surface, desktop_id);

    {
        cJSON *resp = cJSON_CreateObject();

        if (resp != NULL) {
            cJSON_AddBoolToObject(resp, "ok", 1);
        }
        return resp;
    }
}


/**
 * @brief Shared body for the four commands that just look a client
 *        up by ID and act on it: "focus_client", "close_client",
 *        "iconify_client", "deiconify_client"
 *
 * @param wm      Window manager instance
 * @param args    The request object; must have a numeric
 *                @c "client_id"
 * @param wm_arg  Whether the action needs @p wm itself (only
 *                "focus_client" does, for @c focus_apply's own
 *                unfocus-previous step); passed through to @p
 *                action either way, unused by it when @c false
 * @param action  The one action to run once the client is found
 *
 * @return The standard success or failure response
 *
 * @note Complexity: @e O(1)
 */
static cJSON *s_cmd_act_on_client(wm_td *wm, const cJSON *args,
        void (*action)(wm_td *wm, client_td *client,
            surface_td *surface, desktop_td *desktop))
{
    uint32_t client_id;
    client_td *client;
    surface_td *surface = NULL;
    desktop_td *desktop = NULL;
    cJSON *resp;

    if (!s_get_uint_arg(args, "client_id", &client_id)) {
        return s_error_response("missing or invalid 'client_id'");
    }

    client = lookup_find_client(wm->surfaces,
            (xcb_window_t) client_id, &surface, &desktop);
    if (client == NULL) {
        return s_error_response("no such client");
    }

    action(wm, client, surface, desktop);

    resp = cJSON_CreateObject();
    if (resp != NULL) {
        cJSON_AddBoolToObject(resp, "ok", 1);
    }
    return resp;
}


static void s_action_focus_client(wm_td *wm, client_td *client,
        surface_td *surface, desktop_td *desktop)
{
    focus_apply(wm->surfaces, surface, desktop, client, true, wm->config);
}


static void s_action_close_client(wm_td *wm, client_td *client,
        surface_td *surface, desktop_td *desktop)
{
    (void) wm;
    (void) surface;
    (void) desktop;
    enact_client_close(client);
}


static void s_action_iconify_client(wm_td *wm, client_td *client,
        surface_td *surface, desktop_td *desktop)
{
    (void) wm;
    (void) surface;
    (void) desktop;
    enact_client_iconify(client);
}


static void s_action_deiconify_client(wm_td *wm, client_td *client,
        surface_td *surface, desktop_td *desktop)
{
    (void) wm;
    (void) surface;
    (void) desktop;
    enact_client_restore(client);
}


static cJSON *s_cmd_focus_client(wm_td *wm, const cJSON *args)
{
    return s_cmd_act_on_client(wm, args, s_action_focus_client);
}


static cJSON *s_cmd_close_client(wm_td *wm, const cJSON *args)
{
    return s_cmd_act_on_client(wm, args, s_action_close_client);
}


static cJSON *s_cmd_iconify_client(wm_td *wm, const cJSON *args)
{
    return s_cmd_act_on_client(wm, args, s_action_iconify_client);
}


static cJSON *s_cmd_deiconify_client(wm_td *wm, const cJSON *args)
{
    return s_cmd_act_on_client(wm, args, s_action_deiconify_client);
}


/* "rearrange": {"desktop_id": N (optional; current desktop of the
 * resolved surface otherwise), "surface_id": M (optional)} */
static cJSON *s_cmd_rearrange(wm_td *wm, const cJSON *args)
{
    surface_td *surface;
    desktop_td *desktop;
    uint32_t desktop_id;
    cJSON *resp;

    surface = s_resolve_surface(wm, args);
    if (surface == NULL) {
        return s_error_response("no such surface");
    }

    if (s_get_uint_arg(args, "desktop_id", &desktop_id)) {
        if (desktop_id >= surface->desktop_count) {
            return s_error_response("no such desktop on that surface");
        }
        desktop = surface_desktop_get(surface, desktop_id);
    } else {
        desktop = lookup_current_desktop(surface);
    }
    if (desktop == NULL) {
        return s_error_response("no such desktop");
    }

    enact_desktop_clients_rearrange(wm, surface, desktop);

    resp = cJSON_CreateObject();
    if (resp != NULL) {
        cJSON_AddBoolToObject(resp, "ok", 1);
    }
    return resp;
}


/* "reload_config": no arguments */
static cJSON *s_cmd_reload_config(wm_td *wm, const cJSON *args)
{
    cJSON *resp = cJSON_CreateObject();

    (void) wm;
    (void) args;
    if (resp == NULL) {
        return NULL;
    }
    cJSON_AddBoolToObject(resp, "ok",
            (wm_action_config_reload() == 0) ? 1 : 0);
    return resp;
}


/** The dispatch table itself, in the order commands are documented
 *  in ipc/commands.h's own doc comment */
static const struct s_ipc_cmd_def_s s_commands[] = {
    { "get_version",      s_cmd_get_version },
    { "list_desktops",    s_cmd_list_desktops },
    { "list_clients",     s_cmd_list_clients },
    { "get_focused",      s_cmd_get_focused },
    { "goto_desktop",     s_cmd_goto_desktop },
    { "focus_client",     s_cmd_focus_client },
    { "close_client",     s_cmd_close_client },
    { "iconify_client",   s_cmd_iconify_client },
    { "deiconify_client", s_cmd_deiconify_client },
    { "rearrange",        s_cmd_rearrange },
    { "reload_config",    s_cmd_reload_config },
};

/** Number of entries in 's_commands' */
#define S_IPC_COMMAND_COUNT \
    (sizeof(s_commands) / sizeof(s_commands[0]))


/* Handle one complete IPC request line and produce a response */
char *ipc_commands_dispatch(wm_td *wm, const char *request)
{
    cJSON *parsed;
    cJSON *cmd_item;
    cJSON *resp = NULL;
    bool found = false;
    char *out;

    parsed = cJSON_Parse(request);
    if (parsed == NULL || !cJSON_IsObject(parsed)) {
        cJSON_Delete(parsed);
        resp = s_error_response("request is not a JSON object");
        out = (resp != NULL) ? cJSON_PrintUnformatted(resp) : NULL;
        cJSON_Delete(resp);
        return out;
    }

    cmd_item = cJSON_GetObjectItem(parsed, "cmd");
    if (cmd_item == NULL || !cJSON_IsString(cmd_item)) {
        cJSON_Delete(parsed);
        resp = s_error_response("request is missing a string 'cmd'");
        out = (resp != NULL) ? cJSON_PrintUnformatted(resp) : NULL;
        cJSON_Delete(resp);
        return out;
    }

    for (size_t i = 0; i < S_IPC_COMMAND_COUNT; ++i) {
        if (strcmp(s_commands[i].name, cmd_item->valuestring) == 0) {
            found = true;
            resp = s_commands[i].handler(wm, parsed);
            break;
        }
    }

    if (!found) {
        /* No entry in 's_commands' matched 'cmd' at all, as opposed
         * to matching one whose own handler returned NULL from an
         * allocation failure (the 'resp == NULL' case just below):
         * that second case keeps a more specific message, since a
         * handler whose own request DID match a real command is a
         * different failure than the request never matching one at
         * all. */
        resp = s_error_response("unknown command");
    } else if (resp == NULL) {
        resp = s_error_response("internal error building the response");
    }

    cJSON_Delete(parsed);

    out = (resp != NULL) ? cJSON_PrintUnformatted(resp) : NULL;
    cJSON_Delete(resp);
    return out;
}
