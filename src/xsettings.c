/**
 * @file xsettings.c
 *
 * @brief Built-in XSETTINGS manager implementation
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
#include <stdio.h>      /* snprintf */
#include <stdlib.h>     /* free, malloc */
#include <string.h>     /* memset, memcpy */

/* XCB includes */
#include <xcb/xcb.h>

/* ADT includes */
#include <adt/list.h>

/* Utils includes */
#include <utils/safe/safestr.h>
#include <utils/xcb/atom.h>
#include <utils/xcb/selection.h>

/* Project includes */
#include <config.h>
#include <logger.h>
#include <surface.h>
#include <wm.h>

/* Local includes */
#include <xsettings.h>


/**
 * @brief Module-level built-in XSETTINGS manager state
 *
 * A single instance for the whole window manager, matching
 * @c config.xsettings being a single global (not per-surface) setting.
 */
static struct {
    bool is_window_ready;           /**< Window created, atoms interned;
                                         persists across is-enabled
                                         toggles */
    bool is_selection_owned;        /**< Currently owns the
                                         @c _XSETTINGS_Sn selection */
    xcb_connection_t *connection;
    surface_td *surface;
    xcb_window_t window;            /**< Settings-holder window */
    xcb_atom_t selection_atom;      /**< @c _XSETTINGS_Sn */
    xcb_atom_t settings_atom;       /**< @c _XSETTINGS_SETTINGS */
    xcb_atom_t manager_atom;        /**< @c MANAGER */
    uint32_t serial;                /**< Incremented on every publish */
    char gtk_theme_name[CONFIG_MAX_LENGTH_NAME];
    char icon_theme_name[CONFIG_MAX_LENGTH_NAME];
    char cursor_theme_name[CONFIG_MAX_LENGTH_NAME];
    unsigned int cursor_theme_size;
    unsigned int dpi;
} s_xs;



/**
 * @brief Padded length of an XSETTINGS name/value byte string
 *
 * XSETTINGS pads every variable-length byte string to a multiple of
 * 4 bytes.
 *
 * @param len Unpadded length in bytes
 *
 * @return @p len rounded up to the next multiple of 4
 */
static size_t s_xs_padded_len(size_t len)
{
    size_t rem = len % 4u;

    return (rem == 0u) ? len : (len + (4u - rem));
}


/**
 * @brief Serialized size of one Integer-type XSETTINGS entry
 *
 * @param name Setting name (e.g., "Xft/DPI")
 *
 * @return Size in bytes this entry occupies in the property
 */
static size_t s_xs_int_entry_size(const char *name)
{
    return 1u + 1u + 2u + s_xs_padded_len(safe_strlen(name)) + 4u + 4u;
}


/**
 * @brief Serialized size of one String-type XSETTINGS entry
 *
 * @param name  Setting name (e.g., "Net/ThemeName")
 * @param value Setting value
 *
 * @return Size in bytes this entry occupies in the property
 */
static size_t s_xs_string_entry_size(const char *restrict name,
        const char *restrict value)
{
    return 1u + 1u + 2u + s_xs_padded_len(safe_strlen(name)) + 4u
        + 4u + s_xs_padded_len(safe_strlen(value));
}


/**
 * @brief Append a little-endian @c CARD16 to a buffer and advance the
 *        offset
 */
static void s_xs_put_u16(uint8_t *buf, size_t *off, uint16_t v)
{
    buf[*off + 0u] = (uint8_t) (v & 0xffu);
    buf[*off + 1u] = (uint8_t) ((v >> 8) & 0xffu);
    *off += 2u;
}


/**
 * @brief Append a little-endian @c CARD32 to a buffer and advance the
 *        offset
 */
static void s_xs_put_u32(uint8_t *buf, size_t *off, uint32_t v)
{
    buf[*off + 0u] = (uint8_t) (v & 0xffu);
    buf[*off + 1u] = (uint8_t) ((v >> 8) & 0xffu);
    buf[*off + 2u] = (uint8_t) ((v >> 16) & 0xffu);
    buf[*off + 3u] = (uint8_t) ((v >> 24) & 0xffu);
    *off += 4u;
}


/**
 * @brief Append raw bytes, then zero padding up to a multiple of 4
 */
static void s_xs_put_padded_bytes(uint8_t *buf, size_t *off,
        const char *src, size_t len)
{
    size_t padded;

    memcpy(buf + *off, src, len);
    *off += len;

    padded = s_xs_padded_len(len);
    for (size_t i = len; i < padded; ++i) {
        buf[*off] = 0u;
        *off += 1u;
    }
}


