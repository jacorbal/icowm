/**
 * @file defs/compact.h
 *
 * @brief Documents @c COMPACT, a compile-time (not run-time) option
 *        that shrinks several fixed-size array capacities throughout
 *        the codebase
 *
 * @c COMPACT and restricted-memory mode (@c (icowm -M <mib>)) are two
 * separate mechanisms, one compile-time and one run-time, that shrink
 * different kinds of thing.  A @c COMPACT build does turn the other
 * one on, at the lowest ceiling, but that is the only tie between
 * them: neither is the other, and what one shrinks the other
 * cannot.
 *
 * - Restricted-memory mode is a run-time choice.  The same binary
 *   behaves differently depending on the @c (-M <mib>) flag it happens
 *   to be launched with.  It cannot shrink anything that is
 *   a compile-time array capacity rather than a value read from
 *   configuration or computed at startup, since C sizes those once, at
 *   compile time, regardless of what any run-time flag later asks for;
 *   @c config_base_s's @p (screens[CONFIG_MAX_SCREENS]) of
 *   @p (desktops[CONFIG_MAX_DESKTOPS]) each, @c surface_td's
 *   @p (monitors[WM_SURFACE_MAX_MONITORS]), and the message dialog's
 *   own @p (lines[DIALOG_MSG_MAX_LINES]) are three examples.
 * - @c COMPACT is a compile-time choice.  Defining it,
 *   @c (make COMPACT=1), which the top-level @c Makefile turns into
 *   @c (-D COMPACT), shrinks exactly those compile-time capacities, in
 *   a separate binary that has to be rebuilt to change.  It also
 *   starts restricted-memory mode at @c MEMGUARD_MIN_CEILING_MIB
 *   without being asked, which @c (-M <mib>) can then raise but not
 *   turn off (see @a main, @c main.c).  So a @c COMPACT binary
 *   launched with no flags at all is already a restricted session, and
 *   everything that mode does, refusing to start without enough free
 *   memory, warning once memory or the managed-window count reaches
 *   the ceiling, forcing icon pixmaps and modern font rendering off,
 *   and overriding the window and icon placement policies with
 *   @c smart (see @c config/memguard/defaults.c), applies to it.  A
 *   build without @c COMPACT stays unrestricted until @c (-M <mib>)
 *   asks for it.
 *
 * A user who knows they are always going to run on a severely
 * memory-constrained target gets both at once from @c COMPACT alone:
 * a build genuinely sized for that target from the ground up, rather
 * than one that merely behaves more conservatively at run time while
 * still carrying the full capacity of everything it never uses.
 * Restricted-memory mode alone, in an ordinary build, remains
 * perfectly usable for a target that only needs the run-time half.
 *
 * @note Nothing is declared here: each affected constant defines both
 *       its ordinary and its compact value where it already lived
 *       before this option existed, conditioned on whether this macro
 *       is defined, rather than the compact values being gathered
 *       apart from the ordinary ones they are variants of
 * @note This file exists so that the mechanism as a whole has one
 *       place describing it, no single constant's file being the
 *       right one for that
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

#ifndef DEFS_COMPACT_H
#define DEFS_COMPACT_H


#endif  /* ! DEFS_COMPACT_H */
