/**
 * @file eventq/internal.h
 *
 * @brief Private singleton accessor shared across eventq sub-modules
 *
 * Declares the singleton priority queue pointer and associated state
 * that are defined in @c eventq.c and used by @c eventq/dispatch.c.
 *
 * @note This header is private to the eventq subsystem and must not be
 *       included outside of @c src/eventq/, for it is NOT part of the
 *       public API
 */
/*
 * Copyright (c) 2026, J. A. Corbal.
 * All rights reserved.
 *
 * This file is licensed under the 'ISC License'.
 * Read the 'LICENSE' file in the root of this repository for details.
 */

#ifndef EVENTQ_INTERNAL_H
#define EVENTQ_INTERNAL_H


/* Project includes */
#include <eventq.h>


/**
 * @brief Extract the highest-priority event from the event queue
 *
 * This is the same function declared in the public @c eventq.h and
 * implemented in @c eventq.c.  The declaration is repeated here so that
 * @c eventq/dispatch.c can call it without needing to include the full
 * public header in a way that could create circular dependencies in
 * future refactors.
 *
 * @note Defined in @c eventq.c
 * @note All eventq sub-modules access the queue exclusively through the
 *       public @c eventq_add / @c eventq_extract interface
 */


#endif  /* ! EVENTQ_INTERNAL_H */
