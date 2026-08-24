/**
 * @file defs/text.h
 *
 * @brief Buffer sizes and capacity limits for text rendering
 *
 * Groups the constants shared by the two halves of the text renderer:
 * the font-description parser that turns a configuration string into
 * an XLFD pattern (see @c render/text.c), and the FreeType glyph
 * backend that rasterizes and caches what gets drawn (see
 * @c render/glyph.c).
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

#ifndef DEFS_TEXT_H
#define DEFS_TEXT_H


/* Font description parsing */
/**
 * @brief Maximum number of whitespace-separated tokens one font
 *        description string is split into
 *
 * A description names a family, an optional pixel size, up to three
 * style keywords, and an optional charset spec, so anything past this
 * is a malformed entry rather than a longer legitimate one
 */
#define WM_TEXT_FONT_MAX_TOKENS (8u)

/** Size of one font description token buffer, including the null
 *  terminator */
#define WM_TEXT_FONT_TOKEN_LENGTH (64)

/** Size of the assembled font family name buffer, including the null
 *  terminator */
#define WM_TEXT_FONT_FAMILY_LENGTH (128)

/**
 * @brief Size of each half of a charset spec, including the null
 *        terminator
 *
 * Both the registry ('iso8859') and the encoding ('1') halves of a
 * trailing charset token are cut from a single token, so neither can
 * be longer than one
 */
#define WM_TEXT_FONT_CHARSET_LENGTH (WM_TEXT_FONT_TOKEN_LENGTH)


/* Glyph rasterization and caching */
/**
 * @brief Maximum distinct codepoints the glyph renderer will cache
 *        and upload for one font
 *
 * A window manager's own text uses a small, stable alphabet, so a
 * flat array with linear search is simpler than a hash table and fast
 * enough at this size
 */
#define WM_TEXT_GLYPH_CACHE_MAX (512u)

/** Fallback pixel size when fontconfig's match does not resolve one */
#define WM_TEXT_GLYPH_DEFAULT_PIXEL_SIZE (12)

/**
 * @brief Maximum codepoints drawn or measured in a single call
 *
 * Long enough for any label this window manager itself draws (window
 * titles, menu entries, dialog text)
 */
#define WM_TEXT_GLYPH_MAX_STRING_LENGTH (512u)


#endif  /* ! DEFS_TEXT_H */
