/**
 * @file tools/icowm-msg.c
 *
 * @brief Command-line client for IcoWM's own IPC control socket
 *
 * A thin, self-contained wrapper around IcoWM's own IPC wire protocol.
 * Builds one JSON request line out of its own command-line arguments,
 * sends it to a running IcoWM's control socket, and prints back
 * whatever it answers with.  Deliberately independent of the rest of
 * the project (only @c defs/ipc.h, for the handful of constants that
 * must stay in step with the socket itself, and @c cjson, for the JSON
 * this reads and writes): a small client tool has no reason to pull in
 * anything IcoWM itself needs only to be a window manager.
 *
 * Usage:
 *   icowm-msg <command> [<key>=<value> ...]
 *
 * Each @c key=value becomes one field of the request object alongside
 * "cmd", naming that command's own argument.  A value is sent as a JSON
 * boolean when it is exactly @c true or @c false, as a JSON number when
 * it parses as one in full, and as a JSON string otherwise.
 *
 * Exit status:
 *   0  The command reached IcoWM and it reported success
 *   1  The command reached IcoWM but it reported failure (see the
 *      response's own "error" field, printed to stdout either way)
 *   2  The request never reached IcoWM at all (socket, connection, or
 *      local argument-parsing failure); nothing was printed to stdout
 *      in this case
 */
/*
 * Copyright (c) 2026, J. A. Corbal.
 * All rights reserved.
 *
 * This file is licensed under the 'ISC License'.
 * Read the 'LICENSE' file in the root of this repository for details.
 */


/* System includes */
#include <errno.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>     /* getopt, getuid, close, read, write */
#include <sys/socket.h>
#include <sys/un.h>

/* JSON includes */
#include <cjson/cJSON.h>

/* Default initial values */
#include <defs/ipc.h>


/**
 * @brief Longest @c key=value argument's own key this tool accepts
 */
#define S_MAX_KEY_LENGTH (64)

/**
 * @brief Longest full request or response line this tool will hold
 *
 * @note Kept in step with the server's own @c IPC_MSG_MAX_LENGTH
 */
#define S_MAX_LINE_LENGTH (IPC_MSG_MAX_LENGTH)


/**
 * @brief Every command name this build knows about, for @c -K alone
 *
 * A plain, hand-maintained snapshot of the server's own dispatch table
 * (@c s_dispatch in @c src/ipc/commands.c), kept here rather than
 * queried live so @c -K works the same offline way @c -h and @c -v
 * already do, without needing a running server.  The trade-off that
 * buys: this list can drift out of sync with the server's own table
 * over time if one side gains or loses a command and the other is not
 * updated to match, something a live query could never do.
 *
 * Deliberately used only to answer @c -K's own question ("what commands
 * does this build know the names of"), never to locally validate or
 * reject a command before sending it: an ordinary request still reaches
 * the server exactly as before, unfiltered, so a drifted-stale list
 * here only ever makes @c -K's own output incomplete, never breaks
 * a command that the server itself would otherwise have accepted.
 *
 * @note Whoever adds or removes a command from @c s_dispatch in
 *       @c src/ipc/commands.c is responsible for updating this array to
 *       match; that file carries the same note pointing back here
 */
