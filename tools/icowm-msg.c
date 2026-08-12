/**
 * @file tools/icowm-msg.c
 *
 * @brief Command-line client for IcoWM's own IPC control socket
 *
 * A thin, self-contained wrapper around the wire protocol manual.md
 * section 5 documents: builds one JSON request line out of its own
 * command-line arguments, sends it to a running IcoWM's control socket,
 * and prints back whatever it answers with.  Deliberately independent
 * of the rest of the project (only @c defs/ipc.h, for the handful of
 * constants that must stay in step with the socket itself, and
 * @c cjson, for the JSON this reads and writes): a small client tool
 * has no reason to pull in anything IcoWM itself needs only to be
 * a window manager.
 *
 * Usage:
 *   icowm-msg <command> [<key>=<value> ...]
 *
 * Each @c key=value becomes one field of the request object alongside
 * "cmd" (see @c manual.md section 5.3 for every command and its own
 * arguments).  A value is sent as a JSON boolean when it is exactly
 * @c true or @c false, as a JSON number when it parses as one in full,
 * and as a JSON string otherwise.
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
 * @brief Longest full request or response line this tool will hold;
 *        kept in step with the server's own @c IPC_MSG_MAX_LENGTH
 */
#define S_MAX_LINE_LENGTH (IPC_MSG_MAX_LENGTH)


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
            PROJECT_VERSION,
            PROJECT_VERSION_CODENAME);
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
 * @brief Display help on screen
 *
 * @param fp File pointer to the stream where to write the output
 *
 * @note Complexity: @e O(1)
 */
static void s_show_help(FILE *fp)
{
    fprintf(fp, "%s-msg -- Command-line client for %s's own IPC control" \
                " socket\n", PROJECT_NAME_SHORT, PROJECT_NAME_SHORT);
    fprintf(fp, "Usage: %s-msg <command> [<key>=<value> ...]\n",
            PROJECT_NAME_PROG);
    fprintf(fp, "       %s-msg -h\n", PROJECT_NAME_PROG);
    fprintf(fp, "       %s-msg -v\n", PROJECT_NAME_PROG);
    fprintf(fp, "\n");

/*
    fprintf(fp, "Every command and its own arguments are documented" \
                " in manual.md,\n");
    fprintf(fp, "section 5.3 ('IPC control socket')");
*/
    fprintf(fp, "Examples:\n");
    fprintf(fp, "   %s-msg get_version\n", PROJECT_NAME_PROG);
    fprintf(fp, "   %s-msg list_clients\n", PROJECT_NAME_PROG);
    fprintf(fp, "   %s-msg goto_desktop desktop_id=1\n",
            PROJECT_NAME_PROG);
    fprintf(fp, "   %s-msg move_client client_id=23068673 x=100 y=200\n",
            PROJECT_NAME_PROG);
    fprintf(fp, "\n");
    fprintf(fp, "Options:\n");
    fprintf(fp, "   -h    Show this help information, and exit\n");
    fprintf(fp, "   -v    Show version and license information," \
                " and exit\n");
    fprintf(fp, "\n");
    fprintf(fp, "Exit status:\n");
    fprintf(fp, "   0 on success\n");
    fprintf(fp, "   1 when %s itself reports failure\n", PROJECT_NAME_SHORT);
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
        snprintf(out, out_size, "%s/icowm/%s", xdg_runtime,
                IPC_SOCKET_FILENAME);
    } else {
        snprintf(out, out_size, "%s%u/icowm/%s", IPC_TMP_FALLBACK_PREFIX,
                (unsigned int) getuid(), IPC_SOCKET_FILENAME);
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
        char *eq = strchr(argv[i], '=');
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
 * @brief Connect to the socket, send one request line, and read the
 *        response line back
 *
 * @param socket_path   Path to the listening @c AF_UNIX socket
 * @param request_line  Full request, without its own trailing newline
 *                      (this function adds one)
 * @param out_response  Destination buffer for the response, without
 *                      its own trailing newline
 * @param out_size      Size of @p out_response, in bytes
 *
 * @return @c 0 on success, @c -1 on any failure (reported to
 *         @c stderr already)
 *
 * @note Complexity: @e O(n), where @e n is the length of the
 *       request or response, whichever is longer
 */
static int s_send_and_receive(const char *socket_path,
        const char *request_line, char *out_response, size_t out_size)
{
    int fd;
    struct sockaddr_un addr;
    size_t len;
    size_t total = 0;

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

    len = strlen(request_line);
    if (write(fd, request_line, len) != (ssize_t) len ||
            write(fd, "\n", 1) != 1) {
        fprintf(stderr, "%s-msg: failed to send the request: %s\n",
                PROJECT_NAME_PROG, strerror(errno));
        close(fd);
        return -1;
    }

    while (total < out_size - 1) {
        ssize_t n = read(fd, out_response + total, out_size - 1 - total);

        if (n < 0) {
            fprintf(stderr, "%s-msg: failed to read the response: %s\n",
                    PROJECT_NAME_PROG, strerror(errno));
            close(fd);
            return -1;
        }
        if (n == 0) {
            break;
        }
        total += (size_t) n;
        if (out_response[total - 1] == '\n') {
            break;
        }
    }
    out_response[total] = '\0';
    close(fd);

    if (total == 0) {
        fprintf(stderr, "%s-msg: connection closed with no response\n",
                PROJECT_NAME_PROG);
        return -1;
    }
    return 0;
}


int main(int argc, char **argv)
{
    char socket_path[sizeof(((struct sockaddr_un *) 0)->sun_path)];
    char response[S_MAX_LINE_LENGTH];
    cJSON *request;
    cJSON *parsed_response;
    cJSON *ok_field;
    char *request_line;
    int status;
    int opt;

    while ((opt = getopt(argc, argv, "hv")) != -1) {
        switch (opt) {
        case 'h':
            s_show_help(stdout);
            return 0;
        case 'v':
            s_show_version(stdout);
            return 0;
        default:
            s_show_help(stderr);
            return 2;
        }
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

    s_resolve_socket_path(socket_path, sizeof(socket_path));

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
