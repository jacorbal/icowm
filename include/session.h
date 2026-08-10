/**
 * @file session.h
 *
 * @brief Session hook loader and dispatcher
 *
 * Provides a session table that stores shell commands to run at
 * specific window manager lifecycle events: startup, configuration
 * reload, and exit.  Commands are loaded from a JSON file and executed
 * via @c fork / @c execvp.  Child process accounting lets the main loop
 * reap zombies cleanly through @a session_reap_children.
 *
 * @defgroup session Session lifecycle hooks
 * @ingroup wm
 */
/*
 * Copyright (c) 2026, J. A. Corbal.
 * All rights reserved.
 *
 * This file is licensed under the 'ISC License'.
 * Read the 'LICENSE' file in the root of this repository for details.
 */

#ifndef SESSION_SESSION_H
#define SESSION_SESSION_H


/* XCB includes */
#include <xcb/xcb.h>

/* Default initial values */
#include <defs/config.h>   /* CONFIG_MAX_LENGTH_PATH_BASE */


/** Maximum length of a single command string (shared with path limit) */
#define SESSION_MAX_CMD_LEN (CONFIG_MAX_LENGTH_PATH_BASE)

/** Maximum number of child PIDs tracked simultaneously */
#define SESSION_TRACKED_PIDS_MAX (256u)


/**
 * @brief Lifecycle event that triggers a set of session hook commands
 */
enum session_hook_e {
    SESSION_HOOK_START = 0, /**< Commands run when the WM starts */
    SESSION_HOOK_RELOAD,    /**< Commands run when the configuration is
                                 reloaded */
    SESSION_HOOK_EXIT,      /**< Commands run just before the WM exits */
};


#ifndef SESSION_TD_DECLARED
#define SESSION_TD_DECLARED
/**
 * @brief Opaque session hooks table
 */
typedef struct session_s session_td;
#endif


/**
 * @brief Allocate and zero-initialize a session table
 *
 * Allocates a new @c session_td structure and returns it ready for use
 * with @a session_load.
 *
 * @return Pointer to the newly allocated session table, or @c NULL on
 *         allocation failure
 *
 * @note The caller owns the returned pointer and must eventually pass
 *       it to @a session_destroy
 * @note Complexity: @e O(1)
 */
session_td *session_init(void);


/**
 * @brief Destroy a session table and free all of its resources
 *
 * @param session Session table to destroy, or @c NULL (no-op)
 *
 * @note Complexity: @e O(1)
 */
void session_destroy(session_td *session);

/**
 * @brief Load session hook commands from a JSON configuration file
 *
 * Reads the session file in the directory resolved from
 * @p config_dir_prefix (or the XDG/home default when @c NULL) and
 * populates @p session with up to the internal per-hook maximum number
 * of command strings.  A missing or malformed file is silently treated
 * as an empty hook set.
 *
 * @param session           Session table to populate, previously
 *                          returned by @a session_init
 * @param config_dir_prefix Path to the configuration directory, or
 *                          @c NULL to use the XDG default
 *
 * @return Status of the operation
 * @retval  0 Success (even when no session file was found)
 * @retval  1 @p session is @c NULL
 *
 * @note Complexity: @e O(n), where @e n is the total number of command
 *       strings found across all hook lists
 */
int session_load(session_td *session, const char *config_dir_prefix);

/**
 * @brief Execute all commands registered for a lifecycle hook
 *
 * Iterates over the command list for @p hook and spawns each entry via
 * @c fork / @c execvp.  The XCB file descriptor is closed in the child
 * before execution to prevent interference with the parent's
 * X connection.
 *
 * @param session    Session table that holds the command lists;
 *                   if @c NULL the function returns immediately
 * @param connection XCB connection whose file descriptor is closed in
 *                   each child before @c execvp (may be null)
 * @param hook       Lifecycle event whose commands are to be run
 *
 * @note Spawned processes are tracked so @a session_reap_children can
 *       log their exit status
 * @note Complexity: @e O(n), where @e n is the number of commands in
 *       the hook's list
 */
void session_run_hook(const session_td *session,
        xcb_connection_t *connection, enum session_hook_e hook);

/**
 * @brief Reap all finished child processes spawned by session hooks
 *
 * Calls @c waitpid in a non-blocking loop until no more children have
 * exited.  For each PID found in the internal tracking table its exit
 * status is logged and the entry is cleared; untracked PIDs are logged
 * at debug level only.
 *
 * @note This function is intended to be called from the main loop
 *       whenever @c SIGCHLD has been received via
 *       @a startup_child_reap_requested
 * @note Complexity: @e O(n * m), where @e n is the number of reaped
 *       children and @e m is the size of the tracking table
 */
void session_reap_children(void);


#endif  /* ! SESSION_SESSION_H */
