/**
 * @file handler.h
 *
 * @brief X event handler functions for the window manager core
 *
 * Declares the event handler functions for the X events that the window
 * manager processes after the input and menu events have been filtered
 * out.  Each handler receives the XCB connection, the managed surfaces
 * list, the active configuration, and the specific event pointer.
 *
 * Holds no declarations of its own: it is a thin umbrella over its
 * @c handler/ header siblings, one per event or closely related group
 * of events, kept only for @c loop/dispatch.c, the one caller that
 * genuinely needs every one of them together to route each event type
 * to its handler.  Any other caller, including a test battery for one
 * specific handler, should include the one sibling header it actually
 * needs instead of this whole umbrella.
 *
 * @see @c handler/configure.h, @c handler/window.h,
 *      @c handler/property.h, @c handler/focus.h, @c handler/mapping.h,
 *      @c handler/leave.h, @c handler/colormap.h, @c handler/expose.h,
 *      @c handler/message.h, @c handler/selection.h,
 *      @c handler/error.h, @c handler/randr.h, @c handler/sync.h
 *
 * @ingroup handle
 */
/*
 * Copyright (c) 2026, J. A. Corbal.
 * All rights reserved.
 *
 * This file is licensed under the 'ISC License'.
 * Read the 'LICENSE' file in the root of this repository for details.
 */

#ifndef HANDLER_H
#define HANDLER_H


/* Handler includes */
#include <handler/colormap.h>
#include <handler/configure.h>
#include <handler/error.h>
#include <handler/expose.h>
#include <handler/focus.h>
#include <handler/leave.h>
#include <handler/mapping.h>
#include <handler/message.h>
#include <handler/property.h>
#include <handler/randr.h>
#include <handler/selection.h>
#include <handler/sync.h>
#include <handler/window.h>


#endif  /* ! HANDLER_H */
