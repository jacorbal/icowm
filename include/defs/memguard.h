/**
 * @file defs/memguard.h
 *
 * @brief Tunable constants for restricted-memory mode (@c icowm -M)
 *
 * Every number restricted-memory mode's behavior depends on lives here,
 * edited and recompiled to retune the mode, rather than spread across
 * @c memguard.c itself; see @c memguard.h for the module that actually
 * uses them.
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

#ifndef DEFS_MEMGUARD_H
#define DEFS_MEMGUARD_H


/**
 * @brief How often the runtime watchdog actually re-reads this
 *        process's own memory usage, in seconds
 *
 * Checking on every single main-loop iteration would mean a file read
 * many times a second for no benefit, since usage cannot realistically
 * climb meaningfully faster than this.
 */
#define MEMGUARD_CHECK_INTERVAL_SECONDS (5)

/**
 * @brief Once a warning has been shown for climbing at or above the
 *        configured ceiling, usage has to drop back below this fraction
 *        of it before a renewed climb can warn again
 *
 * Without this hysteresis, usage hovering right at the ceiling would
 * show the same dialog repeatedly every check interval
 */
#define MEMGUARD_HYSTERESIS_PERCENT (90u)

/**
 * @brief Mebibytes of the @c -M ceiling reserved for this process's own
 *        baseline overhead, before any of it is divided up among
 *        managed clients
 *
 * A ceiling entirely divided up among clients with nothing held back
 * would let @c memguard_max_clients compute a number that leaves no
 * headroom at all for IcoWM's own connection, surface and desktop
 * state, and every other piece of it that exists regardless of how many
 * windows are open.
 *
 * @note Set from an actual measurement (a fresh restricted-memory
 *       session's own @c VmRSS, sampled with no client windows open)
 *       rather than a guess, the highest reading seen across several
 *       runs, including some under heavy window/desktop churn, was
 *       a little under 7 MiB, so this sits one MiB above that, both to
 *       round to a whole number and to leave a little slack for
 *       a system with somewhat heavier XCB, font, or @c libc overhead
 *       than whichever one that measurement was taken on
 *
 * @see @c memguard_max_clients
 */
#define MEMGUARD_BASELINE_MIB (8u)

/**
 * @brief Smallest @c -M ceiling IcoWM will actually accept, and the
 *        default ceiling a @c COMPACT build enables on its own when
 *        @c -M is not given
 *
 * Derived from @c MEMGUARD_BASELINE_MIB and
 * @c MEMGUARD_HYSTERESIS_PERCENT together, rather than picked
 * independently of either.  This floor is the smallest ceiling that
 * still keeps the baseline alone, with no client windows open at all,
 * comfortably under the hysteresis threshold
 * @c (MEMGUARD_BASELINE_MIB / @c MEMGUARD_HYSTERESIS_PERCENT) rounds up
 * to 9 MiB exactly; this sits further above even that, at the baseline
 * occupying 60% of the ceiling, so a session starts out well clear of
 * the warning threshold rather than teetering right at its edge).
 * @c -M rejects anything below this floor outright (see @c main.c),
 * since a ceiling that tight could not realistically run IcoWM at all.
 */
#define MEMGUARD_MIN_CEILING_MIB (14u)

/**
 * @brief Font name every theme text style is redirected to under
 *        restricted-memory mode
 *
 * The font "fixed" is an X core bitmap font alias present on
 * effectively every X server, so redirecting to it reliably keeps text
 * rendering on @c render/text.c's own, lighter X-core-font path rather
 * than falling back to the @c xcb-render/FreeType2/fontconfig backend
 * a TrueType/OpenType family name (what most themes actually specify)
 * would otherwise select.
 *
 * @see @a config_load
 */
#define MEMGUARD_FONT_NAME "fixed"

/**
 * @brief Estimated resident memory cost of one additional managed
 *        client, in kibibytes
 *
 * A deliberately generous estimate of IcoWM's per-client tracking
 * state.  It covers only local state: geometry and hint tracking,
 * a decoration frame, cached picture handles for an icon window when
 * iconified, and entries in a desktop's client hash table and stacking
 * list.
 *
 * This does not attempt to include the application's own memory
 * footprint.  @a sysmem_self_rss_mib in @c utils/sysmem.c, which is
 * what the runtime watchdog compares with the ceiling, reads only this
 * process's @c VmRSS.  It never includes memory belonging to a separate
 * client process.  Consequently, changing this per-client margin cannot
 * affect that process's memory usage, regardless of this constant's
 * value.
 *
 * The tracked state is itself small.  It consists primarily of
 * a client's structure fields and the fixed, small amount of storage in
 * @c wmicon_cache_td.  Its cached values are X-server resource IDs, not
 * image data stored in this process's heap.  Actual use is therefore
 * expected to remain well below this estimate.
 *
 * The estimate nevertheless includes headroom for allocator
 * fragmentation and for font, glyph, or GC state that may grow with the
 * number of simultaneously decorated windows.  A generous value makes
 * @c memguard_max_clients underestimate, rather than overestimate, the
 * number of clients that fit within a given ceiling.  That is the safer
 * failure mode.
 *
 * @see @c memguard_max_clients
 */
#define MEMGUARD_KIB_PER_CLIENT (256u)

/**
 * @brief Lower bound @a memguard_max_clients will ever return
 *
 * Even an extremely tight ceiling still allows managing at least this
 * many clients.  A window manager that could refuse to manage any
 * window at all would not be a usable one.
 *
 * @see @c memguard_max_clients
 */
#define MEMGUARD_MIN_CLIENTS (1u)

/**
 * @brief Upper bound @a memguard_max_clients will ever return
 *
 * A generous @c -M ceiling should still not compute an arbitrarily
 * large client cap.  Past some point the mode's own point (bounding
 * resource usage predictably) is better served by a fixed, sane ceiling
 * than by an ever-growing one.
 *
 * @see @c memguard_max_clients
 */
#define MEMGUARD_ABSOLUTE_MAX_CLIENTS (64u)


#endif  /* ! DEFS_MEMGUARD_H */