/**
 * @brief Write one Integer-type XSETTINGS entry into a buffer
 *
 * @param buf    Destination buffer
 * @param off    Current write offset; advanced past the entry
 * @param name   Setting name
 * @param serial Change serial to stamp this entry with
 * @param value  Integer value
 */
static void s_xs_write_int_entry(uint8_t *buf, size_t *off,
        const char *name, uint32_t serial, int32_t value)
{
    size_t name_len = safe_strlen(name);

    buf[*off] = (uint8_t) XS_TYPE_INTEGER;
    *off += 1u;
    buf[*off] = 0u;                         /* unused */
    *off += 1u;
    s_xs_put_u16(buf, off, (uint16_t) name_len);
    s_xs_put_padded_bytes(buf, off, name, name_len);
    s_xs_put_u32(buf, off, serial);
    s_xs_put_u32(buf, off, (uint32_t) value);
}


/**
 * @brief Write one String-type XSETTINGS entry into a buffer
 *
 * @param buf    Destination buffer
 * @param off    Current write offset; advanced past the entry
 * @param name   Setting name
 * @param serial Change serial to stamp this entry with
 * @param value  String value
 */
static void s_xs_write_string_entry(uint8_t *buf, size_t *off,
        const char *restrict name, uint32_t serial,
        const char *restrict value)
{
    size_t name_len = safe_strlen(name);
    size_t value_len = safe_strlen(value);

    buf[*off] = (uint8_t) XS_TYPE_STRING;
    *off += 1u;
    buf[*off] = 0u;                         /* unused */
    *off += 1u;
    s_xs_put_u16(buf, off, (uint16_t) name_len);
    s_xs_put_padded_bytes(buf, off, name, name_len);
    s_xs_put_u32(buf, off, serial);
    s_xs_put_u32(buf, off, (uint32_t) value_len);
    s_xs_put_padded_bytes(buf, off, value, value_len);
}


/**
 * @brief Build the serialized @c _XSETTINGS_SETTINGS property
 *
 * @param out_len Receives the buffer length in bytes
 *
 * @return Newly allocated buffer (caller frees it), or @c NULL on
 *         allocation failure
 */
static uint8_t *s_xs_build_property(uint32_t *out_len)
{
    size_t total;
    uint8_t *buf;
    size_t off;
    uint32_t dpi_1024;

    total = 4u + 4u + 4u;   /* byte-order+pad, serial, n-settings */
    total += s_xs_string_entry_size("Net/ThemeName",
            s_xs.gtk_theme_name);
    total += s_xs_string_entry_size("Net/IconThemeName",
            s_xs.icon_theme_name);
    total += s_xs_string_entry_size("Gtk/CursorThemeName",
            s_xs.cursor_theme_name);
    total += s_xs_int_entry_size("Gtk/CursorThemeSize");
    total += s_xs_int_entry_size("Xft/DPI");

    buf = (uint8_t *) malloc(total);
    if (buf == NULL) {
        return NULL;
    }

    off = 0u;
    buf[off] = (uint8_t) XS_BYTE_ORDER_LSB;
    off += 1u;
    buf[off] = 0u; buf[off + 1u] = 0u; buf[off + 2u] = 0u;  /* unused */
    off += 3u;
    s_xs_put_u32(buf, &off, s_xs.serial);
    s_xs_put_u32(buf, &off, 5u);   /* n-settings */

    s_xs_write_string_entry(buf, &off, "Net/ThemeName", s_xs.serial,
            s_xs.gtk_theme_name);
    s_xs_write_string_entry(buf, &off, "Net/IconThemeName", s_xs.serial,
            s_xs.icon_theme_name);
    s_xs_write_string_entry(buf, &off, "Gtk/CursorThemeName",
            s_xs.serial, s_xs.cursor_theme_name);
    s_xs_write_int_entry(buf, &off, "Gtk/CursorThemeSize", s_xs.serial,
            (int32_t) s_xs.cursor_theme_size);

    /* 'Xft/DPI' is conventionally published as the DPI value scaled by
     * 1024 (a fixed-point representation), per the specification */
    dpi_1024 = s_xs.dpi * 1024u;
    s_xs_write_int_entry(buf, &off, "Xft/DPI", s_xs.serial,
            (int32_t) dpi_1024);

    *out_len = (uint32_t) total;
    return buf;
}


