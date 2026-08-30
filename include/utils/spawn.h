/**
 * @file utils/spawn.h
 *
 * @brief Launching an external command from the window manager
 *
 * One place where @c fork and @c execvp happen, so that the care the
 * child path needs is written once: the inherited X connection is
 * closed before anything else, the command line is word-expanded in
 * the parent rather than between @c fork and @c exec, and a failure
 * to execute is reported back over a close-on-exec pipe instead of
 * being lost in a child that simply exits.
 *
 * @ingroup utils
 */
/*
 * Copyright (c) 2026, J. A. Corbal.
 * All rights reserved.
 *
 * This file is licensed under the 'ISC License'.
 * Read the 'LICENSE' file in the root of this repository for details.
 */

#ifndef UTILS_SPAWN_H
#define UTILS_SPAWN_H


/* System includes */
#include <sys/types.h>  /* pid_t */

/* XCB includes */
#include <xcb/xcb.h>


/**
 * @brief What a spawned command needs from its launcher
 *
 * Every field is optional: a zeroed structure asks for a plain
 * launch, with no environment of its own and nothing to close.
 */
typedef struct spawn_opts_s {
    /**
     * @brief Value for the child's @c DESKTOP_STARTUP_ID
     *
     * A startup-notification aware application reads this and
     * broadcasts its completion once its main window is ready.
     */
    const char *startup_id;

    /**
     * @brief Value for the child's @c RESOURCE_NAME and
     *        @c RESOURCE_CLASS
     *
     * How a launcher asks for a @c WM_CLASS other than whatever the
     * program would set for itself.
     */
    const char *class_name;
} spawn_opts_td;


/* Public interface */
/**
 * @brief Run a command as a child process
 *
 * Word-expands @p command with @c WRDE_NOCMD, so that a configured
 * command line cannot run a subshell of its own, and executes the
 * result.  Returns as soon as the child has either executed the
 * program or failed to, which is decided by the time the error pipe
 * closes and so costs no measurable wait.
 *
 * @param command  Command line to expand and run
 * @param opts     What the child needs; @c NULL asks for a plain
 *                 launch
 * @param out_pid  Receives the child's PID on success; may be @c NULL
 *
 * @return Status of the operation
 * @retval  0 The child is running the program
 * @retval -1 @p command was missing or empty
 * @retval -2 The command could not be expanded or executed; the
 *            reason is logged by this function
 * @retval  1 The pipe or the fork itself failed
 *
 * @note The expansion runs in the parent deliberately: between
 *       @c fork and @c exec a child may call only what is
 *       async-signal-safe, and @c wordexp allocates
 * @note Harmless while the process is single-threaded, but a deadlock
 *       waiting to happen the day one thread holds the allocator lock
 *       at the moment another forks
 * @note Complexity: @e O(n), where @e n is the number of words
 *       @p command expands to
 */
int spawn_command(const char *command, const spawn_opts_td *opts,
        pid_t *out_pid);


#endif  /* ! UTILS_SPAWN_H */