static const char *const s_known_commands[] = {
    "add_desktop",
    "center_client",
    "close_client",
    "cycle_layer_client",
    "deiconify_all",
    "deiconify_client",
    "exit_wm",
    "focus_client",
    "fullscreen_client",
    "get_focused",
    "get_version",
    "goto_desktop",
    "goto_east_desktop",
    "goto_north_desktop",
    "goto_south_desktop",
    "goto_west_desktop",
    "hide_client",
    "iconify_all",
    "iconify_client",
    "kill_client",
    "list_clients",
    "list_desktops",
    "lower_client",
    "maximize_client",
    "maximize_client_horz",
    "maximize_client_vert",
    "move_client",
    "move_client_to_monitor",
    "move_client_to_monitor_east",
    "move_client_to_monitor_north",
    "move_client_to_monitor_south",
    "move_client_to_monitor_west",
    "move_resize_client",
    "pin_client",
    "raise_client",
    "rearrange_desktop",
    "reclass_client",
    "reload_config",
    "remove_desktop",
    "rename_client",
    "rerole_client",
    "resize_client",
    "restart_wm",
    "send_client_to_back",
    "send_client_to_desktop",
    "send_client_to_front",
    "set_client_icon",
    "set_desktop_background",
    "set_layer_above_client",
    "set_layer_below_client",
    "set_layer_normal_client",
    "shade_client",
    "show_desktop",
    "toggle_decorate_client",
    "toggle_fullscreen_client",
    "toggle_pin_client",
    "toggle_scratchpad",
    "toggle_shade_client",
    "toggle_strutless_maximize",
    "unfocus_client",
    "unfullscreen_client",
    "unhide_client",
    "unpin_client",
    "unshade_client",
    "unurge_client",
    "urge_client",
};

/**
 * @brief Every event name this build knows about, for @c -W alone
 *
 * The exact same trade-off @c s_known_commands above already accepts,
 * for the same reason: a plain, hand-maintained snapshot of the
 * server's own name table (the left-hand column of the array in
 * @c s_event_name_to_bit, @c src/ipc.c) instead of a live query, so
 * @c -W works offline the same way @c -K does.  These are the exact
 * names @c -w itself accepts, comma-separated, to subscribe to; @c -W
 * lists them, it does not subscribe to anything on its own.
 *
 * @note Whoever adds or removes an event from that same array in
 *       @c src/ipc.c is responsible for updating this one to match;
 *       that file carries the same note pointing back here
 */
static const char *const s_known_events[] = {
    "window_mapped",
    "window_closed",
    "desktop_switched",
    "focus_changed",
    "urgency_set",
    "urgency_cleared",
    "window_moved",
    "window_resized",
    "rule_applied",
    "pin_set",
    "pin_cleared",
    "fullscreen_set",
    "fullscreen_cleared",
    "shade_set",
    "shade_cleared",
    "hide_set",
    "hide_cleared",
    "decoration_set",
    "decoration_cleared",
    "client_iconified",
    "client_deiconified",
    "layer_changed",
    "client_desktop_changed",
    "client_renamed",
    "client_reclassed",
    "client_reroled",
    "client_icon_changed",
    "desktop_background_changed",
    "desktop_shown",
    "desktop_hidden",
    "config_reloaded",
    "stacking_changed",
};

#define S_KNOWN_EVENTS_COUNT \
    (sizeof(s_known_events) / sizeof(s_known_events[0]))

#define S_KNOWN_COMMANDS_COUNT \
    (sizeof(s_known_commands) / sizeof(s_known_commands[0]))


/**
 * @brief Print copyright string
 *
 * @param fp File pointer to the stream where to write the output
 *
 * @note Complexity: @e O(1)
 */
static inline void s_show_copyright_str(FILE *fp)
{
    fprintf(fp, "'%s'; %s, %s\n", LICENSE, COPYRIGHT, AUTHOR);
}


/**
 * @brief Print version string
 *
 * @param fp File pointer to the stream where to write the output
 *
 * @note Complexity: @e O(1)
 */
static inline void s_show_version_str(FILE *fp)
{
    fprintf(fp, "%s (\"%s\")\n",
            PROJECT_VERSION, PROJECT_VERSION_CODENAME);
}


/**
 * @brief Display version and license information
 *
 * @param fp File pointer to the stream where to write the output
 *
 * @note Complexity: @e O(1)
 */