/**
 * @brief Rebuild and publish the @c _XSETTINGS_SETTINGS property
 *
 * Increments the serial number, so applications watching the property
 * (via a @c PropertyNotify selected on @c s_xs.window) see the change,
 * even if the new content happens to be byte-identical to the old.
 */
static void s_xs_publish(void)
{
    uint8_t *buf;
    uint32_t len;

    if (!s_xs.is_window_ready) {
        return;
    }

    s_xs.serial += 1u;

    buf = s_xs_build_property(&len);
    if (buf == NULL) {
        LOGGER_WARNING("Failed to allocate XSETTINGS property buffer;" \
                " published settings not updated", L_NARG);
        return;
    }

    xcb_change_property(s_xs.connection, XCB_PROP_MODE_REPLACE,
            s_xs.window, s_xs.settings_atom, s_xs.settings_atom,
            8, len, buf);
    xcb_flush(s_xs.connection);

    free(buf);

    LOGGER_DEBUG("Published XSETTINGS (serial=%u, theme='%s'," \
            " icons='%s', cursor='%s', cursor-size=%u, dpi=%u)",
            (unsigned int) s_xs.serial, s_xs.gtk_theme_name,
            s_xs.icon_theme_name, s_xs.cursor_theme_name,
            s_xs.cursor_theme_size, s_xs.dpi);
}


/**
 * @brief Copy @p wm's configured values into @c s_xs
 *
 * @param wm Window manager state
 */
static void s_xs_load_config(const wm_td *wm)
{
    config_td *config = wm_config(wm);

    safe_strncpy(s_xs.gtk_theme_name,
            config->theme.xsettings.theme.gtk_theme_name,
            sizeof(s_xs.gtk_theme_name));
    safe_strncpy(s_xs.icon_theme_name,
            config->theme.xsettings.theme.icon_theme_name,
            sizeof(s_xs.icon_theme_name));
    safe_strncpy(s_xs.cursor_theme_name,
            config->theme.xsettings.theme.cursor_theme_name,
            sizeof(s_xs.cursor_theme_name));
    s_xs.cursor_theme_size =
        config->theme.xsettings.theme.cursor_theme_size;
    s_xs.dpi = config->theme.xsettings.dpi;
}


/**
 * @brief Whether @p wm's configured values differ from what is
 *        currently published in @c s_xs
 *
 * @param wm Window manager state
 *
 * @return @c true if any published value would change
 */
static bool s_xs_config_changed(const wm_td *wm)
{
    config_td *config = wm_config(wm);

    return safe_strcmp(s_xs.gtk_theme_name,
                config->theme.xsettings.theme.gtk_theme_name) != 0 ||
        safe_strcmp(s_xs.icon_theme_name,
                config->theme.xsettings.theme.icon_theme_name) != 0 ||
        safe_strcmp(s_xs.cursor_theme_name,
                config->theme.xsettings.theme.cursor_theme_name) != 0 ||
        s_xs.cursor_theme_size !=
            config->theme.xsettings.theme.cursor_theme_size ||
        s_xs.dpi != config->theme.xsettings.dpi;
}


/**
 * @brief Create the settings window and intern its atoms, once
 *
 * Idempotent: does nothing (beyond returning success) if
 * @c s_xs.is_window_ready is already true.
 *
 * @param wm Window manager state
 *
 * @return @c true on success (or if already ready), @c false if it
 *         could not be created
 */
