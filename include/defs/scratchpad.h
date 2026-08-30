/**
 * @file defs/scratchpad.h
 *
 * @brief Recognition marker for the scratchpad client
 *
 * @ingroup defs
 */
/*
 * Copyright (c) 2026, J. A. Corbal.
 * All rights reserved.
 *
 * This file is licensed under the 'ISC License'.
 * Read the 'LICENSE' file in the root of this repository for details.
 */

#ifndef DEFS_SCRATCHPAD_H
#define DEFS_SCRATCHPAD_H


/**
 * @brief Class name for @c WM_CLASS hint on a newly mapped client must
 *        carry to be recognized as the scratchpad
 */
#define WM_SCRATCHPAD_WM_CLASS "Scratchpad"

/**
 * @brief How long @c scratchpad_toggle waits for a launch it started to
 *        produce a matching client before giving up and allowing
 *        a fresh attempt
 *
 * A legitimate launch ('fork', 'exec', the application's startup,
 * connecting to the X server, and creating its first window) ordinarily
 * finishes well under this.  The whole point of this timeout is only to
 * recover from a launch that is never coming back (the process died
 * before creating any window, or never managed to connect at all), not
 * to second-guess an ordinary one still in progress, so this still
 * leaves real margin above a typical launch rather than cutting it as
 * close as the fastest ordinary case would allow.
 */
#define WM_SCRATCHPAD_AWAIT_TIMEOUT_SECONDS (4L)


#endif  /* ! DEFS_SCRATCHPAD_H */
