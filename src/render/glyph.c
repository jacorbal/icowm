/**
 * @file render/glyph.c
 *
 * @brief TrueType/OpenType text rendering fallback implementation
 */
/*
 * Copyright (c) 2026, J. A. Corbal.
 * All rights reserved.
 *
 * This file is licensed under the 'ISC License'.
 * Read the 'LICENSE' file in the root of this repository for details.
 */

/* System includes */
#include <stdbool.h>
#include <stdint.h>
#include <stdlib.h>     /* NULL, calloc, free */
#include <string.h>     /* memcpy, memset */

/* XCB includes */
#include <xcb/xcb.h>
#include <xcb/render.h>
#include <xcb/xcb_renderutil.h>

/* FreeType includes */
/* 'ft2build.h' declares no FreeType functions itself; it only defines
 * macros like 'FT_FREETYPE_H' that expand to the real header path
 * (@c <freetype/freetype.h> on this system), so client code stays
 * unaffected if that internal layout ever changes between versions */
#include <ft2build.h>
#include FT_FREETYPE_H

/* Fontconfig includes */
#include <fontconfig/fontconfig.h>

/* Utils includes */
#include <utils/safe/safestr.h>

/* Default initial values */
#include <defs/text.h>

/* Local includes */
#include <render/glyph.h>


/**
 * @brief One rasterized-and-uploaded glyph's cached metrics
 */
typedef struct {
    uint32_t codepoint;
    int16_t advance_x;
} s_glyph_cache_entry_td;



/**
 * @brief Global state for X Render glyph loading, caching, and drawing
 */
static struct {
    xcb_connection_t *connection;
    FT_Library ft_library;
    FT_Face ft_face;
    const xcb_render_query_pict_formats_reply_t *formats;
    xcb_render_pictformat_t a8_format;
    xcb_render_pictformat_t visual_format;
    xcb_render_glyphset_t glyphset;
    xcb_render_picture_t fg_picture;
    uint32_t fg_color;
    s_glyph_cache_entry_td cache[WM_TEXT_GLYPH_CACHE_MAX];
    int16_t ascent;
    int16_t descent;
    uint16_t cache_count;
    bool ft_ready;
    bool initialized;
    char font_name[256];
} s_glyph = {
    .connection = NULL,
    .font_name = {'\0'},
    .ft_ready = false,
    .formats = NULL,
    .a8_format = 0,
    .visual_format = 0,
    .glyphset = 0,
    .fg_picture = 0,
    .fg_color = 0xFFFFFFu,
    .ascent = 10,
    .descent = 3,
    .cache_count = 0u,
    .initialized = false
};


/**
 * @brief Decode the next UTF-8 codepoint from @p text
 *
 * Malformed sequences are treated permissively: an invalid leading
 * byte is returned as its own Latin-1 codepoint rather than rejecting
 * the whole string, since this draws UI text, not untrusted input,
 * and a best-effort result reads better than nothing at all.
 *
 * @param text   Null-terminated UTF-8 string
 * @param index  Byte offset to start decoding from; advanced past the
 *               consumed bytes on return
 *
 * @return The decoded codepoint, or 0 at the end of the string
 *
 * @note Complexity: @e O(1)
 */
uint32_t glyph_utf8_next(const char *text, size_t *index)
{
    unsigned char b0;
    uint32_t codepoint = 0u;
    int extra = 0;

    b0 = (unsigned char) text[*index];
    if (b0 == 0u) {
        return 0u;
    }

    if (b0 < 0x80u) {
        *index += 1u;
        return b0;
    }
    if ((b0 & 0xe0u) == 0xc0u) {
        codepoint = b0 & 0x1fu;
        extra = 1;
    } else if ((b0 & 0xf0u) == 0xe0u) {
        codepoint = b0 & 0x0fu;
        extra = 2;
    } else if ((b0 & 0xf8u) == 0xf0u) {
        codepoint = b0 & 0x07u;
        extra = 3;
    } else {
        *index += 1u;
        return b0;
    }

    *index += 1u;
    for (int i = 0; i < extra; ++i) {
        unsigned char bn = (unsigned char) text[*index];

        if ((bn & 0xc0u) != 0x80u) {
            return codepoint;
        }
        codepoint = (codepoint << 6) | (bn & 0x3fu);
        *index += 1u;
    }

    return codepoint;
}


