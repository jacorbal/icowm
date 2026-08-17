/**
 * @file session.c
 *
 * @brief Session hooks loader and launcher
 */
/*
 * Copyright (c) 2026, J. A. Corbal.
 * All rights reserved.
 *
 * This file is licensed under the 'ISC License'.
 * Read the 'LICENSE' file in the root of this repository for details.
 */

#define _POSIX_C_SOURCE 200112L


/* System includes */
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>     /* free, calloc */
#include <string.h>
#include <sys/wait.h>
#include <unistd.h>
#include <wordexp.h>

/* ADT includes */
#include <adt/list.h>

/* JSON includes */
#include <cjson/cJSON.h>

/* Default initial values */
#include <defs/config.h>

/* Project includes */
#include <config.h>
#include <logger.h>
#include <utils/config/json.h>
#include <utils/safe/safestr.h>

/* Local includes */
#include <session.h>



/** Session table holding the three hook command lists */
struct session_s {
    list_td *on_start;
    list_td *on_reload;
    list_td *on_exit;
};


/** Entry tracking one spawned child process by PID and origin */
struct session_tracked_pid_s {
    pid_t pid;
    char hook[16];
    char command[SESSION_MAX_CMD_LEN];
};

/** Table of in-flight child processes spawned by session hooks */
static struct session_tracked_pid_s
    s_session_tracked[SESSION_TRACKED_PIDS_MAX];


/**
 * @brief Destroy a session hook command string
 *
 * @param data Dynamically allocated command string, or @c NULL
 *
 * @note Complexity: @e O(1)
 */
static void s_session_command_destroy(void *data)
{
    free(data);
}


/**
 * @brief Return the JSON key name used for a session hook
 *
 * Maps each @c session_hook_e value to the string key present in the
 * session configuration file and used in log messages.
 *
 * @param hook Lifecycle hook identifier
 *
 * @return A null-terminated string literal naming @p hook; @c on-exit
 *         is returned for any unrecognized value
 *
 * @note Complexity: @e O(1)
 */
static const char *s_session_hook_name(enum session_hook_e hook)
{
    if (hook == SESSION_HOOK_START) {
        return "on-start";
    }
    if (hook == SESSION_HOOK_RELOAD) {
        return "on-reload";
    }

    return "on-exit";
}


/**
 * @brief Return the mutable command list for a session hook
 *
 * @param session Session table that holds the lists
 * @param hook    Lifecycle hook identifier
 *
 * @return Pointer to the mutable @c list_td pointer for @p hook; the
 *         @c on_exit list is returned for any unrecognized value
 *
 * @note Complexity: @e O(1)
 */
static list_td **s_session_hook_list(session_td *session,
        enum session_hook_e hook)
{
    if (hook == SESSION_HOOK_START) {
        return &session->on_start;
    }
    if (hook == SESSION_HOOK_RELOAD) {
        return &session->on_reload;
    }

    return &session->on_exit;
}


/**
 * @brief Return the read-only command list for a session hook
 *
 * @param session Session table that holds the lists
 * @param hook    Lifecycle hook identifier
 *
 * @return Pointer to the read-only @c list_td for @p hook; the
 *         @c on_exit list is returned for any unrecognized value
 *
 * @note Complexity: @e O(1)
 */
static const list_td *s_session_hook_list_const(const session_td *session,
        enum session_hook_e hook)
{
    if (hook == SESSION_HOOK_START) {
        return session->on_start;
    }
    if (hook == SESSION_HOOK_RELOAD) {
        return session->on_reload;
    }

    return session->on_exit;
}


/**
 * @brief Record a spawned child PID in the tracking table
 *
 * Finds the first free slot in @c s_session_tracked and stores @p pid
 * together with the originating hook name and command string.  When the
 * table is full the function returns silently and the PID will not be
 * tracked.
 *
 * @param pid     PID of the spawned child process
 * @param hook    Name of the session hook that spawned the child
 * @param command Command string used to spawn the child
 *
 * @note Complexity: @e O(n), where @e n is @c SESSION_TRACKED_PIDS_MAX
 */
