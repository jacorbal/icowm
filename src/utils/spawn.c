/**
 * @file utils/spawn.c
 *
 * @brief Launching an external command from the window manager
 */
/*
 * Copyright (c) 2026, J. A. Corbal.
 * All rights reserved.
 *
 * This file is licensed under the 'ISC License'.
 * Read the 'LICENSE' file in the root of this repository for details.
 */

#define _POSIX_C_SOURCE 200112L /* execvp, fcntl, fork, pipe */


/* System includes */
#include <errno.h>      /* errno */
#include <fcntl.h>      /* fcntl, FD_CLOEXEC */
#include <stddef.h>     /* NULL */
#include <stdlib.h>     /* setenv */
#include <string.h>     /* strerror */
#include <sys/types.h>  /* pid_t, ssize_t */
#include <unistd.h>     /* _exit, close, execvp, fork, read, write */
#include <wordexp.h>    /* wordexp, wordfree */

/* XCB includes */
#include <xcb/xcb.h>

/* Project includes */
#include <logger.h>

/* Local includes */
#include <utils/spawn.h>


/** Exit status of a child that never managed to execute anything */
#define S_SPAWN_EXIT_NOEXEC (127)


/**
 * @brief Replace the child image with the requested program
 *
 * Everything here runs between @c fork and @c exec, so it stays as
 * close to async-signal-safe as launching a program allows: the
 * argument vector was already built by the caller, and the only
 * remaining allocation risk is @c setenv, which POSIX offers no
 * safe alternative to short of resolving @c PATH by hand.
 *
 * @param argv     Argument vector to execute, already expanded
 * @param opts     What the child needs; never @c NULL here
 * @param err_pipe Pipe whose write end carries @c errno back to the
 *                 parent should @c execvp fail
 *
 * @note Never returns
 * @note Complexity: @e O(1)
 */
static void s_spawn_child(char **argv, const spawn_opts_td *opts,
        int err_pipe[2])
{
    int child_errno;
    ssize_t write_result;

    close(err_pipe[0]);

    /* The child has no business holding the window manager's own
     * socket to the X server open once it becomes another program */
    if (opts->connection != NULL) {
        close(xcb_get_file_descriptor(opts->connection));
    }

    /* Set here, in the child, and not in the parent after 'fork':
     * 'setenv' only ever affects the calling process's own
     * environment, and the child has had an independent copy of it
     * from the moment 'fork' returned */
    if (opts->startup_id != NULL && opts->startup_id[0] != '\0') {
        (void) setenv("DESKTOP_STARTUP_ID", opts->startup_id, 1);
    }
    if (opts->class_name != NULL && opts->class_name[0] != '\0') {
        (void) setenv("RESOURCE_NAME", opts->class_name, 1);
        (void) setenv("RESOURCE_CLASS", opts->class_name, 1);
    }

    execvp(argv[0], argv);

    /* 'execvp' failed: report 'errno' to the parent.  The write
     * result itself is deliberately unchecked, since the child is
     * about to '_exit' either way and has nothing it could do
     * differently; captured in a real variable rather than cast to
     * 'void' on the call, since GCC's own 'warn_unused_result' on
     * 'write' does not treat a bare '(void)' cast as acknowledging
     * it. */
    child_errno = errno;
    write_result = write(err_pipe[1], &child_errno,
            sizeof(child_errno));
    (void) write_result;

    _exit(S_SPAWN_EXIT_NOEXEC);
}


/* Run a command as a child process */
int spawn_command(const char *command, const spawn_opts_td *opts,
        pid_t *out_pid)
{
    static const spawn_opts_td s_spawn_no_opts = { NULL, NULL, NULL };
    wordexp_t words = (wordexp_t) {0};
    int wordexp_flags = WRDE_NOCMD;
    int err_pipe[2];
    int exec_errno;
    pid_t pid;
    ssize_t nread;

    if (command == NULL || command[0] == '\0') {
        LOGGER_ERROR("Refusing to spawn an empty command", L_NARG);
        return -1;
    }

    if (opts == NULL) {
        opts = &s_spawn_no_opts;
    }

#ifdef WRDE_NOENV
    wordexp_flags |= WRDE_NOENV;
#endif

    /* Expanded before forking: see this function's own note in
     * utils/spawn.h for why the child cannot do this itself */
    if (wordexp(command, &words, wordexp_flags) != 0 ||
            words.we_wordc == 0u) {
        LOGGER_WARNING("Failed to expand command '%s'", command);
        if (words.we_wordv != NULL) {
            wordfree(&words);
        }
        return -2;
    }

    /* A close-on-exec pipe is how the parent tells an 'execvp' that
     * failed apart from one that succeeded: on success the kernel
     * closes the write end and the read below returns 0 bytes */
    if (pipe(err_pipe) != 0) {
        LOGGER_ERROR("Failed to create error pipe for '%s'", command);
        wordfree(&words);
        return 1;
    }
    (void) fcntl(err_pipe[1], F_SETFD, FD_CLOEXEC);

    pid = fork();
    if (pid < 0) {
        LOGGER_ERROR("Failed to fork for command '%s'", command);
        close(err_pipe[0]);
        close(err_pipe[1]);
        wordfree(&words);
        return 1;
    }

    if (pid == 0) {
        s_spawn_child(words.we_wordv, opts, err_pipe);
    }

    close(err_pipe[1]);
    exec_errno = 0;
    nread = read(err_pipe[0], &exec_errno, sizeof(exec_errno));
    close(err_pipe[0]);
    wordfree(&words);

    if (nread > 0) {
        LOGGER_WARNING("Failed to launch '%s': %s", command,
                strerror(exec_errno));
        return -2;
    }

    LOGGER_DEBUG("Command '%s' running with PID %d", command,
            (int) pid);

    if (out_pid != NULL) {
        *out_pid = pid;
    }

    return 0;
}