static void s_show_version(FILE *fp)
{
    fprintf(fp, "Standalone command-line client for %s's own IPC" \
                " control socket\n", PROJECT_NAME_SHORT);
    fprintf(fp, "Licensed under ");
    s_show_copyright_str(fp);
    fprintf(fp, "Part of %s, version ", PROJECT_NAME_SHORT);
    s_show_version_str(fp);
}


/**
 * @brief Display every command name this build knows about, one per
 *        line
 *
 * @param fp File pointer to the stream where to write the output
 *
 * @note Complexity: @e O(n), where @e n is @c S_KNOWN_COMMANDS_COUNT
 */
static void s_show_commands(FILE *fp)
{
    for (size_t i = 0; i < S_KNOWN_COMMANDS_COUNT; ++i) {
        fprintf(fp, "%s\n", s_known_commands[i]);
    }
}


/**
 * @brief Display every event name this build knows about, one per
 *        line
 *
 * @param fp File pointer to the stream where to write the output
 *
 * @note Complexity: @e O(n), where @e n is @c S_KNOWN_EVENTS_COUNT
 */
static void s_show_events(FILE *fp)
{
    for (size_t i = 0; i < S_KNOWN_EVENTS_COUNT; ++i) {
        fprintf(fp, "%s\n", s_known_events[i]);
    }
}


/**
 * @brief Display help on screen
 *
 * @param fp File pointer to the stream where to write the output
 *
 * @note Complexity: @e O(1)
 */
static void s_show_help(FILE *fp)
{
    fprintf(fp,
            "%s-msg -- Command-line client for %s's own IPC control" \
            " socket\n", PROJECT_NAME_SHORT, PROJECT_NAME_SHORT);

    fprintf(fp, "Usage: %s-msg (<command> [<key>=<value> ...] |\n",
            PROJECT_NAME_PROG);
    fprintf(fp, "                 " \
            " -w <events> [-n <count>] | -K | -W | -h | -v)\n");
    fprintf(fp, "\n");

    fprintf(fp, "A '<key>=<value>' argument's own value is parsed as" \
                " 'true'/'false', a\n");
    fprintf(fp, "number, or else taken as a plain string; a number may" \
                " be given in decimal\n");
    fprintf(fp, "or, prefixed with '0x', in hexadecimal, whichever" \
                " reads more naturally\n");
    fprintf(fp, "for that particular value (a window ID or a packed" \
                " RRGGBB color, say).\n");
    fprintf(fp, "\n");

    fprintf(fp, "Examples:\n");
    fprintf(fp, "   %s-msg list_clients\n", PROJECT_NAME_PROG);
    fprintf(fp, "   %s-msg goto_desktop desktop_id=1\n",
            PROJECT_NAME_PROG);
    fprintf(fp, "   %s-msg set_desktop_background desktop_id=0" \
                " color=0xaaccff\n", PROJECT_NAME_PROG);
    fprintf(fp, "   %s-msg move_client client_id=23068673 x=100 y=200\n",
            PROJECT_NAME_PROG);
    fprintf(fp, "   %s-msg resize_client client_id=0x1600001" \
                " w=300 h=200\n", PROJECT_NAME_PROG);
    fprintf(fp, "\n");

    fprintf(fp, "Options:\n");
    fprintf(fp, "   -h          Show this help information, and exit\n");
    fprintf(fp, "   -v          Show version and license information," \
                " and exit\n");
    fprintf(fp, "   -K          Known commands, one per line, and exit\n");
    fprintf(fp, "   -W          Known events (see '-w' below), one per" \
                " line, and exit\n");
    fprintf(fp, "   -w <events> Subscribe instead of sending a command;\n" \
                "               watch for a comma-separated list of" \
                " events printing\n" \
                "               one line per event as it arrives, until" \
                " '-n' is reached\n" \
                "               or the connection ends\n");
    fprintf(fp, "   -n <count>  Stop watching after this many" \
                " events (only meaningful\n");
    fprintf(fp, "               together with '-w')\n");
    fprintf(fp, "\n");
    fprintf(fp, "Exit status:\n");
    fprintf(fp, "   0 on success\n");
    fprintf(fp, "   1 when %s itself reports failure\n",
            PROJECT_NAME_SHORT);
    fprintf(fp, "   2 when the request never reached it at all" \
                " (socket or connection failure,\n" \
                "     or a local argument-parsing error)\n");
}