/**
 * @brief Resolve @p font_name through fontconfig to a font file, face
 *        index, and pixel size
 *
 * @param font_name       Fontconfig pattern string, or a plain family
 *                        name
 * @param out_file        Buffer to receive the matched font file path
 * @param out_file_size   Size of @p out_file
 * @param out_face_index  Receives the face index within the file
 * @param out_pixel_size  Receives the matched pixel size
 *
 * @return @c true if a font was matched and a file path recovered
 *
 * @note Complexity: @e O(1), aside from fontconfig's own internal
 *       matching cost
 */
static bool s_resolve_font(const char *restrict font_name,
        char *restrict out_file,
        size_t out_file_size, int *restrict out_face_index,
        int *restrict out_pixel_size)
{
    FcPattern *pattern;
    FcPattern *matched;
    FcResult result;
    FcChar8 *file;
    int index;
    double pixel_size;
    bool ok = false;

    if (font_name == NULL || font_name[0] == '\0') {
        return false;
    }

    (void) FcInit();

    pattern = FcNameParse((const FcChar8 *) font_name);
    if (pattern == NULL) {
        return false;
    }

    FcDefaultSubstitute(pattern);
    if (FcConfigSubstitute(NULL, pattern, FcMatchPattern) == FcFalse) {
        FcPatternDestroy(pattern);
        return false;
    }

    matched = FcFontMatch(NULL, pattern, &result);
    FcPatternDestroy(pattern);
    if (matched == NULL || result != FcResultMatch) {
        if (matched != NULL) {
            FcPatternDestroy(matched);
        }
        return false;
    }

    if (FcPatternGetString(matched, FC_FILE, 0, &file) == FcResultMatch) {
        (void) safe_strncpy(out_file, (const char *) file,
                out_file_size - 1u);
        out_file[out_file_size - 1u] = '\0';
        ok = true;
    }

    index = 0;
    (void) FcPatternGetInteger(matched, FC_INDEX, 0, &index);
    *out_face_index = index;

    pixel_size = (double) WM_TEXT_GLYPH_DEFAULT_PIXEL_SIZE;
    (void) FcPatternGetDouble(matched, FC_PIXEL_SIZE, 0, &pixel_size);
    *out_pixel_size = (int) (pixel_size + 0.5);
    if (*out_pixel_size <= 0) {
        *out_pixel_size = WM_TEXT_GLYPH_DEFAULT_PIXEL_SIZE;
    }

    FcPatternDestroy(matched);
    return ok;
}


/**
 * @brief Free every xcb-render object the renderer currently owns,
 *        without touching the FreeType or fontconfig state
 *
 * Shared by @a glyph_renderer_destroy and by @a glyph_renderer_init
 * when reinitializing for a new font, since both need the previous
 * glyph set and picture gone before a new one is created.
 *
 * @note Complexity: @e O(1)
 */
static void s_render_objects_free(void)
{
    if (s_glyph.connection == NULL) {
        return;
    }

    if (s_glyph.fg_picture != 0) {
        xcb_render_free_picture(s_glyph.connection, s_glyph.fg_picture);
        s_glyph.fg_picture = 0;
    }
    if (s_glyph.glyphset != 0) {
        xcb_render_free_glyph_set(s_glyph.connection, s_glyph.glyphset);
        s_glyph.glyphset = 0;
    }
}


/**
 * @brief Look up @p codepoint's cached advance, rasterizing and
 *        uploading it to the glyph set first if this is the first
 *        time it is needed
 *
 * @param codepoint  Unicode codepoint to look up
 * @param out_advance Receives the glyph's horizontal advance in
 *                    pixels
 *
 * @return @c true if the glyph is now cached and usable; @c false if
 *         rasterization failed (a blank space-width advance is still
 *         written to @p out_advance in that case, so a missing glyph
 *         does not throw off the layout of the rest of the string)
 *
 * @note Complexity: @e O(c), where @e c is
 *       @c WM_TEXT_GLYPH_CACHE_MAX, for the linear cache lookup;
 *       @e O(1) amortized in practice since the cache is small and
 *       lookups cluster around a stable alphabet
 */
