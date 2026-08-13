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
 * @brief 'WM_CLASS' class name a newly mapped client must carry to be
 *        recognized as the scratchpad
 *
 * 'scratchpad.command' (config.json) is expected to pass this along,
 * e.g. 'xterm -class Scratchpad'; a command that does not set this
 * class is never recognized as the scratchpad client, no matter what
 * it launches.
 */
#define WM_SCRATCHPAD_WM_CLASS "Scratchpad"


#endif  /* ! DEFS_SCRATCHPAD_H */
