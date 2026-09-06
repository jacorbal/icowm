/**
 * @file defs/config.h
 *
 * @brief Definitions related to the configuration structure
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

#ifndef DEFS_CONFIG_H
#define DEFS_CONFIG_H


/* Maximum length in strings */
#define CONFIG_MAX_LENGTH_COMMAND (128)
#define CONFIG_MAX_LENGTH_BINDING (128)
#define CONFIG_MAX_LENGTH_OPTION (40)
#define CONFIG_MAX_LENGTH_FONTNAME (80)

/* Maximum length for file names and paths */
#define CONFIG_MAX_LENGTH_NAME (256)
#define CONFIG_MAX_LENGTH_FILENAME (256)
#define CONFIG_MAX_LENGTH_PATH_BASE (1024)
#define CONFIG_MAX_LENGTH_PATH_CONFIG \
    ((CONFIG_MAX_LENGTH_PATH_BASE) + (CONFIG_MAX_LENGTH_FILENAME))
#define CONFIG_MAX_LENGTH_PATH_THEME \
    ((CONFIG_MAX_LENGTH_PATH_BASE) + (CONFIG_MAX_LENGTH_FILENAME) + \
    (CONFIG_MAX_LENGTH_FILENAME))

/* Base default directories */
#define CONFIG_DIR_BASE "icowm"
#define CONFIG_DIR_THEMES "themes"
#define CONFIG_FILENAME_BASE "config.json"
#define CONFIG_FILENAME_BINDINGS "bindings.json"
#define CONFIG_FILENAME_RANDR "randr.json"
#define CONFIG_FILENAME_RULES "rules.json"
#define CONFIG_FILENAME_SESSION "session.json"
#define CONFIG_FILENAME_MENU "menu.json"
#define CONFIG_FILENAME_MEMGUARD "memguard.json"
#define CONFIG_FILENAME_A11Y "a11y.json"

/* Initial maximum number of screens, and of desktops per screen
 *
 * Both are smaller under 'COMPACT'; 'defs/compact.h' has an extended
 * comment on that topic.  Each screen's worth of desktops nests
 * inside every screen slot, so this pair sizes a genuinely
 * multiplicative chunk of 'config_base_s', not just two independent
 * numbers. */
#ifdef COMPACT
#define CONFIG_MAX_SCREENS (1)
#define CONFIG_MAX_DESKTOPS (4)
#else
#define CONFIG_MAX_SCREENS (6)
#define CONFIG_MAX_DESKTOPS (16)
#endif

/* Maximum pannable-viewport size, in whole screens per axis
 *
 * 'columns' and 'rows' are each capped independently against this;
 * their product is never taken, so this alone cannot overflow a
 * 32-bit accumulator the way 'CONFIG_MAX_DESKTOPS' pairs with
 * 'desktop_layout' can.  Smaller under 'COMPACT' for the same reason
 * as the pair above. */
#ifdef COMPACT
#define CONFIG_VIEWPORT_MAX_PAGES (4)
#else
#define CONFIG_VIEWPORT_MAX_PAGES (16)
#endif

/* Viewport mesh limits and defaults
 *
 * The mesh is the dot pattern painted on the root window so that
 * panning a viewport larger than one screen is visibly a movement
 * rather than a silent jump.  Every one of these is applied by
 * 'config/base/load.c': an absent key takes the '_DEFAULT', while a
 * present but out-of-range one is corrected to whichever bound it
 * crossed, with a warning naming both values.
 *
 * The spacing minimum keeps the dots far enough apart to read as a
 * mesh rather than as a solid tint, and its maximum keeps at least a
 * few dots on screen at ordinary resolutions.  The thickness maximum
 * is not a constant but a fraction of the smaller spacing, since a
 * dot approaching its own spacing fills the tile completely;
 * 'CONFIG_VIEWPORT_MESH_THICKNESS_DIVISOR' is that fraction.  The
 * tone shift is a percentage away from the desktop background color,
 * and its minimum is where the mesh stops being visible against that
 * background at all. */
#define CONFIG_VIEWPORT_MESH_SPACING_DEFAULT (64)
#define CONFIG_VIEWPORT_MESH_SPACING_MIN (8)
#define CONFIG_VIEWPORT_MESH_SPACING_MAX (512)
#define CONFIG_VIEWPORT_MESH_THICKNESS_DEFAULT (1)
#define CONFIG_VIEWPORT_MESH_THICKNESS_MIN (1)
#define CONFIG_VIEWPORT_MESH_THICKNESS_DIVISOR (4)
#define CONFIG_VIEWPORT_MESH_TONE_SHIFT_DEFAULT (20)
#define CONFIG_VIEWPORT_MESH_TONE_SHIFT_MIN (10)
#define CONFIG_VIEWPORT_MESH_TONE_SHIFT_MAX (100)

/* XRandR output profile configuration limits
 *
 * 'CONFIG_RANDR_MAX_OUTPUTS' is also smaller under 'COMPACT', for the
 * same reason as 'CONFIG_MAX_SCREENS' above.  A target that build is
 * meant for is unlikely to drive many outputs at once regardless.  The
 * ordinary value covers real setups with several outputs across
 * multiple GPUs (a common shape in control rooms, digital signage, or
 * multi-card workstations), not just a typical single-GPU laptop or
 * desktop. */
/* Maximum number of per-output profiles */
#ifdef COMPACT
#define CONFIG_RANDR_MAX_OUTPUTS (2)
#else
#define CONFIG_RANDR_MAX_OUTPUTS (16)
#endif

/** Maximum length of an output name */
#define CONFIG_RANDR_OUTPUT_NAME_LENGTH (64)


#endif  /* ! DEFS_CONFIG_H */