static bool s_glyph_ensure(uint32_t codepoint, int16_t *out_advance)
{
    FT_UInt glyph_index;
    const FT_Bitmap *bitmap;
    xcb_render_glyphinfo_t ginfo;
    uint32_t gid;
    uint16_t stride;
    uint8_t *padded;

    for (uint16_t i = 0u; i < s_glyph.cache_count; ++i) {
        if (s_glyph.cache[i].codepoint == codepoint) {
            *out_advance = s_glyph.cache[i].advance_x;
            return true;
        }
    }

    *out_advance = (int16_t) (s_glyph.ascent / 2);

    if (!s_glyph.ft_ready) {
        return false;
    }

    glyph_index = FT_Get_Char_Index(s_glyph.ft_face, codepoint);
    if (glyph_index == 0u) {
        return false;
    }
    if (FT_Load_Glyph(s_glyph.ft_face, glyph_index,
                FT_LOAD_RENDER | FT_LOAD_FORCE_AUTOHINT) != 0) {
        return false;
    }

    bitmap = &s_glyph.ft_face->glyph->bitmap;

    ginfo.x = (int16_t) (-s_glyph.ft_face->glyph->bitmap_left);
    ginfo.y = (int16_t) s_glyph.ft_face->glyph->bitmap_top;
    ginfo.width = (uint16_t) bitmap->width;
    ginfo.height = (uint16_t) bitmap->rows;
    ginfo.x_off = (int16_t) (s_glyph.ft_face->glyph->advance.x / 64);
    ginfo.y_off = (int16_t) (s_glyph.ft_face->glyph->advance.y / 64);

    /* Each glyph row is stored padded to a 4-byte boundary, per the
     * RENDER protocol's AddGlyphs image format */
    stride = (uint16_t) ((ginfo.width + 3u) & ~3u);
    padded = calloc((size_t) stride * (size_t) ginfo.height,
            sizeof(uint8_t));
    if (padded == NULL) {
        return false;
    }

    /* FreeType's own 'bitmap->pitch', not 'bitmap->width', is the real
     * byte stride between rows in 'bitmap->buffer': the rasterizer is
     * free to pad each row for its own alignment reasons, and a
     * negative pitch means the bitmap is stored bottom-up rather than
     * top-down (FreeType's own documented convention for either case).
     * Copying at a fixed 'width'-sized stride regardless would read
     * every row but the first from the wrong offset whenever the two
     * differ, and in the wrong order entirely for a bottom-up bitmap;
     * both silently produce a garbled or vertically flipped glyph
     * rather than any error to notice. */
    for (uint16_t row = 0u; row < ginfo.height; ++row) {
        const uint8_t *src_row = (bitmap->pitch >= 0)
            ? bitmap->buffer + (size_t) row * (size_t) bitmap->pitch
            : bitmap->buffer +
                (size_t) (ginfo.height - 1u - row) *
                    (size_t) (-bitmap->pitch);

        memcpy(padded + (size_t) row * (size_t) stride, src_row,
                bitmap->width);
    }

    gid = codepoint;
    (void) xcb_render_add_glyphs_checked(s_glyph.connection,
            s_glyph.glyphset, 1u, &gid, &ginfo,
            (uint32_t) stride * (uint32_t) ginfo.height, padded);
    free(padded);

    if (s_glyph.cache_count < WM_TEXT_GLYPH_CACHE_MAX) {
        s_glyph.cache[s_glyph.cache_count].codepoint = codepoint;
        s_glyph.cache[s_glyph.cache_count].advance_x = ginfo.x_off;
        ++s_glyph.cache_count;
    }

    *out_advance = ginfo.x_off;
    return true;
}


/**
 * @brief Advance width for one codepoint, ensuring its glyph exists
 *        first
 *
 * A thin wrapper around @c s_glyph_ensure returning the advance width
 * directly instead of through an output parameter, so a caller never
 * holds a local variable whose initialization depends on a call whose
 * own success or failure it does not otherwise care about; callers
 * that only need the width call this, callers that also need to know
 * whether the glyph was newly rendered (there are none currently, but
 * the distinction is real) would still call @c s_glyph_ensure
 * directly instead.
 *
 * @param codepoint Unicode codepoint to look up or render
 *
 * @return The glyph's advance width in pixels; the same fallback
 *         value @c s_glyph_ensure itself falls back to when the glyph
 *         cannot be rendered
 *
 * @note Complexity: @e O(1) amortized (see @c s_glyph_ensure)
 */
