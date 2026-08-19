/**
 * @file wm/startup/install.c
 *
 * @brief Registering signal handlers with the OS
 *
 * Split out of what used to be a single, flat @c startup.c.
 */
/*
 * Copyright (c) 2026, J. A. Corbal.
 * All rights reserved.
 *
 * This file is licensed under the 'ISC License'.
 * Read the 'LICENSE' file in the root of this repository for details.
 */

#define _POSIX_C_SOURCE 200112L /* sigaction, sigemptyset */


/* System includes */
#include <signal.h>
#include <string.h>     /* memset */

/* Project includes */
#include <logger.h>

/* Local includes */
#include <wm/startup/handle.h>
#include <wm/startup/install.h>


/**
 * @brief Install a single POSIX signal handler
 *
 * @param signum Signal number to configure
 * @param handler Function to invoke when the signal arrives
 * @param flags   Extra @c sigaction flags for the registration
 *
 * @return 0 on success, -1 if @c sigaction fails
 */
static int s_startup_install_handler(int signum,
        void (*handler)(int), int flags)
{
    struct sigaction sa;

    memset(&sa, 0, sizeof(sa));
    sa.sa_handler = handler;
    sa.sa_flags = flags;
    sigemptyset(&sa.sa_mask);

    return sigaction(signum, &sa, NULL);
}


/* Install POSIX signal handlers for graceful termination */
int wm_startup_install_signals(void)
{
    if (s_startup_install_handler(SIGHUP,
                wm_startup_handle_reload, 0) != 0 ||
            s_startup_install_handler(SIGINT,
                wm_startup_handle_signal, 0) != 0 ||
            s_startup_install_handler(SIGQUIT,
                wm_startup_handle_signal, 0) != 0 ||
            s_startup_install_handler(SIGTERM,
                wm_startup_handle_signal, 0) != 0 ||
            s_startup_install_handler(SIGCONT,
                wm_startup_handle_resume, 0) != 0 ||
            s_startup_install_handler(SIGCHLD,
                wm_startup_handle_child, SA_NOCLDSTOP) != 0) {
        LOGGER_ERROR("Failed to install startup signal handlers",
                L_NARG);
        return -1;
    }

    return 0;
}


/* Install handlers for fatal signals that log a diagnostic before
 * dying */
int wm_startup_install_crash_handlers(void)
{
    if (s_startup_install_handler(SIGSEGV,
                wm_startup_handle_crash, 0) != 0 ||
            s_startup_install_handler(SIGABRT,
                wm_startup_handle_crash, 0) != 0 ||
            s_startup_install_handler(SIGBUS,
                wm_startup_handle_crash, 0) != 0 ||
            s_startup_install_handler(SIGFPE,
                wm_startup_handle_crash, 0) != 0) {
        LOGGER_ERROR("Failed to install fatal-signal handlers",
                L_NARG);
        return -1;
    }

    return 0;
}
