/**
 * @file defs/stage.h
 *
 * @brief Capacity limits for a stage's RandR monitor list
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

#ifndef DEFS_STAGE_H
#define DEFS_STAGE_H


/**
 * @brief Maximum number of physical monitors tracked per stage
 *
 * Smaller under @c COMPACT (see @c defs/compact.h).  A target that
 * build is meant for is unlikely to drive many monitors at once
 * regardless.  The ordinary value matches @c CONFIG_RANDR_MAX_OUTPUTS
 * (@c defs/config.h) for the same reason that one covers real setups
 * spanning multiple GPUs, not just a typical single-GPU laptop or
 * desktop.
 */
#ifdef COMPACT
#define WM_STAGE_MAX_MONITORS (2)
#else
#define WM_STAGE_MAX_MONITORS (16)
#endif


#endif  /* ! DEFS_STAGE_H */