/**
 * @brief Resolve the running IcoWM's own IPC socket path
 *
 * Mirrors @a xdg_resolve_dir(XDG_DIR_RUNTIME, ...) , simplified to the
 * one thing this tool needs: read where the socket already is, never
 * create anything.
 *
 * @param out      Destination buffer
 * @param out_size Size of @p out, in bytes
 *
 * @note Complexity: @e O(1)
 *
 * @see @a xdg_resolve_dir in @c utils/config/path.h
 */
static void s_resolve_socket_path(char *out, size_t out_size)
{
    const char *xdg_runtime = getenv("XDG_RUNTIME_DIR");

    if (xdg_runtime != NULL && xdg_runtime[0] != '\0') {
        snprintf(out, out_size, IPC_SOCKET_PATH_FMT_XDG, xdg_runtime,
                IPC_SOCKET_FILENAME);
    } else {
        snprintf(out, out_size, IPC_SOCKET_PATH_FMT_TMP,
                IPC_TMP_FALLBACK_PREFIX, (unsigned int) getuid(),
                IPC_SOCKET_FILENAME);
    }
}


/**
 * @brief Turn one command-line argument's own value into the right JSON
 *        type
 *
 * @param raw Value half of a 'key=value' argument, as given on the
 *            command line
 *
 * @return A newly allocated JSON boolean when @p raw is exactly @c true
 *         or @c false, a JSON number when @p raw parses as one in full,
 *         or a JSON string otherwise; @c NULL only on allocation
 *         failure
 *
 * @note Complexity: @e O(n), where @e n is the length of @p raw
 */
static cJSON *s_parse_value(const char *raw)
{
    char *endptr;
    double num;

    if (strcmp(raw, "true") == 0) {
        return cJSON_CreateBool(1);
    }
    if (strcmp(raw, "false") == 0) {
        return cJSON_CreateBool(0);
    }

    errno = 0;
    num = strtod(raw, &endptr);
    if (errno == 0 && endptr != raw && *endptr == '\0') {
        return cJSON_CreateNumber(num);
    }

    return cJSON_CreateString(raw);
}


/**
 * @brief Build the full request object out of a command name and its
 *        own @c key=value arguments
 *
 * @param cmd   Command name, becomes the request's own "cmd" field
 * @param argc  Argument count, as given to @a main
 * @param argv  Argument vector, as given to @a main
 * @param start Index of the first @c key=value argument in @p argv
 *
 * @return The newly allocated request object, or @c NULL on a malformed
 *         argument (reported to @c stderr already) or an allocation
 *         failure
 *
 * @note Complexity: @e O(n), where @e n is the number of arguments
 */
static cJSON *s_build_request(const char *cmd, int argc, char **argv,
        int start)
{
    cJSON *req = cJSON_CreateObject();
    int i;

    if (req == NULL) {
        return NULL;
    }
    cJSON_AddStringToObject(req, "cmd", cmd);

    for (i = start; i < argc; ++i) {
        const char *eq = strchr(argv[i], '=');
        char key[S_MAX_KEY_LENGTH];
        size_t key_len;

        if (eq == NULL) {
            fprintf(stderr, "%s-msg: argument '%s' is not in" \
                    " 'key=value' form\n", PROJECT_NAME_PROG, argv[i]);
            cJSON_Delete(req);
            return NULL;
        }

        key_len = (size_t) (eq - argv[i]);
        if (key_len == 0 || key_len >= sizeof(key)) {
            fprintf(stderr, "%s-msg: argument name in '%s' is empty" \
                    " or too long\n", PROJECT_NAME_PROG, argv[i]);
            cJSON_Delete(req);
            return NULL;
        }
        memcpy(key, argv[i], key_len);
        key[key_len] = '\0';

        cJSON_AddItemToObject(req, key, s_parse_value(eq + 1));
    }

    return req;
}


