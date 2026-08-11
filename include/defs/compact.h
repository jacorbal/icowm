/**
 * @file defs/compact.h
 *
 * @brief Documents @c COMPACT, a compile-time (not run-time) option
 *        that shrinks several fixed-size array capacities throughout
 *        the codebase
 *
 * @c COMPACT and restricted-memory mode (@c icowm -M <mib>, see
 * @c memguard.h) are two entirely independent mechanisms that happen
 * to be designed to complement each other, not one triggering the
 * other:
 *
 * - Restricted-memory mode is a run-time choice: the same binary
 *   behaves differently depending on the @c -M <mib> flag it happens
 *   to be launched with.  It cannot shrink anything that is a
 *   compile-time array capacity rather than a value read from
 *   configuration or computed at startup, since C sizes those once,
 *   at compile time, regardless of what any run-time flag later asks
 *   for; @c config_base_s's own @c screens[CONFIG_MAX_SCREENS] of
 *   @c desktops[CONFIG_MAX_DESKTOPS] each, @c surface_td's own
 *   @c monitors[WM_SURFACE_MAX_MONITORS], and the message dialog's
 *   own @c lines[DIALOG_MSG_MAX_LINES] are three examples; see
 *   @c config.md §10 for the fuller list and the exact size of each.
 * - @c COMPACT is a compile-time choice: defining it (@c make
 *   COMPACT=1, which the top-level Makefile turns into @c -D COMPACT)
 *   shrinks exactly those compile-time capacities instead, in a
 *   separate binary that has to be rebuilt to change.  It does
 *   nothing else: it does not turn restricted-memory mode on by
 *   itself, and it does not supply any default for @c -M <mib> when
 *   that flag is left off.  A @c COMPACT binary launched without
 *   @c -M <mib> at all runs an entirely ordinary, unrestricted
 *   session, just one whose compiled-in ceilings on screens,
 *   desktops, monitors, and so on happen to be smaller; restricted-
 *   memory mode's own behaviors (see @c memguard.h: refusing to start
 *   without enough free memory, warning once running memory or the
 *   managed-window count reaches @c -M <mib>'s own ceiling, forcing
 *   icon pixmaps and modern font rendering off) only ever happen when
 *   @c -M <mib> is actually given, in either kind of build.
 *
 * A person who knows they are always going to run on a severely
 * memory-constrained target can combine the two, for a build genuinely
 * sized for that target from the ground up rather than one that merely
 * behaves more conservatively at run time while still carrying the
 * full, ordinary capacity of everything it never uses; but each works
 * perfectly well without the other too.
 *
 * Nothing is declared here: each affected constant's own file defines
 * both its ordinary and its compact value, conditioned on whether
 * this macro is defined, right where the constant already lived
 * before this option existed, rather than centralizing the compact
 * values somewhere separate from the ordinary ones they are each a
 * variant of.  This file exists purely so the mechanism as a whole
 * has one place documenting what it is, since no single constant's
 * own file is the right place for that.
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
