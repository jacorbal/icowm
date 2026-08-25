/**
 * @file policy/placement/smart.h
 *
 * @brief Smart placement: the search for the least-covered spot
 *
 * Walks the positions a client could take and keeps the one
 * overlapping least of what is already on screen, which is what
 * 'smart' means here.  The arithmetic it walks with lives in
 * @c policy/placement/rect.h, and the area it walks over comes from
 * @c policy/placement/monitor.h.
 */
/*
 * Copyright (c) 2026, J. A. Corbal.
 * All rights reserved.
 *
 * This file is licensed under the 'ISC License'.
 * Read the 'LICENSE' file in the root of this repository for details.
 */

#ifndef POLICY_PLACEMENT_SMART_H
#define POLICY_PLACEMENT_SMART_H

/* System includes */
#include <stdint.h>

/* Utils includes */
#include <utils/geom.h>

/* Type includes */
#include <types/handles.h>
#include <types/pair.h>

/* Local includes */
#include <defs/placement.h>


#endif  /* ! POLICY_PLACEMENT_SMART_H */