/**
 * @brief Connect to the socket
 *
 * @param socket_path Path to the listening @c AF_UNIX socket
 *
 * @return The connected descriptor, or @c -1 on any failure
 *         (reported to @c stderr already)
 *
 * @note Complexity: @e O(1)
 */
static int s_connect_socket(const char *socket_path)
{
    int fd;
    struct sockaddr_un addr;

    fd = socket(AF_UNIX, SOCK_STREAM, 0);
    if (fd < 0) {
        fprintf(stderr, "%s-msg: failed to create a socket: %s\n",
                PROJECT_NAME_PROG, strerror(errno));
        return -1;
    }

    memset(&addr, 0, sizeof(addr));
    addr.sun_family = AF_UNIX;
    snprintf(addr.sun_path, sizeof(addr.sun_path), "%s", socket_path);

    if (connect(fd, (struct sockaddr *) &addr, sizeof(addr)) != 0) {
        fprintf(stderr, "%s-msg: failed to connect to '%s': %s\n" \
                "Is %s running, with its IPC socket up?\n",
                PROJECT_NAME_PROG, socket_path, strerror(errno),
                PROJECT_NAME_SHORT);
        close(fd);
        return -1;
    }

    return fd;
}


/**
 * @brief Send one line, adding its own trailing newline
 *
 * @param fd   Connected descriptor
 * @param line Line to send, without its own trailing newline
 *
 * @return Status of the operation
 * @retval  0 on success
 * @retval -1 on failure (reported to @c stderr already)
 *
 * @note Complexity: @e O(n), where @e n is the length of @p line
 */
static int s_send_line(int fd, const char *line)
{
    size_t len = strlen(line);

    if (write(fd, line, len) != (ssize_t) len ||
            write(fd, "\n", 1) != 1) {
        fprintf(stderr, "%s-msg: failed to send the request: %s\n",
                PROJECT_NAME_PROG, strerror(errno));
        return -1;
    }
    return 0;
}


/**
 * @brief Read one newline-terminated line back
 *
 * @param fd       Connected descriptor
 * @param out      Destination buffer, without its own trailing newline
 * @param out_size Size of @p out, in bytes
 *
 * @return Status of the operation
 * @retval  0 on success
 * @retval -1 on any failure or on the connection closing with nothing
 *            (or an incomplete line) read (reported to @c stderr
 *            already)
 *
 * @note Complexity: @e O(n), where @e n is the length of the line
 *       read
 */
static int s_read_line(int fd, char *out, size_t out_size)
{
    size_t total = 0;

    while (total < out_size - 1) {
        ssize_t n = read(fd, out + total, out_size - 1 - total);

        if (n < 0) {
            fprintf(stderr, "%s-msg: failed to read: %s\n",
                    PROJECT_NAME_PROG, strerror(errno));
            return -1;
        }
        if (n == 0) {
            break;
        }
        total += (size_t) n;
        if (out[total - 1] == '\n') {
            out[total - 1] = '\0';
            return 0;
        }
    }
    out[total] = '\0';

    fprintf(stderr, "%s-msg: connection closed%s\n", PROJECT_NAME_PROG,
            (total == 0) ? " with no response" : " mid-line");
    return -1;
}


/**
 * @brief Connect, send one request line, and read the response line
 *        back, closing the connection either way
 *
 * @param socket_path  Path to the listening @c AF_UNIX socket
 * @param request_line Full request, without its own trailing newline
 * @param out_response Destination buffer for the response, without its
 *                     own trailing newline
 * @param out_size     Size of @p out_response, in bytes
 *
 * @return Status of the operation
 * @retval  0 on success
 * @retval -1 on any failure (reported to @c stderr already)
 *
 * @note Complexity: @e O(n), where @e n is the length of the
 *       request or response, whichever is longer
 */