static int16_t s_glyph_advance_for(uint32_t codepoint)
{
    int16_t advance = 0;

    (void) s_glyph_ensure(codepoint, &advance);

    return advance;
}


/* Try to initialize the glyph renderer for the given font description */
int glyph_renderer_init(xcb_connection_t *connection,
        const char *font_name)
{
    char file_path[512];
    int face_index;
    int pixel_size;
    xcb_screen_t *screen;
    const xcb_render_pictforminfo_t *a8_info;
    const xcb_render_pictvisual_t *visual_info;
    xcb_render_color_t color;

    if (connection == NULL || font_name == NULL ||
            font_name[0] == '\0') {
        return -1;
    }

    if (s_glyph.initialized && s_glyph.connection == connection &&
            safe_strcmp(s_glyph.font_name, font_name) == 0) {
        return 0;
    }

    if (!s_resolve_font(font_name, file_path, sizeof(file_path),
                &face_index, &pixel_size)) {
        return -1;
    }

    glyph_renderer_destroy();

    if (FT_Init_FreeType(&s_glyph.ft_library) != 0) {
        return -1;
    }
    if (FT_New_Face(s_glyph.ft_library, file_path, face_index,
                &s_glyph.ft_face) != 0) {
        FT_Done_FreeType(s_glyph.ft_library);
        return -1;
    }
    if (FT_Set_Pixel_Sizes(s_glyph.ft_face, 0u,
                (FT_UInt) pixel_size) != 0) {
        FT_Done_Face(s_glyph.ft_face);
        FT_Done_FreeType(s_glyph.ft_library);
        return -1;
    }
    s_glyph.ft_ready = true;
    s_glyph.ascent = (int16_t) (s_glyph.ft_face->size->metrics.ascender
            / 64);
    s_glyph.descent = (int16_t) (-s_glyph.ft_face->size->metrics.descender
            / 64);

    screen = xcb_setup_roots_iterator(xcb_get_setup(connection)).data;
    if (screen == NULL) {
        glyph_renderer_destroy();
        return -1;
    }

    /* Cached across calls (re-fetched only when the connection itself
     * changes).  This is queried on every font switch otherwise, and
     * a single redraw pass can switch fonts many times (once per
     * differently styled label), which would otherwise mean a full
     * round trip to the X server for something that never actually
     * changes while the connection is open. */
    if (s_glyph.formats == NULL || s_glyph.connection != connection) {
        s_glyph.formats = xcb_render_util_query_formats(connection);
    }
    if (s_glyph.formats == NULL) {
        glyph_renderer_destroy();
        return -1;
    }

    a8_info = xcb_render_util_find_standard_format(s_glyph.formats,
            XCB_PICT_STANDARD_A_8);
    visual_info = xcb_render_util_find_visual_format(s_glyph.formats,
            screen->root_visual);
    if (a8_info == NULL || visual_info == NULL) {
        glyph_renderer_destroy();
        return -1;
    }
    s_glyph.a8_format = a8_info->id;
    s_glyph.visual_format = visual_info->format;

    s_glyph.connection = connection;
    s_glyph.glyphset = xcb_generate_id(connection);
    (void) xcb_render_create_glyph_set_checked(connection,
            s_glyph.glyphset, s_glyph.a8_format);

    color.red = (uint16_t) (((s_glyph.fg_color >> 16) & 0xffu) * 257u);
    color.green = (uint16_t) (((s_glyph.fg_color >> 8) & 0xffu) * 257u);
    color.blue = (uint16_t) ((s_glyph.fg_color & 0xffu) * 257u);
    color.alpha = 0xffffu;
    s_glyph.fg_picture = xcb_generate_id(connection);
    (void) xcb_render_create_solid_fill_checked(connection,
            s_glyph.fg_picture, color);

    (void) safe_strncpy(s_glyph.font_name, font_name,
            sizeof(s_glyph.font_name) - 1u);
    s_glyph.font_name[sizeof(s_glyph.font_name) - 1u] = '\0';
    s_glyph.cache_count = 0u;
    s_glyph.initialized = true;

    return 0;
}