static void s_session_track_pid(pid_t pid, const char *hook,
        const char *command)
{
    for (uint32_t i = 0u; i < SESSION_TRACKED_PIDS_MAX; ++i) {
        if (s_session_tracked[i].pid == 0) {
            s_session_tracked[i].pid = pid;
            safe_strncpy(s_session_tracked[i].hook, hook,
                    sizeof(s_session_tracked[i].hook));
            safe_strncpy(s_session_tracked[i].command, command,
                    sizeof(s_session_tracked[i].command));
            return;
        }
    }
}


/**
 * @brief Find the tracking entry for a given child PID
 *
 * @param pid PID to look up
 *
 * @return Pointer to the matching @c session_tracked_pid_s entry, or
 *         @c NULL when @p pid is not in the table
 *
 * @note Complexity: @e O(n), where @e n is @c SESSION_TRACKED_PIDS_MAX
 */
static struct session_tracked_pid_s *s_session_find_pid(pid_t pid)
{
    for (uint32_t i = 0u; i < SESSION_TRACKED_PIDS_MAX; ++i) {
        if (s_session_tracked[i].pid == pid) {
            return &s_session_tracked[i];
        }
    }

    return NULL;
}


/**
 * @brief Fork a child process and execute a shell command
 *
 * Calls @c fork; in the child the XCB file descriptor is closed, the
 * command string is word-expanded with @c wordexp, and the resulting
 * argument vector is handed to @c execvp.  The child exits with status
 * 127 on any @c wordexp or @c execvp failure.  In the parent the new
 * PID is recorded in the tracking table.
 *
 * @param connection XCB connection whose file descriptor is closed in
 *                   the child before executing (may be null)
 * @param command    Shell command to run; @a word-expanded before
 *                   @a exec
 * @param hook       Hook name used only for logging and tracking
 *
 * @return Status of the operation
 * @retval  0 @c fork succeeded (execution result is asynchronous)
 * @retval  1 @c fork failed
 *
 * @note Complexity: @e O(1) in the parent path
 */
static int s_session_spawn_command(xcb_connection_t *connection,
        const char *command, const char *hook)
{
    pid_t pid;

    pid = fork();
    if (pid < 0) {
        LOGGER_ERROR("Failed to fork session hook '%s' command '%s'",
                hook, command);
        return 1;
    }

    if (pid == 0) {
        wordexp_t words = (wordexp_t) {0};
        int wordexp_flags = WRDE_NOCMD;
        int wr;

#ifdef WRDE_NOENV
        wordexp_flags |= WRDE_NOENV;
#endif

        if (connection != NULL) {
            close(xcb_get_file_descriptor(connection));
        }

        wr = wordexp(command, &words, wordexp_flags);
        if (wr != 0 || words.we_wordc == 0u) {
            if (words.we_wordv != NULL) {
                wordfree(&words);
            }
            _exit(127);
        }

        execvp(words.we_wordv[0], words.we_wordv);
        wordfree(&words);
        _exit(127);
    }

    s_session_track_pid(pid, hook, command);
    LOGGER_DEBUG("Session hook '%s' command '%s' started with PID %d",
            hook, command, (int) pid);
    return 0;
}


/* Allocate and zero-initialize a new session table */
session_td *session_init(void)
{
    session_td *session = calloc(1, sizeof(session_td));

    if (session == NULL) {
        return NULL;
    }

    session->on_start = list_init(s_session_command_destroy);
    session->on_reload = list_init(s_session_command_destroy);
    session->on_exit = list_init(s_session_command_destroy);

    if (session->on_start == NULL || session->on_reload == NULL ||
            session->on_exit == NULL) {
        if (session->on_start != NULL) {
            list_destroy(session->on_start);
        }
        if (session->on_reload != NULL) {
            list_destroy(session->on_reload);
        }
        if (session->on_exit != NULL) {
            list_destroy(session->on_exit);
        }
        free(session);
        return NULL;
    }

    return session;
}


/* Destroy a session table and free its allocated memory */
void session_destroy(session_td *session)
{
    if (session != NULL) {
        list_destroy(session->on_start);
        list_destroy(session->on_reload);
        list_destroy(session->on_exit);
        free(session);
    }
}