static int s_send_and_receive(const char *socket_path,
        const char *request_line, char *out_response, size_t out_size)
{
    int fd = s_connect_socket(socket_path);
    int status;

    if (fd < 0) {
        return -1;
    }

    status = (s_send_line(fd, request_line) == 0 &&
            s_read_line(fd, out_response, out_size) == 0) ? 0 : -1;
    close(fd);
    return status;
}


/**
 * @brief Build the @c subscribe request out of a comma-separated
 *        list of event names
 *
 * @param event_list Comma-separated event names, e.g.,
 *                   @c window_mapped,desktop_switched
 *
 * @return The newly allocated request object, or @c NULL on an
 *         allocation failure
 *
 * @note Complexity: @e O(n), where @e n is the length of
 *       @p event_list
 */
static cJSON *s_build_subscribe_request(const char *event_list)
{
    cJSON *req = cJSON_CreateObject();
    cJSON *events;
    const char *start = event_list;

    if (req == NULL) {
        return NULL;
    }
    cJSON_AddStringToObject(req, "cmd", "subscribe");
    events = cJSON_AddArrayToObject(req, "events");

    for (;;) {
        const char *comma = strchr(start, ',');
        size_t len = (comma != NULL)
            ? (size_t) (comma - start) : strlen(start);
        char name[S_MAX_KEY_LENGTH];

        if (len == 0 || len >= sizeof(name)) {
            fprintf(stderr, "%s-msg: event name in '-w %s' is empty" \
                    " or too long\n", PROJECT_NAME_PROG, event_list);
            cJSON_Delete(req);
            return NULL;
        }
        memcpy(name, start, len);
        name[len] = '\0';
        cJSON_AddItemToArray(events, cJSON_CreateString(name));

        if (comma == NULL) {
            break;
        }
        start = comma + 1;
    }

    return req;
}


/**
 * @brief Subscribe, then print one line per event as it arrives,
 *        forever or until @p limit is reached
 *
 * @param socket_path Path to the listening @c AF_UNIX socket
 * @param event_list  Comma-separated event names to subscribe to
 * @param limit       Stop after this many events; @c 0 means no limit
 *                    at all (watch until the connection drops or the
 *                    process is killed)
 *
 * @return Status of the operation
 * @retval  0 on reaching @p limit (or, when @p limit is @c 0, this
 *            never returns that way at all)
 * @retval  1 when the subscribe request itself was rejected (@c ok:
 *            @c false; the reason was already printed)
 * @retval  2 when the connection could never be made, the subscribe
 *            request could not be sent, or the connection was lost
 *            while watching (each case already reported to @c stderr)
 *
 * @note Complexity: unbounded; runs for as long as @p limit (or the
 *       connection, or the process) allows
 */
