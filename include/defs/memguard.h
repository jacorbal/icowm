/**
 * @file defs/memguard.h
 *
 * @brief Tunable constants for restricted-memory mode (@c icowm -M)
 *
 * Every number restricted-memory mode's behavior depends on lives
 * here, edited and recompiled to retune the mode, rather than spread
 * across @c memguard.c itself; see @c memguard.h for the module that
 * actually uses them.
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


/** How often the runtime watchdog actually re-reads this process's
 *  own memory usage, in seconds; checking on every single main-loop
 *  iteration would mean a file read many times a second for no
 *  benefit, since usage cannot realistically climb meaningfully
 *  faster than this */
#define MEMGUARD_CHECK_INTERVAL_SECONDS (5)

/** Once a warning has been shown for climbing at or above the
 *  configured ceiling, usage has to drop back below this fraction of
 *  it before a renewed climb can warn again; without this hysteresis,
 *  usage hovering right at the ceiling would show the same dialog
 *  repeatedly every check interval */
#define MEMGUARD_HYSTERESIS_PERCENT (90u)

/**
 * @brief Mebibytes of the @c -M ceiling reserved for this process's
 *        own baseline overhead, before any of it is divided up among
 *        managed clients
 *
 * A ceiling entirely divided up among clients with nothing held back
 * would let @c memguard_max_clients compute a number that leaves no
 * headroom at all for IcoWM's own connection, event queue, surface
 * and desktop state, and every other piece of it that exists
 * regardless of how many windows are open.
 *
 * @see @c memguard_max_clients
 */
#define MEMGUARD_BASELINE_MIB (8u)

/**
 * @brief Smallest @c -M ceiling IcoWM will actually accept, and the
 *        default ceiling a @c LOWMEM build enables on its own when
 *        @c -M is not given
 *
 * @c MEMGUARD_BASELINE_MIB alone (8 MiB) is only this module's own
 * estimate of IcoWM's overhead, not a measured figure; real font
 * rendering, XCB, and EWMH support in practice tend to need a little
 * more than that estimate alone suggests.  This floor sits a small
 * margin above it, so @c memguard_max_clients still has some real
 * budget left over once that baseline is subtracted rather than
 * falling straight back to its own one-window minimum.  @c -M rejects
 * anything below this floor outright (see @c main.c), since a
 * ceiling that tight could not realistically run IcoWM at all.
 */
#define MEMGUARD_MIN_CEILING_MIB (16u)

/**
 * @brief Font name every theme text style is redirected to under
 *        restricted-memory mode
 *
 * "fixed" is an X core bitmap font alias present on effectively every
 * X server, so redirecting to it (see @c config_load) reliably keeps
 * text rendering on @c render/text.c's own, lighter X-core-font path
 * rather than falling back to the xcb-render/FreeType2/fontconfig
 * backend a TrueType/OpenType family name (what most themes actually
 * specify) would otherwise select.
 */
#define MEMGUARD_FONT_NAME "fixed"

/**
 * @brief Default desktops per screen when restricted-memory mode is
 *        active and nothing else specifies a count
 *
 * Used only as the fallback desktop count @c config_set_default_
 * values gives a screen when neither @c config.json nor anything else
 * says otherwise, the same role @c 4u plays for an ordinary session.
 * Never enforced as a cap: a @c config.json that explicitly defines,
 * say, 6 desktops gets 6 desktops under restricted-memory mode too,
 * exactly as an ordinary session would; this value only ever governs
 * what happens in the absence of that information, not something
 * layered on top of it.
 */
#define MEMGUARD_DEFAULT_DESKTOPS (2u)

/**
 * @brief Estimated resident memory cost of one additional managed
 *        client, in kibibytes
 *
 * A deliberately generous (i.e., large) round estimate covering a
 * client's own tracked state (geometry and hint tracking, a
 * decoration frame, an icon window if iconified, its entry in a
 * desktop's client hash table and stacking list) together with a
 * margin for the underlying application's own memory footprint, which
 * restricted-memory mode has no way to measure or control directly
 * but which competes for the same system memory regardless.  Erring
 * generous here means @c memguard_max_clients underestimates rather
 * than overestimates how many clients actually fit in a given
 * ceiling, which is the safer of the two mistakes to make.
 *
 * @see @c memguard_max_clients
 */
#define MEMGUARD_KIB_PER_CLIENT (512u)

/**
 * @brief Lower bound @c memguard_max_clients will ever return
 *
 * Even an extremely tight ceiling still allows managing at least this
 * many clients: a window manager that could refuse to manage any
 * window at all would not be a usable one.
 *
 * @see @c memguard_max_clients
 */
#define MEMGUARD_MIN_CLIENTS (1u)

/**
 * @brief Upper bound @c memguard_max_clients will ever return
 *
 * A generous @c -M ceiling should still not compute an arbitrarily
 * large client cap: past some point the mode's own point (bounding
 * resource usage predictably) is better served by a fixed, sane
 * ceiling than by an ever-growing one.
 *
 * @see @c memguard_max_clients
 */
#define MEMGUARD_ABSOLUTE_MAX_CLIENTS (64u)


#endif  /* ! DEFS_MEMGUARD_H */