/* Load session hook commands from the JSON configuration file */
int session_load(session_td *session, const char *config_dir_prefix)
{
    char config_dir[CONFIG_MAX_LENGTH_PATH_BASE];
    char session_file[CONFIG_MAX_LENGTH_PATH_CONFIG];
    cJSON *json = NULL;

    if (session == NULL) {
        return 1;
    }

    config_resolve_dir(config_dir_prefix, config_dir);
    snprintf(session_file, sizeof(session_file), "%s/%s",
            config_dir, CONFIG_FILENAME_SESSION);

    if (json_load_config(session_file, &json) != 0 || json == NULL) {
        LOGGER_DEBUG("Session file '%s' not loaded; hooks disabled",
                session_file);
        return 0;
    }

    for (int h = (int) SESSION_HOOK_START;
            h <= (int) SESSION_HOOK_EXIT;
            ++h) {
        enum session_hook_e hook = (enum session_hook_e) h;
        const char *hook_name = s_session_hook_name(hook);
        list_td **list = s_session_hook_list(session, hook);
        cJSON *arr = json_get_item(json, hook_name);
        cJSON *it;

        list_clear(*list);
        if (!cJSON_IsArray(arr)) {
            continue;
        }

        cJSON_ArrayForEach(it, arr) {
            if (cJSON_IsString(it) && it->valuestring != NULL &&
                    it->valuestring[0] != '\0') {
                char *command = calloc(SESSION_MAX_CMD_LEN,
                        sizeof(char));

                if (command == NULL) {
                    continue;
                }

                safe_strncpy(command, it->valuestring,
                        SESSION_MAX_CMD_LEN);
                if (list_ins_next(*list, list_tail(*list),
                            command) != 0) {
                    free(command);
                }
            }
        }
    }

    cJSON_Delete(json);

    LOGGER_DEBUG("Loaded session hooks from '%s'" \
            " (start=%u, reload=%u, exit=%u)",
            session_file,
            (unsigned int) list_size(session->on_start),
            (unsigned int) list_size(session->on_reload),
            (unsigned int) list_size(session->on_exit));

    return 0;
}


/* Spawn every command registered for the given session lifecycle hook */
void session_run_hook(const session_td *session,
        xcb_connection_t *connection, enum session_hook_e hook)
{
    const list_td *list;
    const char *hook_name;

    if (session == NULL) {
        return;
    }

    list = s_session_hook_list_const(session, hook);
    hook_name = s_session_hook_name(hook);
    for (list_item_td *item = list_head(list);
            item != NULL; item = list_next(item)) {
        (void) s_session_spawn_command(connection,
                (const char *) list_data(item), hook_name);
    }
}


/* Reap all finished child processes previously spawned by session hooks */
void session_reap_children(void)
{
    int status;
    pid_t pid;

    while ((pid = waitpid(-1, &status, WNOHANG)) > 0) {
        struct session_tracked_pid_s *tracked = s_session_find_pid(pid);

        if (tracked != NULL) {
            if (WIFEXITED(status)) {
                LOGGER_DEBUG("Session hook '%s' PID %d ('%s') exited" \
                        " with status %d",
                        tracked->hook, (int) pid, tracked->command,
                        WEXITSTATUS(status));
            } else if (WIFSIGNALED(status)) {
                LOGGER_WARNING("Session hook '%s' PID %d ('%s')" \
                        " terminated by signal %d",
                        tracked->hook, (int) pid, tracked->command,
                        WTERMSIG(status));
            }
            tracked->pid = 0;
            tracked->hook[0] = '\0';
            tracked->command[0] = '\0';
        } else {
            if (WIFEXITED(status)) {
                LOGGER_DEBUG("Child PID %d exited with status %d",
                        (int) pid, WEXITSTATUS(status));
            } else if (WIFSIGNALED(status)) {
                LOGGER_DEBUG("Child PID %d terminated by signal %d",
                        (int) pid, WTERMSIG(status));
            }
        }
    } /* ! while (pid) */
}
