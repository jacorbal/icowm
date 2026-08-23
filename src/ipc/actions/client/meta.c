/**
 * @file ipc/actions/client/meta.c
 *
 * @brief IPC commands mirroring cmds/client/meta.h implementation
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

/* JSON includes */
#include <cjson/cJSON.h>

/* Project includes */
#include <client.h>
#include <enact.h>
#include <wm.h>

/* Local includes */
#include <ipc/args.h>
#include <ipc/resolve.h>
#include <ipc/response.h>
#include <ipc/actions/client/meta.h>


/* Rename the client */
cJSON *ipc_action_rename_client(const wm_td *wm, const cJSON *args)
{
    const char *name;
    client_td *client;
    cJSON *error = NULL;

    if (!ipc_args_get_string(args, "name", &name)) {
        return ipc_response_error("missing or invalid 'name'");
    }

    client = ipc_resolve_client(wm, args, NULL, NULL, &error);
    if (client == NULL) {
        return error;
    }

    enact_client_rename(client, name);
    return ipc_response_ok();
}


/* Change the client's own 'WM_CLASS' */
cJSON *ipc_action_reclass_client(const wm_td *wm, const cJSON *args)
{
    const char *class_name;
    const char *instance_name;
    client_td *client;
    cJSON *error = NULL;

    if (!ipc_args_get_string(args, "class_name", &class_name)) {
        return ipc_response_error("missing or invalid 'class_name'");
    }
    if (!ipc_args_get_string(args, "instance_name", &instance_name)) {
        return ipc_response_error("missing or invalid 'instance_name'");
    }

    client = ipc_resolve_client(wm, args, NULL, NULL, &error);
    if (client == NULL) {
        return error;
    }

    enact_client_reclass(client, class_name, instance_name);
    return ipc_response_ok();
}


/* Change the client's own 'WM_WINDOW_ROLE' */
cJSON *ipc_action_rerole_client(const wm_td *wm, const cJSON *args)
{
    const char *role;
    client_td *client;
    cJSON *error = NULL;

    if (!ipc_args_get_string(args, "role", &role)) {
        return ipc_response_error("missing or invalid 'role'");
    }

    client = ipc_resolve_client(wm, args, NULL, NULL, &error);
    if (client == NULL) {
        return error;
    }

    enact_client_rerole(client, role);
    return ipc_response_ok();
}


/* Override the client's own icon */
cJSON *ipc_action_set_client_icon(const wm_td *wm, const cJSON *args)
{
    const char *icon_name;
    client_td *client;
    cJSON *error = NULL;

    if (!ipc_args_get_string(args, "icon_name", &icon_name)) {
        return ipc_response_error("missing or invalid 'icon_name'");
    }

    client = ipc_resolve_client(wm, args, NULL, NULL, &error);
    if (client == NULL) {
        return error;
    }

    enact_client_set_icon(client, icon_name);
    return ipc_response_ok();
}