static int s_watch(const char *socket_path, const char *event_list,
        long limit)
{
    cJSON *request;
    char *request_line;
    char line[S_MAX_LINE_LENGTH];
    cJSON *parsed;
    cJSON *ok_field;
    int fd;
    long seen = 0;

    request = s_build_subscribe_request(event_list);
    if (request == NULL) {
        return 2;
    }
    request_line = cJSON_PrintUnformatted(request);
    cJSON_Delete(request);
    if (request_line == NULL) {
        fprintf(stderr, "%s-msg: failed to build the request\n",
                PROJECT_NAME_PROG);
        return 2;
    }

    fd = s_connect_socket(socket_path);
    if (fd < 0) {
        free(request_line);
        return 2;
    }

    if (s_send_line(fd, request_line) != 0) {
        free(request_line);
        close(fd);
        return 2;
    }
    free(request_line);

    if (s_read_line(fd, line, sizeof(line)) != 0) {
        close(fd);
        return 2;
    }
    printf("%s\n", line);

    parsed = cJSON_Parse(line);
    ok_field = (parsed != NULL)
        ? cJSON_GetObjectItem(parsed, "ok")
        : NULL;
    if (parsed == NULL || !cJSON_IsBool(ok_field) ||
            !cJSON_IsTrue(ok_field)) {
        cJSON_Delete(parsed);
        close(fd);
        return 1;
    }
    cJSON_Delete(parsed);

    while (limit == 0 || seen < limit) {
        if (s_read_line(fd, line, sizeof(line)) != 0) {
            /* Connection lost mid-watch: 's_read_line' already reported
             * why on stderr; report it as the same class of failure
             * a connection that was never made at all would be, since
             * either way the watch could not continue. */
            close(fd);
            return 2;
        }
        printf("%s\n", line);
        ++seen;
    }

    close(fd);
    return 0;
}


/* Main entry */
int main(int argc, char **argv)
{
    char socket_path[sizeof(((struct sockaddr_un *) 0)->sun_path)];
    char response[S_MAX_LINE_LENGTH];
    cJSON *request;
    cJSON *parsed_response;
    cJSON *ok_field;
    char *request_line;
    const char *watch_events = NULL;
    char *watch_limit_end;
    long watch_limit = 0;
    bool limit_given = false;
    int status;
    int opt;

    while ((opt = getopt(argc, argv, "hvKWw:n:")) != -1) {
        switch (opt) {
        case 'h':
            s_show_help(stdout);
            return 0;
        case 'v':
            s_show_version(stdout);
            return 0;
        case 'K':
            s_show_commands(stdout);
            return 0;
        case 'W':
            s_show_events(stdout);
            return 0;
        case 'w':
            watch_events = optarg;
            break;
        case 'n':
            errno = 0;
            watch_limit = strtol(optarg, &watch_limit_end, 10);
            if (errno != 0 || *watch_limit_end != '\0' ||
                    watch_limit_end == optarg || watch_limit <= 0) {
                fprintf(stderr, "%s-msg: '-n %s' is not a positive" \
                        " integer\n", PROJECT_NAME_PROG, optarg);
                return 2;
            }
            limit_given = true;
            break;
        default:
            s_show_help(stderr);
            return 2;
        }
    }

    s_resolve_socket_path(socket_path, sizeof(socket_path));

    if (watch_events != NULL) {
        return s_watch(socket_path, watch_events, watch_limit);
    }
    if (limit_given) {
        fprintf(stderr, "%s-msg: '-n' only makes sense together" \
                " with '-w'\n", PROJECT_NAME_PROG);
        return 2;
    }

    if (optind >= argc) {
        fprintf(stderr, "%s-msg: missing <command>\n\n",
                PROJECT_NAME_PROG);
        s_show_help(stderr);
        return 2;
    }

    request = s_build_request(argv[optind], argc, argv, optind + 1);
    if (request == NULL) {
        return 2;
    }

    request_line = cJSON_PrintUnformatted(request);
    cJSON_Delete(request);
    if (request_line == NULL) {
        fprintf(stderr, "%s-msg: failed to build the request\n",
                PROJECT_NAME_PROG);
        return 2;
    }

    status = s_send_and_receive(socket_path, request_line, response,
            sizeof(response));
    free(request_line);
    if (status != 0) {
        return 2;
    }

    printf("%s\n", response);

    parsed_response = cJSON_Parse(response);
    if (parsed_response == NULL) {
        /* Malformed response from the server itself: already printed
         * above verbatim, nothing more to add */
        return 2;
    }
    ok_field = cJSON_GetObjectItem(parsed_response, "ok");
    status = (cJSON_IsBool(ok_field) && cJSON_IsTrue(ok_field)) ? 0 : 1;
    cJSON_Delete(parsed_response);

    return status;
}