/* Destroy the glyph renderer's resources */
void glyph_renderer_destroy(void)
{
    s_render_objects_free();

    if (s_glyph.ft_ready) {
        FT_Done_Face(s_glyph.ft_face);
        FT_Done_FreeType(s_glyph.ft_library);
        s_glyph.ft_ready = false;
    }

    s_glyph.connection = NULL;
    s_glyph.font_name[0] = '\0';
    s_glyph.cache_count = 0u;
    s_glyph.initialized = false;
}


/* Update the foreground and background colors used to draw text */
void glyph_renderer_set_color(uint32_t fg, uint32_t bg)
{
    xcb_render_color_t color;

    (void) bg;

    s_glyph.fg_color = fg;

    if (!s_glyph.initialized || s_glyph.connection == NULL) {
        return;
    }

    if (s_glyph.fg_picture != 0) {
        xcb_render_free_picture(s_glyph.connection, s_glyph.fg_picture);
    }

    color.red = (uint16_t) (((fg >> 16) & 0xffu) * 257u);
    color.green = (uint16_t) (((fg >> 8) & 0xffu) * 257u);
    color.blue = (uint16_t) ((fg & 0xffu) * 257u);
    color.alpha = 0xffffu;
    s_glyph.fg_picture = xcb_generate_id(s_glyph.connection);
    (void) xcb_render_create_solid_fill_checked(s_glyph.connection,
            s_glyph.fg_picture, color);
}


/* Draw a UTF-8 string at the specified baseline position */
void glyph_draw_string(xcb_connection_t *connection,
        xcb_drawable_t drawable, struct position_s pos, const char *text)
{
    uint32_t codepoints[WM_TEXT_GLYPH_MAX_STRING_LENGTH];
    uint32_t len;
    size_t byte_index;
    xcb_render_picture_t dst_picture;
    xcb_render_util_composite_text_stream_t *stream;

    if (connection == NULL || drawable == XCB_NONE || text == NULL ||
            !s_glyph.initialized || s_glyph.connection != connection) {
        return;
    }

    len = 0u;
    byte_index = 0u;
    while (len < WM_TEXT_GLYPH_MAX_STRING_LENGTH) {
        uint32_t codepoint = glyph_utf8_next(text, &byte_index);

        if (codepoint == 0u) {
            break;
        }
        (void) s_glyph_advance_for(codepoint);
        codepoints[len] = codepoint;
        ++len;
    }
    if (len == 0u) {
        return;
    }

    dst_picture = xcb_generate_id(connection);
    (void) xcb_render_create_picture_checked(connection, dst_picture,
            drawable, s_glyph.visual_format, 0u, NULL);

    stream = xcb_render_util_composite_text_stream(s_glyph.glyphset,
            len, 0u);
    if (stream != NULL) {
        xcb_render_util_glyphs_32(stream, (int16_t) pos.x,
                (int16_t) pos.y, len, codepoints);
        xcb_render_util_composite_text(connection,
                XCB_RENDER_PICT_OP_OVER, s_glyph.fg_picture, dst_picture,
                0u, 0, 0, stream);
        xcb_render_util_composite_text_free(stream);
    }

    xcb_render_free_picture(connection, dst_picture);
}


/* Measure the rendered width of a UTF-8 string */
uint16_t glyph_measure_string(const char *text)
{
    int32_t total;
    size_t byte_index;

    if (text == NULL || !s_glyph.initialized) {
        return 0u;
    }

    total = 0;
    byte_index = 0u;
    while (true) {
        uint32_t codepoint = glyph_utf8_next(text, &byte_index);

        if (codepoint == 0u) {
            break;
        }
        total += s_glyph_advance_for(codepoint);
        if (total > (int32_t) UINT16_MAX) {
            return UINT16_MAX;
        }
    }

    return (uint16_t) ((total < 0) ? 0 : total);
}


/* Pixels the baseline sits below the top of a line, for the current
 * font */
int16_t glyph_font_ascent(void)
{
    return s_glyph.ascent;
}


/* Pixels the baseline sits above the bottom of a line, for the
 * current font */
int16_t glyph_font_descent(void)
{
    return s_glyph.descent;
}
