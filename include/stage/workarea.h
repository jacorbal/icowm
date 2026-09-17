/**
 * @file stage/workarea.h
 *
 * @brief Work area recomputation for a stage's desktops
 *
 * Split out of @c stage.h, alongside its sibling @c stage
 * headers, so a file that only needs work area recomputation does
 * not also pull in, and rebuild against, every other unrelated
 * stage concern (desktop, viewport, monitor, action, client)
 * declared in the same file.
 *
 * @see @p stage_s, in @c stage.h
 *
 * @defgroup stage_workarea Stage work area recomputation
 * @ingroup stage
 */
/*
 * Copyright (c) 2026, J. A. Corbal.
 * All rights reserved.
 *
 * This file is licensed under the 'ISC License'.
 * Read the 'LICENSE' file in the root of this repository for details.
 */

#ifndef STAGE_WORKAREA_H
#define STAGE_WORKAREA_H


/* Project includes */
#include <stage.h>


/**
 * @brief Recompute the work area for every desktop on a stage
 *
 * Iterates over all desktops belonging to the given stage and updates
 * each desktop work area using the stage's current width and height.
 * This keeps per-desktop usable geometry in sync after changes such as
 * screen resizing or strut updates.
 *
 * @param stage Pointer to the stage whose desktops will be
 *                refreshed
 *
 * @note Complexity: @e O(n), where @e n is the number of desktops
 */
void stage_workarea_refresh_all(stage_td *stage);


#endif /* STAGE_WORKAREA_H */
