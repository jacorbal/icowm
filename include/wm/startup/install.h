/**
 * @file wm/startup/install.h
 *
 * @brief Registering signal handlers with the OS
 *
 * @ingroup startup
 */
/*
 * Copyright (c) 2026, J. A. Corbal.
 * All rights reserved.
 *
 * This file is licensed under the 'ISC License'.
 * Read the 'LICENSE' file in the root of this repository for details.
 */

#ifndef WM_STARTUP_INSTALL_H
#define WM_STARTUP_INSTALL_H


/**
 * @brief Install POSIX signal handlers for graceful termination
 *
 * Installs handlers for @c SIGHUP, @c SIGINT, @c SIGQUIT, @c SIGTERM,
 * @c SIGCONT, and @c SIGCHLD; see @c wm/startup/handle.h for each one's
 * own individual purpose.
 *
 * @retval  0 on success
 * @retval -1 if any @c sigaction call fails
 */
int wm_startup_install_signals(void);

/**
 * @brief Install handlers for fatal signals that log a diagnostic
 *        before dying
 *
 * Installs @c wm_startup_handle_crash for @c SIGSEGV, @c SIGABRT,
 * @c SIGBUS, and @c SIGFPE, so a crash leaves a diagnostic on standard
 * error before the process actually terminates, rather than dying
 * silently.
 *
 * @retval  0 on success
 * @retval -1 if any @c sigaction call fails
 */
int wm_startup_install_crash_handlers(void);


#endif  /* ! WM_STARTUP_INSTALL_H */