static bool s_xs_ensure_window(const wm_td *wm)
{
    surface_td *surface;
    char selection_name[24];
    uint32_t mask;
    uint32_t values[1];
    xcb_connection_t *connection = wm_connection(wm);
    list_td *surfaces = wm_surfaces(wm);

    if (s_xs.is_window_ready) {
        return true;
    }

    if (wm == NULL || connection == NULL || surfaces == NULL) {
        return false;
    }

    surface = (surface_td *) list_data(list_head(surfaces));
    if (surface == NULL || surface->screen == NULL) {
        return false;
    }

    s_xs.connection = connection;
    s_xs.surface = surface;

    (void) snprintf(selection_name, sizeof(selection_name),
            "_XSETTINGS_S%u", (unsigned int) surface->id);
    s_xs.selection_atom = atom_intern(connection, selection_name,
            false);
    s_xs.settings_atom = atom_intern(connection,
            "_XSETTINGS_SETTINGS", false);
    s_xs.manager_atom = atom_intern(connection, "MANAGER", false);

    if (s_xs.selection_atom == XCB_ATOM_NONE ||
            s_xs.settings_atom == XCB_ATOM_NONE) {
        LOGGER_WARNING("Failed to intern XSETTINGS atoms;" \
                " built-in XSETTINGS manager disabled", L_NARG);
        return false;
    }

    s_xs.window = xcb_generate_id(connection);
    mask = XCB_CW_OVERRIDE_REDIRECT;
    values[0] = 1;   /* override_redirect: never managed as a client */

    xcb_create_window(connection, XCB_COPY_FROM_PARENT,
            s_xs.window, surface->screen->root,
            -1, -1, 1, 1, 0,
            XCB_WINDOW_CLASS_INPUT_OUTPUT, XCB_COPY_FROM_PARENT,
            mask, values);
    xcb_flush(connection);

    s_xs.is_window_ready = true;
    return true;
}


/**
 * @brief Acquire the XSETTINGS selection and publish the settings
 *
 * @return @c true if ownership was acquired (or already held),
 *         @c false if another settings manager already owns it or the
 *         window is not ready yet
 */
static bool s_xs_acquire_selection(void)
{
    if (s_xs.is_selection_owned) {
        return true;
    }
    if (!s_xs.is_window_ready) {
        return false;
    }

    if (!util_xcb_acquire_manager_selection(s_xs.connection,
                s_xs.window, s_xs.selection_atom, s_xs.manager_atom,
                s_xs.surface->screen->root)) {
        LOGGER_NOTICE("Another XSETTINGS manager already owns the" \
                " settings selection; built-in one stays disabled",
                L_NARG);
        return false;
    }

    s_xs.is_selection_owned = true;

    LOGGER_INFO("XSETTINGS manager active on surface %u (selection" \
            " atom 0x%x)", s_xs.surface->id,
            (unsigned int) s_xs.selection_atom);

    return true;
}


/**
 * @brief Release the XSETTINGS selection, keeping the window
 */
static void s_xs_release_selection(void)
{
    if (!s_xs.is_selection_owned) {
        return;
    }

    xcb_set_selection_owner(s_xs.connection, XCB_NONE,
            s_xs.selection_atom, XCB_CURRENT_TIME);
    xcb_flush(s_xs.connection);
    s_xs.is_selection_owned = false;

    LOGGER_INFO("XSETTINGS selection released", L_NARG);
}


/* Acquire the XSETTINGS selection and publish the settings */
void xsettings_init(const wm_td *wm)
{
    const config_td *config = wm_config(wm);

    if (wm == NULL || config == NULL ||
            !config->theme.xsettings.is_enabled) {
        return;
    }

    s_xs_load_config(wm);

    if (!s_xs_ensure_window(wm)) {
        return;
    }
    if (!s_xs_acquire_selection()) {
        return;
    }

    s_xs_publish();
}


/* Fully tear down: release the selection and destroy the window */
void xsettings_shutdown(wm_td *wm)
{
    (void) wm;

    s_xs_release_selection();

    if (s_xs.is_window_ready && s_xs.connection != NULL &&
            s_xs.window != XCB_WINDOW_NONE) {
        xcb_destroy_window(s_xs.connection, s_xs.window);
        xcb_flush(s_xs.connection);
    }

    memset(&s_xs, 0, sizeof(s_xs));
}


/* React to a configuration reload */
void xsettings_reload(const wm_td *wm)
{
    bool should_be_enabled;
    config_td *config = wm_config(wm);

    if (wm == NULL || config == NULL) {
        return;
    }

    should_be_enabled = config->theme.xsettings.is_enabled;

    if (s_xs.is_selection_owned && !should_be_enabled) {
        LOGGER_INFO("XSETTINGS disabled by configuration reload", L_NARG);
        s_xs_release_selection();
        return;
    }

    if (!s_xs.is_selection_owned && should_be_enabled) {
        LOGGER_INFO("XSETTINGS enabled by configuration reload", L_NARG);
        s_xs_load_config(wm);
        if (s_xs_ensure_window(wm) && s_xs_acquire_selection()) {
            s_xs_publish();
        }
        return;
    }

    if (s_xs.is_selection_owned) {
        bool values_changed = s_xs_config_changed(wm);

        s_xs_load_config(wm);
        if (values_changed) {
            s_xs_publish();
        }
    }
}
